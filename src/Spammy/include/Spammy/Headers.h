#pragma once

#include <array>
#include <atomic>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#define _USE_MATH_DEFINES
#include <cmath>

#include <Windows.h>
#include <Windowsx.h>
#include <d3d9.h>
#include <psapi.h>
#include <shellapi.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx9.h>
#include <imgui/imgui_impl_win32.h>
#include <nlohmann/json.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui_internal.h>

#pragma comment(lib, "d3d9")
