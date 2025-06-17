#pragma once

#include <algorithm>
#include <iostream>
#include <print>
#include <ranges>
#include <set>
#include <source_location>

#include "config.hpp"
#include "frontend/popup.hpp"
#include "infra/graph.hpp"
#include "infra/processor.hpp"
#include "infra/runner.hpp"

#include "frontend/font.hpp"
#include "frontend/imgui-context.hpp"
#include "frontend/sdl-context.hpp"

// 应用程序主类
// - 管理应用程序状态
// - 启动/停止预览
// - 绘制界面
class App
{
	/* 界面逻辑相关 */

	enum class State
	{
		Editing,            // 正在编辑
		Preview_requested,  // 请求预览
		Previewing,         // 正在运行预览
		Preview_cancelling  // 停止预览
	};

	enum class Node_editor_context_menu_state
	{
		Closed,
		Open_requested,
		Opened
	};

	std::atomic<State> state = State::Editing;  // 程序状态
	std::atomic<Node_editor_context_menu_state> node_editor_context_menu_state
		= Node_editor_context_menu_state::Closed;           // 节点编辑器右键菜单状态
	std::optional<infra::Id_t> new_node_id = std::nullopt;  // 新创建的节点ID，用于设置新节点到鼠标的位置

	/* UI 相关对象 */

	SDL_context sdl_context;            // SDL窗口上下文
	Imgui_context imgui_context;        // IMGUI上下文
	Popup_modal_manager popup_manager;  // 弹窗管理器

	/* 节点相关对象 */

	infra::Graph graph;                     // 节点图
	std::unique_ptr<infra::Runner> runner;  // 预览运行器

	/* 逻辑处理与绘制 */

	// 添加错误弹窗
	void add_error_popup_window(
		std::string message,
		std::string explanation = "",
		std::string detail = "",
		std::source_location loc = std::source_location::current()
	);

	// 绘制一个节点
	void draw_node(infra::Id_t id, const infra::Graph::Node& node);

	// 绘制节点编辑器
	void draw_node_editor();

	// 绘制节点编辑器的右键菜单
	void draw_node_editor_context_menu();

	//处理新增/删除节点
	void handle_node_actions();

	// （辅助）删除选中的节点
	void remove_selected_nodes();

	// （辅助）绘制节点添加菜单
	void draw_add_node_menu();

	// 绘制左面板
	void draw_left_panel();

	// 绘制右面板（编辑器）
	void draw_right_panel();

	// 绘制主菜单栏
	void draw_menubar();

	// 总绘制函数
	void draw();

	// 更新APP状态
	void poll_state();

	// 创建预览Runner
	void create_preview_runner();

	// 显示Runner发生的错误弹窗
	void show_preview_runner_error(const std::any& error);

  public:

	void run();

	App() :
		imgui_context(sdl_context.get_window_ptr(), sdl_context.get_renderer_ptr())
	{
		infra::register_all_processors();
	}
};
