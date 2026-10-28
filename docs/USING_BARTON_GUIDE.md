# Using Barton

## Overview
Since Barton is ultimately a library, it is intended to be linked in with a host application. A reference application is provided in the top-level `reference` directory and should be the preferred example. The reference application should be the primary example of how to use Barton and its capabilities.

A client can easily provide a full IoT gateway service by incorporating the Barton library and routing requests and events between it and a local user interface or cloud service.

Barton's API is based upon GObject (the GLib Object System) in native C as the primary language.  Other language bindings are available through GObject's introspection layer, GIR.

## Basic Steps

### Preparing your Project

1. Configure Barton as appropriate through cmake options (see `config/cmake/options.cmake`)
2. If using Matter, configure and build your Matter SDK as described in [MATTER_SUPPORT.md]().
3. If using Zigbee, see [ZIGBEE_SUPPORT.md]().
4. If using OpenThread's Border Router, see [THREAD_BORDER_ROUTER_SUPPORT.md]().

### Interacting with Barton from your Code

1. Include `barton-core-client.h`
2. Create a `BCoreInitializeParamsContainer` with `b_core_initialize_params_container_new` and set the main storge directory with `b_core_initialize_params_container_set_storage_dir`
3. If Matter support is included, additional related configuration should be provided by calls to
   - `b_core_initialize_params_container_set_matter_storage_dir`
   - `b_core_initialize_params_container_set_matter_attestation_trust_store_dir`
   - `b_core_initialize_params_container_set_network_credentials_provider`
   - `b_core_initialize_params_container_set_sbmd_dirs`
4. Create the `BCoreClient` instance with `b_core_client_new`
5. Start the client with `b_core_client_start`
6. Wire up any desired events using `g_signal_connect`.  Some typical top-level signals of interest include:
   - `B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STARTED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DISCOVERY_STOPPED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_REJECTED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_ADDED`
   - `B_CORE_CLIENT_SIGNAL_NAME_ENDPOINT_ADDED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_COMPLETED`
   - `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_DISCOVERY_FAILED`

   Once devices are added, additional signals can be connected to get events from those as well.
7. Perform some action with the client such as commissioning a new Matter device with `b_core_client_commission_device` or changing a resource on a device to perform some action with `b_core_client_write_resource`

### Service Readiness and Device Reconfiguration

Barton occasionally rebuilds an existing device in the background (for example after a firmware
upgrade or a driver update), which replaces its stored data. To avoid handing out data that is about
to change, the single-device read functions (`b_core_client_get_device_by_id`, `_get_device_by_uri`,
`_get_endpoint_by_id`, `_get_endpoint_by_uri`, `_get_resource_by_uri`, `_read_resource`,
`_read_metadata`) and `b_core_client_write_metadata` can fail with a `GError` in the
`B_CORE_CLIENT_ERROR` domain:

- `B_CORE_SERVICE_STATE_ERROR_NOT_READY`: the service is not ready for device operation yet (or is
  shutting down). Readiness is announced by the `status-changed` event with reason
  `READY_FOR_DEVICE_OPERATION`, which may already have been sent before you connected to it. So
  connect to `B_CORE_CLIENT_SIGNAL_NAME_STATUS_CHANGED`, then check the `ready-for-operation` property of
  `b_core_client_get_status()`, and retry once either says the service is ready. Stopping the client
  waits for the handlers of the status-changed event that reports readiness (unless the stop is made
  from one of them), so don't block in such a handler on a thread that is stopping the client.
- `B_CORE_SERVICE_STATE_ERROR_DEVICE_RECONFIGURING`: the device is being reconfigured. Retry after
  `B_CORE_CLIENT_SIGNAL_NAME_DEVICE_CONFIGURATION_COMPLETED` or `_FAILED` arrives for that device with
  session discovery type `B_CORE_DISCOVERY_TYPE_NONE`. Those events are sent whenever a reconfiguration
  finishes, so they are also the cue to re-read a device you have cached. Connect to these signals
  before querying, so a reconfiguration that finishes in between is not missed. If the device is
  removed while it is being reconfigured, its configuration event arrives before `device-removed`,
  unless the reconfiguration takes longer than the removal's 60-second wait or you remove the device
  from that event's own handler. Removing the device or stopping the client waits for these
  events' handlers to return, so don't block in a handler on a thread that is doing either. No
  event is sent if the reconfiguration could not be scheduled, the device was removed before its
  reconfiguration started, the device's record is gone when it finishes (for example because saving
  the rebuilt device failed), or the service is shutting down, so also retry with a bounded backoff
  rather than waiting only for the event.

Treat both as "try again later", not "not found", and pass a `GError **` rather than `NULL` so you can
tell them apart. As with any GLib function, the `GError *` you pass must be `NULL` (a cleared error);
`b_core_client_read_resource` and `b_core_client_read_metadata` now check this and return `NULL` if it is
not. List functions (such as `b_core_client_get_devices`), `b_core_client_write_resource` and
`b_core_client_execute_resource` are never refused; lists include devices that are being reconfigured,
except for the brief moment in which a reconfiguration replaces the device's stored record. So don't
treat a device missing from one list as removed: rely on `device-removed` for that.

How long a device stays in reconfiguration depends on why it is being rebuilt. After a firmware
upgrade the drivers wait about 60 seconds for the device to restart before reconfiguring it. A
battery-powered ("sleepy") Zigbee device is only reconfigured when it next checks in, which can take
up to 30 minutes. If a reconfiguration cannot start at all, the device stays in this state until it is
removed or Barton restarts. Size your retries accordingly.

Signal handlers are callers like any other: a handler that looks a device or endpoint up with one of
the functions above (for example to add details to a `resource-updated` event) gets the same errors.
Use what the event already carries where you can (a `BCoreResource` includes its device UUID,
endpoint ID and URI), and otherwise retry the lookup after the configuration event for that device.

Configuration events for the same device can overlap if it is reconfigured again while the previous
reconfiguration's event is still being delivered. An older event can then arrive after a newer one, or
not at all, so treat each event as a cue to re-read the device (with a bounded retry), not as its
current state. In that case
a handler that removes the device can also wait up to 60 seconds for the other event's handlers.

`b_core_client_stop` lets a reconfiguration that is still running finish, and likewise a startup pass
that is still deciding which devices to reconfigure. Each wait lasts up to 120 seconds, so stopping can
take up to 240 seconds longer than usual. If your host ends the process sooner, the interrupted reconfiguration is
retried the next time Barton starts.

