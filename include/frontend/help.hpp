#pragma once
#include "frontend/popup.hpp"
#include <string>

// 前置声明
class App;

namespace about
{
	// 原有的内容获取函数
	std::string get_help_documentation();
	std::string get_online_help_documentation();
	std::string get_about_information();
	std::string get_about_information_online_link();
	std::string get_about_update_information();
	std::string get_about_update_information_online_link();

	// 新增的Help窗口函数
	void show_help_documentation_window(App* app, Popup_modal_manager& popup_manager);
	void show_about_window(App* app, Popup_modal_manager& popup_manager);
	void show_updates_window(App* app, Popup_modal_manager& popup_manager);
	void show_simple_info_popup(
		Popup_modal_manager& popup_manager,
		const std::string& title,
		const std::string& message,
		const std::string& url
	);
}