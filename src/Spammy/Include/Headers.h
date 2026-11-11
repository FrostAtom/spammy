#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <functional>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#define NOMINMAX
#include <Windows.h>
#include <Windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>

#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <uni_algo/case.h>
#include <uni_algo/conv.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
