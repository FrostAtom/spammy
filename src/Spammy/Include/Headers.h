#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#define _USE_MATH_DEFINES
#include <cmath>

#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h>
#include <d3d9.h>
#include <dwmapi.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <winhttp.h>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include <nlohmann/json.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_internal.h>

#pragma comment(lib, "d3d9")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "winhttp")
#pragma comment(lib, "winmm")
