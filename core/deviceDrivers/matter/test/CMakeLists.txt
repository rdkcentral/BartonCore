#--------------------------------------------------------------------------------------
#
# CMake definition for Matter C++ unit tests
# Should not be utilized directly, but imported via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

add_zilker_cpp_test(NAME testMatterConfigureSubscriptionSpecs
        LIBS xhDeviceServiceStatic
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../..
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/MatterConfigureSubscriptionSpecsTest.cpp
)

add_zilker_cpp_test(NAME testMatterSubscribeInteraction
        LIBS xhDeviceServiceStatic
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../..
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/MatterSubscribeInteractionTest.cpp)

configure_glib()
