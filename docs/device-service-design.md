# Barton Device Service Overview
In Barton, Device Service is responsible for the management and interactions
with all devices.  Devices can connect with various protocols and technologies
including Zigbee, Matter, and OpenHome. Note that much of the discussion in this
documentation is about Zigbee devices, but many of the concepts applies to
devices on other technologies.  Devices belong to a Device Class which defines
the type of device.  Some examples of Device Classes are Camera, Light, Door Lock,
and Sensor (and many others).  Each Device Class supports one or more
endpoints, each of which supports an Endpoint Profile.  An endpoint on a device
can be thought of as a port number in networking.  An endpoint provides a
logical set of functionality such as a light switch.  For example, a single
physical device such as a refrigerator could have endpoints for temperature,
lights, door sensors, etc.

Devices are exposed to clients as a set of resources that can be read, written,
and executed depending on their type and purpose.  More details on how devices
are composed and how to interact with their resources are below.

![arch](device-service-arch.png)

# Hardware Dependencies
Device Service can function without any hardware dependencies, however its
capabilities might be reduced.  Below are the common potential hardware
dependencies depending on configuration.
## Zigbee
If Device Service is built with Zigbee support there should be an implementation of
ZHAL, which is the Zigbee Hardware Abstraction Layer.  This HAL is an atypical HAL
due to the high level application capabilities it is required to provide.  The intent
of ZHAL is to enable portability to different Zigbee stack and silicon vendors.  These
stacks typically provide high level features such as OTA software updating, however if
a stack does not provide these features, the ZHAL implementor must.
## Thread
If Device Service is built with Thread support it will backup and restore configuration
for a local OpenThread Border Router instance.  Additionally it will retrieve and use
the associated Thread network credentials when commissioning Matter over Thread devices.
Additional capabilities such as detailed Thread network health monitoring, Thread 1.4
ephemeral code commissioning and others are anticipated enhanceents.  Device Service
expects an OpenThread API over dbus.
## Bluetooth/BLE
If Device Service is built with BLE support, Matter devices may be commissioned directly
by Device Service for Thread or Wi-Fi Operational Network access.  Device Service, the
Matter stack actually, expects a Bluez API over dbus.
## Matter
Matter has no hardware requirements for basic functionality since it is an IPv6 based
specification.  It can make use of Thread and BLE for commissioning of new devices,
but after a device has been commissioned for the first time (by any admin/Fabric),
subsequent commissioning is performed only over IPv6.


# Key Concepts
## Device Descriptor List (aka Whitelist / Allowlist)
Devices that are allowed in our ecosystem are vetted by our Ecosystem team
which works with vendors to ensure their device is well behaved and tested.
Once a device passes our certification a Device Descriptor is created for it
and it is added to an 'allow list' (also known as the Whitelist).  This list is
published to a content server and downloaded by Zilker when it changes.

When a Zigbee device joins our network as part of the pairing process Zilker
reads its manufacturer name, model, hardware version, and firmware version.
These 4 attributes are used to locate the appropriate Device Descriptor.  If no
Descriptor is found, the device is rejected and told to leave the network.  An
example of a Device Descriptor is:

~~~xml
<DeviceDescriptor>
   <uuid>f7d49c2b-7324-4857-96eb-cdee9a44d996</uuid>
   <description>ZigBee_Gen2_PIR_Std</description>
   <category>zigbee</category>
   <manufacturer>Bosch</manufacturer>
   <model>RFPR-ZB-MS</model>
   <hardwareVersions>19</hardwareVersions>
   <firmwareVersions>0x21115401,0x22115401,0x23115401,0x23125401,0x23125402</firmwareVersions>
   <latestFirmware>
      <version>0x23125402</version>
      <filename>Bosch_Motion_G2_V23125402.ota</filename>
      <type>ota</type>
   </latestFirmware>
</DeviceDescriptor>
~~~

As shown, the Device Descriptor also contains information about the latest
firmware for the device.  This is downloaded and supplied to the device if it
needs a firmware upgrade.

## Resource Model
As previously described, devices use a URI based resource model to expose
functionality.  One architectural goal of the resource model is to hide the
underlying technology that the device uses (Zigbee, Matter, etc.).  This allows
clients of devices to function without change against devices of any class and
to get automatic support for new devices once new subsystems and device drivers
are added.

Here are example resources and their values for a door/window sensor:

~~~
000d6f000f486af8: Bedroom 1 Window 2, Class: sensor
    /000d6f000f486af8/r/battVolts = 2700
    /000d6f000f486af8/r/communicationFailure = true
    /000d6f000f486af8/r/dateAdded = 1575386127578
    /000d6f000f486af8/r/dateLastContacted = 1579810368616
    /000d6f000f486af8/r/feLqi = 255
    /000d6f000f486af8/r/feRssi = -84
    /000d6f000f486af8/r/firmwareUpdateStatus = (null)
    /000d6f000f486af8/r/firmwareVersion = 0x20190422
    /000d6f000f486af8/r/hardwareVersion = 1
    /000d6f000f486af8/r/lowBatt = false
    /000d6f000f486af8/r/manufacturer = Universal Electronics Inc
    /000d6f000f486af8/r/model = URC4460BC0-X-R
    /000d6f000f486af8/r/neLqi = 243
    /000d6f000f486af8/r/neRssi = -54
    /000d6f000f486af8/r/resetToFactoryDefaults = (null)
    /000d6f000f486af8/r/temp = 1900
    Endpoint 1: Profile: sensor
        /000d6f000f486af8/ep/1/r/bypassed = false
        /000d6f000f486af8/ep/1/r/faulted = false
        /000d6f000f486af8/ep/1/r/label = Bedroom 1 Window 2
        /000d6f000f486af8/ep/1/r/qualified = true
        /000d6f000f486af8/ep/1/r/tampered = false
        /000d6f000f486af8/ep/1/r/type = contactSwitch
~~~

Some resources are read-only such as /000d6f000f486af8/r/battVolts.  Some
resources are executable such as /000d6f000f486af8/r/resetToFactoryDefaults.
Resources like /000d6f000f486af8/ep/1/r/label are read-writable.

Most resources will emit events when they change (broadcast locally from Device
Service to other interested services).  In this example when the door/window
sensor is opened /000d6f000f486af8/ep/1/r/faulted will change to 'true' and
SecurityService will get notified to take appropriate action.

Note that the relationship between resources and Zigbee attributes is not
always 1:1.

## Device Drivers
Device Drivers are bits of code responsible for the mapping of resources to
functionality on a device.  The current set of drivers available in Zilker are:

* integratedPiezo
* m1Lte
* openHomeCamera
* philipsHue
* pim
* prm
* xbb
* zigbeeDoorLock
* zigbeeLegacyLight
* zigbeeLegacySecurityController
* zigbeeLegacySensor
* zigbeeLegacySirenRepeater
* zigbeeLight
* zigbeeLightController
* zigbeePresence
* zigbeeSecurityController
* zigbeeSensor
* zigbeeThermostat
* zigbeeWindowCovering

Drivers are responsible for the configuration of a device (performed during
pairing), creation of its resources, and reacting to changes from the device
and to its resources.

## Subsystems
Subsystems in Device Service provide access to lower level parts of the stack
in a manner safe for concurrent use by device drivers.  Currently Zilker has a
subsystem for Zigbee, Matter, and Thread.  Future subsystems for BLE and others are
possible.

## Database
Device Service stores all of its configuration in /opt/etc/storage/devicedb (on
RDK devices it is /nvram/icontrol/etc/storage/devicedb).  An example directory
listing is:

~~~
000d6f000f2081e7  000d6f000f4839e3  000d6f000f4877bc  000d6f0010abe463  systemProperties
000d6f000f4525fb  000d6f000f486af8  000d6f000f6ca643  6cc1d2000052829b
~~~

Note that there will also be backup versions of these files ending in .bak
which were omitted here.

Each file, other than systemProperties represents a single device.  For Zigbee
devices the filenames are EUI64s for the corresponding device.  Below is an
example of a Zigbee sensor's database file contents.

~~~json
{
    "deviceDriver": "ZigBeeSensorDD",
    "deviceClass":  "sensor",
    "deviceClassVersion":   1,
    "uri":  "/000d6f000f2081e7",
    "uuid": "000d6f000f2081e7",
    "deviceEndpoints":  {
        "1":    {
            "uri":  "/000d6f000f2081e7/ep/1",
            "id":   "1",
            "profile":  "sensor",
            "enabled":  true,
            "profileVersion":   0,
            "resources":    {
                "faulted":  {
                    "id":   "faulted",
                    "uri":  "/000d6f000f2081e7/ep/1/r/faulted",
                    "mode": 57,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "false",
                    "type": "com.icontrol.boolean"
                },
                "qualified":    {
                    "id":   "qualified",
                    "uri":  "/000d6f000f2081e7/ep/1/r/qualified",
                    "mode": 1,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "true",
                    "type": "com.icontrol.boolean"
                },
                "bypassed": {
                    "id":   "bypassed",
                    "uri":  "/000d6f000f2081e7/ep/1/r/bypassed",
                    "mode": 35,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "false",
                    "type": "com.icontrol.boolean"
                },
                "type": {
                    "id":   "type",
                    "uri":  "/000d6f000f2081e7/ep/1/r/type",
                    "mode": 1,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "motion",
                    "type": "com.icontrol.sensorType"
                },
                "tampered": {
                    "id":   "tampered",
                    "uri":  "/000d6f000f2081e7/ep/1/r/tampered",
                    "mode": 57,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "false",
                    "type": "com.icontrol.boolean"
                },
                "label":    {
                    "id":   "label",
                    "uri":  "/000d6f000f2081e7/ep/1/r/label",
                    "mode": 59,
                    "cachingPolicy":    1,
                    "dateOfLastSyncMillis": 0,
                    "value":    "Master Bedroom Motion",
                    "type": "com.icontrol.label"
                }
            },
            "metadatas":    {
                "zigbee_epid":  {
                    "id":   "zigbee_epid",
                    "uri":  "/000d6f000f2081e7/ep/1/m/zigbee_epid",
                    "value":    "1"
                },
                "migratedUserProperties":   {
                    "id":   "migratedUserProperties",
                    "uri":  "/000d6f000f2081e7/ep/1/m/migratedUserProperties",
                    "value":    {
                        "Zone.SensorId":    "29",
                        "Zone.FunctionType":    "10",
                        "Zone.ZoneType":    "3",
                        "orderIndex":   "18",
                        "Zone.Edited":  "true",
                        "Zone.Bypassed":    "false",
                        "Zone.TestMode":    "false",
                        "NearEndLqi":   "255",
                        "Zone.ZoneNumber":  "32",
                        "label":    "Master Bedroom Motion",
                        "NearEndRssi":  "-54",
                        "Zone.SwingerShutdown": "false",
                        "IASZoneStatus":    "32"
                    }
                },
                "migrationAlias":   {
                    "id":   "migrationAlias",
                    "uri":  "/000d6f000f2081e7/ep/1/m/migrationAlias",
                    "value":    "000d6f000f2081e7.1"
                },
                "secZone":  {
                    "id":   "secZone",
                    "uri":  "/000d6f000f2081e7/ep/1/m/secZone",
                    "value":    {
                        "zoneNumber":   32,
                        "sensorId": 29,
                        "displayIndex": 5,
                        "isTroubled":   false,
                        "isConfigured": true,
                        "inSwingerShutdown":    false,
                        "isInTestMode": false,
                        "isWirelessDevice": false,
                        "isBatteryDevice":  false,
                        "hasTemperature":   false,
                        "zoneType": 3,
                        "zoneFunction": 10,
                        "zoneMute": 0
                    }
                }
            }
        }
    },
    "deviceResources":  {
        "manufacturer": {
            "id":   "manufacturer",
            "uri":  "/000d6f000f2081e7/r/manufacturer",
            "mode": 1,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "Visonic",
            "type": "com.icontrol.string"
        },
        "model":    {
            "id":   "model",
            "uri":  "/000d6f000f2081e7/r/model",
            "mode": 1,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "MP-841",
            "type": "com.icontrol.string"
        },
        "hardwareVersion":  {
            "id":   "hardwareVersion",
            "uri":  "/000d6f000f2081e7/r/hardwareVersion",
            "mode": 1,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "1",
            "type": "com.icontrol.version"
        },
        "firmwareVersion":  {
            "id":   "firmwareVersion",
            "uri":  "/000d6f000f2081e7/r/firmwareVersion",
            "mode": 57,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "0x00000025",
            "type": "com.icontrol.version"
        },
        "firmwareUpdateStatus": {
            "id":   "firmwareUpdateStatus",
            "uri":  "/000d6f000f2081e7/r/firmwareUpdateStatus",
            "mode": 57,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "upToDate",
            "type": "com.icontrol.firmwareVersionStatus"
        },
        "dateAdded":    {
            "id":   "dateAdded",
            "uri":  "/000d6f000f2081e7/r/dateAdded",
            "mode": 1,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "1516290469399",
            "type": "com.icontrol.dateTime"
        },
        "dateLastContacted":    {
            "id":   "dateLastContacted",
            "uri":  "/000d6f000f2081e7/r/dateLastContacted",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "1579809077821",
            "type": "com.icontrol.dateTime"
        },
        "communicationFailure": {
            "id":   "communicationFailure",
            "uri":  "/000d6f000f2081e7/r/communicationFailure",
            "mode": 57,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 1582149774116,
            "value":    "true",
            "type": "com.icontrol.trouble"
        },
        "resetToFactoryDefaults":   {
            "id":   "resetToFactoryDefaults",
            "uri":  "/000d6f000f2081e7/r/resetToFactoryDefaults",
            "mode": 4,
            "cachingPolicy":    0,
            "dateOfLastSyncMillis": 0,
            "type": "com.icontrol.resetToFactoryOperation"
        },
        "feRssi":   {
            "id":   "feRssi",
            "uri":  "/000d6f000f2081e7/r/feRssi",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "-51",
            "type": "rssi"
        },
        "feLqi":    {
            "id":   "feLqi",
            "uri":  "/000d6f000f2081e7/r/feLqi",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "255",
            "type": "lqi"
        },
        "neRssi":   {
            "id":   "neRssi",
            "uri":  "/000d6f000f2081e7/r/neRssi",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "-54",
            "type": "rssi"
        },
        "neLqi":    {
            "id":   "neLqi",
            "uri":  "/000d6f000f2081e7/r/neLqi",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "255",
            "type": "lqi"
        },
        "temp": {
            "id":   "temp",
            "uri":  "/000d6f000f2081e7/r/temp",
            "mode": 89,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "1900",
            "type": "com.icontrol.temperature"
        },
        "lowBatt":  {
            "id":   "lowBatt",
            "uri":  "/000d6f000f2081e7/r/lowBatt",
            "mode": 57,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "false",
            "type": "com.icontrol.boolean"
        },
        "battVolts":    {
            "id":   "battVolts",
            "uri":  "/000d6f000f2081e7/r/battVolts",
            "mode": 121,
            "cachingPolicy":    1,
            "dateOfLastSyncMillis": 0,
            "value":    "3000",
            "type": "battery.voltage"
        }
    },
    "metadatas":    {
        "discoveredDetails":    {
            "id":   "discoveredDetails",
            "uri":  "/000d6f000f2081e7/m/discoveredDetails",
            "value":    {
                "eui64":    "000d6f000f2081e7",
                "manufacturer": "Visonic",
                "model":    "MP-841",
                "hwVer":    1,
                "fwVer":    37,
                "appVer":   1,
                "type": "unknown",
                "power":    "unknown",
                "endpoints":    [{
                        "id":   1,
                        "profileId":    260,
                        "deviceId": 1026,
                        "deviceVer":    0,
                        "serverClusterInfos":   [{
                                "id":   2821,
                                "isServer": true,
                                "attributeIds": [283, 284, 285]
                            }, {
                                "id":   0,
                                "isServer": true,
                                "attributeIds": [0, 1, 2, 3, 4, 5, 7]
                            }, {
                                "id":   1,
                                "isServer": true,
                                "attributeIds": [62, 32, 49, 51, 54, 55, 56, 57]
                            }, {
                                "id":   32,
                                "isServer": true,
                                "attributeIds": [1, 2, 3, 4, 6, 0]
                            }, {
                                "id":   1280,
                                "isServer": true,
                                "attributeIds": [0, 16, 1, 17, 2]
                            }, {
                                "id":   3,
                                "isServer": true,
                                "attributeIds": [0]
                            }, {
                                "id":   1026,
                                "isServer": true,
                                "attributeIds": [0, 1, 2]
                            }],
                        "clientClusterInfos":   [{
                                "id":   25,
                                "isServer": false,
                                "attributeIds": [0, 1, 2, 4, 6, 7, 8, 9, 10]
                            }]
                    }]
            }
        },
        "migratedDevice":   {
            "id":   "migratedDevice",
            "uri":  "/000d6f000f2081e7/m/migratedDevice",
            "value":    "true"
        },
        "lpmPolicy":    {
            "id":   "lpmPolicy",
            "uri":  "/000d6f000f2081e7/m/lpmPolicy",
            "value":    "armedAway"
        },
        "troubles": {
            "id":   "troubles",
            "uri":  "/000d6f000f2081e7/m/troubles",
            "value":    {
                "32548":    {
                    "_evId":    32543,
                    "_evCode":  255,
                    "_evVal":   0,
                    "_evTime":  1582149774139,
                    "troubleEvent": {
                        "trouble":  {
                            "troubleObj":   {
                                "description":  "sensor TROUBLE_REASON_COMM_FAIL",
                                "eventId":  32543,
                                "troubleId":    32548,
                                "acknowledged": false,
                                "restored": false,
                                "treatAsLifeSafety":    false,
                                "eventTime":    1582149774139,
                                "type": 5,
                                "reason":   4,
                                "critical": 2,
                                "indicationGroup":  1,
                                "indication":   3,
                                "extra":    {
                                    "rootId":   "000d6f000f2081e7",
                                    "ownerUri": "/000d6f000f2081e7",
                                    "resourceUri":  "/000d6f000f2081e7/r/communicationFailure",
                                    "deviceClass":  "sensor",
                                    "zoneNumber":   32,
                                    "zoneType": 3,
                                    "alarmArmType": 0,
                                    "alarmStatusType":  0
                                }
                            }
                        },
                        "panelStatus":  {
                            "systemPanelStatus":    {
                                "testModeSecsRemaining":    0,
                                "trouble":  true,
                                "bypassActive": false,
                                "alarmStatus":  2,
                                "armMode":  0
                            }
                        },
                        "alarm":    {
                            "alarmDetails": {
                                "contactId":    "138101005",
                                "alarmSessionId":   0,
                                "sendImmediately":  false,
                                "isZoneInSwingerShutdown":  false,
                                "isDuressCodeAlarm":    false,
                                "isExitError":  false,
                                "isRecentClosing":  false,
                                "isWithinCancelWindow": false,
                                "isZonesFaulted":   false,
                                "alarmReason":  0
                            }
                        }
                    }
                }
            }
        }
    }
}
~~~

The systemProperties file contains some Device Service specific configuration
details.  For example:

~~~json
{
    "ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT":   "false",
    "zigbeeTelemetryStartDate": "1588944626812",
    "currentDeviceDescriptorUrl":   "https://pod-bundle.beta.xhsyndication.com/bundle/lists/ConvergeDevicesAllowed20200624-Beta.xml",
    "currentDenylistMd5":  "e4999307cad3e2df24e46a0e4245a725",
    "ZIGBEE_NETWORK_CONFIG_DATA":   "oxMAAAQQAAAULkZFblsJTyShkzLZS/SvAAAAAAAAAAAAAAAAAAAAAAAAKUQDGQIBAABZsmTDjXOEauQQwl8Pn4utGwCaC297bXQGPhLSmvl/jF0KAPj/BwAAAAA=",
    "ZIGBEE_LOCAL_EUI64":   "001bad8b9f0f5fc2",
    "schemaVersion":    "1",
    "ZIGBEE_REJECT_UNKNOWN_DEVICES":    "true",
    "currentBlacklistUrl":  "http://pod-bundle.beta.xhsyndication.com/bundle/lists/blacklist-nothing.xml",
    "deviceDescriptorBypass":   "false",
    "currentDeviceDescriptorMd5":   "95c55b67c9cfe93318be19c7a6eab752"
}
~~~

## Device Communication Watchdog
Device Service has a component that is responsible for detecting communication
failure with the devices it manages.  Once the Device Communication Watchdog
determines that a device is in communication failure, the device's
communicationFailure resource (/000d6f000f486af8/r/communicationFailure from
the example device earlier) is set to "true".  This triggers a resource changed
event that is handled by Security Service which creates a trouble that gets
reported to the server through Comm Service.

For Zigbee devices, the watchdog tracks the amount of time since the last
successful message to or from the device.  By default the watchdog will report
communication failure after 1 hour without any successful communication.  This
allows for two missed checkin intervals, which are configured for 27 minutes.
Some devices use different failure detection intervals such as the M1 LTE
module which will detect communication failure in 200 seconds per requirements.

# Zigbee Support
These components are in the xhfw/zigbee git repository.
![zigbee-arch](zigbee-arch.png)

## ZigbeeSubsystem
The ZigbeeSubsystem is a layer in between Zigbee device drivers and ZHAL
responsible for safely sharing the Zigbee infrastructure between device
drivers.

## ZHAL
ZHAL is the Zigbee Hardware Abstraction Layer.  It is a C library
(source/libs/zhal) that converts function calls and events into IPC to the
Zigbee implementation.

## ZigbeeCore
ZigbeeCore is an implementation of the ZHAL API and is the lowest level process
that runs on the main CPE processor for Zigbee support.  It has the following
responsibilities:

* Provide access and functionality of a Zigbee Coordinator and Trust Center
* Receive requests from Device Service's Zigbee Subsystem
* Sends events to Zigbee Subsystem
* Serves firmware images to devices
* Manages firmware on the Zigbee processor

ZigbeeCore's log can be found at /tmp/logs/ZigbeeCore.log on non gateway
devices.  For gateways this log data is sent to standard output and therefore
does not appear in the standard log files.  It can be accessed with journalctl
(specify the 'zilker' unit).

## xNCP
The firmware that runs on the Zigbee processor is the lowest level of Zilker
related to Zigbee.  It is built upon Silicon Laboratories NCP framework that
handles the lower level radio and Zigbee Trust center among other things.
Comcast modifications include support for low power mode, RMA, and some Zigbee
3 security features.

# Firmware Management
Most Zigbee devices are compliant with the Over the Air Upgrade cluster's
firmware management mechanisms that were introduced with the Home Automation
profile version 1.2.  There are some device's in our ecosystem that were
created before this standard was available.  These devices we call 'legacy' and
their firmware is managed differently as discussed below.

As mentioned in the Device Descriptor section, when a new firmware image is
available for a device, the Device Descriptor List (aka whitelist/allowlist) is
updated with the new firmware version number and filename.  When this updated
list is published and downloaded by Zilker it is scanned for descriptors
matching paired devices.  If a paired device needs an upgrade its file is
downloaded from the content server and placed in /opt/zigbeeFirmware/ota (or
/nvram/icontrol/zigbeeFirmware/ota on gateways).  This upgrade mechanism is
"pulled" from the device through image block requests made to ZigbeeCore.  The
process starts when the device sends a "query next image" request which
contains its current firmware version.  The response will indicate if
ZigbeeCore has a newer image for it and the device will start pulling blocks
until it finishes.

This upgrade mechanism can take a while, especially on battery powered devices.
While the device is downloading the firmware image it remains fully functional
until the image is fully downloaded and verified at which point it reboots to
the new image.  Reboots typically take less than a second.

Note that once a firmware image for a device is downloaded an "image notify"
command is sent to the device in an attempt to start the upgrade immediately,
however many of our devices are "sleepy" so they will not receive this message.
Devices are required to check once a day, minimum, to see if ZigbeeCore has a
new firmware image available.

## Legacy (pre HA 1.2)
Devices that do not support the Over the Air Upgrade cluster include older
security sensors, the PIM, some older Centralite lighting modules, and the
older RTCoA thermostat.  This upgrade mechanism is risky due to the way the
packets are routed and because the device is non-functional for the duration of
the upgrade.  For these reasons we only upgrade these devices if there are
critical issues.

Similar to the above process, these firmware files are downloaded to
/opt/zigbeeFirmware/legacy (or /nvram/icontrol/zigbeeFirmware/legacy on
gateways).  This upgrade mechanism is a "push" model where ZigbeeCore requests
the device boot into its bootloader then starts pushing packets at it until
complete.  Once this process starts the device will not be functional until it
completes successfully since it overwrites its flash as packets are received.
If the process fails or is interrupted, the device will change to channel 13
and await rescue.

# xhDeviceUtil
Device Service has a utility called xhDeviceUtil which provides access to
devices and can control certain aspects of Device Service.  The utility can be
run either interactively (when no options are specified) or can be fed command
line arguments for non-interactive operation.  It has detailed help and offers
history with command callback and tab key initiated command completion when in
interactive mode.  The help output as of the time of this writing is:

~~~
deviceUtil> help
Core:
    history  : print the history of commands run interactively
    listDevices|list [device class] : list all devices, or all devices in a class
    getDeviceCountBySubsystem <subsystem> : Get the number of devices by subsystem
    Examples:
        getDeviceCountBySubsystem zigbee

    printDevice|pd <uuid> : print information for a device
    printAllDevices|pa [device class] : print information for all devices, or all devices in a class
    readResource|rr <uri> : read the value of a resource
    Examples:
        readResource /000d6f000aae8410/r/communicationFailure

    writeResource|wr <uri> [value] : write the value of a resource
    Examples:
        writeResource /000d6f000aae8410/ep/1/r/label "Front Door"

    execResource|er <uri> [value] : execute a resource
    queryResources|qr <uri pattern> : query resources with a pattern
    Examples:
        qr */lowBatt

    readMetadata|rm <uri> : read metadata
    Examples:
        rm /000d6f000aae8410/m/lpmPolicy

    writeMetadata|wm <uri> : write metadata
    Examples:
        wm /000d6f000aae8410/m/lpmPolicy never

    queryMetadata|qm <uri pattern> : query metadata through a uri pattern
    Examples:
        qm */rejoins

    discoverStart|dstart <device class> : Start discovery for a device class
    discoverRepairStart|drstart <device class> : Start discovery for orphaned devices in a device class
    discoverStop|dstop  : Stop device discovery
    removeDevice|rd <uuid> : Remove a device by uuid
    removeEndpoint|re <uri> : Remove an endpoint by uri
    dumpDevice|dd <uuid> : Dump all details about a device
    dumpAllDevices|dump  : Dump all details about all devices
    readSystemProperty <key> : Read a device service system property
    writeSystemProperty <key> [value] : Write a device service system property
    ddl override <path> | clearoverride | process | bypass | clearbypass : Configure and control device descriptor processing
    Examples:
        ddl override /opt/etc/AllowList.xml.override
        ddl clearoverride
        ddl process
        ddl bypass
        ddl clearbypass

    getStatus|gs  : Get the status of device service
Zigbee:
    zigbeeStatus|zs  : get the status of the zigbee subsystem
    zigbeeDumpCounters  : dump the current zigbee counters
    zigbeeNetworkMap  : Print the Zigbee network map
    zigbeeSetChannel <channel number or 0 to pick best> [dryrun] : Change the Zigbee channel
    Examples:
        zigbeeSetChannel 25
        zigbeeSetChannel 0 dryrun

    zigbeeEnergyScan [<duration mS> <number of scans> <channels to scan...>] : Scan for background noise on Zigbee channels
    Examples:
        zigbeeEnergyScan
        zigbeeEnergyScan 30 16 11 20 25
~~~

To see what devices are paired we use the "list" command:

~~~
deviceUtil> list
7894b4ef0ca1: Class: camera
    Endpoint camera: Profile: camera, Label: Front Door
    Endpoint sensor: Profile: sensor, Label: (null)
000d6f0004ae46af: Class: light
    Endpoint 1: Profile: light, Label: Garage Door Indicator
000d6f0012c529a0: Class: sensor
    Endpoint 1: Profile: sensor, Label: Back Door
d187bc81-927a-4ad6-8634-a74b38bf7c0c: Class: integratedPiezo
    Endpoint 1: Profile: warningDevice, Label: Integrated Piezo
e0606614be56: Class: camera
    Endpoint camera: Profile: camera, Label: Back Door
    Endpoint sensor: Profile: sensor, Label: (null)
0022a3000004e039: Class: light
    Endpoint 1: Profile: light, Label: Upstairs Nightlight
000d6f0012c47ebf: Class: sensor
    Endpoint 1: Profile: sensor, Label: Front Door
000d6f0012c6087b: Class: sensor
    Endpoint 1: Profile: sensor, Label: Garage Door
0022a300002f164c: Class: light
    Endpoint 1: Profile: light, Label: Garge Door Indicator Up
    Endpoint 3: Profile: lightSwitch, Label: (null)
~~~

To display the resources for a device in a user-friendly way we use
"printDevice" (or "pd" for short):

~~~
deviceUtil> printDevice 000d6f0004ae46af
000d6f0004ae46af: Garage Door Indicator, Class: light
    /000d6f0004ae46af/r/communicationFailure = true
    /000d6f0004ae46af/r/dateAdded = 1578949352641
    /000d6f0004ae46af/r/dateLastContacted = 1595336347583
    /000d6f0004ae46af/r/feLqi = 255
    /000d6f0004ae46af/r/feRssi = -13
    /000d6f0004ae46af/r/firmwareUpdateStatus = upToDate
    /000d6f0004ae46af/r/firmwareVersion = 0x10015110
    /000d6f0004ae46af/r/hardwareVersion = 1
    /000d6f0004ae46af/r/manufacturer = CentraLite
    /000d6f0004ae46af/r/model = 3420
    /000d6f0004ae46af/r/neLqi = 255
    /000d6f0004ae46af/r/neRssi = -35
    /000d6f0004ae46af/r/resetToFactoryDefaults = (null)
    Endpoint 1: Profile: light
        /000d6f0004ae46af/ep/1/r/currentLevel = 100
        /000d6f0004ae46af/ep/1/r/isDimmableMode = true
        /000d6f0004ae46af/ep/1/r/isOn = true
        /000d6f0004ae46af/ep/1/r/label = Garage Door Indicator
~~~

To perform an action on a device we write to a resource (or in rare cases,
execute a resource).  For example, to turn on a light:

~~~
deviceUtil> writeResource /0022a300002f164c/ep/1/r/isOn true

resourceUpdated: id=isOn, uri=/0022a300002f164c/ep/1/r/isOn, ownerId=1, ownerClass=light, value=true, type=com.icontrol.boolean, mode=0x7b (rw-del-)
deviceUtil>
resourceUpdated: id=currentLevel, uri=/0022a300002f164c/ep/1/r/currentLevel, ownerId=1, ownerClass=light, value=0, type=com.icontrol.lightLevel, mode=0x7b (rw-del-)

resourceUpdated: id=currentLevel, uri=/0022a300002f164c/ep/1/r/currentLevel, ownerId=1, ownerClass=light, value=35, type=com.icontrol.lightLevel, mode=0x7b (rw-del-)

resourceUpdated: id=currentLevel, uri=/0022a300002f164c/ep/1/r/currentLevel, ownerId=1, ownerClass=light, value=69, type=com.icontrol.lightLevel, mode=0x7b (rw-del-)

resourceUpdated: id=currentLevel, uri=/0022a300002f164c/ep/1/r/currentLevel, ownerId=1, ownerClass=light, value=99, type=com.icontrol.lightLevel, mode=0x7b (rw-del-)

resourceUpdated: id=currentLevel, uri=/0022a300002f164c/ep/1/r/currentLevel, ownerId=1, ownerClass=light, value=100, type=com.icontrol.lightLevel, mode=0x7b (rw-del-)
~~~

Note how in the above example writing "true" to the light's "isOn" resource
triggered several resource updated events that are printed when running device
util in interactive mode.  The first event shown is the "isOn" resource
changing to true which happens as a result of the light device sending an
attribute report for the ON_OFF attribute.  The remaining events are similarly
triggered by attribute reports as the light slowly transitions from 0% dim
level to 100%.

Wildcards can be used when querying resources which can be useful to see the
same resource across multiple devices.  For example:

~~~
deviceUtil> queryResources */communicationFailure
resources:
    /000d6f0004ae46af/r/communicationFailure = true
    /000d6f0012c47ebf/r/communicationFailure = false
    /000d6f0012c529a0/r/communicationFailure = false
    /000d6f0012c6087b/r/communicationFailure = false
    /0022a3000004e039/r/communicationFailure = false
    /0022a300002f164c/r/communicationFailure = false
    /7894b4ef0ca1/r/communicationFailure = false
    /d187bc81-927a-4ad6-8634-a74b38bf7c0c/r/communicationFailure = false
    /e0606614be56/r/communicationFailure = false
~~~

