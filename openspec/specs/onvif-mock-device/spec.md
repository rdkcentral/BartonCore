# onvif-mock-device Specification

## Purpose
A test-only ONVIF camera mock used by the ONVIF driver integration tests. It answers WS-Discovery probes and serves canned ONVIF SOAP responses (using only the Python standard library), and runs a live GStreamer RTSP server plus a canned snapshot image so the ONVIF media plane can be validated end-to-end without a real camera.

## Requirements
### Requirement: Mock answers ONVIF WS-Discovery probes

A test-only ONVIF device mock SHALL listen for WS-Discovery `Probe` messages and respond with a
`ProbeMatch` that advertises a camera device type and a stable endpoint reference
(`urn:uuid:…`) plus the mock's ONVIF service address. The WS-Discovery and SOAP responder **logic**
SHALL use the Python standard library only (no third-party pip packages); the live RTSP server (see
below) uses the GStreamer GObject-Introspection bindings provided by the builder image, so the shared
mock module imports those GI bindings (guarded) at import time even though the responders themselves
do not depend on them.

#### Scenario: Mock responds to a discovery probe
- **WHEN** a WS-Discovery `Probe` for the ONVIF camera type is received
- **THEN** the mock SHALL send a `ProbeMatch` containing a `urn:uuid:…` endpoint reference and its ONVIF service address

#### Scenario: Discovery and SOAP use only the Python standard library
- **WHEN** the mock's WS-Discovery and SOAP responders are started in the test environment
- **THEN** they SHALL run without installing any pip package beyond the Python standard library

### Requirement: Mock serves canned ONVIF SOAP responses

The mock SHALL serve deterministic ONVIF SOAP responses over HTTP for `GetDeviceInformation`,
`GetProfiles`, `GetStreamUri`, and `GetSnapshotUri`, returning a fixed manufacturer/model/firmware, a
media profile token, an `rtsp://` stream URI, and an HTTP JPEG snapshot URI respectively. Returned
stream and snapshot URIs SHALL NOT contain embedded credentials.

#### Scenario: Mock returns device information
- **WHEN** the mock receives a `GetDeviceInformation` request
- **THEN** it SHALL return a fixed manufacturer, model, and firmware version

#### Scenario: Mock returns a credential-free stream URI
- **WHEN** the mock receives a `GetStreamUri` request for its media profile
- **THEN** it SHALL return an `rtsp://` URI that does not contain embedded credentials

#### Scenario: Mock returns a snapshot URI
- **WHEN** the mock receives a `GetSnapshotUri` request for its media profile
- **THEN** it SHALL return an HTTP JPEG URI

### Requirement: Mock publishes a live RTSP stream and snapshot image

The mock SHALL run a live RTSP server that publishes a dummy H.264 test pattern at the mount point
referenced by its `GetStreamUri` response, and SHALL serve a valid JPEG image at the URL referenced
by its `GetSnapshotUri` response, so the ONVIF media plane can be validated end-to-end without a
real camera.

#### Scenario: RTSP media URL streams live H.264
- **WHEN** a client opens the mock's reported `rtsp://` URI
- **THEN** the mock SHALL stream H.264 media so the client receives one or more media buffers

#### Scenario: Snapshot URL returns JPEG bytes
- **WHEN** a client fetches the mock's reported snapshot URL
- **THEN** the mock SHALL return a valid JPEG image (starting with the `0xFFD8` SOI marker)

### Requirement: Integration test drives the ONVIF flow through the public API

An integration test SHALL, using the public `BCoreClient` API against the mock, discover the mock
camera, execute `stream` and follow the returned entry point to obtain a `mediaUrl` event, and
execute `takePicture` and follow the returned entry point to obtain a `snapshotUrl` event.

#### Scenario: End-to-end discover, stream, and snapshot
- **WHEN** the integration test runs discovery for `camera`, sets credentials, and executes `stream` then `takePicture`
- **THEN** it SHALL observe the mock camera discovered, a `mediaUrl` event carrying the mock's `rtsp://` URI, and a `snapshotUrl` event carrying the mock's JPEG URI

#### Scenario: End-to-end media flows on the reported URLs
- **WHEN** the integration test opens the reported `mediaUrl` RTSP URI and fetches the reported `snapshotUrl`
- **THEN** it SHALL receive live RTSP media buffers and JPEG snapshot bytes
