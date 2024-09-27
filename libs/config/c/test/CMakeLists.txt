#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore 'config library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------
include(BDSAddCMockaTest)

# define source files (using wildcard)
# Don't dynamically link to xhConfig - we need to wrap its undefined getDynamicConfiPath symbol
file(GLOB SOURCES "src/*.c" "../src/*.c")

bds_add_cmocka_test(
        NAME testConfigLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhJsonHelper $<TARGET_PROPERTY:xhConfig,LINK_LIBRARIES>
        INCLUDES $<TARGET_PROPERTY:xhConfig,INCLUDE_DIRECTORIES>
        TEST_ARGS "-a"
)

bds_add_cmocka_test(
        NAME testSimpleProtectLib
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/simpleProtect/simpleProtectTest.c
        LINK_LIBRARIES xhConfig
)
