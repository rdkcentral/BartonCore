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
          read: readLockedState,
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
  // Each handler declares which cluster items it handles and the function
  // to call.  Items can be specified three ways:
  //   - Single:   { clusterId, attributeId }
  //   - Multiple: { clusterId, attributeIds: [...] }
  //   - Wildcard: { clusterId, attributeId: "*" }
  //
  // Handlers may declare optional "inputs" to request additional context.
  // The runtime pre-fetches these and populates args before calling:
  //
  //   inputs.resources — Barton resource values, keyed by resource name.
  //       Populated in args.resources as { resourceName: currentValue }.
  //       Use endpoint-qualified names for endpoint resources (e.g. "1/locked").
  //
  //   inputs.attributes — Matter attribute values, each { clusterId, attributeId }.
  //       Populated in args.attributes as [{ clusterId, attributeId, value }].
  //
  // If an input is unavailable (e.g. device offline), its value is null.
  // =========================================================================

  attributeHandlers: {
    // Single attribute — one specific attribute, one handler
    lockState: {
      clusterId: DOOR_LOCK_CLUSTER,
      attributeId: 0x0000,
      handler: handleLockStateAttribute,
    },

    // Multiple attributes — several attributes share one handler.
    // Requests the current "locked" resource value so the handler can
    // make decisions based on existing Barton state.
    lockActuator: {
      clusterId: DOOR_LOCK_CLUSTER,
      attributeIds: [0x0002, 0x0003],   // ActuatorEnabled, DoorState
      inputs: {
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
    // Single event.
    // Requests both a Barton resource value and a Matter attribute so
    // the handler has full context when processing the event.
    lockOperation: {
      clusterId: DOOR_LOCK_CLUSTER,
      eventId: 0x0002,
      inputs: {
        resources:  [LOCK_ENDPOINT + "/locked"],
        attributes: [{ clusterId: DOOR_LOCK_CLUSTER, attributeId: 0x0002 }],  // ActuatorEnabled
      },
      handler: handleLockOperation,
    },

    // Multiple events — several events share one handler
    lockAlarms: {
      clusterId: DOOR_LOCK_CLUSTER,
      eventIds: [0x0000, 0x0003],       // DoorLockAlarm, LockUserChange
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
    // Single command response.
    // Requests a Matter attribute (CredentialRulesSupport) to help
    // interpret the response.
    getCredentialStatus: {
      clusterId: DOOR_LOCK_CLUSTER,
      commandId: 0x0024,
      inputs: {
        attributes: [{ clusterId: DOOR_LOCK_CLUSTER, attributeId: 0x001b }],  // CredentialRulesSupport
      },
      handler: handleGetCredentialStatusResponse,
    },

    // Multiple command responses
    userCommands: {
      clusterId: DOOR_LOCK_CLUSTER,
      commandIds: [0x001a, 0x001c],     // GetUserResponse, SetCredentialResponse
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
// ===========================================================================

/**
 * Transform LockState attribute TLV into a Barton boolean.
 *
 * LockState enum values:
 *   0 = NotFullyLocked, 1 = Locked, 2 = Unlocked, 3 = Unlatched
 */
function readLockedState(args) {
  var value = SbmdUtils.Tlv.decode(args.tlvBase64);
  var isLocked = (value === 1);

  return SbmdUtils.result()
    .updateResource(isLocked ? "true" : "false");
}

/** Read the current identify string. */
function readIdentify(args) {
  return SbmdUtils.result()
    .read(IDENTIFY_CLUSTER, 0x0000);
}

/** Write a new identify duration. */
function writeIdentify(args) {
  var schema = { IdentifyTime: { tag: 0, type: "uint16" } };
  var tlvBase64 = SbmdUtils.Tlv.encodeStruct({ IdentifyTime: parseInt(args.value) }, schema);

  return SbmdUtils.result()
    .write(IDENTIFY_CLUSTER, 0x0000, tlvBase64);
}

/** Reboot the device via a General Diagnostics cluster command. */
function executeReboot(args) {
  return SbmdUtils.result()
    .invoke(GENERAL_DIAGNOSTICS_CLUSTER, 0x0000, null, {});
}

/**
 * Execute LockDoor or UnlockDoor command based on which resource was invoked.
 *
 * args.resourceId is "lock" or "unlock", supplied by the runtime.
 * LockDoor = command 0x0000, UnlockDoor = command 0x0001.
 */
function executeLockAction(args) {
  var commandId = (args.resourceId === "lock") ? 0x0000 : 0x0001;
  var featureMap = args.clusterFeatureMaps[DOOR_LOCK_CLUSTER] || 0;
  var tlvBase64 = buildPinPayload(featureMap, args.input);

  return SbmdUtils.result()
    .invoke(DOOR_LOCK_CLUSTER, commandId, tlvBase64, { timedInvokeTimeoutMs: 10000 });
}

// ===========================================================================
// Attribute handler implementations
// ===========================================================================

/** Handle LockState attribute — same transform as the resource read. */
function handleLockStateAttribute(args) {
  var value = SbmdUtils.Tlv.decode(args.tlvBase64);
  var isLocked = (value === 1);

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "locked", isLocked ? "true" : "false");
}

/**
 * Handle ActuatorEnabled + DoorState attributes together.
 *
 * args.resources["1/locked"] contains the current Barton "locked" value,
 * allowing the handler to skip redundant updates.
 */
function handleActuatorAttributes(args) {
  var value = SbmdUtils.Tlv.decode(args.tlvBase64);
  var currentLocked = args.resources[LOCK_ENDPOINT + "/locked"];

  if (args.attributeId === 0x0002) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "actuatorEnabled", value ? "true" : "false");
  } else if (args.attributeId === 0x0003) {
    return SbmdUtils.result()
      .updateEndpointResource(LOCK_ENDPOINT, "doorState", String(value))
      .log("doorState changed while locked=" + currentLocked);
  }

  return SbmdUtils.result(); // no update
}

/** Wildcard handler — log any DoorLock attribute for diagnostics. */
function handleLockDiagnostics(args) {
  return SbmdUtils.result()
    .log("DoorLock attr 0x" + args.attributeId.toString(16) + " changed");
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
 * args.resources["1/locked"]  — current Barton locked state
 * args.attributes[0].value    — current ActuatorEnabled attribute value
 */
function handleLockOperation(args) {
  var event = SbmdUtils.Tlv.decode(args.tlvBase64);
  var opType = event[0];
  var actuatorEnabled = args.attributes[0].value;

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
  var event = SbmdUtils.Tlv.decode(args.tlvBase64);
  var alarmCode = event[0];
  var count = parseInt(SbmdUtils.getPersistentKey("alarmCount") || "0") + 1;

  return SbmdUtils.result()
    .setTransientKey("lastAlarmCode", String(alarmCode), 300)
    .setPersistentKey("alarmCount", String(count))
    .log("DoorLock alarm event 0x" + args.eventId.toString(16) + " code=" + alarmCode + " total=" + count);
}

/** Wildcard event handler — catch-all for unhandled DoorLock events. */
function handleLockEventCatchAll(args) {
  return SbmdUtils.result()
    .log("DoorLock event 0x" + args.eventId.toString(16) + " received");
}

// ===========================================================================
// Command handler implementations
// ===========================================================================

/**
 * Handle GetCredentialStatusResponse.
 *
 * args.attributes[0].value — current CredentialRulesSupport, used to
 * determine which credential fields are meaningful in the response.
 */
function handleGetCredentialStatusResponse(args) {
  var response = SbmdUtils.Tlv.decode(args.tlvBase64);
  var credRules = args.attributes[0].value;

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "credentialStatus", JSON.stringify(response))
    .log("credential status updated (rules=" + credRules + ")");
}

/** Handle GetUserResponse + SetCredentialResponse — update resource. */
function handleUserCommandResponses(args) {
  var response = SbmdUtils.Tlv.decode(args.tlvBase64);

  return SbmdUtils.result()
    .updateEndpointResource(LOCK_ENDPOINT, "userCommandResult", JSON.stringify(response));
}

/** Wildcard command handler — catch-all for unhandled command responses. */
function handleLockCommandCatchAll(args) {
  return SbmdUtils.result()
    .log("DoorLock command response 0x" + args.commandId.toString(16) + " received");
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
