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

bds_add_glib_test(
    NAME device-service-resource-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-resource-test.c
    GLOG_DOMAIN BDeviceServiceResourceTest
)

bds_add_glib_test(
    NAME device-service-metadata-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-metadata-test.c
    GLOG_DOMAIN BDeviceServiceMetadataTest
)

bds_add_glib_test(
    NAME device-service-endpoint-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-endpoint-test.c
    GLOG_DOMAIN BDeviceServiceEndpointTest
)

bds_add_glib_test(
    NAME device-service-device-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-device-test.c
    GLOG_DOMAIN BDeviceServiceDeviceTest
)

bds_add_glib_test(
    NAME device-service-discovery-filter-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-discovery-filter-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryFilterTest
)

bds_add_glib_test(
    NAME device-service-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-event-test.c
    GLOG_DOMAIN BDeviceServiceEventTest
)

bds_add_glib_test(
    NAME device-service-status-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-status-test.c
    GLOG_DOMAIN BDeviceServiceStatusTest
)

bds_add_glib_test(
    NAME device-service-device-found-details-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-device-found-details-test.c
    GLOG_DOMAIN BDeviceServiceDeviceFoundDetailsTest
)

bds_add_glib_test(
    NAME device-service-discovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-discovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStartedEventTest
)

bds_add_glib_test(
    NAME device-service-recovery-started-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-recovery-started-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStartedEventTest
)

bds_add_glib_test(
    NAME device-service-discovery-session-info-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-discovery-session-info-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoverySessionInfoEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovery-failed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-discovery-failed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryFailedEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-discovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoverEventTest
)

bds_add_glib_test(
    NAME device-service-device-rejected-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-rejected-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRejectedEventTest
)

bds_add_glib_test(
    NAME device-service-device-configuration-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-configuration-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceConfigurationEventTest
)

bds_add_glib_test(
    NAME device-service-device-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-added-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceAddedEventTest
)

bds_add_glib_test(
    NAME device-service-device-recovered-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-recovered-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRecoveredEventTest
)

bds_add_glib_test(
    NAME device-service-device-discovery-completed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-discovery-completed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDiscoveryCompletedEventTest
)

bds_add_glib_test(
    NAME device-service-discovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-discovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceDiscoveryStoppedEventTest
)

bds_add_glib_test(
    NAME device-service-metadata-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-metadata-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceMetadataUpdatedEventTest
)

bds_add_glib_test(
    NAME device-service-recovery-stopped-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-recovery-stopped-event-test.c
    GLOG_DOMAIN BDeviceServiceRecoveryStoppedEventTest
)

bds_add_glib_test(
    NAME device-service-resource-updated-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-resource-updated-event-test.c
    GLOG_DOMAIN BDeviceServiceResourceUpdatedEventTest
)

bds_add_glib_test(
    NAME device-service-endpoint-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-endpoint-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointRemovedEventTest
)

bds_add_glib_test(
    NAME device-service-endpoint-added-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-endpoint-added-event-test.c
    GLOG_DOMAIN BDeviceServiceEndpointAddedEventTest
)

bds_add_glib_test(
    NAME device-service-device-removed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-removed-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceRemovedEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-channel-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-zigbee-channel-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeChannelChangedEventTest
)

bds_add_glib_test(
    NAME device-service-storage-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-storage-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceStorageChangedEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-interference-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-zigbee-interference-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeeInterferenceEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-pan-id-attack-changed-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-zigbee-pan-id-attack-changed-event-test.c
    GLOG_DOMAIN BDeviceServiceZigbeePanIdAttackChangedEventTest
)

bds_add_glib_test(
    NAME device-service-device-database-failure-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-device-database-failure-event-test.c
    GLOG_DOMAIN BDeviceServiceDeviceDatabaseFailureEventTest
)

bds_add_glib_test(
    NAME device-service-zigbee-remote-cli-response-received-event-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/events/device-service-zigbee-remote-cli-response-received-event-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../src ${PRIVATE_API_INCLUDES}
    GLOG_DOMAIN BDeviceServiceZigbeeRemoteCliResponseReceivedEventTest
)

bds_add_glib_test(
    NAME device-service-utils-test
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-utils-test.c
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../src ${PRIVATE_API_INCLUDES}
    GLOG_DOMAIN BDeviceServiceUtilsTest
)

bds_add_cmocka_test(
    NAME device-service-client-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/device-service-client-test.c
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
    LINK_LIBRARIES brtnDeviceServiceStatic
    INCLUDES ${PRIVATE_API_INCLUDES} ${PROJECT_SOURCE_DIR}/core/src
)
if(TARGET device-service-client-test)
    target_compile_definitions(device-service-client-test PRIVATE G_LOG_DOMAIN="BDeviceServiceClientTest")
endif()

bds_add_cmocka_test(
    NAME device-service-property-provider-test
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/provider/device-service-property-provider-test.c
    WRAPPED_FUNCTIONS
        deviceServiceGetSystemProperty
        deviceServiceSetSystemProperty
    LINK_LIBRARIES brtnDeviceServiceStatic
)
if(TARGET device-service-property-provider-test)
    target_compile_definitions(device-service-property-provider-test PRIVATE G_LOG_DOMAIN="BDeviceServicePropertyProviderTest")
endif()
