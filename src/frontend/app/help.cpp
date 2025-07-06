
#include "frontend/app/help.hpp"
#include "config.hpp"
#include "frontend/app.hpp"
#include "frontend/app/popup.hpp"

#include <imgui.h>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
	const std::string_view detailed_help_content =
		R"(## Basic Usage

- New/Open/Save Project: Use the "File" menu to create, open, or save projects.
- Add Node: Right-click in the node editor area to add nodes.
- Connect Nodes: Drag from output port to input port to create connections.
- Delete Node/Link: Select and press Delete key.

## Audio Operations

- Preview: Click the "Preview" button or use toolbar to listen to your audio graph.
- Stop: Use the stop button to halt audio playbook.

## Editing Features

- Undo/Redo: Use Ctrl+Z and Ctrl+Y to undo or redo actions.
- Copy/Paste: Use Ctrl+C and Ctrl+V to copy and paste nodes.

## View Controls

- Grid Settings: Configure grid spacing and snapping from View menu.
- Performance Monitor: Toggle with View → Status → Show Performance Metrics.

## Tips & Tricks

- Save your project frequently to avoid data loss.
- Use right-click context menu for quick access to frequently-used operations.

)";

	const std::string detailed_about_content = std::format(
		R"(# Nodey Audio Editor

- Build Time: {} {}

Nodey Audio Editor is a powerful, user-friendly audio editing tool based on a node graph interface. It allows you to create, edit, and process audio in a flexible and visual way.

## Features

- Open source and cross-platform
- Real-time audio preview
- Support for various audio effects
- Easy-to-use node-based interface
)",
		__DATE__,
		__TIME__
	);

	void display_markdown(const std::string_view& string)
	{
		std::istringstream ss((std::string(string)));
		std::string line;

		while (std::getline(ss, line))
		{
			if (line.empty())
			{
				ImGui::Dummy({1, 5 * runtime_config::ui_scale});
				continue;
			}

			if (line.starts_with("# "))
			{
				auto drawlist = ImGui::GetWindowDrawList();
				drawlist->AddText(
					ImGui::GetFont(),
					ImGui::GetFontSize() * 1.5,
					ImGui::GetCursorScreenPos(),
					ImGui::GetColorU32(ImGuiCol_Text),
					line.substr(2).c_str()
				);
				ImGui::Dummy({1, ImGui::GetTextLineHeight() * 1.5f});
				ImGui::Separator();
			}
			else if (line.starts_with("## "))
			{
				ImGui::SeparatorText(line.substr(3).c_str());
			}
			else if (line.starts_with("- "))
			{
				ImGui::BulletText("%s", line.substr(2).c_str());
			}
			else
			{
				ImGui::TextWrapped("%s", line.c_str());
			}
		}
	}

}

Popup_manager::Window help_documentation_window()
{
	return Popup_manager::Window{
		.title = "Help Documentation",
		.flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		.has_close_button = true,
		.keep_centered = true,
		.render_func = [](bool close_button_pressed)
		{
			display_markdown(detailed_help_content);
			return close_button_pressed;
		}
	};
}

Popup_manager::Window about_window()
{
	return Popup_manager::Window{
		.title = "About",
		.flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		.has_close_button = true,
		.keep_centered = true,
		.render_func = [](bool close_button_pressed)
		{
			display_markdown(detailed_about_content);
			return close_button_pressed;
		}
	};
}