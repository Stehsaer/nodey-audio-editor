#include "frontend/settings.hpp"
#include "config.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <json/json.h>
#include <SDL.h>
#include<imgui.h>

namespace app_settings {

// 全局设置实例
static std::unique_ptr<AppSettings> g_app_settings = nullptr;

AppSettings& get_app_settings() {
    if (!g_app_settings) {
        g_app_settings = std::make_unique<AppSettings>();
        g_app_settings->load_from_file();
    }
    return *g_app_settings;
}

// AudioSettings 实现
void AudioSettings::apply() {
    // 更新 runtime_config 中的全局音频参数
    runtime_config::audio_sample_rate = sample_rate;
    runtime_config::audio_buffer_size = buffer_size;
    runtime_config::audio_channels = channels;
    runtime_config::master_volume = master_volume;
    runtime_config::audio_device_id = audio_device_id;
    
    // 注意：实际的音频系统重新初始化需要在这里调用
    // 例如：audio_system::reinitialize();
}

// UISettings 实现
void UISettings::apply() {
    runtime_config::ui_scale = ui_scale;
    runtime_config::show_toolbar = show_toolbar;
    runtime_config::show_side_panel = show_side_panel;
    runtime_config::show_minimap = show_minimap;
    runtime_config::show_grid = show_grid;
    runtime_config::grid_size = grid_size;
    runtime_config::snap_to_grid = snap_to_grid;
    runtime_config::side_panel_width = side_panel_width;
    
    // 应用主题设置（需要实现主题应用逻辑）
    apply_themes(theme);
}

// EditorSettings 实现
void EditorSettings::apply() {
    // 编辑器设置通常不需要立即应用到全局变量
    // 它们在使用时直接从设置对象读取
    // 但可以设置一些全局标志
    
    // 例如：可以设置自动保存计时器
    // if (auto_save) {
    //     start_auto_save_timer(auto_save_interval);
    // } else {
    //     stop_auto_save_timer();
    // }
}

// PerformanceSettings 实现
void PerformanceSettings::apply() {
    runtime_config::max_audio_threads = max_audio_threads;
    runtime_config::render_cache_size = render_cache_size;
    runtime_config::enable_gpu_acceleration = enable_gpu_acceleration;
    runtime_config::enable_multithreading = enable_multithreading;
    runtime_config::fps_limit = fps_limit;
    runtime_config::vsync = vsync;
    
    // 这里可以重新配置线程池、渲染缓存等
    // thread_pool::resize(max_audio_threads);
    // render_cache::resize(render_cache_size * 1024 * 1024); // MB to bytes
}

// DebugSettings 实现
void DebugSettings::apply() {
    runtime_config::show_performance_overlay = show_performance_overlay;
    runtime_config::enable_debug_logging = enable_debug_logging;
    runtime_config::show_memory_usage = show_memory_usage;
    runtime_config::enable_profiling = enable_profiling;
    
    // 配置日志系统
    // logger::set_level(log_level);
    // logger::enable_debug_logging(enable_debug_logging);
}

// RenderSettings 实现
void RenderSettings::apply() {
    // 渲染设置通常在渲染时使用，不需要立即应用到全局变量
    // 但可以验证输出目录是否存在
    if (!std::filesystem::exists(default_output_directory)) {
        try {
            std::filesystem::create_directories(default_output_directory);
        } catch (const std::exception& e) {
            // 处理目录创建失败
            std::cerr << "无法创建输出目录: " << e.what() << std::endl;
        }
    }
}

// WindowSettings 实现
void WindowSettings::apply() {
    // 窗口设置通常在窗口创建或更新时应用
    // 这里可以标记需要更新窗口状态
    // window_manager::request_resize(window_width, window_height);
    // window_manager::request_move(window_pos_x, window_pos_y);
    // if (maximized) window_manager::maximize();
    // if (fullscreen) window_manager::set_fullscreen(true);
}

// AppSettings 主要实现
std::string AppSettings::get_default_settings_path() {
    std::filesystem::path home_dir;
    
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        home_dir = std::filesystem::path(appdata) / "NodeyAudioEditor";
    } else {
        home_dir = std::filesystem::path(std::getenv("USERPROFILE")) / "AppData" / "Roaming" / "NodeyAudioEditor";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        home_dir = std::filesystem::path(home) / ".config" / "nodey-audio-editor";
    } else {
        home_dir = std::filesystem::current_path() / "config";
    }
#endif
    
    return (home_dir / "settings.json").string();
}

void AppSettings::load_from_file(const std::string& path) {
    std::string settings_path = path.empty() ? get_default_settings_path() : path;
    
    if (!std::filesystem::exists(settings_path)) {
        // 设置文件不存在，使用默认设置并保存
        reset_to_defaults();
        save_to_file(settings_path);
        return;
    }
    
    try {
        std::ifstream file(settings_path);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开设置文件");
        }
        
        Json::Value root;
        Json::Reader reader;
        
        if (!reader.parse(file, root)) {
            throw std::runtime_error("设置文件格式错误");
        }
        
        // 解析音频设置
        if (root.isMember("audio")) {
            const auto& audio_json = root["audio"];
            audio.sample_rate = audio_json.get("sample_rate", 48000).asInt();
            audio.buffer_size = audio_json.get("buffer_size", 2048).asInt();
            audio.bit_depth = audio_json.get("bit_depth", 32).asInt();
            audio.channels = audio_json.get("channels", 2).asInt();
            audio.audio_driver = audio_json.get("audio_driver", "auto").asString();
            audio.master_volume = audio_json.get("master_volume", 1.0f).asFloat();
            audio.audio_device_id = audio_json.get("audio_device_id", 0).asInt();
        }
        
        // 解析UI设置
        if (root.isMember("ui")) {
            const auto& ui_json = root["ui"];
            ui.ui_scale = ui_json.get("ui_scale", 1.0f).asFloat();
            ui.theme = ui_json.get("theme", "dark").asString();
            ui.show_toolbar = ui_json.get("show_toolbar", true).asBool();
            ui.show_side_panel = ui_json.get("show_side_panel", true).asBool();
            ui.show_minimap = ui_json.get("show_minimap", true).asBool();
            ui.show_grid = ui_json.get("show_grid", true).asBool();
            ui.grid_size = ui_json.get("grid_size", 20.0f).asFloat();
            ui.snap_to_grid = ui_json.get("snap_to_grid", false).asBool();
            ui.side_panel_width = ui_json.get("side_panel_width", 300).asInt();
            ui.node_corner_rounding = ui_json.get("node_corner_rounding", 5.0f).asFloat();
        }
        
        // 解析编辑器设置
        if (root.isMember("editor")) {
            const auto& editor_json = root["editor"];
            editor.auto_save = editor_json.get("auto_save", true).asBool();
            editor.auto_save_interval = editor_json.get("auto_save_interval", 300).asInt();
            editor.max_undo_levels = editor_json.get("max_undo_levels", 30).asInt();
            editor.enable_animations = editor_json.get("enable_animations", true).asBool();
            editor.animation_speed = editor_json.get("animation_speed", 1.0f).asFloat();
            editor.confirm_on_exit = editor_json.get("confirm_on_exit", true).asBool();
            editor.restore_window_state = editor_json.get("restore_window_state", true).asBool();
        }
        
        // 解析性能设置
        if (root.isMember("performance")) {
            const auto& perf_json = root["performance"];
            performance.max_audio_threads = perf_json.get("max_audio_threads", 4).asInt();
            performance.render_cache_size = perf_json.get("render_cache_size", 64).asInt();
            performance.enable_gpu_acceleration = perf_json.get("enable_gpu_acceleration", false).asBool();
            performance.enable_multithreading = perf_json.get("enable_multithreading", true).asBool();
            performance.fps_limit = perf_json.get("fps_limit", 60).asInt();
            performance.vsync = perf_json.get("vsync", true).asBool();
        }
        
        // 解析调试设置
        if (root.isMember("debug")) {
            const auto& debug_json = root["debug"];
            debug.show_performance_overlay = debug_json.get("show_performance_overlay", false).asBool();
            debug.enable_debug_logging = debug_json.get("enable_debug_logging", false).asBool();
            debug.show_memory_usage = debug_json.get("show_memory_usage", false).asBool();
            debug.enable_profiling = debug_json.get("enable_profiling", false).asBool();
            debug.log_level = debug_json.get("log_level", "info").asString();
        }
        
        // 解析渲染设置
        if (root.isMember("render")) {
            const auto& render_json = root["render"];
            render.default_output_format = render_json.get("default_output_format", "wav").asString();
            render.default_output_directory = render_json.get("default_output_directory", "./output/").asString();
            render.normalize_output = render_json.get("normalize_output", true).asBool();
            render.default_output_gain = render_json.get("default_output_gain", 1.0f).asFloat();
            render.render_quality = render_json.get("render_quality", 1).asInt();
            render.enable_dithering = render_json.get("enable_dithering", true).asBool();
        }
        
        // 解析窗口设置
        if (root.isMember("window")) {
            const auto& window_json = root["window"];
            window.window_width = window_json.get("window_width", 1200).asInt();
            window.window_height = window_json.get("window_height", 800).asInt();
            window.window_pos_x = window_json.get("window_pos_x", -1).asInt();
            window.window_pos_y = window_json.get("window_pos_y", -1).asInt();
            window.maximized = window_json.get("maximized", false).asBool();
            window.fullscreen = window_json.get("fullscreen", false).asBool();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "加载设置文件失败: " << e.what() << std::endl;
        reset_to_defaults();
    }
}

void AppSettings::save_to_file(const std::string& path) const {
    std::string settings_path = path.empty() ? get_default_settings_path() : path;
    
    try {
        // 确保目录存在
        std::filesystem::path parent_dir = std::filesystem::path(settings_path).parent_path();
        if (!parent_dir.empty()) {
            std::filesystem::create_directories(parent_dir);
        }
        
        Json::Value root;
        
        // 音频设置
        Json::Value audio_json;
        audio_json["sample_rate"] = audio.sample_rate;
        audio_json["buffer_size"] = audio.buffer_size;
        audio_json["bit_depth"] = audio.bit_depth;
        audio_json["channels"] = audio.channels;
        audio_json["audio_driver"] = audio.audio_driver;
        audio_json["master_volume"] = audio.master_volume;
        audio_json["audio_device_id"] = audio.audio_device_id;
        root["audio"] = audio_json;
        
        // UI设置
        Json::Value ui_json;
        ui_json["ui_scale"] = ui.ui_scale;
        ui_json["theme"] = ui.theme;
        ui_json["show_toolbar"] = ui.show_toolbar;
        ui_json["show_side_panel"] = ui.show_side_panel;
        ui_json["show_minimap"] = ui.show_minimap;
        ui_json["show_grid"] = ui.show_grid;
        ui_json["grid_size"] = ui.grid_size;
        ui_json["snap_to_grid"] = ui.snap_to_grid;
        ui_json["side_panel_width"] = ui.side_panel_width;
        ui_json["node_corner_rounding"] = ui.node_corner_rounding;
        root["ui"] = ui_json;
        
        // 编辑器设置
        Json::Value editor_json;
        editor_json["auto_save"] = editor.auto_save;
        editor_json["auto_save_interval"] = editor.auto_save_interval;
        editor_json["max_undo_levels"] = editor.max_undo_levels;
        editor_json["enable_animations"] = editor.enable_animations;
        editor_json["animation_speed"] = editor.animation_speed;
        editor_json["confirm_on_exit"] = editor.confirm_on_exit;
        editor_json["restore_window_state"] = editor.restore_window_state;
        root["editor"] = editor_json;
        
        // 性能设置
        Json::Value perf_json;
        perf_json["max_audio_threads"] = performance.max_audio_threads;
        perf_json["render_cache_size"] = performance.render_cache_size;
        perf_json["enable_gpu_acceleration"] = performance.enable_gpu_acceleration;
        perf_json["enable_multithreading"] = performance.enable_multithreading;
        perf_json["fps_limit"] = performance.fps_limit;
        perf_json["vsync"] = performance.vsync;
        root["performance"] = perf_json;
        
        // 调试设置
        Json::Value debug_json;
        debug_json["show_performance_overlay"] = debug.show_performance_overlay;
        debug_json["enable_debug_logging"] = debug.enable_debug_logging;
        debug_json["show_memory_usage"] = debug.show_memory_usage;
        debug_json["enable_profiling"] = debug.enable_profiling;
        debug_json["log_level"] = debug.log_level;
        root["debug"] = debug_json;
        
        // 渲染设置
        Json::Value render_json;
        render_json["default_output_format"] = render.default_output_format;
        render_json["default_output_directory"] = render.default_output_directory;
        render_json["normalize_output"] = render.normalize_output;
        render_json["default_output_gain"] = render.default_output_gain;
        render_json["render_quality"] = render.render_quality;
        render_json["enable_dithering"] = render.enable_dithering;
        root["render"] = render_json;
        
        // 窗口设置
        Json::Value window_json;
        window_json["window_width"] = window.window_width;
        window_json["window_height"] = window.window_height;
        window_json["window_pos_x"] = window.window_pos_x;
        window_json["window_pos_y"] = window.window_pos_y;
        window_json["maximized"] = window.maximized;
        window_json["fullscreen"] = window.fullscreen;
        root["window"] = window_json;
        
        // 写入文件
        std::ofstream file(settings_path);
        if (!file.is_open()) {
            throw std::runtime_error("无法创建设置文件");
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &file);
        
    } catch (const std::exception& e) {
        std::cerr << "保存设置文件失败: " << e.what() << std::endl;
    }
}

void AppSettings::reset_to_defaults() {
    audio = AudioSettings();
    ui = UISettings();
    editor = EditorSettings();
    performance = PerformanceSettings();
    debug = DebugSettings();
    render = RenderSettings();
    window = WindowSettings();
}

void AppSettings::apply_all_settings() {
    audio.apply();
    ui.apply();
    editor.apply();
    performance.apply();
    debug.apply();
    render.apply();
    window.apply();
    
    notify_change();
}

void AppSettings::set_change_callback(std::function<void()> callback) {
    change_callback_ = std::move(callback);
}

void AppSettings::notify_change() {
    if (change_callback_) {
        change_callback_();
    }
}

void apply_themes(const std::string& theme_name) {
    // 这里可以实现主题应用逻辑
    // 例如：根据 theme_name 加载对应的 ImGui 主题
    if (theme_name == "dark") {
        // 应用暗色主题
        ImGui::StyleColorsDark();
    } else if (theme_name == "light") {
        // 应用亮色主题
        ImGui::StyleColorsLight();
    } else {
        // 默认主题
        ImGui::StyleColorsClassic();
    }

}

} // namespace app_settings