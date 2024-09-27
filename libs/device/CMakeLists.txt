#--------------------------------------------------------------------------------------
#
# CMake definition for the fcore device libraries
# Should not be utilized directly, but imported from our top-level
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# the device libraries to compile
#
add_subdirectory(descriptors/c)
add_subdirectory(helper/c)

# add 'philips hue lights' if the option is set
if (BARTON_CONFIG_PHILIPS_HUE)
    add_subdirectory(xhPhilipsHue/c)
endif (BARTON_CONFIG_PHILIPS_HUE)