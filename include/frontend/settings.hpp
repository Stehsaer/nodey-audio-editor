#pragma once

#include <string>
#include <functional>
#include <memory>

namespace app_settings {

// 音频设置
struct AudioSettings {
    int sample_rate = 48000;           
    int buffer_size = 2048;            
    int bit_depth = 32;                
    int channels = 2;                  
    std::string audio_driver = "auto"; 
    float master_volume = 1.0f;        
    int audio_device_id = 0;           // 音频设备ID
    
    void apply();  // 应用音频设置到全局
};

// UI/界面设置
struct UISettings {
    float ui_scale = 1.0f;             
    std::string theme = "dark";        
    bool show_toolbar = true;          
    bool show_side_panel = true;       
    bool show_minimap = true;          
    bool show_grid = true;             
    float grid_size = 20.0f;           
    bool snap_to_grid = false;         
    int side_panel_width = 300;        
    float node_corner_rounding = 5.0f; 
    
    void apply();  // 应用UI设置到全局
};

// 编辑器设置
struct EditorSettings {
    bool auto_save = true;
    int auto_save_interval = 300;      
    int max_undo_levels = 30;          
    bool enable_animations = true;     
    float animation_speed = 1.0f;      
    bool confirm_on_exit = true;       
    bool restore_window_state = true;  
    
    void apply();  // 应用编辑器设置
};

// 性能设置
struct PerformanceSettings {
    int max_audio_threads = 4;         
    int render_cache_size = 64;        // MB
    bool enable_gpu_acceleration = false;
    bool enable_multithreading = true;
    int fps_limit = 60;                
    bool vsync = true;                 
    
    void apply();  // 应用性能设置
};

// 调试设置
struct DebugSettings {
    bool show_performance_overlay = false;
    bool enable_debug_logging = false;
    bool show_memory_usage = false;
    bool enable_profiling = false;
    std::string log_level = "info";    // debug, info, warning, error
    
    void apply();  // 应用调试设置
};

// 渲染设置
struct RenderSettings {
    std::string default_output_format = "wav";
    std::string default_output_directory = "./output/";
    bool normalize_output = true;      
    float default_output_gain = 1.0f;  
    int render_quality = 1;            // 0=draft, 1=normal, 2=high
    bool enable_dithering = true;      
    
    void apply();  // 应用渲染设置
};

// 窗口状态设置（运行时）
struct WindowSettings {
    int window_width = 1200;
    int window_height = 800;
    int window_pos_x = -1;             // -1 表示居中
    int window_pos_y = -1;
    bool maximized = false;
    bool fullscreen = false;
    
    void apply();  // 应用窗口设置
};

// 主设置类
class AppSettings {
public:
    AudioSettings audio;
    UISettings ui;
    EditorSettings editor;
    PerformanceSettings performance;
    DebugSettings debug;
    RenderSettings render;
    WindowSettings window;
    
    // 设置文件管理
    void load_from_file(const std::string& path = "");
    void save_to_file(const std::string& path = "") const;
    void reset_to_defaults();
    
    // 应用所有设置到全局状态
    void apply_all_settings();
    
    // 获取默认设置文件路径
    static std::string get_default_settings_path();
    
    // 设置变更回调
    void set_change_callback(std::function<void()> callback);
    
private:
    std::function<void()> change_callback_;
    void notify_change();
    
    // 内部辅助函数
    std::string get_settings_directory() const;
    void ensure_settings_directory_exists() const;
};

// 全局设置实例访问器
AppSettings& get_app_settings();

void apply_themes(const std::string& theme_name);

} // namespace app_settings