cmake_minimum_required (VERSION 2.6)
project(BlenderXR_SteamVR)

macro(use_cxx11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
    endif()
  else()
    set (CMAKE_CXX_STANDARD 11)
  endif()
endmacro(use_cxx11)

add_subdirectory(${PROJECT_SOURCE_DIR}/src)

set_target_properties (${PROJECT_NAME}
    PROPERTIES
if(APPLE)
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/MacOS/SteamVR"
else()
	LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/Linux/SteamVR"
endif()
)