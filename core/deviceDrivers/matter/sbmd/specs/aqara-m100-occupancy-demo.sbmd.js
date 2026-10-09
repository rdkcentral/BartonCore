// Demo-specific occupancy driver for the Aqara M100.
//
// One M100 Matter node bridges four occupancy sensors in this demo:
//   Matter EP 2: Person detected on camera
//   Matter EP 3: Kevin (face) detected
//   Matter EP 4: Package detected
//   Matter EP 5: Pet detected
//
// This spec assumes those endpoint numbers remain stable. It does not
// dynamically discover newly added or renumbered signals.

SbmdDriver({
    schemaVersion: '5.0',
    driverVersion: 1,
    name: 'Aqara M100 Occupancy Demo',

    constants: {
        CL_OCCUPANCY_SENSING: 0x0406,
        ATTR_OCCUPANCY: 0x0000,
        RES_FAULTED: 'faulted'
    },

    barton: {
        deviceClass: 'sensor',
        deviceClassVersion: 1
    },

    matter: {
        // The generic occupancy-sensor spec also matches device type 0x0107.
        // Supplying both VID and PID makes this vendor-specific, so Barton
        // tries it before generic drivers and leaves other sensors alone.
        vendorId: 0x115f,   // 4447, read from the M100
        productId: 0x0804,  // 2052, read from the M100

        // Required to identify the matching occupancy endpoints on this node.
        deviceTypes: [0x0107],
        revision: 1
    },

    reporting: {
        minSecs: 1,
        maxSecs: 3600
    },

    aliases: {
        occupancy: {
            clusterId: CL_OCCUPANCY_SENSING,
            attributeId: ATTR_OCCUPANCY,
            type: 'uint8'
        }
    },

    endpoints: {
        // Matter EP 2: Person detected on camera.
        '2': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_OCCUPANCY_SENSING]
                }
            }
        },

        // Matter EP 3: Kevin (face) detected.
        '3': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_OCCUPANCY_SENSING]
                }
            }
        },

        // Matter EP 4: Package detected.
        '4': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_OCCUPANCY_SENSING]
                }
            }
        },

        // Matter EP 5: Pet detected.
        '5': {
            profile: 'sensor',
            profileVersion: 2,
            resources: {
                faulted: {
                    type: 'com.icontrol.boolean',
                    modes: ['read'],
                    prerequisites: [CL_OCCUPANCY_SENSING]
                }
            }
        }
    },

    attributeHandlers: {
        handleOccupancy: {
            aliases: ['occupancy'],
            handler: function (args) {
                // With multiple declared endpoints, args.endpointId retains
                // the originating Matter endpoint ID. Ignore undeclared ones.
                if (args.endpointId !== '2' && args.endpointId !== '3' &&
                    args.endpointId !== '4' && args.endpointId !== '5') {
                    return Sbmd.result().success();
                }

                var value = Sbmd.Tlv.decode(args.attribute.tlvBase64);
                if (value === null) {
                    return Sbmd.result()
                        .error('TLV decode failed for Occupancy');
                }

                // Bit 0 of the Matter occupancy bitmap means occupied.
                // Each endpoint has an independent Barton faulted resource.
                var occupied = (value & 0x01) !== 0;

                return Sbmd.result()
                    .dataModel.updateResource(
                        args.endpointId,
                        RES_FAULTED,
                        occupied ? 'true' : 'false'
                    )
                    .success();
            }
        }
    }
});
