# Adding Thread Border Router Support to Barton

If Thread support is enabled (`BCORE_THREAD`), Barton will attempt to communicate with the `otbr-agent` over D-Bus. Be sure to also enable Thread and Border Router support in the Matter SDK if Matter is also enabled.

## Runtime configuration

Barton reads the following properties from its property provider. Both are optional.

| Property | Default | Description |
| --- | --- | --- |
| `barton.thread.interfaceName` (`B_CORE_BARTON_THREAD_INTERFACE_NAME`) | `wpan0` | The `otbr-agent` Thread interface to use. It selects the D-Bus name `io.openthread.BorderRouter.<interface>`. Read once at startup. |
| `barton.thread.attachWaitSeconds` (`B_CORE_BARTON_THREAD_ATTACH_WAIT_SECONDS`) | `25` | How long network creation waits for the border router to attach, from 1 to 600 seconds. Slow links (for example, a radio reached over a network tunnel) may need more. |
| `barton.thread.defaultNetworkName` (`B_CORE_BARTON_DEFAULT_THREAD_NETWORK_NAME`) | | The name used when Barton creates a Thread network. |

The reference app also maps the environment variables `BARTON_THREAD_INTERFACE` and `BARTON_THREAD_ATTACH_WAIT_SECONDS` to the first two properties.

## Development with real radios

To develop against a physical Thread radio in the Docker environment, see [REMOTE_RADIO_FOR_DEVELOPMENT.md](REMOTE_RADIO_FOR_DEVELOPMENT.md).
