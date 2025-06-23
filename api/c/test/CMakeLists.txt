# ------------------------------ tabstop = 4 ----------------------------------
#
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
# ------------------------------ tabstop = 4 ----------------------------------

#
# Created by Christian Leithner on 5/23/2025.
#

bcore_add_glib_test(
    NAME barton-core-resource-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-resource-test.c
    GLOG_DOMAIN BDeviceServiceResourceTest
)

bcore_add_glib_test(
    NAME barton-core-metadata-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-metadata-test.c
    GLOG_DOMAIN BDeviceServiceMetadataTest
)

bcore_add_glib_test(
    NAME barton-core-endpoint-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-endpoint-test.c
    GLOG_DOMAIN BDeviceServiceEndpointTest
)

bcore_add_glib_test(
    NAME barton-core-device-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-device-test.c
    GLOG_DOMAIN BDeviceServiceDeviceTest
)

bcore_add_glib_test(
    NAME barton-core-discovery-filter-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-discovery-filter-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryFilterTest
)

bcore_add_glib_test(
    NAME barton-core-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-event-test.c
    GLOG_DOMAIN BDeviceServiceEventTest
)

bcore_add_glib_test(
    NAME barton-core-status-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-status-test.c
    GLOG_DOMAIN BDeviceServiceStatusTest
)

bcore_add_glib_test(
    NAME barton-core-device-found-details-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-device-found-details-test.c
    GLOG_DOMAIN BDeviceServiceDeviceFoundDetailsTest
)

bcore_add_glib_test(
    NAME barton-core-discovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-discovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStartedEventTest
)

bcore_add_glib_test(
    NAME barton-core-recovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-recovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStartedEventTest
)

bcore_add_glib_test(
    NAME barton-core-discovery-session-info-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-discovery-session-info-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoverySessionInfoEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-discovery-failed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-discovery-failed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryFailedEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-discovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-discovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoverEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-rejected-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-rejected-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRejectedEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-configuration-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-configuration-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceConfigurationEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-added-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceAddedEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-recovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-recovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRecoveredEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-discovery-completed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-discovery-completed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryCompletedEventTest
)

bcore_add_glib_test(
    NAME barton-core-discovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-discovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStoppedEventTest
)

bcore_add_glib_test(
    NAME barton-core-metadata-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-metadata-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceMetadataUpdatedEventTest
)

bcore_add_glib_test(
    NAME barton-core-recovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-recovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStoppedEventTest
)

bcore_add_glib_test(
    NAME barton-core-resource-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-resource-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceResourceUpdatedEventTest
)

bcore_add_glib_test(
    NAME barton-core-endpoint-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-endpoint-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointRemovedEventTest
)

bcore_add_glib_test(
    NAME barton-core-endpoint-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-endpoint-added-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointAddedEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRemovedEventTest
)

bcore_add_glib_test(
    NAME barton-core-zigbee-channel-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-zigbee-channel-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeChannelChangedEventTest
)

bcore_add_glib_test(
    NAME barton-core-storage-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-storage-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceStorageChangedEventTest
)

bcore_add_glib_test(
    NAME barton-core-zigbee-interference-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-zigbee-interference-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeInterferenceEventTest
)

bcore_add_glib_test(
    NAME barton-core-zigbee-pan-id-attack-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-zigbee-pan-id-attack-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeePanIdAttackChangedEventTest
)

bcore_add_glib_test(
    NAME barton-core-device-database-failure-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-device-database-failure-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDatabaseFailureEventTest
)

bcore_add_glib_test(
    NAME barton-core-zigbee-remote-cli-response-received-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/barton-core-zigbee-remote-cli-response-received-event-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../src ${PRIVATE_API_INCLUDES}
    GLOG_DOMAIN BDeviceServiceZigbeeRemoteCliResponseReceivedEventTest
)

bcore_add_glib_test(
    NAME barton-core-utils-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-utils-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../src ${PRIVATE_API_INCLUDES}
    GLOG_DOMAIN BDeviceServiceUtilsTest
)

bcore_add_cmocka_test(
    NAME barton-core-client-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/barton-core-client-test.c
    WRAPPED_FUNCTIONS
        deviceServiceInitialize
        deviceServiceStart
        allServicesAvailableNotify
        deviceServiceShutdown
        deviceServiceGetEndpointsByProfile
        deviceServiceDiscoverStart
        deviceServiceDiscoverStop
        deviceServiceCommissionDevice
        deviceServiceAddMatterDevice
        deviceServiceGetStatus
        deviceServiceGetDevice
        deviceServiceGetEndpointById
        deviceServiceGetEndpointByUri
        deviceServiceGetAllDevices
        deviceServiceGetDevicesByDeviceClass
        deviceServiceGetResourceByUri
        deviceServiceGetDevicesBySubsystem
        deviceServiceGetDeviceByUri
        deviceServiceGetResourceByUri
        deviceServiceWriteResource
        deviceServiceExecuteResource
        deviceServiceRemoveDevice
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
        deviceServiceRemoveEndpointById
        deviceServiceSetMetadata
        deviceServiceGetMetadata
        deviceServiceGetResourcesByUriPattern
        deviceServiceChangeResourceMode
        zigbeeSubsystemChangeChannel
        deviceServiceGetMetadataByUriPattern
        deviceServiceProcessDeviceDescriptors
        zhalTest
        zigbeeSubsystemPerformEnergyScan
        deviceServiceRestoreConfig
    LINK_LIBRARIES BartonCoreStatic
    INCLUDES ${PRIVATE_API_INCLUDES} ${PROJECT_SOURCE_DIR}/core/src
)
if(TARGET barton-core-client-test)
    target_compile_definitions(barton-core-client-test PRIVATE G_LOG_DOMAIN="BDeviceServiceClientTest")
endif()

bcore_add_cmocka_test(
    NAME barton-core-property-provider-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/provider/barton-core-property-provider-test.c
    WRAPPED_FUNCTIONS
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
    LINK_LIBRARIES BartonCoreStatic
)
if(TARGET barton-core-property-provider-test)
    target_compile_definitions(barton-core-property-provider-test PRIVATE G_LOG_DOMAIN="BDeviceServicePropertyProviderTest")
endif()
