file(GLOB SOURCES "src/*.c" "../src/*.c")

bds_add_cmocka_test(
        NAME testCryptoLib
        TEST_SOURCES ${SOURCES}
        INCLUDES $<TARGET_PROPERTY:xhCrypto,INCLUDE_DIRECTORIES>
        LINK_LIBRARIES $<TARGET_PROPERTY:xhCrypto,LINK_LIBRARIES>
)

if(BUILD_TESTING)
    set_property(
            TARGET testCryptoLib
            PROPERTY COMPILE_DEFINITIONS "RES_DIR=\"${CMAKE_CURRENT_LIST_DIR}/resources\""
    )
endif()
