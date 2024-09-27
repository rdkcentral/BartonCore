//------------------------------ tabstop = 4 ----------------------------------
//
// Copyright (C) 2015 iControl Networks, Inc.
//
// All rights reserved.
//
// This software is protected by copyright laws of the United States
// and of foreign countries. This material may also be protected by
// patent laws of the United States and of foreign countries.
//
// This software is furnished under a license agreement and/or a
// nondisclosure agreement and may only be used or copied in accordance
// with the terms of those agreements.
//
// The mere transfer of this software does not imply any licenses of trade
// secrets, proprietary technology, copyrights, patents, trademarks, or
// any other form of intellectual property whatsoever.
//
// iControl Networks retains all ownership rights.
//
//------------------------------ tabstop = 4 ----------------------------------
/*
 * Created by Thomas Lea on 3/10/16.
 */

#ifndef FLEXCORE_SUBSYSTEMMANAGER_H
#define FLEXCORE_SUBSYSTEMMANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <device/icDevice.h>
#include <icTypes/icLinkedList.h>
#include "subsystemManagerCallbacks.h"

typedef struct Subsystem
{
    /**
     * Perform any migration steps needed to go from a previous version of the subsystem
     * to the recorded new one reported by the registered subsystem.
     * @see subsystemVersion
     * @return true if the migration routine ran without error and the new version should be saved, false otherwise.
     */
    bool (*migrate)(uint16_t oldVersion, uint16_t newVersion);

    /**
     * Initialize. Synchronously perform any initialization required to support subsystem API functions in DeviceDriver::startup().
     * Asynchronously configure supporting hardware required to support normal device operations.
     * When the subsystem is ready for normal operations, invoke initializedCallback.
     * If the subsystem becomes unready for any reason, invoke deInitializedCallback.
     * @note required
     */
    bool (*initialize)(subsystemInitializedFunc initializedCallback, subsystemDeInitializedFunc deInitializedCallback);

    /**
     * Release any resources and shut down supporting hardware, if required.
     * @note required
     */
    void (*shutdown)(void);

    /**
     * This is invoked after all drivers have been started and the subsystem may use device drivers
     */
    void (*onAllDriversStarted)(void);

    /**
     * This is invoked when all services are up, i.e., the callee may use any service
     */
    void (*onAllServicesAvailable)(void);

    /**
     * This is invoked after a backup has been unpacked to a temporary directory.
     * The callee may read subsystem specific resources from restoreSrcDir and write a valid configuration for
     * normal operations to configDir. Typically, a simple copy operation is sufficient.
     * @note Any configuration storage provided by jsonDatabase MUST NOT be restored here.
     * @param restoreSrcDir
     * @param configDir
     * @return
     */
    bool (*onRestoreConfig)(const char *restoreSrcDir, const char *configDir);

    /**
     * This is invoked after all configuration is restored
     */
    void (*onPostRestoreConfig)(void);

    /**
     * This is invoked when the system is about to suspend.
     * The callee shall prepare for suspend (e.g., tell supporting hardware to enter a low power state)
     */
    void (*onLPMStart)(void);

    /**
     * This is invoked when resuming from suspend.
     * The callee shall return to normal operation
     */
    void (*onLPMEnd)(void);

    /**
     * The callee will delay offering any OTA upgrades for delaySeconds
     * @param delaySeconds
     */
    void (*setOtaUpgradeDelay)(uint32_t delaySeconds);

    /**
     * Get a JSON object containing name/value pairs that represent the status of the subsystem
     * @return a cJSON object containing the status.  Caller destroys with cJSON_Delete
     */
    cJSON *(*getStatusJson)(void);

    /**
     * The subsystem's name
     * @note required
     */
    const char *name;

    /**
     * The subsystem's version. Note: if a subsystem plans to track versions, it should register a "migrate" routine
     * even if it is just pass-through.
     * @see migrate
     * @note optional but defaults to 0
     */
    uint16_t version;
} Subsystem;


/*
 * Callback for when all subsystems are ready for devices
 */
typedef void (*subsystemManagerReadyForDevicesFunc)();

/**
 * Initialize the subsystem manager
 * @param readyForDevicesCallback This will be invoked when *all* subsystems are ready for operations
 */
void subsystemManagerInitialize(subsystemManagerReadyForDevicesFunc readyForDevicesCallback);

/*
 * Shutdown the subsystem manager
 */
void subsystemManagerShutdown(void);

/*
 * Inform the subsystem manager that all device drivers have loaded.
 */
void subsystemManagerAllDriversStarted(void);

/*
 * Inform the subsystem manager that all services are available.
 */
void subsystemManagerAllServicesAvailable(void);

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem);

/*
 * Check if all subsystems are ready for devices
 */
bool subsystemManagerIsReadyForDevices(void);

/*
 * Restore config for RMA
 */
bool subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

/*
 * Post restore config actions for RMA
 */
void subsystemManagerPostRestoreConfig(void);

/**
 * Direct all subsystems to wait delaySeconds before offering any OTA upgrades
 * @param delaySeconds
 */
void subsystemManagerSetOtaUpgradeDelay(uint32_t delaySeconds);

/**
 * Direct all subsystems to enter low power mode
 */
void subsystemManagerEnterLPM(void);

/**
 * Direct all subsystems to exit low power mode
 */
void subsystemManagerExitLPM(void);

/**
 * Register your subsystem
 * @param subsystem A Subsystem that h
 */
void subsystemManagerRegister(Subsystem *subsystem);

/**
 * Get the names of the registered subsystems.  Caller destroys.
 */
icLinkedList *subsystemManagerGetRegisteredSubsystems(void);

/**
 * Retrieve the JSON representation of the named subsystems's status
 *
 * @param subsystemName the name of the subsystem
 * @return the cJSON object containing the subsystems's status.  Caller destroys with cJSON_Delete
 */
cJSON *subsystemManagerGetSubsystemStatusJson(const char *subsystemName);

#endif //FLEXCORE_SUBSYSTEMMANAGER_H
