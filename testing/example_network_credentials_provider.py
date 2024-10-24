from gi.repository import GObject
from gi.repository import BDeviceService


class ExampleNetworkCredentialsProvider(
    GObject.GObject, BDeviceService.NetworkCredentialsProvider
):
    __gtype_name__ = "ExampleNetworkCredentialsProvider2"

    def __init__(self):
        super().__init__()

    # Virtual method implementations must have prefix "do_" in pygobject!
    def do_get_wifi_network_credentials(
        self,
    ) -> BDeviceService.WifiNetworkCredentials:
        creds = BDeviceService.WifiNetworkCredentials()
        creds.props.ssid = "my_wifi_network"
        creds.props.psk = "my_wifi_password"
        return creds
