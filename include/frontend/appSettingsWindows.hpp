#pragma once

#include "frontend/settings.hpp"
#include "frontend/popup.hpp"
#include <functional>
#include <imgui.h>

namespace frontend {

// =============================================================================
// 应用程序全局设置窗口
// =============================================================================
class AppSettingsWindow {
public:
    // 显示应用设置窗口
    static void show(Popup_modal_manager& popup_manager,
                    std::function<void()> on_apply = nullptr);

private:
    app_settings::AppSettings& settings_;
    bool settings_changed_;
    std::function<void()> on_apply_callback_;

public:
    AppSettingsWindow(std::function<void()> on_apply = nullptr);
    
    // 主渲染函数
    bool operator()(bool close_button_pressed);

private:
    // 绘制各个设置标签页
    bool draw_audio_settings_tab();
    bool draw_ui_settings_tab();
    bool draw_editor_settings_tab();
    bool draw_performance_settings_tab();
    bool draw_debug_settings_tab();
    bool draw_render_settings_tab();
    bool draw_window_settings_tab();
    
    // 绘制底部按钮
    bool draw_settings_buttons(bool close_button_pressed);
    
    // 辅助函数
    int find_combo_index(const char* items[], int count, int target_value);
    int find_combo_index_string(const char* items[], int count, const std::string& target);
    
    void apply_settings();
};

// =============================================================================
// 项目特定设置窗口
// =============================================================================
class ProjectSettingsWindow {
public:
    // 显示项目设置窗口
    static void show(Popup_modal_manager& popup_manager,
                    std::function<void()> on_apply = nullptr);

private:
    // 项目信息的静态变量（用于ImGui输入框）
    static inline bool static_vars_initialized_ = false;
    static inline char project_name_[256] = "";
    static inline char project_author_[256] = "";
    static inline char project_description_[1024] = "";
    static inline char project_version_[64] = "1.0.0";
    static inline char output_directory_[512] = "./output/";
    static inline char project_notes_[2048] = "";
    
    bool settings_changed_;
    std::function<void()> on_apply_callback_;

public:
    ProjectSettingsWindow(std::function<void()> on_apply = nullptr);
    
    // 主渲染函数
    bool operator()(bool close_button_pressed);

private:
    // 初始化静态变量（从当前项目状态）
    static void initialize_static_vars();
    
    // 绘制各个标签页
    bool draw_project_info_tab();
    bool draw_project_audio_tab();
    bool draw_project_render_tab();
    bool draw_project_metadata_tab();
    
    // 绘制底部按钮
    bool draw_settings_buttons(bool close_button_pressed);
    
    // 辅助函数
    bool draw_sample_rate_combo(int& sample_rate);
    bool draw_buffer_size_combo(int& buffer_size);
    bool draw_bit_depth_combo(int& bit_depth);
    bool draw_output_format_combo(std::string& format);
    bool draw_output_directory_input(char* buffer, size_t buffer_size);
    
    int find_combo_index(const char* items[], int count, int target_value);
    int find_combo_index_string(const char* items[], int count, const std::string& target);
    
    void apply_project_settings();
    void save_project_settings();
};

// =============================================================================
// 快速设置工具栏（可选）
// =============================================================================
class QuickSettingsToolbar {
public:
    static void draw();
    
private:
    static void draw_audio_quick_settings();
    static void draw_ui_quick_settings();
    static void draw_debug_quick_settings();
};

// =============================================================================
// 设置预设管理器（高级功能）
// =============================================================================
class SettingsPresetManager {
public:
    struct Preset {
        std::string name;
        std::string description;
        app_settings::AppSettings settings;
    };
    
    static void show_preset_manager(Popup_modal_manager& popup_manager);
    
    static void save_current_as_preset(const std::string& name, const std::string& description);
    static void load_preset(const std::string& name);
    static std::vector<Preset> get_available_presets();
    static void delete_preset(const std::string& name);
    
private:
    static std::string get_presets_directory();
};

} // namespace frontend