/**
 * Example Door Lock SBMD Driver for new SBMD specification exploration only.
 * This example has ficticious concepts whose purpose is to demonstrate the capabilities
 * of the new SBMD spec. Impractical for real use.
 *
 * Maps Matter Door Lock device type (0x000a) to Barton doorLock device class
 *
 * version 3
 */

SbmdDriver({
  schemaVersion: "4.0",
  driverVersion: "1.0",
  name: "Door Lock",

  constants: {
    LOCK_ENDPOINT: "1",
    DOOR_LOCK_CLUSTER: 0x0101,
    IDENTIFY_CLUSTER: 0x0003,
    GENERAL_DIAGNOSTICS_CLUSTER: 0x0033,
    ATTR_LOCK_STATE: 0x0000,
    ATTR_ACTUATOR_ENABLED: 0x0002,
    ATTR_DOOR_STATE: 0x0003,
    ATTR_IDENTIFY_TIME: 0x0000,
    ATTR_CREDENTIAL_RULES_SUPPORT: 0x001b,
    EVT_DOOR_LOCK_ALARM: 0x0000,
    EVT_LOCK_OPERATION: 0x0002,
    EVT_LOCK_USER_CHANGE: 0x0003,
    CMD_LOCK_DOOR: 0x0000,
    CMD_UNLOCK_DOOR: 0x0001,
    CMD_GET_CREDENTIAL_STATUS_RESP: 0x0024,
    CMD_GET_USER_RESP: 0x001a,
    CMD_SET_CREDENTIAL_RESP: 0x001c,
    CMD_REBOOT: 0x0000,
  },

  barton: {
    deviceClass: "doorLock",
    deviceClassVersion: 3,
  },

  matter: {
    deviceTypes: [0x000a],
    revision: 1,
    featureClusters: [DOOR_LOCK_CLUSTER],
  },

  reporting: {
    minSecs: 1,
    maxSecs: 3600,
  },

  // =========================================================================
  // Device-level resources — available on the device itself, not tied to
  // any specific endpoint.
  // =========================================================================

  resources: {
    reboot: {
      type: "function",
      execute: executeReboot,
    },
    identify: {
      type: "string",
      modes: ["read", "write"],
      read: readIdentify,
      write: writeIdentify,
    },
  },

  // =========================================================================
  // Endpoints — each carries its own profile and resources
  // =========================================================================

  endpoints: {
    [LOCK_ENDPOINT]: {
      profile: "doorLock",
      profileVersion: 3,

      resources: {
        locked: {
          type: "boolean",
          modes: ["read", "dynamic", "emitEvents"],
          read: {
            supplements: {
              attributes: [{ clusterId: DOOR_LOCK_CLUSTER, attributeId: ATTR_LOCK_STATE }],
            },
            handler: readLockedState,
          },
        },
        lock: {
          type: "function",
          execute: executeLockAction,
        },
        unlock: {
          type: "function",
          execute: executeLockAction,
        },
      },
    },
  },

  // =========================================================================
  // Matter device-side handlers — incoming data from the device
  //
  // Each handler declares:
  //   - Trigger: what fires the handler (clusterId + attributeId/eventId/
  //     commandId).  Supports single, multiple (array), or wildcard ("*").
  //     The handler is called once per triggering change.
  //   - Supplements (optional): supplementary data pre-fetched before the
  //     handler runs.
  // =========================================================================

  attributeHandlers: {
    // Single attribute — one specific attribute, one handler
    lockState: {
      clusterId: DOOR_LOCK_CLUSTER,
      attributeId: ATTR_LOCK_STATE,
      handler: handleLockStateAttribute,
    },

    // Multiple attributes — several attributes share one handler.
    // Pre-fetches the current "locked" resource value as supplementary context.
    lockActuator: {
      clusterId: DOOR_LOCK_CLUSTER,
      attributeIds: [ATTR_ACTUATOR_ENABLED, ATTR_DOOR_STATE],
      supplements: {
        resources: [LOCK_ENDPOINT + "/locked"],
      },
      handler: handleActuatorAttributes,
    },

    // Wildcard — catch-all for any attribute on a cluster
    lockDiagnostics: {
      clusterId: DOOR_LOCK_CLUSTER,
      attributeId: "*",
      handler: handleLockDiagnostics,
    },
  },

  eventHandlers: {
    // Single event with supplementary pre-fetched data.
    lockOperation: {
      clusterId: DOOR_LOCK_CLUSTER,
      eventId: EVT_LOCK_OPERATION,
      supplements: {
        attributes: [{ clusterId: DOOR_LOCK_CLUSTER, attributeId: ATTR_ACTUATOR_ENABLED }],
        resources:  [LOCK_ENDPOINT + "/locked"],
      },
      handler: handleLockOperation,
    },

    // Multiple events — several events share one handler
    lockAlarms: {
      clusterId: DOOR_LOCK_CLUSTER,
      eventIds: [EVT_DOOR_LOCK_ALARM, EVT_LOCK_USER_CHANGE],
      handler: handleLockAlarms,
    },

    // Wildcard — catch-all for any event on a cluster
    lockEventCatchAll: {
      clusterId: DOOR_LOCK_CLUSTER,
      eventId: "*",
      handler: handleLockEventCatchAll,
    },
  },

  commandHandlers: {
    // Single command response with supplementary attribute context.
    getCredentialStatus: {
      clusterId: DOOR_LOCK_CLUSTER,
      commandId: CMD_GET_CREDENTIAL_STATUS_RESP,
      supplements: {
        attributes: [{ clusterId: DOOR_LOCK_CLUSTER, attributeId: ATTR_CREDENTIAL_RULES_SUPPORT }],
      },
      handler: handleGetCredentialStatusResponse,
    },

    // Multiple command responses
    userCommands: {
      clusterId: DOOR_LOCK_CLUSTER,
      commandIds: [CMD_GET_USER_RESP, CMD_SET_CREDENTIAL_RESP],
      handler: handleUserCommandResponses,
    },

    // Wildcard — catch-all for any command response on a cluster
    lockCommandCatchAll: {
      clusterId: DOOR_LOCK_CLUSTER,
      commandId: "*",
      handler: handleLockCommandCatchAll,
    },
  },
});

// ===========================================================================
// Resource handler implementations
//
// All handlers receive the same args shape:
//   args.attribute  — { clusterId, attributeId, value }  (attribute handlers)
//   args.event      — { clusterId, eventId, data }       (event handlers)
//   args.command    — { clusterId, commandId, data }     (command handlers)
//   args.resource   — { resourceId, input }              (resource handlers)
//   args.supplements.attributes — { [clusterId]: { [attributeId]: value } }
//   args.supplements.resources  — { "endpointId/resourceId": value }
//   args.clusterFeatureMaps — always available
// ===========================================================================

/**
 * Transform LockState attribute TLV into a Barton boolean.
 *
 * LockState enum values:
 *   0 = NotFullyLocked, 1 = Locked, 2 = Unlocked, 3 = Unlatched
 */
function readLockedState(args) {
  var value = args.supplements.attributes[DOOR_LOCK_CLUSTER][ATTR_LOCK_STATE];
  var isLocked = (value === 1);

  return SbmdUtils.result()
    .updateResource(isLocked ? "true" : "false");
}

/** Read the current identify string. */
function readIdentify(args) {
  return SbmdUtils.result()
    .read(IDENTIFY_CLUSTER, ATTR_IDENTIFY_TIME);
}

/** Write a new identify duration. */
function writeIdentify(args) {
  var schema = { IdentifyTime: { tag: 0, type: "uint16" } };
  var tlvBase64 = SbmdUtils.Tlv.encodeStruct({ IdentifyTime: parseInt(args.resource.input) }, schema);

  return SbmdUtils.result()
    .write(IDENTIFY_CLUSTER, ATTR_IDENTIFY_TIME, tlvBase64);
}

/** Reboot the device via a General Diagnostics cluster command. */
function executeReboot(args) {
  return SbmdUtils.result()
    .invoke(GENERAL_DIAGNOSTICS_CLUSTER, CMD_REBOOT, null, {});
}

/**
 * Execute LockDoor or UnlockDoor command based on which resource was invoked.
 *
 * args.resource.resourceId is "lock" or "unlock", supplied by the runtime.
 * LockDoor = CMD_LOCK_DOOR, UnlockDoor = CMD_UNLOCK_DOOR.
 */
function executeLockAction(args) {
  var commandId = (args.resource.resourceId === "lock") ? CMD_LOCK_DOOR : CMD_UNLOCK_DOOR;
  var featureMap = args.clusterFeatureMaps[DOOR_LOCK_CLUSTER] || 0;
  var tlvBase64 = buildPinPayload(featureMap, args.resource.input);

  return SbmdUtils.result()
    .invoke(DOOR_LOCK_CLUSTER, commandId, tlvBase64, { timedInvokeTimeoutMs: 10000 });
}

// ===========================================================================
// Attribute handler implementations
// ===========================================================================

/** Handle LockState attribute — same transform as the resource read. */
function handleLockStateAttribute(args) {
  var isLocked = (args.attribute.value === 1);

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "locked", isLocked ? "true" : "false");
}

/**
 * Handle ActuatorEnabled + DoorState attributes together.
 *
 * args.supplements.resources["1/locked"] contains the current Barton
 * "locked" value, allowing the handler to skip redundant updates.
 */
function handleActuatorAttributes(args) {
  var currentLocked = args.supplements.resources[LOCK_ENDPOINT + "/locked"];

  if (args.attribute.attributeId === ATTR_ACTUATOR_ENABLED) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "actuatorEnabled", args.attribute.value ? "true" : "false");
  } else if (args.attribute.attributeId === ATTR_DOOR_STATE) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "doorState", String(args.attribute.value))
      .log("doorState changed while locked=" + currentLocked);
  }

  return SbmdUtils.result(); // no update
}

/** Wildcard handler — log any DoorLock attribute for diagnostics. */
function handleLockDiagnostics(args) {
  return SbmdUtils.result()
    .log("DoorLock attr 0x" + args.attribute.attributeId.toString(16) + " changed");
}

// ===========================================================================
// Event handler implementations
// ===========================================================================

/**
 * Handle LockOperation event — update the "locked" resource.
 *
 * LockOperationType enum (tag 0 in event TLV struct):
 *   0 = Lock, 1 = Unlock, 2 = NonAccessUserEvent,
 *   3 = ForcedUserEvent, 4 = Unlatch
 *
 * Only Lock (0) and Unlock (1) change the resource state.
 * Other operation types are acknowledged but produce no update.
 *
 * args.supplements.resources["1/locked"]                    — current Barton locked state
 * args.supplements.attributes[DOOR_LOCK_CLUSTER][ATTR_ACTUATOR_ENABLED]  — ActuatorEnabled
 */
function handleLockOperation(args) {
  var opType = args.event.data[0];
  var actuatorEnabled = args.supplements.attributes[DOOR_LOCK_CLUSTER][ATTR_ACTUATOR_ENABLED];

  if (!actuatorEnabled) {
    return SbmdUtils.result()
      .log("lock operation ignored — actuator disabled");
  }

  if (opType === 0) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "locked", "true")
      .setPersistentKey("lastLockOperation", "lock");
  } else if (opType === 1) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "locked", "false")
      .setPersistentKey("lastLockOperation", "unlock");
  }

  return SbmdUtils.result(); // no update
}

/**
 * Handle DoorLockAlarm + LockUserChange events.
 *
 * Stores the alarm code transiently (auto-cleaned after 300s) for
 * short-lived diagnostics, and persists the total alarm count across
 * reboots.
 *
 * getPersistentKey(key) retrieves a value from non-volatile storage
 * (returns null if not set).  setTransientKey(key, value, ttlSecs)
 * stores in memory with automatic cleanup after ttlSecs seconds.
 */
function handleLockAlarms(args) {
  var alarmCode = args.event.data[0];
  var count = parseInt(SbmdUtils.getPersistentKey("alarmCount") || "0") + 1;

  return SbmdUtils.result()
    .setTransientKey("lastAlarmCode", String(alarmCode), 300)
    .setPersistentKey("alarmCount", String(count))
    .log("DoorLock alarm event 0x" + args.event.eventId.toString(16) + " code=" + alarmCode + " total=" + count);
}

/** Wildcard event handler — catch-all for unhandled DoorLock events. */
function handleLockEventCatchAll(args) {
  return SbmdUtils.result()
    .log("DoorLock event 0x" + args.event.eventId.toString(16) + " received");
}

// ===========================================================================
// Command handler implementations
// ===========================================================================

/**
 * Handle GetCredentialStatusResponse.
 *
 * args.supplements.attributes[DOOR_LOCK_CLUSTER][ATTR_CREDENTIAL_RULES_SUPPORT] —
 * CredentialRulesSupport, used to determine which credential
 * fields are meaningful.
 */
function handleGetCredentialStatusResponse(args) {
  var response = args.command.data;
  var credRules = args.supplements.attributes[DOOR_LOCK_CLUSTER][ATTR_CREDENTIAL_RULES_SUPPORT];

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "credentialStatus", JSON.stringify(response))
    .log("credential status updated (rules=" + credRules + ")");
}

/** Handle GetUserResponse + SetCredentialResponse — update resource. */
function handleUserCommandResponses(args) {
  var response = args.command.data;

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "userCommandResult", JSON.stringify(response));
}

/** Wildcard command handler — catch-all for unhandled command responses. */
function handleLockCommandCatchAll(args) {
  return SbmdUtils.result()
    .log("DoorLock command response 0x" + args.command.commandId.toString(16) + " received");
}

// ---------------------------------------------------------------------------
// Internal helpers (not exposed as entrypoints)
// ---------------------------------------------------------------------------

/**
 * Build optional PINCode TLV payload for lock/unlock commands.
 * Returns encoded TLV base64 if PIN credentials are supported and a PIN
 * is provided, otherwise null.
 *
 * Feature bits: 0x01 = PIN credential, 0x80 = COTA (credential over the air)
 */
function buildPinPayload(featureMap, pinString) {
  if (((featureMap & 0x81) !== 0x81) || !pinString || pinString.length === 0) {
    return null;
  }

  var schema = { PINCode: { tag: 0, type: "octstr" } };
  var pinBytes = new Uint8Array(pinString.length);

  for (var i = 0; i < pinString.length; i++) {
    pinBytes[i] = pinString.charCodeAt(i);
  }

  return SbmdUtils.Tlv.encodeStruct({ PINCode: pinBytes }, schema);
}
