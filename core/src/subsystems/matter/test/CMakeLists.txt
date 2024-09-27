include(BDSAddCppTest)

# Avoid throwing the kitchen sink into sources and LIBS.
# Only build units that are actually under test - this
# can be useful for discovering unintended tight coupling.

file(GLOB TEST_SRC *.cpp)
list(APPEND TEST_SRC ../ReadPrepareParamsBuilder.cpp ../MatterCommon.cpp)

pkg_check_modules(OPENSSL REQUIRED openssl)

# This executable should contain a collection of mini integration (mostly unit)
# tests that validate classes meant to make some deficient SDK classes more usable.
# Add more executables for truly isolated units that don't link to other libraries.
bds_add_cpp_test(NAME testMatterSDKTooling
                LIBS ${BDS_MATTER_LIB} ${OPENSSL_LINK_LIBRARIES} gmock crypto xhLog xhUtil
                INCLUDES ${CMAKE_SOURCE_DIR}/src
                         ${CMAKE_CURRENT_SOURCE_DIR}/..
                         ${CMAKE_PREFIX_PATH}/include/matter
                         ${PRIVATE_API_INCLUDES}
                SOURCES ${TEST_SRC}
)

bds_configure_glib()
