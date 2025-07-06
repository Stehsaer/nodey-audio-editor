#include "utility/imgui-utility.hpp"

#include <imgui.h>
#include<string>
#include<sstream>

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
	void display_processor_description(const std::string& description, bool default_open)
	{
		const ImGuiTreeNodeFlags flags = default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0;

		if (ImGui::CollapsingHeader("Description", flags))
		{
			std::istringstream ss(description);
			std::string line;

			while (std::getline(ss, line))
			{
				if (line.empty())
				{
					ImGui::Dummy({1, 3});
					continue;
				}

				if (line.starts_with("## "))
				{
					ImGui::SeparatorText(line.substr(3).c_str());
				}
				else if (line.starts_with("- ") || line.starts_with("• "))
				{
					// 手动创建项目符号 + 换行文本
					ImGui::Bullet();
					ImGui::SameLine();
					ImGui::TextWrapped("%s", line.substr(2).c_str());
				}
				else
				{
					ImGui::TextWrapped("%s", line.c_str());
				}
			}
		}
	}
}