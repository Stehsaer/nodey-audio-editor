#include "frontend/imgui-utility.hpp"

#include <imgui.h>

#include "config.hpp"

namespace imgui_utility
{
	void shadowed_text(const char* text)
	{
		auto* draw_list = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImFont* font = ImGui::GetFont();
		float font_size = ImGui::GetFontSize();

		draw_list->AddText(
			font,
			font_size,
			ImVec2(pos.x, pos.y + 2 * runtime_config::ui_scale),
			IM_COL32(0, 0, 0, 192),
			text
		);
		draw_list->AddText(
			font,
			font_size,
			ImVec2(pos.x + 1 * runtime_config::ui_scale, pos.y + 1 * runtime_config::ui_scale),
			IM_COL32(0, 0, 0, 96),
			text
		);
		draw_list->AddText(
			font,
			font_size,
			ImVec2(pos.x - 1 * runtime_config::ui_scale, pos.y + 1 * runtime_config::ui_scale),
			IM_COL32(0, 0, 0, 96),
			text
		);
		draw_list->AddText(font, font_size, pos, ImGui::GetColorU32(ImGuiCol_Text), text);

		// Advance cursor to avoid overlap
		ImVec2 text_size = ImGui::CalcTextSize(text);
		ImGui::Dummy(ImVec2(text_size.x, text_size.y));
	}
}