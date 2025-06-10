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

