#pragma once

#include <functional>
#include <json/json.h>
#include <memory>
#include <string>

#include "popup.hpp"

// UI/界面设置
struct UI_settings
{
	bool show_toolbar = true;
	bool show_minimap = true;
	bool show_grid = true;
	float grid_size = 20.0f;
	bool snap_to_grid = false;
	int side_panel_width = 300;

	Json::Value serialize() const;
	void deserialize(const Json::Value& json);
};

// 编辑器设置
struct Editor_settings
{
	bool auto_save = true;
	int auto_save_interval = 300;
	int max_undo_levels = 30;

	Json::Value serialize() const;
	void deserialize(const Json::Value& json);
};

// 渲染设置
struct Export_settings
{
	std::string default_output_directory = "./output/";

	Json::Value serialize() const;
	void deserialize(const Json::Value& json);
};

// 主设置类
struct App_settings
{
	UI_settings ui;
	Editor_settings editor;
	Export_settings export_settings;

	Json::Value serialize() const;
	void deserialize(const Json::Value& json);

	void load_from_file(const std::string& path = "");
	bool save_to_file(const std::string& path = "") const;
	void reset_to_defaults();
};

class Settings_window
{
	App_settings& original_settings;
	App_settings new_settings;

	Settings_window(App_settings& settings) :
		original_settings(settings),
		new_settings(settings)
	{
	}

  public:

	static Popup_modal_manager::Window create(App_settings& settings);

	bool operator()(bool close_button_pressed);

  private:

	void draw_ui_tab();
	void draw_editor_tab();
	void draw_export_tab();
};