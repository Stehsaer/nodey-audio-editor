
#include "frontend/help.hpp"
#include "frontend/app.hpp"
#include "frontend/popup.hpp"
#include "config.hpp"
#include <string>
#include <sstream>
#include <imgui.h>

namespace {
    // 详细的帮助内容，用于窗口显示
    std::string get_detailed_help_content() {
        return R"([Basic Usage]

- New/Open/Save Project: Use the "File" menu to create, open, or save projects.
- Add Node: Right-click in the node editor area to add nodes.
- Connect Nodes: Drag from output port to input port to create connections.
- Delete Node/Link: Select and press Delete key.

[Audio Operations]

- Preview: Click the "Preview" button or use toolbar to listen to your audio graph.
- Stop: Use the stop button to halt audio playbook.

[Editing Features]

- Undo/Redo: Use Ctrl+Z and Ctrl+Y to undo or redo actions.
- Copy/Paste: Use Ctrl+C and Ctrl+V to copy and paste nodes.
- Zoom: Use Ctrl + Mouse Wheel or View menu to zoom in/out.
- Center View: Use Ctrl+0 to enter center view mode.

[View Controls]

- Fit to Window: Use Ctrl+F to fit all nodes in view.
- Fit Selection: Use Ctrl+Shift+F to fit selected nodes.
- Grid Settings: Configure grid spacing and snapping from View menu.
- Performance Monitor: Toggle with View → Status → Show Performance Metrics.

[Tips & Tricks]

- Save your project frequently to avoid data loss.
- Customize layout and theme from the "View" menu.
- Use right-click context menu for quick access to common operations.
- Hold Ctrl while scrolling to zoom in/out at cursor position.
)";
    }

    // 详细的关于信息，用于窗口显示
    std::string get_detailed_about_content() {
        return R"(Nodey Audio Editor

Version: 1.0.0
Release Date: 2025-06-10

Nodey Audio Editor is a powerful, user-friendly audio editing tool based on a node graph interface. It allows you to create, edit, and process audio in a flexible and visual way.

[Key Features]
- Open source and cross-platform
- Real-time audio preview
- Support for various audio effects
- Intuitive drag-and-drop workflow
- Professional node-based interface
)";
    }

    // 详细的更新信息，用于窗口显示
    std::string get_detailed_update_content() {
        return R"(Check for Updates

Current Version: 1.0.0

Latest Version 1.0.0

[New Features]
- Timeline editing support
- Enhanced node graph performance
- New audio effect library

[Improvements]
- Better audio effect processing
- Improved user interface responsiveness
- Enhanced project file compatibility

[Bug Fixes]
- Fixed various stability issues
- Resolved audio playback glitches
- Fixed project save/load issues

Status: You are running the latest version!
)";
    }

} // 匿名命名空间结束

namespace about {

        // 简单的信息弹窗函数
    void show_simple_info_popup(Popup_modal_manager& popup_manager, const std::string& title, const std::string& message, const std::string& url) {
        class Simple_info_window {
            std::string title;
            std::string message;
            std::string url;
            
        public:
            Simple_info_window(const std::string& t, const std::string& m, const std::string& u) 
                : title(t), message(m), url(u) {}
            
            bool operator()(bool close_button_pressed) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {15 * runtime_config::ui_scale, 15 * runtime_config::ui_scale});
                
                ImGui::TextWrapped("%s", message.c_str());
                
                if (!url.empty()) {
                    ImGui::Dummy({30, 15 * runtime_config::ui_scale});
                    ImGui::Separator();
                    ImGui::TextWrapped("URL: %s", url.c_str());
                }
                
                ImGui::Dummy({30, 15 * runtime_config::ui_scale});
                
                if (ImGui::Button("OK", {350 * runtime_config::ui_scale, 20 * runtime_config::ui_scale}) || close_button_pressed) {
                    ImGui::PopStyleVar();
                    return true;
                }
                
                ImGui::PopStyleVar();
                return false;
            }
        };
        
        popup_manager.open_window({
            .title = title,
            .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
            .has_close_button = true,
            .keep_centered = true,
            .render_func = Simple_info_window(title, message, url)
        });
    }
    std::string get_help_documentation() {
        return R"(# Nodey Audio Editor - Local Help

## Basic Usage

- **New/Open/Save Project:** Use the "File" menu to create, open, or save projects.
- **Add Node:** Right-click in the node editor area or use the node library panel to add nodes.
- **Connect Nodes:** Drag from one node's output port to another node's input port to create a connection.
- **Delete Node/Link:** Select a node or link and press the Delete key.
- **Preview:** Click the "Preview" button to listen to your audio graph.
- **Undo/Redo:** Use Ctrl+Z and Ctrl+Y to undo or redo actions.
- **Zoom:** Use Ctrl + Mouse Wheel or the "View" menu to zoom in/out.
- **Settings:** Access project and application settings from the menu.

## Tips

- Save your project frequently to avoid data loss.
- You can customize the layout and theme from the "View" menu.
- For more advanced usage, refer to the official documentation or community forums.

)";
    }

    std::string get_online_help_documentation() {
        return R"(https://nodey-audio-editor.com/help)";
    }

    std::string get_about_information() {
        return R"(Nodey Audio Editor

Version: 1.0.0
Author: Your Name
Release Date: 2025-06-10

Nodey Audio Editor is a powerful, user-friendly audio editing tool based on a node graph interface. 
It allows you to create, edit, and process audio in a flexible and visual way.

- Open source and cross-platform
- Real-time audio preview
- Support for various audio effects and plugins
- Intuitive drag-and-drop workflow

GitHub: https://github.com/your-repo/nodey-audio-editor
Official Website: https://nodey-audio-editor.com
)";
    }

    std::string get_about_information_online_link() {
        return R"(https://nodey-audio-editor.com/about)";
    }

    std::string get_about_update_information() {
        return R"(Nodey Audio Editor Version 1.0.0

- New: Timeline editing support
- Improved: Audio effect library
- Fixed: Various bugs and stability issues

For the full changelog and the latest updates, please visit the update page.
)";
    }

    std::string get_about_update_information_online_link() {
        return R"(https://nodey-audio-editor.com/updates)";
    }

    // 显示帮助文档窗口 - 使用 starts_with
    void show_help_documentation_window(App* app, Popup_modal_manager& popup_manager) {
        class Help_documentation_window {
            std::string help_content;
            Popup_modal_manager* popup_mgr;
            
        public:
            Help_documentation_window(Popup_modal_manager* mgr) : popup_mgr(mgr) {
                help_content = get_detailed_help_content();
            }
            
            bool operator()(bool close_button_pressed) {
                // 设置窗口样式
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {15 * runtime_config::ui_scale, 15 * runtime_config::ui_scale});
                
                // 标题
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 1.0f, 1.0f));
                ImGui::TextWrapped("Nodey Audio Editor - Help Documentation");
                ImGui::PopStyleColor();
                
                ImGui::Dummy({1, 10 * runtime_config::ui_scale});
                ImGui::Separator();
                ImGui::Dummy({1, 5 * runtime_config::ui_scale});
                
                // 滚动区域
                ImGui::BeginChild("HelpContent", 
                                 {600 * runtime_config::ui_scale, 450 * runtime_config::ui_scale}, 
                                 ImGuiChildFlags_Border);
                
                // 解析内容并应用样式 - 使用 starts_with
                std::istringstream iss(help_content);
                std::string line;
                
                while (std::getline(iss, line)) {
                    if (line.empty()) {
                        ImGui::Dummy({1, 5 * runtime_config::ui_scale});
                        continue;
                    }
                    
                    // 根据方括号标题应用不同颜色 - 使用 starts_with
                    if (line.starts_with("[Basic Usage]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                        ImGui::TextWrapped("%s", line.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    } else if (line.starts_with("[Audio Operations]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 1.0f, 0.2f, 1.0f));
                        ImGui::TextWrapped("%s", line.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    } else if (line.starts_with("[Editing Features]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.8f, 1.0f));
                        ImGui::TextWrapped("%s", line.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    } else if (line.starts_with("[View Controls]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 1.0f, 1.0f));
                        ImGui::TextWrapped("%s", line.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    } else if (line.starts_with("[Tips & Tricks]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::TextWrapped("%s", line.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Separator();
                    } else if (line.starts_with("-")) {
                        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 10 * runtime_config::ui_scale);
                        ImGui::BulletText("%s", line.substr(2).c_str()); // 去掉"- "前缀
                        ImGui::PopTextWrapPos();
                    } else {
                        ImGui::TextWrapped("%s", line.c_str());
                    }
                }
                
                ImGui::EndChild();
                
                // 底部按钮
                ImGui::Dummy({1, 10 * runtime_config::ui_scale});
                ImGui::Separator();
                
                if (ImGui::Button("Open Online Documentation", {200 * runtime_config::ui_scale, 30 * runtime_config::ui_scale})) {
                    show_simple_info_popup(*popup_mgr,
                        "Online Documentation",
                        "Opening online documentation in your default browser.",
                        get_online_help_documentation()
                    );
                }
                
                ImGui::SameLine();
                
                if (ImGui::Button("Close", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed) {
                    ImGui::PopStyleVar();
                    return true;
                }
                
                ImGui::PopStyleVar();
                return false;
            }
        };
        
        popup_manager.open_window({
            .title = "Help Documentation",
            .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove,
            .has_close_button = true,
            .keep_centered = true,
            .render_func = Help_documentation_window(&popup_manager)
        });
    }

    // 显示关于窗口 - 使用 starts_with
    void show_about_window(App* app, Popup_modal_manager& popup_manager) {
        class About_window {
            std::string about_content;
            Popup_modal_manager* popup_mgr;
            
        public:
            About_window(Popup_modal_manager* mgr) : popup_mgr(mgr) {
                about_content = get_detailed_about_content();
            }
            
            bool operator()(bool close_button_pressed) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20 * runtime_config::ui_scale, 20 * runtime_config::ui_scale});
                
                // 解析关于内容 - 使用 starts_with
                std::istringstream iss(about_content);
                std::string line;
                
                while (std::getline(iss, line)) {
                    if (line.empty()) {
                        ImGui::Dummy({1, 5 * runtime_config::ui_scale});
                        continue;
                    }
                    
                    if (line.starts_with("Nodey Audio Editor") && line.length() < 30) {
                        // 应用标题
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
                        ImGui::SetWindowFontScale(1.5f);
                        ImGui::Text("%s", line.c_str());
                        ImGui::SetWindowFontScale(1.0f);
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("Version:") || line.starts_with("Release Date:")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("[Key Features]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("-")) {
                        ImGui::Indent(20 * runtime_config::ui_scale);
                        ImGui::BulletText("%s", line.substr(2).c_str());
                        ImGui::Unindent(20 * runtime_config::ui_scale);
                    } else {
                        ImGui::TextWrapped("%s", line.c_str());
                    }
                }
                
                ImGui::Dummy({1, 20 * runtime_config::ui_scale});
                ImGui::Separator();
                ImGui::Dummy({1, 10 * runtime_config::ui_scale});
                
                // 链接按钮
                if (ImGui::Button("GitHub Repository", {180 * runtime_config::ui_scale, 35 * runtime_config::ui_scale})) {
                    show_simple_info_popup(*popup_mgr,
                        "GitHub Repository",
                        "Opening GitHub repository in your default browser.",
                        "https://github.com/your-repo/nodey-audio-editor"
                    );
                }
                
                ImGui::SameLine();
                
                if (ImGui::Button("Official Website", {180 * runtime_config::ui_scale, 35 * runtime_config::ui_scale})) {
                    show_simple_info_popup(*popup_mgr,
                        "Official Website",
                        "Opening official website in your default browser.",
                        get_about_information_online_link()
                    );
                }
                
                ImGui::Dummy({1, 15 * runtime_config::ui_scale});
                
                // 关闭按钮
                if (ImGui::Button("Close", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed) {
                    ImGui::PopStyleVar();
                    return true;
                }
                
                ImGui::PopStyleVar();
                return false;
            }
        };
        
        popup_manager.open_window({
            .title = "About Nodey Audio Editor",
            .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
            .has_close_button = true,
            .keep_centered = true,
            .render_func = About_window(&popup_manager)
        });
    }

    // 显示更新窗口 - 使用 starts_with
    void show_updates_window(App* app, Popup_modal_manager& popup_manager) {
        class Updates_window {
            std::string update_content;
            Popup_modal_manager* popup_mgr;
            
        public:
            Updates_window(Popup_modal_manager* mgr) : popup_mgr(mgr) {
                update_content = get_detailed_update_content();
            }
            
            bool operator()(bool close_button_pressed) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {15 * runtime_config::ui_scale, 15 * runtime_config::ui_scale});
                
                // 滚动区域
                ImGui::BeginChild("UpdateContent", 
                                 {400 * runtime_config::ui_scale, 300 * runtime_config::ui_scale}, 
                                 ImGuiChildFlags_Border);
                
                // 解析更新内容 - 使用 starts_with
                std::istringstream iss(update_content);
                std::string line;
                
                while (std::getline(iss, line)) {
                    if (line.empty()) {
                        ImGui::Dummy({1, 5 * runtime_config::ui_scale});
                        continue;
                    }
                    
                    // 根据不同的方括号标题应用不同颜色 - 使用 starts_with
                    if (line.starts_with("Check for Updates")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("Current Version:")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 1.0f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("Latest Version")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("[New Features]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("[Improvements]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("[Bug Fixes]")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("Status:")) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("%s", line.c_str());
                        ImGui::PopStyleColor();
                    } else if (line.starts_with("-")) {
                        ImGui::BulletText("%s", line.substr(2).c_str());
                    } else {
                        ImGui::TextWrapped("%s", line.c_str());
                    }
                }
                
                ImGui::EndChild();
                
                ImGui::Dummy({1, 15 * runtime_config::ui_scale});
                
                // 按钮区域
                if (ImGui::Button("View Full Changelog", {200 * runtime_config::ui_scale, 35 * runtime_config::ui_scale})) {
                    show_simple_info_popup(*popup_mgr,
                        "Full Changelog",
                        "Opening full changelog in your default browser.",
                        get_about_update_information_online_link()
                    );
                }
                
                ImGui::SameLine();
                
                if (ImGui::Button("Check Again", {150 * runtime_config::ui_scale, 35 * runtime_config::ui_scale})) {
                    show_simple_info_popup(*popup_mgr,
                        "Update Check",
                        "Checking for updates...",
                        "No new updates available at this time."
                    );
                }
                
                ImGui::Dummy({1, 10 * runtime_config::ui_scale});
                
                if (ImGui::Button("Close", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed) {
                    ImGui::PopStyleVar();
                    return true;
                }
                
                ImGui::PopStyleVar();
                return false;
            }
        };
        
        popup_manager.open_window({
            .title = "Software Updates",
            .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
            .has_close_button = true,
            .keep_centered = true,
            .render_func = Updates_window(&popup_manager)
        });
    }

} // namespace about