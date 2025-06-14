#pragma once

// 系统头文件
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#endif

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
#include "frontend/font.hpp"
#include "frontend/help.hpp"
#include "frontend/imgui-context.hpp"
#include "frontend/popup.hpp"
#include "frontend/appSettingsWindows.hpp"
#include "frontend/sdl-context.hpp"
#include "frontend/settings.hpp"
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
    App() : imgui_context(sdl_context.get_window_ptr(), sdl_context.get_renderer_ptr())
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
        Editing,            // 正在编辑模式
        Preview_requested,  // 请求开始预览
        Previewing,         // 正在运行预览
        Preview_cancelling  // 请求停止预览
    };

    // 节点编辑器右键菜单状态
    enum class Node_editor_context_menu_state
    {
        Closed,             // 菜单关闭
        Open_requested,     // 请求打开菜单
        Opened              // 菜单已打开
    };

    // 保存原始样式值（用于缩放功能的基准参考）
    struct Original_style
    {
        float node_corner_rounding;         // 节点圆角半径
        ImVec2 node_padding;                // 节点内边距
        float node_border_thickness;        // 节点边框厚度
        float link_thickness;               // 连接线厚度
        float link_hover_distance;          // 连接线悬停检测距离
        float pin_circle_radius;            // 圆形引脚半径
        float pin_quad_side_length;         // 方形引脚边长
        float pin_triangle_side_length;     // 三角形引脚边长
        float pin_line_thickness;           // 引脚线条厚度
        float pin_hover_radius;             // 引脚悬停半径
        float grid_spacing;                 // 网格间距
        bool initialized = false;           // 是否已初始化
    };


    // =============================================================================
    // 核心状态和对象
    // =============================================================================
    
    // 应用程序状态（线程安全）
    std::atomic<State> state = State::Editing;
    std::atomic<Node_editor_context_menu_state> node_editor_context_menu_state = Node_editor_context_menu_state::Closed;

    // 退出控制
    bool should_exit_after_save = false;                         // 等待退出标志

    // UI核心对象
    SDL_context sdl_context;                           // SDL渲染上下文
    Imgui_context imgui_context;                       // ImGui上下文
    Popup_modal_manager popup_manager;                 // 弹窗管理器
    
    // 图形和运行器
    infra::Graph graph;                                // 节点图数据结构
    std::unique_ptr<infra::Runner> runner;             // 音频预览运行器
    
    // 撤销/重做系统
    std::list<infra::Graph> undo_stack;                // 撤销栈
    std::list<infra::Graph> redo_stack;                // 重做栈
    
    // 复制粘贴系统
    std::string copied_graph_json;                     // 复制的图数据（JSON格式）
    ImVec2 last_paste_position{100.0f, 100.0f};       // 上次粘贴位置

    infra::Id_t selected_node_id = -1;                // 当前选中的节点ID

    // =============================================================================
    // UI设置和状态
    // =============================================================================
    
    // 主菜单栏
    uint32_t main_menu_bar_height = 0;                 // 主菜单栏高度（像素）
    
    // 面板显示控制
    bool show_minimap = true;                          // 显示小地图
    bool show_toolbar = true;                          // 显示工具栏
    bool show_grid = true;                             // 显示网格
    bool show_diagnostics = false;                     // 显示诊断信息
    bool show_side_panel = false;                     // 显示侧边面板
    
    // 网格设置
    bool snap_to_grid = false;                         // 启用网格吸附
    float grid_spacing = 20.0f;                        // 网格间距
    bool show_grid_settings_dialog = false;            // 显示网格设置对话框
    bool show_grid_lines = true;                       // 显示网格线
    
    // 缩放设置
    float current_zoom = 1.0f;                         // 当前缩放级别
    float min_zoom = 0.1f;                             // 最小缩放级别
    float max_zoom = 5.0f;                             // 最大缩放级别
    float zoom_step = 0.1f;                            // 缩放步长
    bool show_zoom_dialog = false;                     // 显示缩放对话框
    Original_style original_style;                     // 原始样式值
    
    // 居中视图模式
    bool center_view_mode = false;                     // 居中视图模式开关
    ImVec2 center_target_position = ImVec2(0, 0);     // 居中目标位置

    // =============================================================================
    // 弹窗管理
    // =============================================================================
    
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
    
    // 确认对话框
    void add_new_project_confirm_window(               // 新建项目确认对话框
        std::function<void()> on_save,                 // 保存回调
        std::function<void()> on_discard,              // 丢弃回调
        std::function<void()> on_cancel                // 取消回调
    );
    
    void show_overwrite_confirm_dialog(std::string& filepath, std::string& json_content);  // 覆盖文件确认对话框
    void show_open_confirm_dialog(const std::string& filepath, const std::string& json_content);  // 打开文件确认对话框
    
    // 设置对话框
    void show_app_settings();                         // 显示app设置对话框
    void show_save_dialog();                          // 显示保存对话框
    void show_open_dialog();                          // 显示打开文件对话框
    void draw_grid_settings_dialog();                 // 绘制网格设置对话框
    void draw_zoom_dialog();                          // 绘制缩放设置对话框

    // =============================================================================
    // 项目管理
    // =============================================================================
    
    void new_create_project();                        // 创建新项目
    void clear_current_project();                     // 清空当前项目
    void save_project_with_path(std::string& directory, std::string& project_name);  // 保存项目到指定路径
    void load_project_from_file(const std::string& json_content, const std::string& filepath);  // 从文件加载项目
    
    // 序列化
    std::string save_graph_as_string() const;         // 将图序列化为JSON字符串
    void load_graph_from_string(const std::string& json_string);  // 从JSON字符串反序列化图

    // =============================================================================
    // 撤销/重做系统
    // =============================================================================
    
    void save_undo_state();                           // 保存当前状态到撤销栈
    void save_redo_state();                           // 保存当前状态到重做栈
    void undo();                                      // 执行撤销操作
    void redo();                                      // 执行重做操作
    void restore_node_positions();                    // 恢复节点位置
    void compress_undo_stack();                       // 压缩撤销栈以节省内存

    // =============================================================================
    // 复制粘贴系统
    // =============================================================================
    
    void copy_selected_nodes();                       // 复制选中的节点和连接
    void paste_nodes();                               // 粘贴节点和连接

    // =============================================================================
    // UI绘制 - 主要组件
    // =============================================================================
    
    // 主绘制函数
    void draw();                                      // 主绘制函数，绘制整个界面
    
    // 面板绘制
    void draw_menubar();                              // 绘制主菜单栏
    void draw_side_panel();                           // 绘制侧边面板
    void draw_main_panel();                           // 绘制主面板（节点编辑器）
    void draw_toolbar();                              // 绘制工具栏
    void draw_performance_overlay();                  // 绘制性能信息覆盖层

    // =============================================================================
    // UI绘制 - 菜单栏
    // =============================================================================
    
    // 菜单栏子菜单
    void draw_menubar_file();                         // 绘制文件菜单
    void draw_menubar_edit();                         // 绘制编辑菜单
    void draw_menubar_view();                         // 绘制视图菜单
    void draw_menubar_help();                         // 绘制帮助菜单
    
    // 视图菜单子项
    void draw_view_panels_menu();                     // 绘制面板显示控制菜单
    void draw_view_center_menu();                      // 绘制居中视图菜单
    void draw_view_zoom_menu();                       // 绘制缩放控制菜单
    void draw_view_theme_menu();                      // 绘制主题选择菜单
    void draw_view_status_menu();                     // 绘制状态信息菜单
    void draw_view_windows_menu();                    // 绘制窗口管理菜单
    void draw_view_zoom_presets();                    // 绘制缩放预设选项
    void draw_view_status_info();                     // 绘制基本状态信息
    void draw_view_preview_status();                  // 绘制预览状态信息

    // =============================================================================
    // 节点编辑器
    // =============================================================================
    
    // 节点编辑器绘制
    void draw_node_editor();                          // 绘制节点编辑器主界面
    void draw_node(infra::Id_t id, const infra::Graph::Node& node);  // 绘制单个节点
    void draw_node_editor_context_menu();             // 绘制节点编辑器右键菜单
    void draw_add_node_menu();                        // 绘制添加节点菜单
    
    // 节点编辑器操作
    void handle_node_actions();                       // 处理节点编辑器的键盘和鼠标操作
    void remove_selected_nodes();                     // 删除选中的节点和连接

    // =============================================================================
    // 视图和缩放功能
    // =============================================================================
    
    // 居中视图功能
    void center_view_at_cursor();                     // 将视图居中到当前鼠标光标位置
    void enter_center_view_mode();                    // 进入居中视图模式
    void exit_center_view_mode();                     // 退出居中视图模式
    void handle_center_view_mode();                   // 处理居中视图模式的逻辑更新
    
    // 缩放功能
    void zoom_in();                                   // 放大视图（按步长增加缩放级别）
    void zoom_out();                                  // 缩小视图（按步长减少缩放级别）
    void reset_zoom();                                // 重置缩放级别到100%（1.0倍）
    void set_zoom_level(float level);                 // 设置特定的缩放级别
    void apply_zoom();                                // 应用当前缩放级别到节点编辑器样式
    void apply_zoom_with_center(float new_zoom, ImVec2 zoom_center);  // 以指定点为中心应用缩放
    void initialize_original_style();                // 初始化并保存原始样式值作为缩放基准
    
    // 适配功能
    void fit_to_window();                             // 调整缩放以适应窗口显示所有节点
    void fit_selection();                             // 调整缩放以适应选中的节点

    // =============================================================================
    // 状态管理和预览
    // =============================================================================
    
    // 状态轮询和预览
    void poll_state();                                // 轮询应用程序状态，处理状态转换
    void create_preview_runner();                     // 创建音频预览运行器
    void show_preview_runner_error(const std::any& error);  // 显示预览运行器错误信息
    
    // 辅助函数
    std::string get_current_state_text() const;       // 获取当前状态的文本描述
    std::tuple<size_t, size_t, size_t> count_processor_states(const auto& resources) const;  // 统计处理器状态数量（运行中、空闲、错误）
	std::string last_saved_graph_json; // 最后保存的图JSON字符串
    bool is_project_modified() const; // 检查项目是否已修改
    void add_exit_confirm_window();
    void add_save_success_popup_with_exit(const std::string& detail);

    // =============================================================================
    //settings和配置
    // =============================================================================
    void on_settings_applied(); // 设置应用回调
};