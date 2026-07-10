cmake_minimum_required(VERSION 3.21)

# cmake general
cmake_policy(SET CMP0091 NEW)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD OFF)
set(CMAKE_SUPPRESS_REGENERATION ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/${CMAKE_BUILD_TYPE}/)
# static CRT (/MT, /MTd) via CMP0091 abstraction — keeps the exe standalone.
# Setting it through the manual flags below is overridden by CMake's default /MD, so it must go here.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# x86-only: the window-proc userdata and other Win32 code target a 32-bit process
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Only the x86 (32-bit) build is supported. Configure from an x86 developer environment (vcvarsall.bat x86 / 'x86 Native Tools Command Prompt') or use the CMake presets.")
endif()

# msvc general
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX23_STANDARD_COMPILE_OPTION "-std:c++latest")
set(CMAKE_CXX23_EXTENSION_COMPILE_OPTION "-std:c++latest")
set(COMMON_COMPILE_FLAGS "/W3 /DUNICODE /D_UNICODE /D_WIN32_WINNT=0x0601 /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS")
# /Gy + /Gw: split functions/data into COMDATs so the linker can strip and fold them
# /GL + /LTCG: whole-program optimization; /OPT:REF drops unused code, /OPT:ICF folds identical COMDATs
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_COMPILE_FLAGS} /O2 /Gw /Gy -flto /DNDEBUG")
	set(CMAKE_C_FLAGS_RELEASE	"${COMMON_COMPILE_FLAGS} /O2 /Gw /Gy -flto /DNDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG	"${COMMON_COMPILE_FLAGS} /Od /Zi /DDEBUG /D_DEBUG")
	set(CMAKE_C_FLAGS_DEBUG		"${COMMON_COMPILE_FLAGS} /Od /Zi /DDEBUG /D_DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/OPT:REF /OPT:ICF /INCREMENTAL:NO")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_COMPILE_FLAGS} /O2 /Gw /Gy /GL /wd4458 /DNDEBUG")
	set(CMAKE_C_FLAGS_RELEASE	"${COMMON_COMPILE_FLAGS} /O2 /Gw /Gy /GL /wd4458 /DNDEBUG")
	set(CMAKE_CXX_FLAGS_DEBUG	"${COMMON_COMPILE_FLAGS} /Od /Zi /wd4458 /DDEBUG /D_DEBUG")
	set(CMAKE_C_FLAGS_DEBUG		"${COMMON_COMPILE_FLAGS} /Od /Zi /wd4458 /DDEBUG /D_DEBUG")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO")
	# /GL objects require /LTCG when archived into the static lib too
	set(CMAKE_STATIC_LINKER_FLAGS_RELEASE "/LTCG")
else()
	message(FATAL_ERROR "Unsupported toolset, use MSVC or Clang for build")
endif()

set(OUT_DIR "" CACHE PATH "Dist dir")

# sources
add_subdirectory( deps )

# static analysis: opt-in (use the *-tidy presets) because it slows compilation down noticeably.
# Set after deps so vendored code is never analyzed; config comes from the repo-root .clang-tidy.
# CMake passes --driver-mode=cl automatically when the compiler is MSVC.
option(ENABLE_CLANG_TIDY "Run clang-tidy on project sources during compilation" OFF)
if(ENABLE_CLANG_TIDY)
	find_program(CLANG_TIDY_EXE NAMES clang-tidy HINTS "$ENV{ProgramFiles}/LLVM/bin")
	if(NOT CLANG_TIDY_EXE)
		message(FATAL_ERROR "ENABLE_CLANG_TIDY is ON but clang-tidy was not found (install LLVM or add clang-tidy to PATH)")
	endif()
	# /Y- makes clang-tidy ignore the MSVC-format PCH (/Yu /Fp), which clang cannot read;
	# the headers are still parsed normally through CMake's /FI force-include of cmake_pch.hxx
	set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "--extra-arg=/Y-")
endif()

add_subdirectory( src )
