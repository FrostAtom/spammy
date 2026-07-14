include(${CMAKE_SOURCE_DIR}/cmake/FetchFilesInFolder.cmake)

project(Spammy LANGUAGES C CXX)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /MANIFEST:NO")

FetchFilesInFolder(HDRS "Include")
FetchFilesInFolder(SRCS "Source")

add_executable(
	${PROJECT_NAME} WIN32
		${HRDS}
		${SRCS}
		"Include/Resources.h"
		Source/Resources.rc
)

target_precompile_headers(
	${PROJECT_NAME} PRIVATE
		Include/Headers.h
)

target_compile_definitions(
	${PROJECT_NAME} PRIVATE
		APP_NAME="${PROJECT_NAME}"
)

target_link_libraries(
	${PROJECT_NAME} PRIVATE
		imgui
)

target_include_directories(
	${PROJECT_NAME} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_SOURCE_DIR}/deps/
)
