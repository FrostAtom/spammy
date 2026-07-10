project(Spammy LANGUAGES C CXX)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup /MANIFEST:NO")

add_executable(
	${PROJECT_NAME} WIN32
		include/Spammy/App.h
		include/Spammy/Headers.h
		include/Spammy/ImGui.h
		include/Spammy/KeyboardLayout.h
		include/Spammy/MainWindow.h
		include/Spammy/Profile.h
		include/Spammy/Utils.h
		include/Spammy/Win32/Keyboard.h
		include/Spammy/Window/Detail/ImFont_ComicSans.h
		include/Spammy/Window/Renderer/D3d9.h
		include/Spammy/Window/Renderer/Renderer.h
		include/Spammy/Window/TrayIcon.h
		include/Spammy/Window/Window.h

		source/App.cpp
		source/ImGui.cpp
		source/MainWindow.cpp
		source/Utils.cpp
		source/Win32/Keyboard.cpp
		source/Window/Renderer/D3d9.cpp
		source/Window/TrayIcon.cpp
		source/Window/Window.cpp

		source/manifest.rc
		source/resources/resources.h
		source/resources/resources.rc
)

target_precompile_headers(
	${PROJECT_NAME} PRIVATE
		include/Spammy/Headers.h
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
