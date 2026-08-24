## ADDED Requirements

### Requirement: Native ONVIF camera driver registers for the camera device class

A native ONVIF camera device driver SHALL be provided that registers itself with `deviceService`,
declares support for the `camera` device class, and coexists with the Matter camera driver. Device
ownership SHALL be determined by which driver discovers a device: the ONVIF driver SHALL only manage
devices it discovers via ONVIF WS-Discovery.

#### Scenario: Driver registers for the camera device class
- **WHEN** the service starts with the ONVIF driver enabled
- **THEN** a `DeviceDriver` whose `supportedDeviceClasses` includes `"camera"` SHALL be registered

#### Scenario: ONVIF and Matter camera drivers coexist
- **WHEN** both the ONVIF driver and the Matter camera driver are enabled
- **THEN** an ONVIF camera discovered via WS-Discovery SHALL be managed by the ONVIF driver AND a Matter camera commissioned via Matter SHALL be managed by the Matter driver

### Requirement: ONVIF cameras are discovered through the public discovery API

The driver SHALL implement `DeviceDriver.discoverDevices` such that a public discovery request for
the `camera` device class (`b_core_client_discover_start(["camera"], …)`) triggers an ONVIF
WS-Discovery `Probe` on the local network. The call SHALL return immediately and perform discovery on
a background thread. For each responding camera, the driver SHALL derive a stable device `uuid` from
the WS-Discovery ProbeMatch endpoint reference (`urn:uuid:…`), obtain manufacturer, model, and
firmware via an anonymous ONVIF `GetDeviceInformation`, and report the device with
`deviceServiceDeviceFound`.

#### Scenario: Discovery reports a responding ONVIF camera
- **WHEN** a discovery request for `camera` is active and an ONVIF camera answers the WS-Discovery Probe
- **THEN** the driver SHALL call `deviceServiceDeviceFound` with a `DeviceFoundDetails` whose `uuid` is derived from the ProbeMatch `urn:uuid` and whose `deviceClass` is `"camera"`

#### Scenario: Discovery starts without blocking
- **WHEN** `discoverDevices("camera")` is invoked
- **THEN** it SHALL return promptly AND run the WS-Discovery Probe on a background thread

### Requirement: Managed camera exposes the abstract camera endpoint and an ONVIF endpoint

During `configureDevice` the driver SHALL create an endpoint with id `"camera"` and profile
`"camera"` exposing `createSession`, `stream`, `takePicture`, and `destroySession` as executable
resources, and an endpoint with id `"onvif"` and profile `"onvif"` exposing:

- `getMediaUrl` — executable
- `mediaUrl` — string, event-emitting, non-cached (`RESOURCE_MODE_EMIT_EVENTS`, `CACHING_POLICY_NEVER`)
- `getSnapshotUrl` — executable
- `snapshotUrl` — string, event-emitting, non-cached (`RESOURCE_MODE_EMIT_EVENTS`, `CACHING_POLICY_NEVER`)
- `authRequired` — readable boolean (`RESOURCE_TYPE_BOOLEAN`, `RESOURCE_MODE_READABLE`)
- `username` — writable, sensitive (`RESOURCE_TYPE_USER_ID`, `RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE`)
- `password` — writable, sensitive (`RESOURCE_TYPE_PASSWORD`, `RESOURCE_MODE_WRITEABLE | RESOURCE_MODE_SENSITIVE`)

#### Scenario: Endpoints and resources are created
- **WHEN** an ONVIF camera is configured
- **THEN** the device SHALL have an `ep/camera` endpoint with the four lifecycle executes AND an `ep/onvif` endpoint exposing `getMediaUrl`, `mediaUrl`, `getSnapshotUrl`, `snapshotUrl`, `authRequired`, `username`, and `password`

### Requirement: Configuration is lazy and makes no authenticated ONVIF calls

`configureDevice` SHALL create endpoints and credential resources without performing any
authenticated ONVIF SOAP call. Authentication SHALL occur on-demand when `getMediaUrl` or
`getSnapshotUrl` is executed, using the credentials currently stored in the sensitive `username` and
`password` resources.

#### Scenario: No authenticated calls during configuration
- **WHEN** an ONVIF camera is configured before any credentials are written
- **THEN** configuration SHALL succeed AND the driver SHALL NOT issue an authenticated `GetProfiles`/`GetStreamUri`/`GetSnapshotUri` call

#### Scenario: Missing credentials produce an error at stream time
- **WHEN** `getMediaUrl` is executed while `username`/`password` are unset
- **THEN** the execute SHALL return an error indicating credentials are required AND SHALL NOT emit a `mediaUrl` event

### Requirement: stream springboards to the ONVIF media entry point

The `stream` execute on `ep/camera` SHALL return `{ "protocol": "onvif", "entryPoint":
"/<deviceId>/ep/onvif/r/getMediaUrl" }` as its synchronous result and SHALL ignore the value of the
`sessionId` argument (which MAY be null).

#### Scenario: stream returns the ONVIF media entry point
- **WHEN** a client executes `stream` on an ONVIF camera with any or no `sessionId`
- **THEN** the result SHALL be `{ "protocol": "onvif", "entryPoint": "/<deviceId>/ep/onvif/r/getMediaUrl" }`

### Requirement: getMediaUrl returns the RTSP media URL via GetStreamUri

Executing `getMediaUrl` on `ep/onvif` SHALL perform an ONVIF `GetStreamUri` SOAP call authenticated
with the stored credentials and SHALL emit the returned credential-free RTSP URL as a `mediaUrl`
event.

#### Scenario: getMediaUrl emits the RTSP URL
- **WHEN** `getMediaUrl` is executed with valid stored credentials
- **THEN** the driver SHALL emit a `mediaUrl` event whose value is the `rtsp://…` URL returned by `GetStreamUri` AND the URL SHALL NOT contain embedded credentials

### Requirement: takePicture springboards to the ONVIF snapshot entry point

The `takePicture` execute on `ep/camera` SHALL return `{ "protocol": "onvif", "entryPoint":
"/<deviceId>/ep/onvif/r/getSnapshotUrl" }` and SHALL ignore the `sessionId` argument. Executing
`getSnapshotUrl` on `ep/onvif` SHALL perform an ONVIF `GetSnapshotUri` SOAP call authenticated with
the stored credentials and SHALL emit the returned JPEG URL as a `snapshotUrl` event.

#### Scenario: takePicture returns the snapshot entry point
- **WHEN** a client executes `takePicture` on an ONVIF camera
- **THEN** the result SHALL be `{ "protocol": "onvif", "entryPoint": "/<deviceId>/ep/onvif/r/getSnapshotUrl" }`

#### Scenario: getSnapshotUrl emits the JPEG URL
- **WHEN** `getSnapshotUrl` is executed with valid stored credentials
- **THEN** the driver SHALL emit a `snapshotUrl` event whose value is the JPEG URL returned by `GetSnapshotUri`

### Requirement: authRequired signals credential need without exposing secrets

The `authRequired` resource SHALL indicate whether the client must apply credentials to the returned
media/snapshot URLs. The driver SHALL NOT place credentials in any resource value, event payload, or
returned URL; secrets SHALL reside only in the `SENSITIVE` credential resources.

#### Scenario: authRequired is readable and non-secret
- **WHEN** a client reads `authRequired` on `ep/onvif`
- **THEN** it SHALL return `"true"` for a camera that requires authentication AND no credential value SHALL be readable from any resource or event

### Requirement: ONVIF SOAP calls authenticate with WS-UsernameToken

Authenticated ONVIF SOAP requests SHALL include a WS-Security UsernameToken whose password digest is
`Base64(SHA1(nonce + created + password))`, computed from the stored credentials.

#### Scenario: Authenticated request carries a UsernameToken digest
- **WHEN** the driver issues an authenticated `GetStreamUri` or `GetSnapshotUri`
- **THEN** the request SHALL include a UsernameToken with a `nonce`, `created` timestamp, and password digest computed as `Base64(SHA1(nonce + created + password))`

### Requirement: Interim auth model is documented as a known limitation

The credential/auth model in this driver version SHALL be a single static per-device
`username`/`password` pair with no per-stream tokens, rotation, expiry, separate media accounts, or
configuration-driven provisioning. The driver source SHALL prominently document that this auth model
is interim and likely to be reworked once a device-configuration/onboarding mechanism exists, so it is
not mistaken for a finished design.

#### Scenario: Limitation is documented in the driver source
- **WHEN** the ONVIF driver source is reviewed
- **THEN** its module/header documentation SHALL state that the auth model is primitive/interim and subject to change with future configuration support

### Requirement: Driver is gated behind a build flag

The ONVIF driver SHALL be compiled only when the `BCORE_ONVIF` CMake option is enabled, which SHALL
define `BARTON_CONFIG_ONVIF`. The option SHALL default to off.

#### Scenario: Driver excluded by default
- **WHEN** the project is built without `BCORE_ONVIF`
- **THEN** the ONVIF driver sources SHALL NOT be compiled and no ONVIF driver SHALL be registered
