#include "ImGui.h"

void ImGui::Tip(const char* fmt, ...)
{
    if (IsItemHovered() && GImGui->HoveredIdTimer > 1.f) {
        va_list args;
        va_start(args, fmt);
        ImGui::SetTooltipV(fmt, args);
    }
}

ImVec4 ImGui::CycleColor(float s, float v, float a, float f)
{
    float x = fmod(ImGui::GetTime() / f, 1.f);
    return ImColor::HSV(x, s, v, a);
}

ImVec4 ImGui::FlashColor(float r, float g, float b, float f, float min, float max)
{
    float x = fmod(ImGui::GetTime() / f, 1.f);
    float a = min + (max - min) * std::abs(std::sin((M_PI * 2) * x));
    return ImVec4(r, g, b, a);
}

void ImGui::PushStyleColorTriplet(ImGuiCol idx, ImVec4 col)
{
    for (int i = 0; i < 3; i++)
        ImGui::PushStyleColor(idx + i, col);
}