include(BDSAddCMockaTest)

file(GLOB SOURCES "src/*.c")

bds_add_cmocka_test(
        NAME testDeviceDescriptorsLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhDeviceDescriptors
)
