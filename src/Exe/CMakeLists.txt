project( Spammy )
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")

add_executable(
	${PROJECT_NAME} WIN32
		resources/resources.h
		resources/resources.rc
		Window/Renderer/Renderer.h
		Window/Renderer/D3d11.cpp
		Window/Renderer/D3d11.h
		Window/Renderer/D3d9.cpp
		Window/Renderer/D3d9.h
		Window/DropManager.cpp
		Window/DropManager.h
		Window/TrayIcon.cpp
		Window/TrayIcon.h
		Window/Window.cpp
		Window/Window.h
		App.cpp
		App.h
		"Win32/Keyboard.cpp"
		"Win32/Keyboard.h"
		"Win32/Mouse.cpp"
		"Win32/Mouse.h"
		"Win32/WinEvent.cpp"
		"Win32/WinEvent.h"
		KeyboardLayout.h
		
		
		
		"Utils.cpp"
		"Utils.h"
  "ImGui.h" "ImGui.cpp"         "Profile.h"   "LuaApi.cpp")

target_compile_definitions(
	${PROJECT_NAME} PRIVATE
		APP_NAME="${PROJECT_NAME}"
)

target_link_libraries(
	${PROJECT_NAME} PRIVATE
		imgui
		lua
)

target_include_directories(
	${PROJECT_NAME} PRIVATE
		${CMAKE_SOURCE_DIR}/deps/
)