/**
 * Light SBMD Driver
 * Maps Matter light device types to Barton light device class
 */

SbmdDriver({
  schemaVersion: "4.0",
  driverVersion: "1.0",
  name: "Light",

  constants: {
    LIGHT_ENDPOINT: "1",
    ON_OFF_CLUSTER: 0x0006,
    LEVEL_CONTROL_CLUSTER: 0x0008,
    ATTR_ON_OFF: 0x0000,
    ATTR_CURRENT_LEVEL: 0x0000,
    CMD_ON: 0x0001,
    CMD_OFF: 0x0000,
    CMD_MOVE_TO_LEVEL_WITH_ON_OFF: 0x0004,
  },

  barton: {
    deviceClass: "light",
    deviceClassVersion: 0,
  },

  matter: {
    deviceTypes: [
      0x0100, 0x010a,  // On/Off Light, On/Off Plug-in Unit
      0x0101, 0x010b,  // Dimmable Light, Dimmable Plug-in Unit
      0x0102, 0x0200,  // Color Dimmable Light (+ alternate)
      0x010d, 0x0210,  // Extended Color Light (+ alternate)
      0x010c, 0x0220,  // Color Temperature Light (+ alternate)
      0x0103, 0x0104,  // On/Off Light Switch, Dimmable Light Switch
      0x0105,          // Color Dimmable Light Switch
    ],
    revision: 1,
  },

  reporting: {
    minSecs: 1,
    maxSecs: 3600,
  },

  endpoints: {
    [LIGHT_ENDPOINT]: {
      profile: "light",
      profileVersion: 0,

      resources: {
        isOn: {
          type: "boolean",
          modes: ["read", "write", "dynamic", "emitEvents"],
          read: {
            supplements: {
              attributes: [{ clusterId: ON_OFF_CLUSTER, attributeId: ATTR_ON_OFF }],
            },
            handler: readIsOn,
          },
          write: writeIsOn,
        },
        currentLevel: {
          type: "com.icontrol.lightLevel",
          optional: true,
          modes: ["read", "write", "dynamic", "emitEvents"],
          read: {
            supplements: {
              attributes: [{ clusterId: LEVEL_CONTROL_CLUSTER, attributeId: ATTR_CURRENT_LEVEL }],
            },
            handler: readCurrentLevel,
          },
          write: writeCurrentLevel,
        },
      },
    },
  },

  attributeHandlers: {
    onOff: {
      clusterId: ON_OFF_CLUSTER,
      attributeId: ATTR_ON_OFF,
      handler: handleOnOffAttribute,
    },
    currentLevel: {
      clusterId: LEVEL_CONTROL_CLUSTER,
      attributeId: ATTR_CURRENT_LEVEL,
      handler: handleCurrentLevelAttribute,
    },
  },
});

// ===========================================================================
// Resource handlers
// ===========================================================================

function readIsOn(args) {
  var value = args.supplements.attributes[args.constants.ON_OFF_CLUSTER][args.constants.ATTR_ON_OFF];

  return SbmdUtils.result()
    .updateResource((value === true) ? "true" : "false");
}

function writeIsOn(args) {
  var commandId = (args.resource.input === "true") ? args.constants.CMD_ON : args.constants.CMD_OFF;

  return SbmdUtils.result()
    .invoke(args.constants.ON_OFF_CLUSTER, commandId, null, {});
}

function readCurrentLevel(args) {
  var level = args.supplements.attributes[args.constants.LEVEL_CONTROL_CLUSTER][args.constants.ATTR_CURRENT_LEVEL];
  var percent = Math.round(level / 254 * 100);

  return SbmdUtils.result()
    .updateResource(percent.toString());
}

function writeCurrentLevel(args) {
  var percent = parseInt(args.resource.input, 10);

  if (isNaN(percent)) percent = 0;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  var level = Math.round(percent / 100 * 254);
  var payload = {
    Level: level,
    TransitionTime: 0,
    OptionsMask: 0,
    OptionsOverride: 0,
  };
  var schema = {
    Level:           { tag: 0, type: "uint8" },
    TransitionTime:  { tag: 1, type: "uint16" },
    OptionsMask:     { tag: 2, type: "uint8" },
    OptionsOverride: { tag: 3, type: "uint8" },
  };
  var tlvBase64 = SbmdUtils.Tlv.encodeStruct(payload, schema);

  return SbmdUtils.result()
    .invoke(args.constants.LEVEL_CONTROL_CLUSTER, args.constants.CMD_MOVE_TO_LEVEL_WITH_ON_OFF, tlvBase64, {});
}

// ===========================================================================
// Attribute handlers
// ===========================================================================

function handleOnOffAttribute(args) {
  return SbmdUtils.result()
    .updateEndpointResource(args.constants.LIGHT_ENDPOINT, "isOn", (args.attribute.value === true) ? "true" : "false");
}

function handleCurrentLevelAttribute(args) {
  var percent = Math.round(args.attribute.value / 254 * 100);

  return SbmdUtils.result()
    .updateEndpointResource(args.constants.LIGHT_ENDPOINT, "currentLevel", percent.toString());
}
