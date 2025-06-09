#pragma once

#include "third-party/ui.hpp"

#include <functional>
#include <memory>
#include <queue>

// 弹窗管理器
class Popup_modal_manager
{
  public:

	using Render_func = std::function<bool(bool)>;

	// 弹窗的参数
	struct Window
	{
		std::string title;       // 标题
		ImGuiWindowFlags flags;  // Window Flags
		bool has_close_button;   // 有关闭按钮
		bool keep_centered;      // 保持窗口居中

		// 渲染函数 -> bool(bool)
		// - 输入参数为true则代表关闭按钮被按下
		// - 输出为true代表弹窗关闭
		Render_func render_func;
	};

  private:

	struct Internal_window : public Window
	{
		bool opened;
		bool close_requested;
		bool imgui_popen;

		Internal_window(Window&& window);
	};

	std::vector<std::unique_ptr<Internal_window>> windows;
	std::queue<std::unique_ptr<Internal_window>> add_window_queue;

  public:

	// 绘制函数，每一帧都要调用一次
	void draw();

	// 添加新弹窗
	void open_window(Window window);
};