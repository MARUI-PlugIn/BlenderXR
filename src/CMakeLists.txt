use_cxx11()

set(SOURCE_FILES
	"vr_steam.cpp"
)

set(HEADER_FILES
	"vr.h"
	"vr_steam.h"
)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})

# Glew
include_directories(${PROJECT_SOURCE_DIR}/inc/glew_1.13.0)
link_directories(${PROJECT_SOURCE_DIR}/lib/glew_1.13.0)
target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/glew_1.13.0/libGLEW.a)

# OpenVR
include_directories(${PROJECT_SOURCE_DIR}/inc/openvr_1.0.13)
link_directories(${PROJECT_SOURCE_DIR}/lib/openvr_1.0.13)
target_link_libraries(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/lib/openvr_1.0.13/libopenvr_api.a)
