## Context

BartonCore's camera SBMD driver (`camera.sbmd.js`) implements the abstract session lifecycle endpoint (`ep/camera`) with `createSession`, `stream`, `destroySession`, and `sessionStatus` resources. When a client executes `stream`, the driver emits a `sessionStatus` event with `nextAction: "/devices/<id>/ep/webrtc/r/offerSdp"` — but this endpoint does not yet exist. The signaling exchange cannot proceed.

The Matter SDK build already provides generated headers for `WebRTCTransportProvider` (0x0553) and `WebRTCTransportRequestor` (0x0554) clusters. The SBMD runtime supports `commandHandlers` for incoming Matter commands, and `device.sendCommand()` for outgoing commands. The infrastructure is ready — we need to wire it together.

The reference app currently has generic `execResource`/`readResource` commands but no camera-specific workflow. Testing a camera stream requires 6+ manual commands with event monitoring. A dedicated command with integrated media rendering is needed for development and demonstration.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Current Architecture                                 │
│                                                                             │
│  Client (ref app)         Barton (camera.sbmd.js)            Camera         │
│  ═══════════════          ═══════════════════════            ══════          │
│                                                                             │
│  er(createSession) ──────► executeCreateSession()                           │
│  ◄── response: "1"        (allocates session)                               │
│                                                                             │
│  er(stream, "1") ────────► executeStream()                                  │
│  ◄── sessionStatus event   (emits setup + nextAction)                       │
│       nextAction: .../ep/webrtc/r/offerSdp                                  │
│                                                                             │
│  er(offerSdp, sdp) ─────► ??? ep/webrtc DOES NOT EXIST                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Goals / Non-Goals

**Goals:**
- Implement the `ep/webrtc` endpoint in the camera SBMD with full signaling resource set
- Map Barton resource executes to outgoing Matter commands (WebRTCTransportProvider cluster)
- Map incoming Matter commands (WebRTCTransportRequestor cluster) to Barton resource events
- Build a reference app `cameraStream` command that orchestrates the full flow with GStreamer media rendering
- Structure the webrtc endpoint code for future extraction to a standalone driver

**Non-Goals:**
- Adding a WebRTC media stack to Barton core (Barton is signaling-only)
- OpenHome or direct camera endpoint implementation
- STUN/TURN server configuration or NAT traversal beyond what ICE provides
- Multi-stream support or bidirectional audio
- Changes to BCoreClient public API
- Dynamic endpoint registration (future improvement — using static ZAP endpoint for now)
- Production-quality error recovery or session timeout management

## Decisions

### D1: WebRTC endpoint colocated in camera.sbmd.js

**Decision**: Add `ep/webrtc` as a second endpoint in `camera.sbmd.js` rather than creating a separate `webrtc.sbmd.js`.

**Rationale**: SBMD currently has no cross-driver composition mechanism. The session lifecycle in `ep/camera` and the signaling in `ep/webrtc` share transient data (session state, protocol info). Keeping them in one file allows shared access to session state via transient data supplements. The code is structured with clear separation (grouped constants, dedicated handler functions) so extraction is straightforward when SBMD composition becomes available.

**Alternatives considered**:
- Separate `webrtc.sbmd.js`: Would require a cross-driver data sharing mechanism that doesn't exist. The SBMD runtime matches one driver per device — a second driver for the same device type isn't supported.
- Native C++ driver for webrtc: Defeats the purpose of SBMD. The signaling is pure data transformation (resource values → TLV commands, TLV commands → resource events) which SBMD handles well.

### D2: Barton as signaling relay — no media involvement

**Decision**: Barton's webrtc endpoint relays signaling strings (SDP, ICE) between the client and camera via Matter commands. Barton never instantiates a peer connection or receives media.

**Rationale**: Barton is a protocol-agnostic device management library. The media consumer varies per deployment (mobile app, cloud service, reference app). Embedding a media stack would couple Barton to a specific rendering environment and add large dependencies (libdatachannel/GStreamer) to the core library.

```
┌────────────────────────────────────────────────────────────────────────────┐
│                         Target Architecture                                │
│                                                                            │
│  Ref App (WebRTC peer)      Barton (signaling relay)        Camera         │
│  ═════════════════════      ════════════════════════        ══════          │
│  GStreamer + webrtcbin      camera.sbmd.js                  Matter          │
│                                                                            │
│  1. er(createSession) ────► allocate session ──────────────────────────     │
│  2. er(stream, sid) ──────► emit sessionStatus(setup, webrtc, offerSdp)    │
│  3. [create local PC]                                                      │
│     [generate SDP offer]                                                   │
│  4. er(offerSdp, sdp) ───► sendCommand(0x0553, ProvideOffer, {sdp}) ──►    │
│                             ◄── Offer cmd (0x0554, {sdp_answer}) ─────     │
│  5. ◄── remoteSdp event ─── updateResource(webrtc, remoteSdp, answer)      │
│     [set remote SDP]                                                       │
│  6. er(offerIce, [...]) ──► sendCommand(0x0553, ProvideICE, {cands}) ──►   │
│                             ◄── ICECandidates cmd (0x0554, {cands}) ──     │
│  7. ◄── remoteIce event ── updateResource(webrtc, remoteIce, cands)        │
│     [add remote ICE]                                                       │
│  8. [media flows P2P] ◄═══════════════════════════════════════════════►    │
│  9. er(destroySession) ──► sendCommand(0x0553, EndSession) ───────────►    │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

**Thread safety**: All SBMD handler invocations run under `MQuickJsRuntime::GetMutex()`. The `device.sendCommand()` result terminal marshals to the Matter event loop. Incoming commands from Matter arrive via `g_main_context_invoke` before dispatching to SBMD. No additional synchronization needed.

### D2b: WebRTCTransportRequestor cluster added to ZAP endpoint

**Decision**: Add the `WebRTCTransportRequestor` cluster (0x0554) as a server cluster on Barton's existing endpoint in `barton-library.matter` and `barton-library.zap`. Handle its commands (Offer, Answer, ICECandidates, End) via the existing `CommandHandlerInterfaceRegistry` mechanism.

**Rationale**: When Barton sends `ProvideOffer` to the camera, it includes an `originatingEndpointID`. The camera sends signaling commands (Offer, ICECandidates, End) back to that endpoint. For the camera to accept this endpoint as a valid target, Barton must advertise cluster 0x0554 in its Descriptor cluster's server list. Without this, cameras may reject the target or fail to route the command.

**Alternatives considered**:
- Dynamic endpoint registration at runtime (like Matter SDK's `WebRTCTransportRequestorManager`): Cleaner long-term, but Barton has no existing dynamic endpoint infrastructure. Adding it is a separate effort.
- No ZAP change, rely on `CommandHandlerInterface` wildcard: The wildcard intercepts commands regardless of target endpoint, but the camera may refuse to send to an endpoint that doesn't advertise the cluster. Unreliable.

### D3: Reference app uses GStreamer webrtcbin — no Matter or libdatachannel dependency

**Decision**: The reference app uses GStreamer's `webrtcbin` element as its WebRTC peer. It does NOT use Matter's `WebRTCClient` class or link libdatachannel directly.

**Rationale**: The reference app is a Barton client — it talks exclusively through `BCoreClient` APIs. Using Matter's WebRTC classes would violate the abstraction boundary. GStreamer `webrtcbin` provides a complete WebRTC stack (SDP generation, ICE, DTLS/SRTP, media decode) in one element, and the reference app already lives in GLib/GObject land. No bridging or separate UDP forwarding needed.

**GStreamer pipeline structure**:
```
webrtcbin name=webrtc
    → decodebin → videoconvert → autovideosink (display mode)
    → decodebin → x264enc → mp4mux → filesink (file mode)
```

### D4: Command naming — `cameraStream` / `cs`

**Decision**: The reference app command is named `cameraStream` (short: `cs`), not generic `stream`.

**Rationale**: "stream" is too ambiguous — Barton may stream data/media from non-camera devices in the future. `cameraStream` clearly indicates camera video streaming. Follows existing reference app naming: `discoverStart`/`dstart`, `printDevice`/`pd`, `readResource`/`rr`.

**Usage**:
```
cameraStream <deviceId> [--file <path.mp4>]
```
Default: display output. If `--file` specified: record to file. If display unavailable and no `--file`: error with guidance.

### D5: Signaling flow maps to Matter WebRTC clusters

**Decision**: Map Barton resource operations to specific Matter cluster commands:

| Barton operation | Direction | Matter cluster | Command | ID |
|---|---|---|---|---|
| execute `offerSdp` | Barton → Camera | WebRTCTransportProvider (0x0553) | ProvideOffer | 0x02 |
| execute `offerIceCandidates` | Barton → Camera | WebRTCTransportProvider (0x0553) | ProvideICECandidates | 0x05 |
| event `remoteSdp` | Camera → Barton | WebRTCTransportRequestor (0x0554) | Offer | 0x00 |
| event `remoteIceCandidates` | Camera → Barton | WebRTCTransportRequestor (0x0554) | ICECandidates | 0x02 |
| execute `destroySession` | Barton → Camera | WebRTCTransportProvider (0x0553) | EndSession | 0x06 |

**Rationale**: These mappings follow from the Matter 1.5 camera specification. `SolicitOffer` (0x00) is used when the controller wants the camera to generate an offer — but in our flow, the client (reference app) generates the offer and passes it through Barton, so `ProvideOffer` is the primary path.

### D6: SDP and ICE payloads are opaque strings in Barton resources

**Decision**: SDP offers/answers are stored as plain string resource values. ICE candidates are JSON-encoded arrays of candidate strings. Barton does not parse, validate, or transform these payloads.

**Rationale**: Barton is a relay. SDP and ICE are negotiated between the actual WebRTC peers (client and camera). Parsing them would add fragile protocol-version-specific logic to the service layer with no benefit. The SBMD handler simply packages the string into TLV for the Matter command and unpacks TLV back to a string for the event.

## Risks / Trade-offs

- **[TLV encoding complexity in SBMD]** → The SBMD `device.sendCommand()` requires TLV-encoded payloads. SDP strings and ICE candidate lists must be serialized to Matter TLV format from JavaScript. Mitigation: Use `Sbmd.tlv` helpers already available in the runtime (base64 TLV encoding via `sbmd-tlv.js`).
- **[GStreamer availability on target platforms]** → The `cameraStream` command requires GStreamer + webrtcbin. Not all deployments will have this. Mitigation: Gate behind `BCORE_CAMERA_STREAM` CMake flag; command prints clear error if GStreamer unavailable at runtime.
- **[Session correlation between endpoints]** → The webrtc endpoint needs to know which session is active to correlate signaling. Mitigation: The session is stored in transient data; webrtc execute handlers read session state via supplements.
- **[No SolicitOffer support initially]** → Some cameras may expect the controller to call `SolicitOffer` first. The initial implementation uses `ProvideOffer` (client generates offer). Mitigation: Can add `SolicitOffer` path later as an alternative flow triggered by a flag or automatic detection.

## Open Questions

1. **Should `cameraStream` support a `--stun` flag for specifying a STUN server URL?** webrtcbin supports this via the `stun-server` property. Likely useful for testing across network boundaries but not needed for local dev.
2. **How should the reference app handle `sessionStatus: "error"` events during streaming?** Options: immediate teardown with error message, or retry logic. Leaning toward simple teardown for MVP.
