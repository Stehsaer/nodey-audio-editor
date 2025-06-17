#pragma once

// 系统头文件

#include <atomic>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <print>
#include <ranges>
#include <set>
#include <source_location>
#include <string>
#include <tuple>

// 第三方库
#include <imgui.h>

// 项目头文件
#include "config.hpp"
#include "frontend/app/help.hpp"
#include "frontend/app/popup.hpp"
#include "frontend/app/settings.hpp"
#include "frontend/font.hpp"
#include "frontend/imgui-context.hpp"
#include "frontend/sdl-context.hpp"
#include "infra/graph.hpp"
#include "infra/processor.hpp"
#include "infra/runner.hpp"

// 应用程序主类
// - 管理应用程序状态
// - 启动/停止预览
// - 绘制界面
class App
{
  public:

	// 构造函数 - 初始化SDL和ImGui上下文，注册处理器
	App() :
		imgui_context(sdl_context.get_window_ptr(), sdl_context.get_renderer_ptr())
	{
		infra::register_all_processors();
	}

	// 主运行循环 - 处理事件、更新状态、绘制界面
	void run();

  private:

	// =============================================================================
	// 枚举和类型定义
	// =============================================================================

	// 应用程序主要状态
	enum class State
	{
		Editing,             // 正在编辑模式
		Preview_requested,   // 请求开始预览
		Previewing,          // 正在运行预览
		Preview_cancelling,  // 请求停止预览
		IO                   // 输入输出操作中（如保存、打开项目等）
	};

	// =============================================================================
	// 核心状态和对象
	// =============================================================================

	// 应用程序状态（线程安全）
	std::atomic<State> state = State::Editing;

	// UI核心对象
	SDL_context sdl_context;      // SDL渲染上下文
	Imgui_context imgui_context;  // ImGui上下文
	Popup_manager popup_manager;  // 弹窗管理器

	// 图形和运行器
	infra::Graph graph;                     // 节点图数据结构
	std::string graph_path;                 // 当前图的文件路径
	std::unique_ptr<infra::Runner> runner;  // 音频预览运行器

	// 撤销/重做系统
	std::list<infra::Graph> undo_stack;  // 撤销栈
	std::list<infra::Graph> redo_stack;  // 重做栈

	// 复制粘贴系统
	std::string copied_graph_json;               // 复制的图数据（JSON格式）
	ImVec2 last_paste_position{100.0f, 100.0f};  // 上次粘贴位置

	// 延迟选择
	std::optional<infra::Id_t> delayed_selection_node, delayed_selection_link;  // 延迟选择的节点ID
	int open_node_context_menu_tries = 0;  // 是否无条件打开节点上下文菜单

	// =============================================================================
	// UI设置和状态
	// =============================================================================

	uint32_t main_menu_bar_height = 0;  // 主菜单栏高度（像素）

	App_settings app_settings;  // 应用程序设置
	bool show_diagnostics = false;
	bool show_demo_window = false;

	// =============================================================================
	// 弹窗管理
	// =============================================================================

	std::jthread background_thread;

	// 错误和信息弹窗
	void add_error_popup_window(                       // 添加错误弹窗
        std::string message,                           // 错误消息
        std::string explanation = "",                  // 详细说明
        std::string detail = "",                       // 技术细节
        std::source_location loc = std::source_location::current()  // 源码位置
    );

	void add_info_popup_window(                        // 添加信息弹窗
        std::string title,                             // 标题
        std::string explanation = "",                  // 说明
        std::string detail = ""                        // 详细信息
    );

	enum class New_project_window_result
	{
		Save,     // 保存新项目
		Discard,  // 丢弃新项目
		Cancel    // 取消操作
	};

	// 确认对话框
	std::future<New_project_window_result> add_new_project_confirm_window_async();

	void show_open_confirm_dialog(
		const std::string& filepath,
		const std::string& json_content
	);  // 打开文件确认对话框

	// // 设置对话框
	// void show_app_settings();          // 显示app设置对话框
	// void draw_grid_settings_dialog();  // 绘制网格设置对话框

	// =============================================================================
	// 项目管理
	// =============================================================================

	void save_project();       // 保存项目
	void open_project();       // 打开项目
	void clear_project();      // 清空项目
	void new_project_async();  // 创建新项目

	bool save_project_with_path(const std::string& filename);  // 保存项目到指定路径
	bool load_project_from_file(const std::string& filepath);  // 从文件加载项目

	// 序列化
	std::string save_graph_as_string() const;                     // 将图序列化为JSON字符串
	void load_graph_from_string(const std::string& json_string);  // 从JSON字符串反序列化图

	// =============================================================================
	// 撤销/重做系统
	// =============================================================================

	void save_undo_state();         // 保存当前状态到撤销栈
	void save_redo_state();         // 保存当前状态到重做栈
	void undo();                    // 执行撤销操作
	void redo();                    // 执行重做操作
	void restore_node_positions();  // 恢复节点位置
	void compress_undo_stack();     // 压缩撤销栈以节省内存

	// =============================================================================
	// 复制粘贴系统
	// =============================================================================

	void copy_selected_nodes();  // 复制选中的节点和连接
	void paste_nodes();          // 粘贴节点和连接

	// =============================================================================
	// UI绘制 - 主要组件
	// =============================================================================

	// 主绘制函数
	void draw();  // 主绘制函数，绘制整个界面

	// 面板绘制
	void draw_menubar();              // 绘制主菜单栏
	void draw_side_panel();           // 绘制侧边面板
	void draw_main_panel();           // 绘制主面板（节点编辑器）
	void draw_toolbar();              // 绘制工具栏
	void draw_diagnostics_overlay();  // 绘制性能信息覆盖层

	void sync_ui_settings() const;  // 同步UI设置到ImGui/ImNode上下文

	// =============================================================================
	// UI绘制 - 菜单栏
	// =============================================================================

	// 菜单栏子菜单
	void draw_menubar_file();  // 绘制文件菜单
	void draw_menubar_edit();  // 绘制编辑菜单
	void draw_menubar_view();  // 绘制视图菜单
	void draw_menubar_help();  // 绘制帮助菜单

	// 视图菜单子项
	void draw_view_panels_menu();     // 绘制面板显示控制菜单
	void draw_view_center_menu();     // 绘制居中视图菜单
	void draw_view_preview_status();  // 绘制预览状态信息

	// =============================================================================
	// 节点编辑器
	// =============================================================================

	// 节点编辑器绘制
	void draw_node_editor();                                         // 绘制节点编辑器主界面
	void draw_node(infra::Id_t id, const infra::Graph::Node& node);  // 绘制单个节点
	void draw_node_editor_context_menu();                            // 绘制节点编辑器右键菜单
	void draw_add_node_menu();                                       // 绘制添加节点菜单

	// 节点编辑器操作
	void handle_node_actions();        // 处理节点编辑器的键盘和鼠标操作
	void remove_selected_nodes();      // 删除选中的节点和连接
	void handle_keyboard_shortcuts();  // 处理键盘快捷键操作

	// =============================================================================
	// 状态管理和预览
	// =============================================================================

	// 状态轮询和预览
	void poll_state();                                      // 轮询应用程序状态，处理状态转换
	void create_preview_runner();                           // 创建音频预览运行器
	void show_preview_runner_error(const std::any& error);  // 显示预览运行器错误信息

	// 辅助函数
	static std::string get_current_state_text(App::State state);  // 获取当前状态的文本描述
	std::tuple<size_t, size_t, size_t> count_processor_states(
		const auto& resources
	) const;  // 统计处理器状态数量（运行中、空闲、错误）
	void add_exit_confirm_window();
};