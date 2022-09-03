project( imgui )

add_library(
	${PROJECT_NAME} STATIC
		imconfig.h
		imgui_demo.cpp
		imgui_draw.cpp
		imgui_impl_dx9.cpp
		imgui_impl_dx9.h
		imgui_impl_dx11.cpp
		imgui_impl_dx11.h
		imgui_impl_win32.cpp
		imgui_impl_win32.h
		imgui_internal.h
		imgui_stdlib.cpp
		imgui_stdlib.h
		imgui_tables.cpp
		imgui_widgets.cpp
		imgui.cpp
		imgui.h
		imstb_rectpack.h
		imstb_textedit.h
		imstb_truetype.h
)