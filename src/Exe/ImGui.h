#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#define _USE_MATH_DEFINES
#include <cmath>

namespace ImGui {
void Tip(const char* fmt, ...);

// Cycles hue every f seconds
ImVec4 CycleColor(float s = 1.f, float v = 1.f, float a = 1.f, float f = 8.f);
// Flashes alpha every f seconds
ImVec4 FlashColor(float r, float g, float b, float f = 8.f, float min = 0.f, float max = 1.f);

void PushStyleColorTriplet(ImGuiCol idx, ImVec4 col);

inline void PopStyleColorTriplet() { ImGui::PopStyleColor(3); }
}