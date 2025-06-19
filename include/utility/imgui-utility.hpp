#pragma once
#include<string>
// IMGUI辅助控件绘制
namespace imgui_utility
{
	// 绘制带阴影的文本
	void shadowed_text(const char* text);
	//显示处理器描述信息的折叠标题
	void display_processor_description(const std::string& description, bool default_open = true);
}