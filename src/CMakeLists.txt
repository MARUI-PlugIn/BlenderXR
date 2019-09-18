use_cxx11()

if (OPENXR)
set(SOURCE_FILES
	"vr_openxr.cpp"
)

set(HEADER_FILES
	"vr.h"
	"vr_openxr.h"
)
else()
set(SOURCE_FILES
	"vr_steam.cpp"
)

set(HEADER_FILES
	"vr.h"
	"vr_steam.h"
)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})

# Glew
include_directories(${PROJECT_SOURCE_DIR}/inc/glew_1.13.0)
link_directories(${PROJECT_SOURCE_DIR}/lib/glew_1.13.0)
if(APPLE)
target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/glew_1.13.0/libGLEW_macos.a)
else()
target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/glew_1.13.0/libGLEW.a)
endif()

if (OPENXR)
  # OpenXR
  include_directories(${PROJECT_SOURCE_DIR}/inc/openxr_1.0.1/include/openxr)
  link_directories(${PROJECT_SOURCE_DIR}/lib/openxr_1.0.1)
  if(APPLE)
  target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/openxr_1.0.1/libopenxr_loader_macos.a)
  else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/openxr_1.0.1/libopenxr_loader.a)
  endif()
else()
  # OpenVR
  include_directories(${PROJECT_SOURCE_DIR}/inc/openvr_1.3.22)
  link_directories(${PROJECT_SOURCE_DIR}/lib/openvr_1.3.22)
  if(APPLE)
  target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/openvr_1.3.22/libopenvr_api_macos.a)
  else()
  target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/openvr_1.3.22/libopenvr_api.a)
  endif()
endif()