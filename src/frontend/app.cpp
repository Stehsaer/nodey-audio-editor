#include "frontend/app.hpp"
#include "frontend/appSettingsWindows.hpp"
#include "frontend/nerdfont.hpp"
#include "imnodes.h"
#include "processor/audio-io.hpp"
#include "utility/anycast-utility.hpp"

#include <imgui.h>
#include <utility>
#include"frontend/help.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include"utility/dialog-utility.hpp"

// =============================================================================
// MAIN APPLICATION FRAMEWORK
// =============================================================================

// =============================================================================
/*UI构造函数*/
void App::draw()
{   
    initialize_original_style();
	draw_menubar();

	const auto side_panel_width_pixel = config::appearance::side_panel_width * runtime_config::ui_scale;
	const auto [display_size_x, display_size_y] = ImGui::GetIO().DisplaySize;

    // 计算主面板宽度
    float main_panel_width = show_side_panel ? 
        display_size_x - side_panel_width_pixel : 
        display_size_x;

	// 侧面板
    if(show_side_panel)
	{
		ImGui::SetNextWindowPos(ImVec2(display_size_x - side_panel_width_pixel, 0));
		ImGui::SetNextWindowSize({side_panel_width_pixel, ImGui::GetIO().DisplaySize.y});
		ImGui::SetNextWindowBgAlpha(1.0f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		if (!ImGui::Begin(
				"##left_panel",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar
					| ImGuiWindowFlags_NoBringToFrontOnFocus
			))
			THROW_LOGIC_ERROR("Failed to create left window");
		ImGui::PopStyleVar(2);
		draw_side_panel();
		ImGui::End();
	}

	// 主面板
	{
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize({main_panel_width, ImGui::GetIO().DisplaySize.y});
		ImGui::SetNextWindowBgAlpha(1.0f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		if (!ImGui::Begin(
				"##right_panel",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar
					| ImGuiWindowFlags_NoBringToFrontOnFocus
			))
			THROW_LOGIC_ERROR("Failed to create right window");
		ImGui::PopStyleVar(2);
		draw_main_panel();
		ImGui::End();
	}

	// 工具栏
    if(show_toolbar)
	{
		const auto toolbar_margin_pixel = config::appearance::toolbar_margin * runtime_config::ui_scale;
		const auto toolbar_internal_width_pixel
			= config::appearance::toolbar_internal_width * runtime_config::ui_scale;
		const auto toolbar_width_pixel = toolbar_internal_width_pixel + 2 * ImGui::GetStyle().WindowPadding.x;

		ImGui::SetNextWindowPos({toolbar_margin_pixel, toolbar_margin_pixel + main_menu_bar_height});
		ImGui::SetNextWindowSize({toolbar_width_pixel, 0});
		if (!ImGui::Begin(
				"##toolbar",
				nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize
					| ImGuiWindowFlags_NoSavedSettings
			))
			THROW_LOGIC_ERROR("Failed to create toolbar window");
		draw_toolbar();
		ImGui::End();
	}
    //性能指标窗口
    if (show_diagnostics){
        draw_performance_overlay();
    }
    draw_grid_settings_dialog();
    draw_zoom_dialog();
	ImGui::ShowDemoWindow();
}

// =============================================================================
/*菜单栏绘制*/
//主菜单绘制
void App::draw_menubar()
{
	if (ImGui::BeginMainMenuBar())
	{
		main_menu_bar_height = ImGui::GetWindowSize().y;
		if (ImGui::BeginMenu("File"))
		{
			draw_menubar_file();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			draw_menubar_edit();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			draw_menubar_view();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			draw_menubar_help();
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

// 绘制主菜单栏的文件(FILE)选项
void App::draw_menubar_file()
{
	if (ImGui::MenuItem("New", "Ctrl+N"))
	{
		new_create_project();
	}

	if (ImGui::MenuItem("Open", "Ctrl+O"))
	{
		show_open_dialog();
	}

	if (ImGui::MenuItem("Save", "Ctrl+S"))
	{
		show_save_dialog();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Exit", "Ctrl+Q")){
        // 判断是否有未保存更改
        if (is_project_modified()) {
            add_exit_confirm_window();
        } else {
            SDL_Event quit_event;
            quit_event.type = SDL_QUIT;
            SDL_PushEvent(&quit_event);
        }
	} 
}

// 绘制主菜单栏的编辑(EDIT)选项
void App::draw_menubar_edit(){
	if (ImGui::MenuItem("Undo", "Ctrl+Z")){undo();} //graph.undo();
	if (ImGui::MenuItem("Redo", "Ctrl+Y")){redo();}//graph.redo();
	ImGui::Separator();
	if (ImGui::MenuItem("Remove Selected All", "Del"))
	{
		save_undo_state();
		remove_selected_nodes();
	}
	if(ImGui::MenuItem("Remove selected Links", "Ctrl+Del"))
	{
		const auto selected_link_count = ImNodes::NumSelectedLinks();
		if (selected_link_count != 0)
		{	
			save_undo_state();
			std::vector<infra::Id_t> selected_links(selected_link_count);
			ImNodes::GetSelectedLinks(selected_links.data());

			for (auto i : selected_links
							| std::views::filter([this](auto i) { return i >= 0 && graph.links.contains(i); }))
				graph.remove_link(i);
			}
		}
	if (ImGui::MenuItem("Settings")) {
		show_app_settings();
	}
	
}

// 绘制主菜单栏的视图(VIEW)选项
void App::draw_menubar_view(){
    draw_view_panels_menu();
    ImGui::Separator();
    draw_view_center_menu();
    draw_view_zoom_menu();
    ImGui::Separator();
    draw_view_theme_menu();
    ImGui::Separator();
    draw_view_status_menu();
    draw_view_windows_menu();
}

// 绘制主菜单栏的帮助(HELP)选项
void App::draw_menubar_help(){
    if (ImGui::MenuItem("View Documentation", "F1")) {
        about::show_help_documentation_window(this, popup_manager);
    }
    if (ImGui::MenuItem("About")) {
        about::show_about_window(this, popup_manager);
    }
    if (ImGui::MenuItem("Check for Updates")) {
        about::show_updates_window(this, popup_manager);
    }
}


//==============================================================================
/*UI面板绘制*/
// 绘制侧边栏
void App::draw_side_panel()
{
	const auto full_width = config::appearance::side_panel_width * runtime_config::ui_scale
						  - 2 * ImGui::GetStyle().WindowPadding.x;


    // 如果有选中的节点，显示节点信息
    if (selected_node_id != -1 && graph.nodes.contains(selected_node_id))
    {
        const auto& selected_node = graph.nodes.at(selected_node_id);
        
        ImGui::SeparatorText("Selected Node");
        
        // 显示节点信息
        const auto processor_info = selected_node.processor->get_processor_info_non_static();
        ImGui::Text("Type: %s", processor_info.display_name.c_str());
        ImGui::Text("ID: %d", selected_node_id);
        ImGui::Text("Position: (%.1f, %.1f)", selected_node.position.x, selected_node.position.y);
        
        // 显示节点的详细内容（如果支持）
        ImGui::Separator();
        ImGui::Text("Node Settings:");
        
        // 让节点绘制其配置界面
        if (selected_node.processor->draw_content(false))  // true 表示在侧边栏模式
        {
            graph.update_node_pin(selected_node_id);
        }
        ImGui::Separator();
    }

	{
		const ImVec2 button_size = {full_width, ImGui::CalcTextSize("P").y * 2};

		switch (state.load())
		{
		case State::Editing:
			if (ImGui::Button("Preview##preview_button", button_size))
			{
				if (runner == nullptr)
					state = State::Preview_requested;
				else
					THROW_LOGIC_ERROR("Runner already exists, cannot preview again.");
			}
			break;
		case State::Previewing:
			if (ImGui::Button("Stop Preview##preview_button", button_size))
			{
				if (runner)
					state = State::Preview_cancelling;
				else
					THROW_LOGIC_ERROR("Runner does not exist, cannot stop.");
			}
			break;
		case State::Preview_requested:
			ImGui::BeginDisabled(true);
			ImGui::Button("Starting Preview##preview_button", button_size);
			ImGui::EndDisabled();
			break;
		case State::Preview_cancelling:
			ImGui::BeginDisabled(true);
			ImGui::Button("Stopping Preview##preview_button", button_size);
			ImGui::EndDisabled();
			break;
		}
	}
/* 	ImGui::SeparatorText("Diagnostics");
	ImGui::BulletText("%.2f FPS", ImGui::GetIO().Framerate); */

	if (state == State::Previewing)
	{
		ImGui::Separator();

		for (const auto& [id, link] : runner->get_link_products())
		{
			const auto& product_raw = *link;

			try
			{
				const auto& product = dynamic_cast<const processor::Audio_stream&>(product_raw);
				ImGui::Text("Link %d: %s", id, product.get_typeinfo().name());

				const std::string info
					= std::format("{}/{}", product.buffered_count(), processor::max_queue_size);

				ImGui::ProgressBar(
					(float)product.buffered_count() / processor::max_queue_size,
					ImVec2(full_width, ImGui::GetTextLineHeight()),
					info.c_str()
				);
			}
			catch (const std::bad_cast&)
			{
				ImGui::Text("Link %d: %s", id, product_raw.get_typeinfo().name());
				continue;
			}
		}
	}
}

// 绘制主面板
void App::draw_main_panel()
{
	draw_node_editor();
}

// 绘制工具栏
void App::draw_toolbar()
{
    const auto area_width = config::appearance::toolbar_internal_width * runtime_config::ui_scale;

    // 播放/预览按钮
    bool play_disabled = (state == State::Preview_requested || state == State::Preview_cancelling);
    if (play_disabled) ImGui::BeginDisabled();
    
    bool play_button_clicked = false;
    if (state == State::Editing)
    {
        play_button_clicked = ImGui::Button(ICON_PLAY "##toolbar-play", {area_width, area_width});
    }
    else if (state == State::Previewing)
    {
        // 预览状态下显示为暂停按钮
        play_button_clicked = ImGui::Button(ICON_PAUSE "##toolbar-pause", {area_width, area_width});
    }
    
    if (play_disabled) ImGui::EndDisabled();
    
    if (play_button_clicked)
    {
        if (state == State::Editing)
        {
            // 开始预览
            if (runner == nullptr)
                state = State::Preview_requested;
        }
        else if (state == State::Previewing)
        {
            // 停止预览
            if (runner)
                state = State::Preview_cancelling;
        }
    }
    
    if (ImGui::BeginItemTooltip())
    {
        if (state == State::Editing)
            ImGui::Text("Start Preview");
        else if (state == State::Previewing)
            ImGui::Text("Stop Preview");
        else
            ImGui::Text("Preview Control");
        ImGui::EndTooltip();
    }

    // 停止按钮
    bool stop_disabled = (state == State::Editing || state == State::Preview_requested || state == State::Preview_cancelling);
    if (stop_disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_STOP "##toolbar-stop", {area_width, area_width}))
    {
        if (state == State::Previewing && runner)
        {
            state = State::Preview_cancelling;
        }
    }
    
    if (stop_disabled) ImGui::EndDisabled();
    
    if (ImGui::BeginItemTooltip())
    {
        ImGui::Text("Stop Preview");
        ImGui::EndTooltip();
    }

    // 撤销按钮
    bool undo_disabled = undo_stack.empty() || state != State::Editing;
    if (undo_disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_UNDO "##toolbar-undo", {area_width, area_width}))
    {
        undo();
    }
    
    if (undo_disabled) ImGui::EndDisabled();
    
    if (ImGui::BeginItemTooltip())
    {
        if (undo_stack.empty())
            ImGui::Text("Undo - No actions to undo");
        else
            ImGui::Text("Undo - %zu actions available", undo_stack.size());
        ImGui::EndTooltip();
    }

    // 重做按钮
    bool redo_disabled = redo_stack.empty() || state != State::Editing;
    if (redo_disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_REDO "##toolbar-redo", {area_width, area_width}))
    {
        redo();
    }
    
    if (redo_disabled) ImGui::EndDisabled();
    
    if (ImGui::BeginItemTooltip())
    {
        if (redo_stack.empty())
            ImGui::Text("Redo - No actions to redo");
        else
            ImGui::Text("Redo - %zu actions available", redo_stack.size());
        ImGui::EndTooltip();
    }

    // 复制按钮
    bool copy_disabled = (ImNodes::NumSelectedNodes() == 0) || state != State::Editing;
    if (copy_disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_COPY "##toolbar-copy", {area_width, area_width}))
    {
        copy_selected_nodes();
    }
    
    if (copy_disabled) ImGui::EndDisabled();
    
    if (ImGui::BeginItemTooltip())
    {
        int selected_count = ImNodes::NumSelectedNodes();
        if (selected_count == 0)
            ImGui::Text("Copy - No nodes selected");
        else
            ImGui::Text("Copy - %d node%s selected", selected_count, selected_count > 1 ? "s" : "");
        ImGui::EndTooltip();
    }

    // 粘贴按钮
    bool paste_disabled = copied_graph_json.empty() || state != State::Editing;
    if (paste_disabled) ImGui::BeginDisabled();
    
    if (ImGui::Button(ICON_PASTE "##toolbar-paste", {area_width, area_width}))
    {
        paste_nodes();
    }
    
    if (paste_disabled) ImGui::EndDisabled();
    
    if (ImGui::BeginItemTooltip())
    {
        if (copied_graph_json.empty())
            ImGui::Text("Paste - No nodes in clipboard");
        else
            ImGui::Text("Paste - %zu node%s in clipboard", copied_graph_json.size(), copied_graph_json.size() > 1 ? "s" : "");
        ImGui::EndTooltip();
    }
}

// =============================================================================
/*应用主循环*/
void App::run()
{
	while (true)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			imgui_context.process_event(event);
			if (event.type == SDL_QUIT) return;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE
				&& event.window.windowID == SDL_GetWindowID(sdl_context.get_window_ptr()))
				return;
		}

		if (SDL_GetWindowFlags(sdl_context.get_window_ptr()) & SDL_WINDOW_MINIMIZED)
		{
			SDL_Delay(10);
			continue;
		}

		imgui_context.new_frame();
		{
			draw();
			popup_manager.draw();
			// ImGui::ShowDemoWindow();
		}
		imgui_context.render(sdl_context.get_renderer_ptr());

		poll_state();
	}

}


// =============================================================================
// POPUP WINDOW MANAGEMENT
// =============================================================================

/*弹窗管理*/
//================================================================================
/*错误和信息弹窗*/
// 错误弹窗
void App::add_error_popup_window(
	std::string message,
	std::string explanation,
	std::string detail,
	std::source_location loc
)
{
	class Error_window
	{
		std::string message;
		std::string explanation;
		std::string detail;
		std::source_location loc;

	  public:

		Error_window(
			std::string message,
			std::string explanation,
			std::string detail,
			std::source_location loc = std::source_location::current()
		) :
			message(std::move(message)),
			explanation(std::move(explanation)),
			detail(std::move(detail)),
			loc(loc)
		{
		}

		bool operator()(bool close_button_pressed)
		{
			ImGui::Text("%s", message.c_str());

			ImGui::Dummy({1, 5 * runtime_config::ui_scale});

			if (!explanation.empty())
			{
				ImGui::SeparatorText("Explanation");
				ImGui::BeginChild(
					"Explanation",
					{200 * runtime_config::ui_scale, 120 * runtime_config::ui_scale},
					ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize
				);
				{
					ImGui::TextWrapped("%s", explanation.c_str());
				}
				ImGui::EndChild();
			}

			if (!detail.empty())
			{
				if (ImGui::TreeNode("Detail"))
				{
					ImGui::TextWrapped("%s", detail.c_str());
					ImGui::TreePop();
				}
			}

			return close_button_pressed;
		}
	};

	popup_manager.open_window(
		{.title = ICON_WARNING " Error",
		 .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		 .has_close_button = true,
		 .keep_centered = true,
		 .render_func = Error_window(std::move(message), std::move(explanation), std::move(detail), loc)}
	);
}

// 信息输出功能弹窗
void App::add_info_popup_window(
    std::string title,
    std::string explanation,
    std::string detail 
)
{
    class Info_window
    {
        std::string title;	
        std::string explanation;
        std::string detail;
    public:
        Info_window(std::string title, std::string explanation, std::string detail)
            : title(std::move(title)), explanation(std::move(explanation)), detail(std::move(detail)) {}

        bool operator()(bool close_button_pressed)
        {
            ImGui::Text("%s", title.c_str());

			if(!explanation.empty()){
				ImGui::SeparatorText("Explanation");
				ImGui::BeginChild(
					"Explanation",
					{300 * runtime_config::ui_scale, 140 * runtime_config::ui_scale},
					ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize
				);
				ImGui::TextWrapped("%s", explanation.c_str());
				ImGui::EndChild();
			} else {
				ImGui::Dummy({1, 5 * runtime_config::ui_scale});
			}

            if(!detail.empty()){
				if(ImGui::TreeNode("Detail")){
					ImGui::TextWrapped("%s", detail.c_str());
					ImGui::TreePop();
				}
			}
            return close_button_pressed;
        }
    };

    popup_manager.open_window(
        {.title = std::move(title),
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Info_window(std::move(title), std::move(explanation), std::move(detail))}
    );
}

//================================================================================
/*确认对话框*/
// 新建项目确认弹窗
void App::add_new_project_confirm_window(
	std::function<void()> on_save,
	std::function<void()> on_discard,
	std::function<void()> on_cancel
)
{
	class New_project_confirm_window
	{
		std::function<void()> on_save;
		std::function<void()> on_discard;
		std::function<void()> on_cancel;

	  public:
		New_project_confirm_window(
			std::function<void()> on_save,
			std::function<void()> on_discard,
			std::function<void()> on_cancel
		) :
			on_save(std::move(on_save)),
			on_discard(std::move(on_discard)),
			on_cancel(std::move(on_cancel))
		{
		}

		bool operator()(bool close_button_pressed)
		{
			ImGui::Text("Are you sure you want to create a new project?");

			ImGui::Dummy({1, 5 * runtime_config::ui_scale});

			if (ImGui::Button("Save"))
			{
				on_save();
				return true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard"))
			{
				on_discard();
				return true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || close_button_pressed)
			{
				on_cancel();
				return true;
			}

			return false;
		}
	};

	popup_manager.open_window(
		{.title = ICON_WARNING " New Project",
		 .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		 .has_close_button = true,
		 .keep_centered = true,
		 .render_func = New_project_confirm_window(std::move(on_save), std::move(on_discard), std::move(on_cancel))}
	);
}

// 显示保存项目时覆盖已有文件确认对话框
void App::show_overwrite_confirm_dialog( std::string& filepath,   std::string& json_content)
{
    class Overwrite_confirm_window
    {
        std::string filepath;
        std::string json_content;
        App* app_ptr;
        
    public:
        Overwrite_confirm_window( std::string& path,  std::string& content, App* app) 
            : filepath(std::move(path)), json_content(std::move(content)), app_ptr(app) {}
        
        bool operator()(bool close_button_pressed)
        {
            ImGui::Text("File already exists!");
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Text("File: %s", filepath.c_str());
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Text("Do you want to overwrite it?");
            
            ImGui::Dummy({1, 15 * runtime_config::ui_scale});
            ImGui::Separator();
            
            if (ImGui::Button("Overwrite", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                // 直接写入文件
                try
                {
                    std::ofstream file(filepath);
                    if (!file.is_open())
                    {
                        throw std::runtime_error("Failed to open file for writing: " + filepath);
                    }
                    
                    file << json_content;
                    file.close();

                    // 显示成功消息，如果需要退出则在回调中处理
                    if(app_ptr->should_exit_after_save)
                    {   
                        // 创建带退出回调的保存成功弹窗
                        app_ptr->add_save_success_popup_with_exit("File saved to: " + filepath);
                        app_ptr->should_exit_after_save = false; // 重置标志
                    }
                    else
                    {
                        // 普通的保存成功弹窗
                        app_ptr->add_info_popup_window(
                            "Save Successful",
                            "Project has been saved successfully.",
                            "File saved to: " + filepath
                        );
                    }
                }
                catch (const std::exception& e)
                {
                    app_ptr->should_exit_after_save=false; // 设置未保存退出标志
                    app_ptr->add_error_popup_window(
                        "Save Failed",
                        "Failed to save the project.",
                        e.what()
                    );
                }
                return true;
            }
            
            ImGui::SameLine();
            
			return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed;
        }
    };
    
    popup_manager.open_window(
        {.title = "File Exists",
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Overwrite_confirm_window(filepath, json_content, this)}
    );
}

// 显示打开文件确认对话框
void App::show_open_confirm_dialog(const std::string& filepath, const std::string& json_content)
{
    class Open_confirm_window
    {
        std::string filepath;
        std::string json_content;
        App* app_ptr;
        
    public:
        Open_confirm_window(const std::string& path, const std::string& content, App* app) 
            : filepath(path), json_content(content), app_ptr(app) {}
        
        bool operator()(bool close_button_pressed)
        {
            ImGui::Text("Opening a new project will discard current changes.");
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Text("File: %s", filepath.c_str());
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Text("Do you want to continue?");
            
            ImGui::Dummy({1, 15 * runtime_config::ui_scale});
            ImGui::Separator();
            
            if (ImGui::Button("Save Current", {120 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                // 保存当前项目然后打开新项目
                app_ptr->show_save_dialog();
                // 注意：这里可能需要延迟加载新项目，等保存完成后再加载
                return true;
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Discard", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                // 直接加载新项目
                app_ptr->load_project_from_file(json_content, filepath);
                return true;
            }
            
            ImGui::SameLine();
            
            return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed;
        }
    };
    
    popup_manager.open_window(
        {.title = "Open Project",
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Open_confirm_window(filepath, json_content, this)}
    );
}

// =============================================================================
/*设置对话框*/
//app_settings对话框
void App::show_app_settings() {
    frontend::AppSettingsWindow::show(
        popup_manager,
        [this]() {
            // 应用设置时的回调
            // 可以在这里添加特定的应用逻辑
            on_settings_applied();
        }
    );
}

//设置保存项目对话框
void App::show_save_dialog()
{
    class Save_dialog_window
    {
        std::string project_name = "untitled_project";
        std::string save_directory = "projects";
        App* app_ptr;
        
    public:
        Save_dialog_window(App* app) : app_ptr(app) {}
        
        bool operator()(bool close_button_pressed)
        {
            // 项目名称输入
            ImGui::Text("Project Name:");
            ImGui::SetNextItemWidth(300 * runtime_config::ui_scale);
            ImGui::InputText("##project_name", &project_name);
            
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            
            // 保存路径选择
            ImGui::Text("Save Directory:");
            ImGui::SetNextItemWidth(250 * runtime_config::ui_scale);
            ImGui::InputText("##save_directory", &save_directory);
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                auto selected_folder = open_folder_dialog("Select Save Directory", save_directory);
                if (selected_folder.has_value())
                {
                    save_directory = selected_folder.value();
                }
                else if( selected_folder == std::nullopt)
                {
                    // 用户取消了选择
                }
            }
            
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            
            // 预览完整路径
            std::string full_path = save_directory + "/" + project_name + ".json";
            ImGui::Text("Full Path: %s", full_path.c_str());
            
            ImGui::Dummy({1, 15 * runtime_config::ui_scale});
            
            // 按钮区域
            ImGui::Separator();
            
            // 保存按钮
            if (ImGui::Button("Save", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                if (!project_name.empty())
                {
                    app_ptr->save_project_with_path(save_directory, project_name);
                    return true; // 关闭对话框
                }
                else
                {
                    // 显示错误提示
                    app_ptr->add_error_popup_window(
                        "Invalid Project Name",
                        "Project name cannot be empty.",
                        ""
                    );
                }
            }
            
            ImGui::SameLine();
            
            // 取消按钮
            return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed;
        }
    };
    
    popup_manager.open_window(
        {.title = "Save Project",
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Save_dialog_window(this)}
    );
}

// 显示打开文件对话框
void App::show_open_dialog()
{
        auto selected_file = open_file_dialog(
        "Open Project", 
        {"JSON Files", "*.json", "All Files", "*.*"}, 
        ""
    );

    if (selected_file.has_value())
    {
        try
        {
            // 读取文件内容
            std::ifstream file(selected_file.value());
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + selected_file.value());
            }
            
            // 检查文件是否为空
            file.seekg(0, std::ios::end);
            std::streamsize file_size = file.tellg();
            if (file_size == 0)
            {
                throw std::runtime_error("File is empty");
            }
            file.seekg(0, std::ios::beg);
            
            // 读取文件内容
            std::string json_content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
            file.close();
            
            // 如果当前有未保存的工作，显示确认对话框
            if (!graph.nodes.empty())
            {
                show_open_confirm_dialog(selected_file.value(), json_content);
            }
            else
            {
                // 直接加载文件
                load_project_from_file(json_content, selected_file.value());
            }
        }
        catch (const std::exception& e)
        {
            add_error_popup_window(
                "Failed to Open File",
                "Could not read the selected file.",
                e.what()
            );
        }
    }
}

// 绘制网格设置对话框
void App::draw_grid_settings_dialog() {
    if (!show_grid_settings_dialog) return;
    
    class Grid_settings_window {
        float* base_grid_spacing_ptr;
        float* current_zoom_ptr;
        bool* snap_to_grid_ptr;
        App* app_ptr;
        float temp_spacing;
        
    public:
        Grid_settings_window(float* spacing, float* zoom, bool* snap, App* app) 
            : base_grid_spacing_ptr(spacing), current_zoom_ptr(zoom), snap_to_grid_ptr(snap), app_ptr(app) {
            temp_spacing = *base_grid_spacing_ptr;
        }
        
        bool operator()(bool close_button_pressed) {
            ImGui::Text("Grid Settings");
            ImGui::Separator();
            
            // 基础网格间距滑块
            ImGui::Text("Base Grid Spacing:");
            if (ImGui::SliderFloat("##base_grid_spacing", &temp_spacing, 5.0f, 100.0f, "%.1f px")) {
                // 实时预览
                app_ptr->original_style.grid_spacing = temp_spacing;
                app_ptr->apply_zoom();
            }
            
            ImGui::Dummy({1, 5 * runtime_config::ui_scale});
            
            // 显示当前实际大小
            float current_display_size = temp_spacing * (*current_zoom_ptr);
            ImGui::Text("Current Display Size: %.1f px", current_display_size);
            ImGui::Text("Zoom Level: %.0f%%", (*current_zoom_ptr) * 100);
            
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            
            // 预设按钮
            ImGui::Text("Presets:");
            const float presets[] = {10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f};
            for (int i = 0; i < 7; i++) {
                if (i > 0) ImGui::SameLine();
                if (ImGui::Button(std::format("{:.0f}", presets[i]).c_str())) {
                    temp_spacing = presets[i];
                    app_ptr->original_style.grid_spacing = temp_spacing;
                    app_ptr->apply_zoom();
                }
            }
            
            ImGui::Dummy({1, 15 * runtime_config::ui_scale});
            ImGui::Separator();
            
            // 确定和取消按钮
            if (ImGui::Button("OK", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale})) {
                app_ptr->original_style.grid_spacing = temp_spacing;
                app_ptr->apply_zoom();
                
                app_ptr->add_info_popup_window(
                    "Grid Settings Applied",
                    std::format("Base grid spacing set to {:.1f} pixels.\nCurrent display size: {:.1f} pixels.", 
                               temp_spacing, current_display_size),
                    ""
                );
                return true;
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed) {
                // 恢复原值
                app_ptr->apply_zoom();
                return true;
            }
            
            return false;
        }
    };
    
    popup_manager.open_window({
        .title = "Grid Settings",
        .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
        .has_close_button = true,
        .keep_centered = true,
        .render_func = Grid_settings_window(&original_style.grid_spacing, &current_zoom, &snap_to_grid, this)
    });
    
    show_grid_settings_dialog = false;
}

// 绘制缩放设置对话框
void App::draw_zoom_dialog() {
    if (!show_zoom_dialog) return;
    
    class Zoom_dialog_window {
        float* current_zoom_ptr;
        float temp_zoom;
        App* app_ptr;
        
    public:
        Zoom_dialog_window(float* zoom, App* app) 
            : current_zoom_ptr(zoom), app_ptr(app) {
            temp_zoom = *current_zoom_ptr * 100; // 转换为百分比
        }
        
        bool operator()(bool close_button_pressed) {
            ImGui::Text("Custom Zoom Level");
            ImGui::Separator();
            
            // 缩放滑块
            ImGui::Text("Zoom Level:");
            if (ImGui::SliderFloat("##zoom_level", &temp_zoom, 10.0f, 500.0f, "%.0f%%")) {
                // 实时预览
                float new_zoom = temp_zoom / 100.0f;
                if (new_zoom >= app_ptr->min_zoom && new_zoom <= app_ptr->max_zoom) {
                    *current_zoom_ptr = new_zoom;
                    app_ptr->apply_zoom();
                }
            }
            
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            
            // 输入框
            ImGui::Text("Enter exact value:");
            ImGui::SetNextItemWidth(150 * runtime_config::ui_scale);
            if (ImGui::InputFloat("##zoom_input", &temp_zoom, 1.0f, 10.0f, "%.0f%%")) {
                temp_zoom = std::clamp(temp_zoom, 10.0f, 500.0f);
            }
            
            ImGui::Dummy({1, 15 * runtime_config::ui_scale});
            ImGui::Separator();
            
            // 确定和取消按钮
            if (ImGui::Button("Apply", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale})) {
                float new_zoom = temp_zoom / 100.0f;
                app_ptr->set_zoom_level(new_zoom);
                return true;
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed) {
                // 恢复原值
                app_ptr->apply_zoom();
                return true;
            }
            
            return false;
        }
    };
    
    popup_manager.open_window({
        .title = "Custom Zoom",
        .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
        .has_close_button = true,
        .keep_centered = true,
        .render_func = Zoom_dialog_window(&current_zoom, this)
    });
    
    show_zoom_dialog = false;
}


// =============================================================================
// PROJECT MANAGEMENT
// =============================================================================

// =============================================================================
/*项目管理*/
//创建新的项目
void App::new_create_project(){
	//先要确认是否要创建新项目
	add_new_project_confirm_window(
		[this](){
			//如果选择保存，则保存当前项目

		},
		[this](){
			//如果选择放弃，则清空当前项目
			clear_current_project();
		},
		[this](){
			//如果选择取消，则不做任何操作
		}
	);
}

// 清空当前项目
void App::clear_current_project(){
	save_undo_state();
	//如果处于播放状态，先停止播放
	if (state == State::Previewing && runner)
	{
		runner.reset();
		state = State::Editing;
	}
	//清空当前图
	graph=infra::Graph();
	//重置状态
	state = State::Editing;
}

//文件序列化
std::string App::save_graph_as_string() const
{
	const auto json = graph.serialize();
	Json::StreamWriterBuilder writer;
	writer["indentation"] = "  ";  // 设置缩进为两个空格
	return Json::writeString(writer, json);
}

//文件反序列化
void App::load_graph_from_string(const std::string& json_string)
{
	Json::Value json;
	const auto parse_result = Json::Reader().parse(json_string, json, false);
	if (!parse_result) throw infra::Graph::Invalid_file_error("Failed to parse JSON");

	graph = infra::Graph::deserialize(json);

	for (const auto& [id, node] : graph.nodes) ImNodes::SetNodeGridSpacePos(id, node.position);
}

// =============================================================================
// FILE OPERATIONS
// =============================================================================

// =============================================================================
/*文件操作*/
// 使用指定路径和名称保存项目
void App::save_project_with_path( std::string& directory,  std::string& project_name)
{
    try
    {
        // 获取当前图的JSON字符串
        std::string json_string = save_graph_as_string();
        
        // 确保文件名有.json扩展名
        std::string filename = project_name;
        if (!filename.ends_with(".json"))
        {
            filename += ".json";
        }
        
        // 创建保存目录（如果不存在）
        if (!std::filesystem::exists(directory))
        {
            std::filesystem::create_directories(directory);
		}
		
        
        // 完整文件路径
        std::string filepath = directory + "/" + filename;
        
        // 检查文件是否已存在
        if (std::filesystem::exists(filepath))
        {
            // 显示覆盖确认对话框
            show_overwrite_confirm_dialog(filepath, json_string);
            return;
        }
        
        // 写入文件
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file for writing: " + filepath);
        }
        
        file << json_string;
        file.close();
        
        last_saved_graph_json = json_string; // 更新最后保存的图JSON
        // 显示成功消息
        // 显示成功消息，如果需要退出则在回调中处理
        if(should_exit_after_save)
        {   
            // 创建带退出回调的保存成功弹窗
            add_save_success_popup_with_exit("File saved to: " + filepath);
            should_exit_after_save = false; // 重置标志
        }
        else
        {
            // 普通的保存成功弹窗
            add_info_popup_window(
                "Save Successful",
                "Project has been saved successfully.",
                "File saved to: " + filepath
            );
        }
    }
    catch (const std::exception& e)
    {
        should_exit_after_save=false; // 取消退出标志
        add_error_popup_window(
            "Save Failed",
            "Failed to save the project.",
            e.what()
        );
    }
}

// 加载项目文件
void App::load_project_from_file(const std::string& json_content, const std::string& filepath)
{
    try
    {
        // 保存当前状态到撤销栈
        save_undo_state();
        
        // 如果正在预览，先停止预览
        if (state == State::Previewing && runner)
        {
            runner.reset();
            state = State::Editing;
        }
        
        // 清空重做栈
        redo_stack.clear();
        
        // 加载新的图
        load_graph_from_string(json_content);
        
        // 显示成功消息
        std::filesystem::path file_path(filepath);
        std::string filename = file_path.filename().string();
        
        add_info_popup_window(
            "Project Loaded",
            "Project has been loaded successfully.",
            "File: " + filename
        );
    }
    catch (const infra::Graph::Invalid_file_error& e)
    {
        add_error_popup_window(
            "Invalid Project File",
            "The selected file is not a valid project file or is corrupted.",
            e.message
        );
    }
    catch (const std::exception& e)
    {
        add_error_popup_window(
            "Load Failed",
            "Failed to load the project file.",
            e.what()
        );
    }
}


// =============================================================================
// UNDO/REDO SYSTEM
// =============================================================================

/*撤销重做管理*/
//=============================================================================
/*撤销重做状态实现*/
// 保存当前图到撤销栈
void App::save_undo_state(){
    // 限制栈大小
    const size_t max_undo_size = 30;
    
    // 保存当前图到撤销栈
    undo_stack.push_back(graph);
    
    // 如果超过限制，移除最老的状态
    if (undo_stack.size() > max_undo_size) {
        undo_stack.pop_front();
    }
    
    // 清空重做栈（因为新操作会使之前的重做路径失效）
    redo_stack.clear();
    
    //定期压缩内存
    if (undo_stack.size() % 10 == 0) {
        compress_undo_stack();
    }
}

// 保存当前图到重做栈
void App::save_redo_state(){
    // 限制栈大小
    const size_t max_redo_size = 30;
    
    // 保存当前图到重做栈
    redo_stack.push_back(graph);
    
    // 如果超过限制，移除最老的状态
    if (redo_stack.size() > max_redo_size) {
        redo_stack.pop_front();
    }
    
    // 清空撤销栈
    undo_stack.clear();
}

// 撤销和重做操作
void App::undo() {
    if (!undo_stack.empty()) {
        // 限制重做栈大小
        const size_t max_redo_size = 30;
        
        redo_stack.push_back(graph);
        
        // 如果重做栈超过限制，移除最老的状态
        if (redo_stack.size() > max_redo_size) {
            redo_stack.pop_front();
        }
        
        graph = undo_stack.back();
        undo_stack.pop_back();

        restore_node_positions();
    }
}

void App::redo() {
    if (!redo_stack.empty()) {
        // 限制撤销栈大小
        const size_t max_undo_size = 30;
        
        undo_stack.push_back(graph);
        
        // 如果撤销栈超过限制，移除最老的状态
        if (undo_stack.size() > max_undo_size) {
            undo_stack.pop_front();
        }
        
        graph = redo_stack.back();
        redo_stack.pop_back();

        restore_node_positions();
    }
}

/*辅助函数*/
//===============================================================================
// 压缩撤销栈
void App::compress_undo_stack() {
    // 内存压缩策略：每10个状态合并为一个检查点
    if (undo_stack.size() < 20) return;  // 少于20个状态时不压缩
    
    // 保留最近的10个状态，每隔5个状态保留一个检查点
    std::list<infra::Graph> compressed_stack;
    
    auto it = undo_stack.begin();
    
    // 从较早的状态中每隔5个保留一个检查点
    size_t recent_start = undo_stack.size() >= 10 ? undo_stack.size() - 10 : 0;
    for (size_t i = 0; i < recent_start && it != undo_stack.end(); i += 5) {
        auto checkpoint_it = it;
        std::advance(it, std::min(5UL, static_cast<size_t>(std::distance(it, undo_stack.end()))));
        compressed_stack.push_back(std::move(*checkpoint_it));
    }
    
    // 保留最近的10个状态
    auto recent_it = undo_stack.begin();
    std::advance(recent_it, recent_start);
    for (auto iter = recent_it; iter != undo_stack.end(); ++iter) {
        compressed_stack.push_back(std::move(*iter));
    }
    
    undo_stack = std::move(compressed_stack);
}

// 恢复节点位置
void App::restore_node_positions() {
    for (const auto& [id, node] : graph.nodes) {
        if (node.position.x != 0.0f || node.position.y != 0.0f) {
            ImNodes::SetNodeScreenSpacePos(id, node.position);
        }
    }
}


// =============================================================================
// COPY/PASTE SYSTEM
// =============================================================================

// =============================================================================
/*复制粘贴操作*/
// 复制选中的节点和连线
void App::copy_selected_nodes()
{
    const auto selected_node_count = ImNodes::NumSelectedNodes();
    const auto selected_link_count = ImNodes::NumSelectedLinks();
    
    if (selected_node_count == 0 && selected_link_count == 0) return;
    
    std::vector<infra::Id_t> selected_nodes(selected_node_count);
    std::vector<infra::Id_t> selected_links(selected_link_count);
    
    if (selected_node_count > 0)
        ImNodes::GetSelectedNodes(selected_nodes.data());
    if (selected_link_count > 0)
        ImNodes::GetSelectedLinks(selected_links.data());
    
    // 创建一个临时图来保存选中的内容
    infra::Graph temp_graph;
    std::map<infra::Id_t, infra::Id_t> node_id_mapping; // 原ID -> 新ID映射
    
    try
    {
        // 复制选中的节点
        for (auto original_node_id : selected_nodes)
        {
            if (graph.nodes.contains(original_node_id))
            {
                const auto& original_node = graph.nodes.at(original_node_id);
                
                // 创建新的处理器实例
                const auto processor_info = original_node.processor->get_processor_info_non_static();
                auto new_processor = infra::Processor::processor_map.at(processor_info.identifier).generate();
                
                // 复制处理器配置
                new_processor->deserialize(original_node.processor->serialize());
                
                // 添加到临时图并记录ID映射
                const auto new_node_id = temp_graph.add_node(std::move(new_processor));
                temp_graph.nodes[new_node_id].position = original_node.position;
                node_id_mapping[original_node_id] = new_node_id;
            }
        }
        
        // 复制选中的连线（只复制两端都是选中节点的连线）
        for (auto original_link_id : selected_links)
        {
            if (graph.links.contains(original_link_id))
            {
                const auto& original_link = graph.links.at(original_link_id);
                const auto& from_pin = graph.pins.at(original_link.from);
                const auto& to_pin = graph.pins.at(original_link.to);
                
                // 检查连线的两端节点是否都被选中
                if (node_id_mapping.contains(from_pin.parent) && node_id_mapping.contains(to_pin.parent))
                {
                    const auto new_from_node_id = node_id_mapping[from_pin.parent];
                    const auto new_to_node_id = node_id_mapping[to_pin.parent];
                    
                    // 在临时图中找到对应的引脚
                    const auto& from_pin_map = temp_graph.nodes.at(new_from_node_id).pin_name_map;
                    const auto& to_pin_map = temp_graph.nodes.at(new_to_node_id).pin_name_map;
                    
                    if (from_pin_map.contains(from_pin.attribute.identifier) && 
                        to_pin_map.contains(to_pin.attribute.identifier))
                    {
                        const auto new_from_pin_id = from_pin_map.at(from_pin.attribute.identifier);
                        const auto new_to_pin_id = to_pin_map.at(to_pin.attribute.identifier);
                        
                        temp_graph.add_link(new_from_pin_id, new_to_pin_id);
                    }
                }
            }
        }
        
        // 如果没有选中连线，也要复制节点之间的内部连线
        if (selected_link_count == 0 && selected_node_count > 0)
        {
            for (const auto& [link_id, link] : graph.links)
            {
                const auto& from_pin = graph.pins.at(link.from);
                const auto& to_pin = graph.pins.at(link.to);
                
                // 检查连线的两端节点是否都被选中
                if (node_id_mapping.contains(from_pin.parent) && node_id_mapping.contains(to_pin.parent))
                {
                    const auto new_from_node_id = node_id_mapping[from_pin.parent];
                    const auto new_to_node_id = node_id_mapping[to_pin.parent];
                    
                    // 在临时图中找到对应的引脚
                    const auto& from_pin_map = temp_graph.nodes.at(new_from_node_id).pin_name_map;
                    const auto& to_pin_map = temp_graph.nodes.at(new_to_node_id).pin_name_map;
                    
                    if (from_pin_map.contains(from_pin.attribute.identifier) && 
                        to_pin_map.contains(to_pin.attribute.identifier))
                    {
                        const auto new_from_pin_id = from_pin_map.at(from_pin.attribute.identifier);
                        const auto new_to_pin_id = to_pin_map.at(to_pin.attribute.identifier);
                        
                        temp_graph.add_link(new_from_pin_id, new_to_pin_id);
                    }
                }
            }
        }
        

        //
        std::println("DEBUG:temp_graph has {} nodes and {} links",
                     temp_graph.nodes.size(), temp_graph.links.size());
        // 序列化临时图
        const auto json = temp_graph.serialize();
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";  // 紧凑格式
        copied_graph_json = Json::writeString(writer, json);
        
        std::println("DEBUG:copied_graph_json: {}", copied_graph_json);
        // 显示复制成功的提示
        std::string message;
        if (selected_node_count > 0 && selected_link_count > 0)
        {
            message = std::format("Copied {} node{} and {} link{} to clipboard.", 
                                 selected_node_count, selected_node_count > 1 ? "s" : "",
                                 selected_link_count, selected_link_count > 1 ? "s" : "");
        }
        else if (selected_node_count > 0)
        {
            const auto internal_links = temp_graph.links.size();
            if (internal_links > 0)
            {
                message = std::format("Copied {} node{} and {} internal link{} to clipboard.", 
                                     selected_node_count, selected_node_count > 1 ? "s" : "",
                                     internal_links, internal_links > 1 ? "s" : "");
            }
            else
            {
                message = std::format("Copied {} node{} to clipboard.", 
                                     selected_node_count, selected_node_count > 1 ? "s" : "");
            }
        }
        else if (selected_link_count > 0)
        {
            message = std::format("Copied {} link{} to clipboard.", 
                                 selected_link_count, selected_link_count > 1 ? "s" : "");
        }
        
        add_info_popup_window("Copy Successful", message, "");
    }
    catch (const std::exception& e)
    {
        add_error_popup_window(
            "Copy Failed",
            "Failed to copy selected nodes and links.",
            e.what()
        );
    }
}

// 粘贴节点和连线
void App::paste_nodes()
{
    if (copied_graph_json.empty()) 
    {
        std::println("DEBUG: copied_graph_json is empty");
        return;
    }
    
    // 调试：输出要粘贴的JSON
    std::println("DEBUG: pasting JSON: {}", copied_graph_json);
    
    save_undo_state();
    
    try
    {
        // 反序列化复制的图
        Json::Value json;
        const auto parse_result = Json::Reader().parse(copied_graph_json, json, false);
        if (!parse_result) 
        {
            std::println("DEBUG: Failed to parse JSON");
            throw std::runtime_error("Failed to parse copied graph JSON");
        }
        
        std::println("DEBUG: JSON parsed successfully");
        
        infra::Graph temp_graph = infra::Graph::deserialize(json);
        
        std::println("DEBUG: temp_graph deserialized with {} nodes, {} links", 
                    temp_graph.nodes.size(), temp_graph.links.size());
        
        // 如果临时图为空，提前返回
        if (temp_graph.nodes.empty())
        {
            std::println("DEBUG: temp_graph is empty after deserialization");
            add_info_popup_window("Paste Failed", "No nodes to paste.", "");
            return;
        }
        
        // 计算粘贴偏移量
        const ImVec2 paste_offset{50.0f, 50.0f};
        last_paste_position.x += paste_offset.x;
        last_paste_position.y += paste_offset.y;
        
        // 如果位置超出合理范围，重置到起始位置
        if (last_paste_position.x > 500.0f || last_paste_position.y > 500.0f)
        {
            last_paste_position = ImVec2{100.0f, 100.0f};
        }
        
        // 计算位置偏移量（基于第一个节点的位置）
        ImVec2 position_offset = last_paste_position;
        if (!temp_graph.nodes.empty())
        {
            const auto& first_node = temp_graph.nodes.begin()->second;
            position_offset.x -= first_node.position.x;
            position_offset.y -= first_node.position.y;
        }
        
        std::map<infra::Id_t, infra::Id_t> node_id_mapping; // 临时图ID -> 目标图ID映射
        std::vector<infra::Id_t> pasted_node_ids;
        size_t skipped_singleton_count = 0;
        
        // 粘贴节点
        for (const auto& [temp_node_id, temp_node] : temp_graph.nodes)
        {
            std::println("DEBUG: Processing node {}", temp_node_id);
            
            const auto processor_info = temp_node.processor->get_processor_info_non_static();
            
            // 检查是否为单例处理器且已存在
            if (processor_info.singleton && graph.singleton_node_map.contains(processor_info.identifier))
            {
                std::println("DEBUG: Skipping singleton node {}", processor_info.identifier);
                skipped_singleton_count++;
                continue;
            }
            
            // 创建新的处理器实例
            auto new_processor = infra::Processor::processor_map.at(processor_info.identifier).generate();
            new_processor->deserialize(temp_node.processor->serialize());
            
            // 添加到目标图
            const auto new_node_id = graph.add_node(std::move(new_processor));
            
            // 设置新位置
            ImVec2 new_position = temp_node.position;
            new_position.x += position_offset.x;
            new_position.y += position_offset.y;
            
            ImNodes::SetNodeScreenSpacePos(new_node_id, new_position);
            graph.nodes[new_node_id].position = new_position;
            
            node_id_mapping[temp_node_id] = new_node_id;
            pasted_node_ids.push_back(new_node_id);
            
            std::println("DEBUG: Pasted node {} -> {}", temp_node_id, new_node_id);
        }
        
        // 粘贴连线
        size_t pasted_link_count = 0;
        for (const auto& [temp_link_id, temp_link] : temp_graph.links)
        {
            const auto& temp_from_pin = temp_graph.pins.at(temp_link.from);
            const auto& temp_to_pin = temp_graph.pins.at(temp_link.to);
            
            // 检查两端节点是否都已成功粘贴
            if (node_id_mapping.contains(temp_from_pin.parent) && 
                node_id_mapping.contains(temp_to_pin.parent))
            {
                const auto new_from_node_id = node_id_mapping[temp_from_pin.parent];
                const auto new_to_node_id = node_id_mapping[temp_to_pin.parent];
                
                // 在目标图中找到对应的引脚
                const auto& from_pin_map = graph.nodes.at(new_from_node_id).pin_name_map;
                const auto& to_pin_map = graph.nodes.at(new_to_node_id).pin_name_map;
                
                if (from_pin_map.contains(temp_from_pin.attribute.identifier) && 
                    to_pin_map.contains(temp_to_pin.attribute.identifier))
                {
                    const auto new_from_pin_id = from_pin_map.at(temp_from_pin.attribute.identifier);
                    const auto new_to_pin_id = to_pin_map.at(temp_to_pin.attribute.identifier);
                    
                    try
                    {
                        graph.add_link(new_from_pin_id, new_to_pin_id);
                        pasted_link_count++;
                        std::println("DEBUG: Pasted link {} -> {}", temp_link_id, pasted_link_count);
                    }
                    catch (const std::exception& e)
                    {
                        std::println("DEBUG: Failed to paste link: {}", e.what());
                        // 连线创建失败，忽略这个连线
                    }
                }
            }
        }
        
        // 选中粘贴的节点
        ImNodes::ClearNodeSelection();
        ImNodes::ClearLinkSelection();
        for (auto node_id : pasted_node_ids)
        {
            ImNodes::SelectNode(node_id);
        }
        
        std::println("DEBUG: Paste completed. {} nodes, {} links", pasted_node_ids.size(), pasted_link_count);
        
        // 显示粘贴结果的提示
        std::string message;
        if (pasted_node_ids.empty() && pasted_link_count == 0)
        {
            if (skipped_singleton_count > 0)
            {
                message = std::format("No nodes were pasted. {} singleton node{} already exist{} in the graph.",
                                     skipped_singleton_count,
                                     skipped_singleton_count > 1 ? "s" : "",
                                     skipped_singleton_count > 1 ? "" : "s");
            }
            else
            {
                message = "No nodes were pasted.";
            }
        }
        else
        {
            if (pasted_link_count > 0)
            {
                message = std::format("Pasted {} node{} and {} link{} from clipboard.",
                                     pasted_node_ids.size(), pasted_node_ids.size() > 1 ? "s" : "",
                                     pasted_link_count, pasted_link_count > 1 ? "s" : "");
            }
            else
            {
                message = std::format("Pasted {} node{} from clipboard.",
                                     pasted_node_ids.size(), pasted_node_ids.size() > 1 ? "s" : "");
            }
            
            if (skipped_singleton_count > 0)
            {
                message += std::format(" ({} singleton node{} skipped)",
                                      skipped_singleton_count,
                                      skipped_singleton_count > 1 ? "s" : "");
            }
        }
        
        add_info_popup_window("Paste Successful", message, "");
    }
    catch (const std::exception& e)
    {
        std::println("DEBUG: Paste exception: {}", e.what());
        add_error_popup_window(
            "Paste Failed",
            "Failed to paste nodes and links from clipboard.",
            e.what()
        );
    }
}


// =============================================================================
// VIEW AND ZOOM SYSTEM
// =============================================================================

// =============================================================================
/*视图和缩放窗口绘制*/
// 绘制面板显示控制菜单
void App::draw_view_panels_menu() {
    static bool show_side_panel = true;
    
    if (ImGui::MenuItem("Show Toolbar", nullptr, &show_toolbar)) {
        // TODO: 实现工具栏显示/隐藏逻辑

    }
    
    if (ImGui::MenuItem("Show Node Editor Minimap", nullptr, &show_minimap)) {
        // TODO: 实现小地图显示/隐藏逻辑
        // 可以通过控制 ImNodes::MiniMap 的调用来实现
        
    }
}

// 绘制节点编辑器视图控制菜单
void App::draw_view_center_menu() {
    if (ImGui::MenuItem("Center View", "Ctrl+0")) {
        // 实现跟随光标居中视图
        enter_center_view_mode();
    }
    
    // 网格设置子菜单
    if (ImGui::BeginMenu("Grid")) {
        if (ImGui::MenuItem("Snap to Grid", nullptr, &snap_to_grid)) {
            ImNodesStyle& style = ImNodes::GetStyle();
            if (snap_to_grid) {
                style.Flags |= ImNodesStyleFlags_GridSnapping;
                // 使用当前缩放调整后的网格间距
                style.GridSpacing = original_style.grid_spacing * current_zoom;
            } else {
                style.Flags &= ~ImNodesStyleFlags_GridSnapping;
            }
        }
        
        if (ImGui::MenuItem("Show Grid Lines", nullptr, &show_grid_lines)) {
            ImNodesStyle& style = ImNodes::GetStyle();
            if (show_grid_lines) {
                style.Flags |= ImNodesStyleFlags_GridLines;
            } else {
                style.Flags &= ~ImNodesStyleFlags_GridLines;
            }
        }
        
        ImGui::Separator();
        
        // 网格大小预设 - 显示基础网格大小，不考虑缩放
        const float grid_sizes[] = {10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f};
        const char* grid_labels[] = {"10px", "15px", "20px", "25px", "30px", "40px", "50px"};
        
        ImGui::Text("Base Grid Size (before zoom):");
        for (int i = 0; i < 7; i++) {
            bool is_current = (std::abs(original_style.grid_spacing - grid_sizes[i]) < 0.1f);
            if (ImGui::MenuItem(grid_labels[i], nullptr, is_current)) {
                // 更新基础网格大小
                original_style.grid_spacing = grid_sizes[i];
                
                // 立即应用当前缩放
                apply_zoom();
                
                // 如果启用了网格吸附，显示提示
                if (snap_to_grid) {
                    add_info_popup_window(
                        "Grid Size Changed",
                        std::format("Base grid spacing set to {} pixels.\nCurrent display size: {:.1f} pixels ({}% zoom).", 
                                   (int)grid_sizes[i], 
                                   grid_sizes[i] * current_zoom,
                                   (int)(current_zoom * 100)),
                        ""
                    );
                }
            }
        }
        
        ImGui::Separator();
        
        // 显示当前实际网格大小
        ImGui::Text("Current Grid Size: %.1f px", original_style.grid_spacing * current_zoom);
        ImGui::Text("Zoom Level: %.0f%%", current_zoom * 100);
        
        ImGui::Separator();
        
        // 自定义网格大小
        if (ImGui::MenuItem("Custom Grid Size...")) {
            show_grid_settings_dialog = true;
        }
        
        ImGui::EndMenu();
    }
}

// 绘制缩放控制菜单
void App::draw_view_zoom_menu() {
    if (ImGui::BeginMenu("Zoom")) {
        // 显示当前缩放级别
        ImGui::Text("Current: %.0f%%", current_zoom * 100);
        ImGui::Separator();
        
        if (ImGui::MenuItem("Zoom In", "Ctrl++", false, current_zoom < max_zoom)) {
            zoom_in();
        }
        
        if (ImGui::MenuItem("Zoom Out", "Ctrl+-", false, current_zoom > min_zoom)) {
            zoom_out();
        }
        
        if (ImGui::MenuItem("Reset Zoom", "Ctrl+Shift+0")) {
            reset_zoom();
        }
        
        ImGui::Separator();
        
        // 适合窗口选项
        if (ImGui::MenuItem("Fit to Window", "Ctrl+F")) {
            // TODO: 计算合适的缩放级别以适应所有节点
            fit_to_window();
        }
        
        if (ImGui::MenuItem("Fit Selection", "Ctrl+Shift+F")) {
            // TODO: 适应选中的节点
            fit_selection();
        }
        
        ImGui::Separator();
        
        draw_view_zoom_presets();
        
        ImGui::EndMenu();
    }
}

// 绘制缩放预设选项
void App::draw_view_zoom_presets() {
    const std::vector<std::pair<std::string, float>> zoom_levels = {
        {"25%", 0.25f}, {"50%", 0.5f}, {"75%", 0.75f}, {"100%", 1.0f}, 
        {"125%", 1.25f}, {"150%", 1.5f}, {"200%", 2.0f}, {"300%", 3.0f}, {"500%", 5.0f}
    };
    
    for (const auto& [label, level] : zoom_levels) {
        bool is_current = (std::abs(current_zoom - level) < 0.01f);
        if (ImGui::MenuItem(label.c_str(), nullptr, is_current)) {
            set_zoom_level(level);
        }
    }
    
    ImGui::Separator();
    
    // 自定义缩放级别
    if (ImGui::MenuItem("Custom Zoom...")) {
        show_zoom_dialog = true;
    }
}




// 绘制主题控制菜单
void App::draw_view_theme_menu() {
    if (ImGui::BeginMenu("Theme")) {
        static int current_theme = 0; // 0=Dark, 1=Light, 2=Classic
        
        if (ImGui::RadioButton("Dark Theme", &current_theme, 0)) {
            ImGui::StyleColorsDark();
        }
        
        if (ImGui::RadioButton("Light Theme", &current_theme, 1)) {
            ImGui::StyleColorsLight();
        }
        
        if (ImGui::RadioButton("Classic Theme", &current_theme, 2)) {
            ImGui::StyleColorsClassic();
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Customize Theme...")) {
            add_info_popup_window(
                "Theme Customization",
                "Theme customization feature coming soon.",
                "You can manually edit theme settings in the configuration file."
            );
        }
        
        ImGui::EndMenu();
    }
}

// 绘制状态信息菜单
void App::draw_view_status_menu() {
    if (ImGui::BeginMenu("Status")) {
        draw_view_status_info();
        draw_view_preview_status();
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Show Performance Metrics", nullptr, &show_diagnostics)) {
            // 切换性能指标显示
        }
        
        ImGui::EndMenu();
    }
}

// 绘制基本状态信息
void App::draw_view_status_info() {
    std::string state_text = get_current_state_text();
    
    ImGui::Text("Current State: %s", state_text.c_str());
    ImGui::Text("Nodes: %zu", graph.nodes.size());
    ImGui::Text("Links: %zu", graph.links.size());
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
}

// 绘制预览状态信息
void App::draw_view_preview_status() {
    if (state == State::Previewing && runner) {
        ImGui::Separator();
        ImGui::Text("Preview Status:");
        
        auto& resources = runner->get_processor_resources();
        auto [running_count, finished_count, error_count] = count_processor_states(resources);
        
        ImGui::Text("  Running: %zu", running_count);
        ImGui::Text("  Finished: %zu", finished_count);
        ImGui::Text("  Errors: %zu", error_count);
    }
}

// 绘制窗口管理菜单
void App::draw_view_windows_menu() {
        if (ImGui::MenuItem("New Window")) {
            add_info_popup_window(
                "Multiple Windows",
                "Multiple window support is not yet implemented.",
                "This feature will allow you to open multiple project windows."
            );
        }
  
        if (ImGui::MenuItem("Minimize", "Ctrl+M")) {
            SDL_MinimizeWindow(sdl_context.get_window_ptr());
        }
}

// =============================================================================
/*辅助函数*/
// 辅助函数：获取当前状态文本
std::string App::get_current_state_text() const{
    switch (state.load()) {
        case State::Editing:
            return "Editing";
        case State::Previewing:
            return "Previewing";
        case State::Preview_requested:
            return "Starting Preview";
        case State::Preview_cancelling:
            return "Stopping Preview";
        default:
            return "Unknown";
    }
}

// 辅助函数：统计处理器状态数量
std::tuple<size_t, size_t, size_t> App::count_processor_states(
    const auto& resources) const {
    size_t running_count = 0;
    size_t finished_count = 0;
    size_t error_count = 0;
    
    for (const auto& [id, resource] : resources) {
        switch (resource->state) {
            case infra::Runner::State::Running:
                running_count++;
                break;
            case infra::Runner::State::Finished:
                finished_count++;
                break;
            case infra::Runner::State::Error:
                error_count++;
                break;
            default:
                break;
        }
    }
    
    return {running_count, finished_count, error_count};
}

// =============================================================================
/*视图功能*/

// 进入居中模式
void App::enter_center_view_mode() {
    center_view_mode = true;
    // 改变鼠标指针为十字准星
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// 退出居中模式
void App::exit_center_view_mode() {
    center_view_mode = false;
    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
}

// 处理居中模式的逻辑
void App::handle_center_view_mode() {
    if (!center_view_mode) return;
    
    // ESC键退出居中模式
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        exit_center_view_mode();
        return;
    }
    
    // 如果鼠标在节点编辑器区域内
    if (ImNodes::IsEditorHovered()) {
        // 改变鼠标指针为十字准星
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        
        // 左键点击时执行居中
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            center_view_at_cursor();
            exit_center_view_mode();
        }
    } else {
        // 鼠标不在编辑器区域时恢复默认指针
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    }
}

// 实现居中到光标位置的功能
void App::center_view_at_cursor() {
    // 获取当前鼠标在屏幕上的位置
    ImVec2 mouse_pos = ImGui::GetMousePos();
    
    // 如果鼠标在节点编辑器区域内
    if (ImNodes::IsEditorHovered()) {
        // 获取编辑器窗口大小
        ImVec2 canvas_size = ImGui::GetWindowSize();
        ImVec2 canvas_center = ImVec2(canvas_size.x * 0.5f, canvas_size.y * 0.5f);
        
        // 获取当前的平移值
        ImVec2 current_panning = ImNodes::EditorContextGetPanning();
        
        // 获取编辑器窗口位置
        ImVec2 editor_pos = ImGui::GetWindowPos();
        
        // 计算鼠标在编辑器内的相对位置
        ImVec2 mouse_in_editor = ImVec2(mouse_pos.x - editor_pos.x, mouse_pos.y - editor_pos.y);
        
        // 计算需要的平移，使鼠标位置移动到画布中心
        ImVec2 offset_to_center = ImVec2(
            canvas_center.x - mouse_in_editor.x,
            canvas_center.y - mouse_in_editor.y
        );
        
        // 应用新的平移
        ImVec2 new_panning = ImVec2(
            current_panning.x + offset_to_center.x,
            current_panning.y + offset_to_center.y
        );
        
        ImNodes::EditorContextResetPanning(new_panning);
    } else {
        // 如果鼠标不在编辑器内，显示错误信息
        add_info_popup_window(
            "Center View Failed",
            "Please click inside the node editor area.",
            ""
        );
    }
}

// =============================================================================
/*缩放功能*/

// 应用缩放到节点编辑器
void App::apply_zoom() {
    // 确保原始样式已初始化
    initialize_original_style();
    
    // 获取当前样式
    ImNodesStyle& style = ImNodes::GetStyle();
    
    // 基于原始值计算缩放后的值
    style.NodeCornerRounding = original_style.node_corner_rounding * current_zoom;
    style.NodePadding = ImVec2(
        original_style.node_padding.x * current_zoom, 
        original_style.node_padding.y * current_zoom
    );
    style.NodeBorderThickness = original_style.node_border_thickness * current_zoom;
    
    // 调整连线相关的尺寸  
    style.LinkThickness = original_style.link_thickness * current_zoom;
    style.LinkHoverDistance = original_style.link_hover_distance * current_zoom;
    
    // 调整引脚相关的尺寸
    style.PinCircleRadius = original_style.pin_circle_radius * current_zoom;
    style.PinQuadSideLength = original_style.pin_quad_side_length * current_zoom;
    style.PinTriangleSideLength = original_style.pin_triangle_side_length * current_zoom;
    style.PinLineThickness = original_style.pin_line_thickness * current_zoom;
    style.PinHoverRadius = original_style.pin_hover_radius * current_zoom;
    
    // 同步调整网格间距 - 这是关键！
    style.GridSpacing = original_style.grid_spacing * current_zoom;
    
    // 更新全局网格间距变量以保持一致性
    grid_spacing = original_style.grid_spacing * current_zoom;
}

// 放大
void App::zoom_in() {
    float new_zoom = current_zoom + zoom_step;
    if (new_zoom <= max_zoom) {
        // 获取当前鼠标在编辑器中的位置作为缩放中心
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 editor_pos = ImGui::GetWindowPos();
        ImVec2 zoom_center = ImVec2(mouse_pos.x - editor_pos.x, mouse_pos.y - editor_pos.y);
        
        // 如果鼠标不在编辑器内，使用编辑器中心作为缩放中心
        if (!ImNodes::IsEditorHovered()) {
            ImVec2 editor_size = ImGui::GetWindowSize();
            zoom_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
        }
        
        // 应用缩放并调整平移以保持缩放中心不变
        apply_zoom_with_center(new_zoom, zoom_center);
        
        add_info_popup_window(
            "Zoom In",
            std::format("Zoom level set to {:.0f}%", current_zoom * 100),
            ""
        );
    }
}

// 缩小
void App::zoom_out() {
    float new_zoom = current_zoom - zoom_step;
    if (new_zoom >= min_zoom) {
        // 获取当前鼠标在编辑器中的位置作为缩放中心
        ImVec2 mouse_pos = ImGui::GetMousePos();
        ImVec2 editor_pos = ImGui::GetWindowPos();
        ImVec2 zoom_center = ImVec2(mouse_pos.x - editor_pos.x, mouse_pos.y - editor_pos.y);
        
        // 如果鼠标不在编辑器内，使用编辑器中心作为缩放中心
        if (!ImNodes::IsEditorHovered()) {
            ImVec2 editor_size = ImGui::GetWindowSize();
            zoom_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
        }
        
        // 应用缩放并调整平移以保持缩放中心不变
        apply_zoom_with_center(new_zoom, zoom_center);
        
        add_info_popup_window(
            "Zoom Out", 
            std::format("Zoom level set to {:.0f}%", current_zoom * 100),
            ""
        );
    }
}

// 设置特定缩放级别
void App::set_zoom_level(float level) {
    if (level >= min_zoom && level <= max_zoom) {
        // 使用编辑器中心作为缩放中心
        ImVec2 editor_size = ImGui::GetWindowSize();
        ImVec2 zoom_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
        
        apply_zoom_with_center(level, zoom_center);
        
        add_info_popup_window(
            "Zoom Level Changed",
            std::format("Zoom level set to {:.0f}%", current_zoom * 100),
            ""
        );
    }
}

// 以指定点为中心应用缩放
void App::apply_zoom_with_center(float new_zoom, ImVec2 zoom_center) {
    // 获取当前平移
    ImVec2 current_panning = ImNodes::EditorContextGetPanning();
    
    // 计算缩放前在网格空间中的点
    ImVec2 grid_point_before = ImVec2(
        (zoom_center.x - current_panning.x) / current_zoom,
        (zoom_center.y - current_panning.y) / current_zoom
    );
    
    // 更新缩放级别
    current_zoom = new_zoom;
    
    // 应用新的样式缩放
    apply_zoom();
    
    // 计算新的平移以保持缩放中心不变
    ImVec2 new_panning = ImVec2(
        zoom_center.x - grid_point_before.x * current_zoom,
        zoom_center.y - grid_point_before.y * current_zoom
    );
    
    // 应用新的平移
    ImNodes::EditorContextResetPanning(new_panning);
}

// 重置缩放
void App::reset_zoom() {
    // 使用编辑器中心作为缩放中心
    ImVec2 editor_size = ImGui::GetWindowSize();
    ImVec2 zoom_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
    
    apply_zoom_with_center(1.0f, zoom_center);
    
    add_info_popup_window(
        "Reset Zoom",
        "Zoom level reset to 100%",
        ""
    );
}

// 适合窗口 - 缩放以显示所有节点
void App::fit_to_window() {
    if (graph.nodes.empty()) {
        add_info_popup_window(
            "Fit to Window",
            "No nodes to fit in the window.",
            ""
        );
        return;
    }
    
    // 计算所有节点的边界框（使用网格空间坐标）
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    
    for (const auto& [id, node] : graph.nodes) {
        // 使用节点的网格坐标而不是屏幕坐标
        ImVec2 node_pos = ImNodes::GetNodeGridSpacePos(id);
        
        // 估算节点大小（如果无法获取准确大小）
        const float estimated_node_width = 200.0f;
        const float estimated_node_height = 100.0f;
        
        min_x = std::min(min_x, node_pos.x);
        min_y = std::min(min_y, node_pos.y);
        max_x = std::max(max_x, node_pos.x + estimated_node_width);
        max_y = std::max(max_y, node_pos.y + estimated_node_height);
    }
    
    // 添加一些边距
    const float padding = 100.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;
    
    // 计算边界框尺寸
    float content_width = max_x - min_x;
    float content_height = max_y - min_y;
    
    // 获取编辑器窗口尺寸
    ImVec2 editor_size = ImGui::GetWindowSize();
    
    // 计算缩放比例（保持原始缩放，不修改样式）
    float zoom_x = editor_size.x * 0.8f / content_width;  // 留20%边距
    float zoom_y = editor_size.y * 0.8f / content_height;
    float zoom_factor = std::min(zoom_x, zoom_y);
    
    // 计算内容中心
    ImVec2 content_center = ImVec2((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f);
    
    // 计算编辑器中心
    ImVec2 editor_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
    
    // 计算新的平移值来居中内容
    ImVec2 new_panning = ImVec2(
        editor_center.x - content_center.x,
        editor_center.y - content_center.y
    );
    
    // 应用平移
    ImNodes::EditorContextResetPanning(new_panning);
    
    // 如果需要缩放，可以考虑调整缩放级别
    // 但目前先只处理平移，避免样式问题
    
    add_info_popup_window(
        "Fit to Window",
        "View has been adjusted to show all nodes.",
        std::format("Showing {} nodes", graph.nodes.size())
    );
}

// 适合选中内容
void App::fit_selection() {
    const auto selected_node_count = ImNodes::NumSelectedNodes();
    if (selected_node_count == 0) {
        add_info_popup_window(
            "Fit Selection",
            "No nodes selected to fit.",
            ""
        );
        return;
    }
    
    std::vector<infra::Id_t> selected_nodes(selected_node_count);
    ImNodes::GetSelectedNodes(selected_nodes.data());
    
    // 计算选中节点的边界框（使用网格空间坐标）
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_y = std::numeric_limits<float>::lowest();
    
    for (auto node_id : selected_nodes) {
        if (graph.nodes.contains(node_id)) {
            // 使用网格坐标
            ImVec2 node_pos = ImNodes::GetNodeGridSpacePos(node_id);
            
            // 估算节点大小
            const float estimated_node_width = 200.0f;
            const float estimated_node_height = 100.0f;
            
            min_x = std::min(min_x, node_pos.x);
            min_y = std::min(min_y, node_pos.y);
            max_x = std::max(max_x, node_pos.x + estimated_node_width);
            max_y = std::max(max_y, node_pos.y + estimated_node_height);
        }
    }
    
    // 添加一些边距
    const float padding = 50.0f;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;
    
    // 计算内容中心
    ImVec2 content_center = ImVec2((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f);
    
    // 获取编辑器窗口尺寸和中心
    ImVec2 editor_size = ImGui::GetWindowSize();
    ImVec2 editor_center = ImVec2(editor_size.x * 0.5f, editor_size.y * 0.5f);
    
    // 计算新的平移值来居中选中内容
    ImVec2 new_panning = ImVec2(
        editor_center.x - content_center.x,
        editor_center.y - content_center.y
    );
    
    // 应用平移
    ImNodes::EditorContextResetPanning(new_panning);
    
    add_info_popup_window(
        "Fit Selection",
        std::format("View has been adjusted to show {} selected node{}.", 
                   selected_node_count, selected_node_count > 1 ? "s" : ""),
        ""
    );
}

// =============================================================================
/*性能窗口*/

// 绘制悬浮性能指标覆盖层
void App::draw_performance_overlay() {
    if (!show_diagnostics) return;
    
    // 设置覆盖层默认位置（左下角）
    const auto [display_size_x, display_size_y] = ImGui::GetIO().DisplaySize;
    const float overlay_width = 280 * runtime_config::ui_scale;
    const float overlay_height = 200 * runtime_config::ui_scale; // 预估高度
    const float overlay_margin = 10 * runtime_config::ui_scale;
    
    // 左下角位置
    const float overlay_pos_x = overlay_margin;
    const float overlay_pos_y = display_size_y - overlay_height - overlay_margin;
    
    // 只在第一次使用时设置位置，之后允许用户自由移动
    ImGui::SetNextWindowPos(ImVec2(overlay_pos_x,overlay_pos_y), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(overlay_width, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.35f); // 半透明背景
    
    // 创建可移动的覆盖层窗口
    const ImGuiWindowFlags overlay_flags = 
        ImGuiWindowFlags_NoDecoration |           // 无标题栏、边框等
        ImGuiWindowFlags_AlwaysAutoResize |       // 自动调整大小
        ImGuiWindowFlags_NoSavedSettings |        // 不保存设置
        ImGuiWindowFlags_NoFocusOnAppearing |     // 出现时不获取焦点
        ImGuiWindowFlags_NoNav;                   // 禁用导航
    
    // 静态变量控制锁定状态
    static bool is_locked = false;
    
    // 如果锁定，添加 NoMove
    ImGuiWindowFlags current_flags = overlay_flags;
    if (is_locked) {
        current_flags |= ImGuiWindowFlags_NoMove;
    }
    
    if (ImGui::Begin("##PerformanceOverlay", nullptr, current_flags)) {
        // 样式设置
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.9f)); // 白色文字，稍微透明
        
        // 标题栏（显示拖拽提示和控制按钮）
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 1.0f, 0.7f, 1.0f));
        if (is_locked) {
            ImGui::Text("PERFORMANCE MONITOR");
        } else {
            ImGui::Text("PERFORMANCE MONITOR (Drag to move)");
        }
        ImGui::PopStyleColor();
        
        // 控制按钮行
        if (!is_locked) {
            if (ImGui::SmallButton("Lock")) {
                is_locked = true;
            }
            ImGui::SameLine();
        } else {
            if (ImGui::SmallButton("Unlock")) {
                is_locked = false;
            }
            ImGui::SameLine();
        }
        
        if (ImGui::SmallButton("Close")) {
            show_diagnostics = false;
        }
        
        ImGui::Separator();
        
        // 基本性能指标
        const ImGuiIO& io = ImGui::GetIO();
        
        // FPS - 根据性能用不同颜色显示
        ImVec4 fps_color = io.Framerate >= 60.0f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :  // 绿色
                          io.Framerate >= 30.0f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :  // 黄色
                                                  ImVec4(1.0f, 0.0f, 0.0f, 1.0f);   // 红色
        
        ImGui::TextColored(fps_color, "FPS: %.1f", io.Framerate);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "(%.2fms)", 1000.0f / io.Framerate);
        
        // 内存使用（简化版）
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            float memory_mb = pmc.WorkingSetSize / (1024.0f * 1024.0f);
            ImVec4 memory_color = memory_mb < 100.0f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                                 memory_mb < 200.0f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :
                                                     ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
            ImGui::TextColored(memory_color, "RAM: %.1fMB", memory_mb);
        }
        #elif defined(__linux__)
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string dummy;
                int memory_kb;
                iss >> dummy >> memory_kb;
                float memory_mb = memory_kb / 1024.0f;
                ImVec4 memory_color = memory_mb < 100.0f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                                     memory_mb < 200.0f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) :
                                                         ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                ImGui::TextColored(memory_color, "RAM: %.1fMB", memory_mb);
                break;
            }
        }
        #endif
        
        // 分隔线
        ImGui::Separator();
        
        // 图形统计
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "GRAPH");
        ImGui::Text("Nodes: %zu", graph.nodes.size());
        ImGui::Text("Links: %zu", graph.links.size());
        
        // 选中状态
        const int selected_nodes = ImNodes::NumSelectedNodes();
        const int selected_links = ImNodes::NumSelectedLinks();
        if (selected_nodes > 0 || selected_links > 0) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Selected: %dN %dL", selected_nodes, selected_links);
        }
        
        // 编辑器状态
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "STATE");
        
        // 状态用不同颜色显示
        std::string state_text = get_current_state_text();
        ImVec4 state_color = state == State::Editing ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                            state == State::Previewing ? ImVec4(0.0f, 0.8f, 1.0f, 1.0f) :
                                                        ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
        ImGui::TextColored(state_color, "%s", state_text.c_str());
        
        ImGui::Text("Zoom: %.0f%%", current_zoom * 100);
        
        // 撤销/重做状态
        ImGui::Text("Undo: %zu | Redo: %zu", undo_stack.size(), redo_stack.size());
        
        // 如果在预览状态，显示处理器统计
        if (state == State::Previewing && runner) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "AUDIO");
            
            auto& processor_resources = runner->get_processor_resources();
            auto [running_count, finished_count, error_count] = count_processor_states(processor_resources);
            
            if (error_count > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Errors: %zu", error_count);
            }
            if (running_count > 0) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "Running: %zu", running_count);
            }
            if (finished_count > 0) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Finished: %zu", finished_count);
            }
            
            // 显示音频链路状态（简化版）
            auto& link_products = runner->get_link_products();
            if (!link_products.empty()) {
                size_t audio_links = 0;
                for (const auto& [id, link] : link_products) {
                    try {
                        const auto& product = dynamic_cast<const processor::Audio_stream&>(*link);
                        audio_links++;
                        float fill_ratio = (float)product.buffered_count() / processor::max_queue_size;
                        
                        // 简化的缓冲区状态显示
                        ImVec4 buffer_color = fill_ratio > 0.8f ? ImVec4(1,0,0,1) : 
                                             fill_ratio > 0.6f ? ImVec4(1,1,0,1) : 
                                             ImVec4(0,1,0,1);
                        
                        ImGui::TextColored(buffer_color, "L%d: %.0f%%", id, fill_ratio * 100.0f);
                        if (audio_links % 2 == 1 && audio_links < link_products.size()) ImGui::SameLine();
                    }
                    catch (const std::bad_cast&) {
                        // 忽略非音频链路
                    }
                }
            }
        }
        
        // 显示窗口位置信息（仅在解锁状态）
        if (!is_locked) {
            ImGui::Separator();
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Pos: (%.0f, %.0f)", window_pos.x, window_pos.y);
        }
        
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
    ImGui::End();
    
    // 快捷键切换锁定状态（Ctrl+Shift+P）
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_P)) {
        is_locked = !is_locked;
        
        add_info_popup_window(
            "Performance Monitor",
            is_locked ? "Performance monitor locked in place." : "Performance monitor unlocked. You can now drag to move it.",
            ""
        );
    }
}


// =============================================================================
// NODE EDITOR AND RENDERING
// =============================================================================
/*节点编辑器和渲染*/

// 绘制节点编辑器
void App::draw_node_editor()
{
	ImNodes::BeginNodeEditor();
	{
		for (auto& [i, node] : graph.nodes)
		{
			draw_node(i, node);
			node.position = ImNodes::GetNodeGridSpacePos(i);
		}

		for (auto [i, link] : graph.links) ImNodes::Link(i, link.from, link.to);

        if(show_minimap){
			ImNodes::MiniMap(config::appearance::node_editor_minimap_fraction, ImNodesMiniMapLocation_TopRight);
		}

        // 处理居中模式
        handle_center_view_mode();
		if (state == State::Editing)
		{
			// 编辑状态下，右键唤起右键菜单
			if (ImNodes::IsEditorHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
				&& node_editor_context_menu_state == Node_editor_context_menu_state::Closed)
				node_editor_context_menu_state = Node_editor_context_menu_state::Open_requested;
		}
		else
		{
			// 非编辑状态下不允许选中
			ImNodes::ClearNodeSelection();
			ImNodes::ClearLinkSelection();
		}
	}
	ImNodes::EndNodeEditor();

	//保存节点位置
	for(auto& [id, item] : graph.nodes)
	{
		item.position = ImNodes::GetNodeScreenSpacePos(id);
	}

	// 直接右键节点/连结选中并打开菜单
	if (state == State::Editing && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		infra::Id_t hovered_node;
		if (ImNodes::IsNodeHovered(&hovered_node))
		{
			ImNodes::ClearNodeSelection();
			ImNodes::ClearLinkSelection();
			ImNodes::SelectNode(hovered_node);
		}

		infra::Id_t hovered_link;
		if (ImNodes::IsLinkHovered(&hovered_link))
		{
			ImNodes::ClearNodeSelection();
			ImNodes::ClearLinkSelection();
			ImNodes::SelectLink(hovered_link);
		}

		node_editor_context_menu_state = Node_editor_context_menu_state::Open_requested;
	}

	// 右键菜单被关闭时，清除选中节点和连结
	if (state == State::Editing && node_editor_context_menu_state == Node_editor_context_menu_state::Opened
		&& !ImGui::IsPopupOpen("node-editor-context-menu", ImGuiPopupFlags_AnyPopupLevel))
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
		node_editor_context_menu_state = Node_editor_context_menu_state::Closed;
	}

	if (node_editor_context_menu_state == Node_editor_context_menu_state::Open_requested)
	{
		ImGui::OpenPopup("node-editor-context-menu");
		node_editor_context_menu_state = Node_editor_context_menu_state::Opened;
	}

	draw_node_editor_context_menu();
	handle_node_actions();
}

// 绘制单个节点
void App::draw_node(infra::Id_t id, const infra::Graph::Node& node)
{
	ImNodes::BeginNode(id);
	{
		// 绘制标题栏
		ImNodes::BeginNodeTitleBar();
		node.processor->draw_title();
		ImNodes::EndNodeTitleBar();

		// 绘制节点本体
		//if (node.processor->draw_content(false) && state == State::Editing) graph.update_node_pin(id);

		// 绘制节点输入输出端口
		ImGui::NewLine();
		ImGui::BeginGroup();
		for (const auto& pin_id : node.pins)  // 输入端口
		{
			const auto& pin = graph.pins.at(pin_id);

			if (pin.attribute.is_input)
			{
				ImNodes::BeginInputAttribute(pin_id);
				ImGui::Text("%s", pin.attribute.display_name.c_str());
				ImNodes::EndInputAttribute();
			}
		}
		for (const auto& pin_id : node.pins)  // 输出端口
		{
			const auto& pin = graph.pins.at(pin_id);

			if (!pin.attribute.is_input)
			{
				ImNodes::BeginOutputAttribute(pin_id);
				ImGui::Indent(60), ImGui::Text("%s", pin.attribute.display_name.c_str());
				ImNodes::EndOutputAttribute();
			}
		}
		ImGui::EndGroup();
	}
	ImNodes::EndNode();
}

// 绘制添加节点菜单
void App::draw_add_node_menu()
{
	for (const auto& item : infra::Processor::processor_map)
	{
		const auto& info = item.second;

		// 判断单例处理器
		const bool singleton_and_exists
			= info.singleton && graph.singleton_node_map.contains(info.identifier);

		if (ImGui::MenuItem(info.display_name.c_str(), nullptr, singleton_and_exists, !singleton_and_exists))
		{
			save_undo_state();
			const auto new_id = graph.add_node(item.second.generate());
			ImNodes::SetNodeScreenSpacePos(new_id, ImGui::GetMousePos());
		}
	}
}

// 绘制节点编辑器右键菜单
void App::draw_node_editor_context_menu()
{
	if (ImGui::BeginPopup("node-editor-context-menu"))
	{
		// 删除按钮
		if (const auto selected_nodes = ImNodes::NumSelectedNodes(),
			selected_links = ImNodes::NumSelectedLinks();
			selected_nodes > 0 || selected_links > 0)
		{
			std::string number_description_text;
			if (selected_nodes > 0)
			{
				if (selected_links > 0)
					number_description_text = std::format(
						"{} Node{}, {} Link{}",
						selected_nodes,
						*&"s" + (selected_nodes == 1),
						selected_links,
						*&"s" + (selected_links == 1)
					);
				else
					number_description_text
						= std::format("{} Node{}", selected_nodes, *&"s" + (selected_nodes == 1));
			}
			else
			{
				number_description_text
					= std::format("{} Link{}", selected_links, *&"s" + (selected_links == 1));
			}

			if (ImGui::MenuItem("Delete", number_description_text.c_str())) {
				save_undo_state();
				remove_selected_nodes();
			}
		}

        // 复制按钮
        if (const auto selected_nodes = ImNodes::NumSelectedNodes();
            selected_nodes > 0)
        {
            std::string copy_description = std::format("{} Node{}", selected_nodes, selected_nodes > 1 ? "s" : "");
            if (ImGui::MenuItem("Copy", copy_description.c_str()))
            {
                copy_selected_nodes();
            }
        }
        
        // 粘贴按钮
        if (!copied_graph_json.empty())
        {
            if (ImGui::MenuItem("Paste", "Ctrl+V"))
            {
                paste_nodes();
            }
        }
        else
        {
            // 如果剪贴板为空，显示禁用的粘贴选项
            ImGui::BeginDisabled();
            ImGui::MenuItem("Paste", "No content to paste");
            ImGui::EndDisabled();
        }
        
        // 分隔符（如果有复制粘贴选项的话）
        if (ImNodes::NumSelectedNodes() > 0 || !copied_graph_json.empty())
        {
            ImGui::Separator();
        }

		// 新增节点的菜单
		if (ImGui::BeginMenu("Add"))
		{
			draw_add_node_menu();
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
}

// 删除选中节点和连线
void App::remove_selected_nodes()
{
	//save_undo_state();
	const auto selected_node_count = ImNodes::NumSelectedNodes();
	if (selected_node_count != 0)
	{
		std::vector<infra::Id_t> selected_nodes(selected_node_count);
		ImNodes::GetSelectedNodes(selected_nodes.data());

        // 检查是否删除了当前在侧边栏显示的节点
        bool selected_node_will_be_deleted = false;
        for (auto node_id : selected_nodes)
        {
            if (node_id == selected_node_id)
            {
                selected_node_will_be_deleted = true;
                break;
            }
        }

		for (auto i : selected_nodes
						  | std::views::filter([this](auto i) { return i >= 0 && graph.nodes.contains(i); }))
			graph.remove_node(i);
        // 如果删除了侧边栏显示的节点，隐藏侧边栏
        if (selected_node_will_be_deleted)
        {
            show_side_panel = false;
            selected_node_id = -1;
        }
	}

	const auto selected_link_count = ImNodes::NumSelectedLinks();
	if (selected_link_count != 0)
	{
		std::vector<infra::Id_t> selected_links(selected_link_count);
		ImNodes::GetSelectedLinks(selected_links.data());

		for (auto i : selected_links
						  | std::views::filter([this](auto i) { return i >= 0 && graph.links.contains(i); }))
			graph.remove_link(i);
	}
}

// 处理节点编辑器的操作
void App::handle_node_actions()
{
    // 只能在编辑模式下改变图
    if (state != State::Editing) return;

        // 检测节点点击
    infra::Id_t clicked_node = -1;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (ImNodes::IsNodeHovered(&clicked_node))
        {
            // 点击了节点，显示侧边栏
            show_side_panel = true;
            selected_node_id = clicked_node;
            
            // 选中这个节点
            ImNodes::ClearNodeSelection();
            ImNodes::SelectNode(clicked_node);
        }
        else if (ImNodes::IsEditorHovered())
        {
            // 点击了空白区域，隐藏侧边栏
            show_side_panel = false;
            selected_node_id = -1;
            
            // 清除选择
            ImNodes::ClearNodeSelection();
            ImNodes::ClearLinkSelection();
        }
    }

    // 检查当前选中的节点是否还存在
    if (show_side_panel && selected_node_id != -1)
    {
        if (!graph.nodes.contains(selected_node_id))
        {
            // 选中的节点已被删除，隐藏侧边栏
            show_side_panel = false;
            selected_node_id = -1;
        }
    }

    int start, end;
    bool from_snap;
    if (ImNodes::IsLinkCreated(&start, &end, &from_snap))
    {
        save_undo_state();
        try
        {
            graph.add_link(start, end);
            graph.check_graph();
        }
        catch (const std::runtime_error& e)
        {
            graph.remove_link(start, end);
        }
    }

	    // 键盘快捷键处理
    const ImGuiIO& io = ImGui::GetIO();

    // Ctrl+0 进入居中模式
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_0))
    {
        enter_center_view_mode();
    }
    
    // Ctrl+C 复制
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
    {
        copy_selected_nodes();
    }
    
    // Ctrl+V 粘贴
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
    {
        paste_nodes();
    }
    
    // Ctrl+Z 撤销
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift)
    {
        undo();
    }
    
    // Ctrl+Y 或 Ctrl+Shift+Z 重做
    if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) || 
        (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)))
    {
        redo();
    }

        // Ctrl++ 或 Ctrl+= 放大
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)))
    {
        zoom_in();
    }
    
    // Ctrl+- 缩小
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)))
    {
        zoom_out();
    }
    
    // Ctrl+F 适合窗口
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F))
    {
        fit_to_window();
    }
    
    // Ctrl+Shift+F 适合选中内容
    if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F))
    {
        fit_selection();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    {
        save_undo_state();
        remove_selected_nodes();
        ImNodes::ClearNodeSelection();
    }
}


// =============================================================================
// STATE MANAGEMENT AND PREVIEW
// =============================================================================
// 主程序状态轮询
void App::poll_state()
{
	switch (state.load())
	{
	case State::Editing:
		break;

	case State::Preview_requested:
	{
		if (runner != nullptr)
			THROW_LOGIC_ERROR("Unexpected state: Preview_requested when runner is running");

		create_preview_runner();
		break;
	}

	case State::Previewing:
	{
		if (runner == nullptr) THROW_LOGIC_ERROR("Unexpected state: Preview when runner is not running");

		auto& processor_resources = runner->get_processor_resources();
		size_t finished_count = 0;

		for (auto& [_, resource] : processor_resources)
		{
			if (resource->state == infra::Runner::State::Error)
			{
				show_preview_runner_error(resource->exception);
				runner.reset();
				state = State::Editing;
				break;
			}

			finished_count += resource->state == infra::Runner::State::Finished ? 1 : 0;
		}

		if (finished_count == processor_resources.size())
		{
			runner.reset();
			state = State::Editing;
		}

		break;
	}

	case State::Preview_cancelling:
	{
		if (runner == nullptr)
			THROW_LOGIC_ERROR("Unexpected state: Preview_cancelling when runner is not running");

		runner.reset();
		state = State::Editing;
		break;
	}
	}
}

// 创建预览运行器
void App::create_preview_runner()
{
	if (graph.nodes.empty())
	{
		add_error_popup_window(
			"Failed to launch preview",
			"There's no node in the graph. Add some nodes to start previewing."
		);
		state = State::Editing;
		return;
	}

	std::map<infra::Id_t, std::shared_ptr<std::any>> node_data;

	for (auto& [idx, node] : graph.nodes)
	{
		const auto processor_info = node.processor->get_processor_info_non_static();

		if (processor_info.identifier == config::logic::audio_output_node_name)
			node_data[idx] = std::make_shared<std::any>(
				processor::Audio_output::Process_context{.audio_device = sdl_context.get_audio_device()}
			);
	}

	try
	{
		runner = infra::Runner::create_and_run(graph, std::move(node_data));
		state = State::Previewing;
	}
	catch (const std::runtime_error& e)
	{
		add_error_popup_window(
			"Failed to launch preview",
			"Error occured during preview launching, see detail for more info",
			e.what()
		);
		state = State::Editing;
	}
}

// 显示预览运行器错误
void App::show_preview_runner_error(const std::any& error)
{
	const auto runtime_error = try_anycast<infra::Processor::Runtime_error>(error);
	if (runtime_error.has_value())
	{
		add_error_popup_window(
			std::format("Runtime error raised during preview: {}", runtime_error->message),
			runtime_error->explanation,
			runtime_error->detail
		);
		return;
	}

	const auto logic_error = try_anycast<std::logic_error>(error);
	if (logic_error.has_value())
	{
		add_error_popup_window(
			"Unexpected logic error during preview",
			"An internal logic error occured. It's most likely to be a bug",
			logic_error.value().what()
		);
		return;
	}

	add_error_popup_window("Unknown exception raised during preview");
}


// 初始化并保存 ImNodes 的原始样式值，用于后续缩放功能的基准参考
void App::initialize_original_style() {
    if (original_style.initialized) return;
    
    ImNodesStyle& style = ImNodes::GetStyle();
    
    original_style.node_corner_rounding = style.NodeCornerRounding;
    original_style.node_padding = style.NodePadding;
    original_style.node_border_thickness = style.NodeBorderThickness;
    original_style.link_thickness = style.LinkThickness;
    original_style.link_hover_distance = style.LinkHoverDistance;
    original_style.pin_circle_radius = style.PinCircleRadius;
    original_style.pin_quad_side_length = style.PinQuadSideLength;
    original_style.pin_triangle_side_length = style.PinTriangleSideLength;
    original_style.pin_line_thickness = style.PinLineThickness;
    original_style.pin_hover_radius = style.PinHoverRadius;
    original_style.grid_spacing = grid_spacing;
    original_style.initialized = true;
}

// 退出确认弹窗
void App::add_exit_confirm_window()
{
    class Exit_confirm_window
    {
        App* app_ptr;
    public:
        Exit_confirm_window(App* app) : app_ptr(app) {}
        bool operator()(bool close_button_pressed)
        {
            ImGui::Text("You have unsaved changes. Do you want to save before exiting?");
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Separator();

            if (ImGui::Button("Save and Exit", {120 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                app_ptr->should_exit_after_save = true;
                app_ptr->show_save_dialog();
                return true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Exit Without Saving", {160 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
            {
                // 直接退出
                SDL_Event quit_event;
                quit_event.type = SDL_QUIT;
                SDL_PushEvent(&quit_event);
                return true;
            }
            ImGui::SameLine();
            return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed;
        }
    };

    popup_manager.open_window(
        {.title = ICON_WARNING " Unsaved Changes",
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Exit_confirm_window(this)}
    );
}

bool App::is_project_modified()const{
    std::string current_json = save_graph_as_string();
    // 如果 last_saved_graph_json 为空（程序刚启动），
    // 则检查当前图是否为空图
    if (last_saved_graph_json.empty()) {
        // 创建一个空图并序列化，用于比较
        infra::Graph empty_graph;
        const auto empty_json = empty_graph.serialize();
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  ";
        std::string empty_graph_json = Json::writeString(writer, empty_json);
        
        return current_json != empty_graph_json;
    }
    
    return current_json != last_saved_graph_json;
}

// 添加新的带退出回调的保存成功弹窗方法
void App::add_save_success_popup_with_exit(const std::string& detail)
{
    class Save_success_exit_window
    {
        std::string detail;
        App* app_ptr;
        
    public:
        Save_success_exit_window(const std::string& detail, App* app) 
            : detail(detail), app_ptr(app) {}
        
        bool operator()(bool close_button_pressed)
        {
            ImGui::Text("Save Successful");
            ImGui::Dummy({1, 5 * runtime_config::ui_scale});
            
            if (!detail.empty()) {
                ImGui::SeparatorText("Details");
                ImGui::BeginChild(
                    "Details",
                    {300 * runtime_config::ui_scale, 80 * runtime_config::ui_scale},
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize
                );
                ImGui::TextWrapped("Project has been saved successfully.");
                ImGui::TextWrapped("%s", detail.c_str());
                ImGui::EndChild();
            }
            
            ImGui::Dummy({1, 10 * runtime_config::ui_scale});
            ImGui::Separator();
            
            // 当用户点击OK或关闭按钮时，执行退出
            if (ImGui::Button("OK", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}) || close_button_pressed)
            {
                // 弹窗关闭后执行退出
                SDL_Event quit_event;
                quit_event.type = SDL_QUIT;
                SDL_PushEvent(&quit_event);
                return true;
            }
            
            return false;
        }
    };
    
    popup_manager.open_window(
        {.title = "Save Successful",
         .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
         .has_close_button = true,
         .keep_centered = true,
         .render_func = Save_success_exit_window(detail, this)}
    );
}

void App::on_settings_applied() {
    // 设置应用后的处理
    auto& settings = app_settings::get_app_settings();
    
    // 更新UI状态
    show_toolbar = settings.ui.show_toolbar;
    show_side_panel = settings.ui.show_side_panel;
    show_minimap = settings.ui.show_minimap;
    show_grid_lines = settings.ui.show_grid;
    snap_to_grid = settings.ui.snap_to_grid;
    grid_spacing = settings.ui.grid_size;
    
    // 更新调试状态
    show_diagnostics = settings.debug.show_performance_overlay;
    
    // 应用缩放
    current_zoom = 1.0f; // 重置缩放然后应用UI缩放
    apply_zoom();
    
    // 重新初始化音频系统（如果音频设置有变化）
    // reinitialize_audio_system();
}