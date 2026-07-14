project( imgui )


set(HDRS
	imconfig.h
	imgui_impl_dx9.h
	imgui_impl_win32.h
	imgui_internal.h
	imgui_stdlib.h
	imgui.h
	imstb_rectpack.h
	imstb_textedit.h
	imstb_truetype.h
)

set(SRCS
	imgui_demo.cpp
	imgui_draw.cpp
	imgui_impl_dx9.cpp
	imgui_impl_win32.cpp
	imgui_stdlib.cpp
	imgui_tables.cpp
	imgui_widgets.cpp
	imgui.cpp
)

add_library(
	${PROJECT_NAME}_lib STATIC
		${HDRS}
		${SRCS}
)

target_compile_definitions(
	${PROJECT_NAME}_lib PRIVATE
		IMGUI_DISABLE_DEMO_WINDOWS
)

add_library(${PROJECT_NAME} INTERFACE)

target_link_libraries(${PROJECT_NAME} INTERFACE
	${PROJECT_NAME}_lib
)
