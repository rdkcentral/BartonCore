include(BDSAddCMockaTest)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")
#Remove tests that use add_zilker_test from the parent
list(FILTER SOURCES EXCLUDE REGEX ".*fileTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stringTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*pingTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*regexTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*systemCommandUtilsTest.c$")
list(FILTER SOURCES EXCLUDE REGEX ".*stateMachineTest.c$")

bds_add_cmocka_test(
        NAME testUtilLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhUtil xhTypes
        TEST_ARGS "-a"
)

#bds_add_cmocka_test(NAME testPing TEST_SOURCES src/pingTest.c LINK_LIBRARIES xhUtil)

bds_add_cmocka_test(
    NAME testStateMachine
    TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/stateMachineTest.c
    LINK_LIBRARIES xhUtil
)
