#include "frontend/appSettingsWindows.hpp"
#include "frontend/settings.hpp"

namespace frontend {

void AppSettingsWindow::show(Popup_modal_manager& popup_manager, 
                            std::function<void()> on_apply) {
    popup_manager.open_window({
        .title = "Application Settings",
        .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize,
        .has_close_button = true,
        .keep_centered = true,
        .render_func = AppSettingsWindow(std::move(on_apply))
    });
}

AppSettingsWindow::AppSettingsWindow(std::function<void()> on_apply)
    : settings_(app_settings::get_app_settings()), 
      settings_changed_(false), 
      on_apply_callback_(std::move(on_apply)) {
}

bool AppSettingsWindow::operator()(bool close_button_pressed) {
    bool should_close = false;
    
    if (ImGui::BeginTabBar("AppSettingsTabs")) {
        if (ImGui::BeginTabItem("Audio")) {
            settings_changed_ |= draw_audio_settings_tab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Interface")) {
            settings_changed_ |= draw_ui_settings_tab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Editor")) {
            settings_changed_ |= draw_editor_settings_tab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Performance")) {
            settings_changed_ |= draw_performance_settings_tab();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Debug")) {
            settings_changed_ |= draw_debug_settings_tab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::Separator();
    should_close = draw_settings_buttons(close_button_pressed);
    
    return should_close;
}

bool AppSettingsWindow::draw_audio_settings_tab() {
    ImGui::Text("Audio Configuration");
    ImGui::Separator();
    
    bool changed = false;
    
    // 采样率
    const char* sample_rates[] = {"22050", "44100", "48000", "88200", "96000", "192000"};
    int current_sr_idx = find_combo_index(sample_rates, 6, settings_.audio.sample_rate);
    if (ImGui::Combo("Sample Rate", &current_sr_idx, sample_rates, 6)) {
        settings_.audio.sample_rate = std::stoi(sample_rates[current_sr_idx]);
        changed = true;
    }
    
    // 缓冲区大小
    const char* buffer_sizes[] = {"64", "128", "256", "512", "1024", "2048", "4096"};
    int current_buf_idx = find_combo_index(buffer_sizes, 7, settings_.audio.buffer_size);
    if (ImGui::Combo("Buffer Size", &current_buf_idx, buffer_sizes, 7)) {
        settings_.audio.buffer_size = std::stoi(buffer_sizes[current_buf_idx]);
        changed = true;
    }
    
    // 位深度
    const char* bit_depths[] = {"16", "24", "32"};
    int current_bd_idx = find_combo_index(bit_depths, 3, settings_.audio.bit_depth);
    if (ImGui::Combo("Bit Depth", &current_bd_idx, bit_depths, 3)) {
        settings_.audio.bit_depth = std::stoi(bit_depths[current_bd_idx]);
        changed = true;
    }
    
    // 声道数
    if (ImGui::SliderInt("Channels", &settings_.audio.channels, 1, 8)) {
        changed = true;
    }
    
    // 主音量
    if (ImGui::SliderFloat("Master Volume", &settings_.audio.master_volume, 0.0f, 2.0f)) {
        changed = true;
    }
    
    return changed;
}

bool AppSettingsWindow::draw_ui_settings_tab() {
    ImGui::Text("User Interface");
    ImGui::Separator();
    
    bool changed = false;
    
    // UI缩放
    if (ImGui::SliderFloat("UI Scale", &settings_.ui.ui_scale, 0.5f, 3.0f, "%.1f")) {
        changed = true;
    }
    
    // 主题选择
    const char* themes[] = {"dark", "light", "classic"};
    int current_theme_idx = find_combo_index_string(themes, 3, settings_.ui.theme);
    if (ImGui::Combo("Theme", &current_theme_idx, themes, 3)) {
        settings_.ui.theme = themes[current_theme_idx];
        changed = true;
    }
    
    // 界面元素显示
    if (ImGui::Checkbox("Show Toolbar", &settings_.ui.show_toolbar)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Show Side Panel", &settings_.ui.show_side_panel)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Show Minimap", &settings_.ui.show_minimap)) {
        changed = true;
    }
    
    // 网格设置
    ImGui::Separator();
    ImGui::Text("Grid Settings");
    
    if (ImGui::Checkbox("Show Grid", &settings_.ui.show_grid)) {
        changed = true;
    }
    
    if (ImGui::SliderFloat("Grid Size", &settings_.ui.grid_size, 5.0f, 100.0f)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Snap to Grid", &settings_.ui.snap_to_grid)) {
        changed = true;
    }
    
    // 侧边栏宽度
    if (ImGui::SliderInt("Side Panel Width", &settings_.ui.side_panel_width, 200, 500)) {
        changed = true;
    }
    
    return changed;
}

bool AppSettingsWindow::draw_editor_settings_tab() {
    ImGui::Text("Editor Preferences");
    ImGui::Separator();
    
    bool changed = false;
    
    if (ImGui::Checkbox("Auto Save", &settings_.editor.auto_save)) {
        changed = true;
    }
    
    if (settings_.editor.auto_save) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("Interval (sec)", &settings_.editor.auto_save_interval)) {
            settings_.editor.auto_save_interval = std::max(10, settings_.editor.auto_save_interval);
            changed = true;
        }
    }
    
    if (ImGui::SliderInt("Max Undo Levels", &settings_.editor.max_undo_levels, 10, 100)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Enable Animations", &settings_.editor.enable_animations)) {
        changed = true;
    }
    
    if (settings_.editor.enable_animations) {
        if (ImGui::SliderFloat("Animation Speed", &settings_.editor.animation_speed, 0.1f, 3.0f)) {
            changed = true;
        }
    }
    
    if (ImGui::Checkbox("Confirm on Exit", &settings_.editor.confirm_on_exit)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Restore Window State", &settings_.editor.restore_window_state)) {
        changed = true;
    }
    
    return changed;
}

bool AppSettingsWindow::draw_performance_settings_tab() {
    ImGui::Text("Performance Settings");
    ImGui::Separator();
    
    bool changed = false;
    
    if (ImGui::SliderInt("Max Audio Threads", &settings_.performance.max_audio_threads, 1, 16)) {
        changed = true;
    }
    
    if (ImGui::SliderInt("Render Cache Size (MB)", &settings_.performance.render_cache_size, 16, 512)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Enable GPU Acceleration", &settings_.performance.enable_gpu_acceleration)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Enable Multithreading", &settings_.performance.enable_multithreading)) {
        changed = true;
    }
    
    if (ImGui::SliderInt("FPS Limit", &settings_.performance.fps_limit, 15, 240)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("V-Sync", &settings_.performance.vsync)) {
        changed = true;
    }
    
    return changed;
}

bool AppSettingsWindow::draw_debug_settings_tab() {
    ImGui::Text("Debug and Diagnostics");
    ImGui::Separator();
    
    bool changed = false;
    
    if (ImGui::Checkbox("Show Performance Overlay", &settings_.debug.show_performance_overlay)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Enable Debug Logging", &settings_.debug.enable_debug_logging)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Show Memory Usage", &settings_.debug.show_memory_usage)) {
        changed = true;
    }
    
    if (ImGui::Checkbox("Enable Profiling", &settings_.debug.enable_profiling)) {
        changed = true;
    }
    
    // 日志级别
    const char* log_levels[] = {"debug", "info", "warning", "error"};
    int current_log_level_idx = find_combo_index_string(log_levels, 4, settings_.debug.log_level);
    if (ImGui::Combo("Log Level", &current_log_level_idx, log_levels, 4)) {
        settings_.debug.log_level = log_levels[current_log_level_idx];
        changed = true;
    }
    
    return changed;
}

bool AppSettingsWindow::draw_settings_buttons(bool close_button_pressed) {
    bool should_close = false;
    
    if (ImGui::Button("Apply")) {
        apply_settings();
        settings_changed_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("OK")) {
        apply_settings();
        should_close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || close_button_pressed) {
        should_close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults")) {
        settings_.reset_to_defaults();
        settings_changed_ = true;
    }
    
    return should_close;
}

void AppSettingsWindow::apply_settings() {
    settings_.apply_all_settings();
    settings_.save_to_file();
    
    if (on_apply_callback_) {
        on_apply_callback_();
    }
}

int AppSettingsWindow::find_combo_index(const char* items[], int count, int target_value) {
    for (int i = 0; i < count; i++) {
        if (std::stoi(items[i]) == target_value) {
            return i;
        }
    }
    return 0;
}

int AppSettingsWindow::find_combo_index_string(const char* items[], int count, const std::string& target) {
    for (int i = 0; i < count; i++) {
        if (target == items[i]) {
            return i;
        }
    }
    return 0;
}

} // namespace frontend