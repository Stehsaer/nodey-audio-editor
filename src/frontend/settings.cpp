#include "frontend/settings.hpp"
#include "config.hpp"
#include "imnodes.h"
#include "utility/dialog-utility.hpp"

#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <json/json.h>

#define SET_KEY(key, type) json[#key] = key
#define GET_KEY(key, type)                                                                                   \
	if (json.isMember(#key)) key = json[#key].as##type();

// UI_settings
Json::Value UI_settings::serialize() const
{
	Json::Value json;
	SET_KEY(show_toolbar, Bool);
	SET_KEY(show_minimap, Bool);
	SET_KEY(show_grid, Bool);
	SET_KEY(grid_size, Float);
	SET_KEY(snap_to_grid, Bool);
	SET_KEY(side_panel_width, Int);
	return json;
}

void UI_settings::deserialize(const Json::Value& json)
{
	GET_KEY(show_toolbar, Bool);
	GET_KEY(show_minimap, Bool);
	GET_KEY(show_grid, Bool);
	GET_KEY(grid_size, Float);
	GET_KEY(snap_to_grid, Bool);
	GET_KEY(side_panel_width, Int);
}

// EditorSettings
Json::Value Editor_settings::serialize() const
{
	Json::Value json;
	SET_KEY(auto_save, Bool);
	SET_KEY(auto_save_interval, Int);
	SET_KEY(max_undo_levels, Int);
	return json;
}

void Editor_settings::deserialize(const Json::Value& json)
{
	GET_KEY(auto_save, Bool);
	GET_KEY(auto_save_interval, Int);
	GET_KEY(max_undo_levels, Int);
}

// Export_settings
Json::Value Export_settings::serialize() const
{
	Json::Value json;
	SET_KEY(default_output_directory, String);
	return json;
}

void Export_settings::deserialize(const Json::Value& json)
{
	GET_KEY(default_output_directory, String);
}

// App_settings
Json::Value App_settings::serialize() const
{
	Json::Value json;
	json["ui"] = ui.serialize();
	json["editor"] = editor.serialize();
	json["render"] = export_settings.serialize();
	return json;
}

void App_settings::deserialize(const Json::Value& json)
{
	if (json.isMember("ui")) ui.deserialize(json["ui"]);
	if (json.isMember("editor")) editor.deserialize(json["editor"]);
	if (json.isMember("render")) export_settings.deserialize(json["render"]);
}
// 设置文件管理
void App_settings::load_from_file(const std::string& path)
{
	// 检查文件是否存在
	if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) return;

	std::ifstream file(path);
	if (!file.is_open()) return;

	Json::Value json;
	Json::parseFromStream(Json::CharReaderBuilder(), file, &json, nullptr);

	deserialize(json);
}

bool App_settings::save_to_file(const std::string& path) const
{
	std::ofstream file(path);
	if (!file.is_open()) return false;

	Json::StreamWriterBuilder writer;
	std::unique_ptr<Json::StreamWriter> json_writer(writer.newStreamWriter());
	return json_writer->write(serialize(), &file) != 0;
}

void App_settings::reset_to_defaults()
{
	ui = UI_settings();
	editor = Editor_settings();
	export_settings = Export_settings();
}

void Settings_window::draw_ui_tab()
{
	ImGui::SeparatorText("User Interface");

	// 界面元素显示
	ImGui::Checkbox("Show Toolbar", &new_settings.ui.show_toolbar);
	ImGui::Checkbox("Show Minimap", &new_settings.ui.show_minimap);

	ImGui::SeparatorText("Grid Settings");

	ImGui::Checkbox("Show Grid", &new_settings.ui.show_grid);
	ImGui::SliderFloat("Grid Size", &new_settings.ui.grid_size, 5.0f, 100.0f);
	ImGui::Checkbox("Snap to Grid", &new_settings.ui.snap_to_grid);
	ImGui::SliderInt("Side Panel Width", &new_settings.ui.side_panel_width, 200, 500);
}

void Settings_window::draw_editor_tab()
{
	ImGui::SeparatorText("Editor Preferences");

	ImGui::Checkbox("Auto Save", &new_settings.editor.auto_save);

	if (new_settings.editor.auto_save)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::InputInt("Interval (sec)", &new_settings.editor.auto_save_interval);
		new_settings.editor.auto_save_interval = std::max(10, new_settings.editor.auto_save_interval);
	}

	ImGui::SliderInt("Max Undo Levels", &new_settings.editor.max_undo_levels, 10, 100);
}

void Settings_window::draw_export_tab()
{
	ImGui::SeparatorText("Export Settings");

	if (ImGui::Button("Select"))
	{
		const std::string starting_directory
			= std::filesystem::exists(new_settings.export_settings.default_output_directory)
				   && std::filesystem::is_directory(new_settings.export_settings.default_output_directory)
				? new_settings.export_settings.default_output_directory
				: std::filesystem::current_path().string();

		const auto selected_directory = open_folder_dialog("Select Output Directory", starting_directory);

		if (selected_directory.has_value())
			new_settings.export_settings.default_output_directory = selected_directory.value();
	}

	ImGui::SameLine();
	ImGui::Text("Default Directory: %s", new_settings.export_settings.default_output_directory.c_str());
}

bool Settings_window::operator()(bool close_button_pressed)
{
	draw_ui_tab();
	draw_editor_tab();
	draw_export_tab();

	ImGui::PushItemWidth(100);
	const bool cancel_button_clicked = ImGui::Button("Cancel");
	const bool apply_button_clicked = ImGui::Button("Save");
	ImGui::PopItemWidth();

	if (cancel_button_clicked) return true;

	if (apply_button_clicked)
	{
		original_settings = new_settings;
		return true;
	}

	if (close_button_pressed) return true;

	return false;
}

Popup_modal_manager::Window Settings_window::create(App_settings& settings)
{
	return Popup_modal_manager::Window{
		.title = "Settings",
		.flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize,
		.has_close_button = false,
		.keep_centered = true,
		.render_func = Settings_window(settings),
	};
}
