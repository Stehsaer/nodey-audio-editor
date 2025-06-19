#include "frontend/app.hpp"
#include "frontend/nerdfont.hpp"
#include "imnodes.h"

#include "processor/audio-io.hpp"

#include "utility/anycast-utility.hpp"
#include "utility/dialog-utility.hpp"
#include "utility/imgui-utility.hpp"
#include "utility/system.hpp"

#include <imgui_stdlib.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <utility>

void App::draw()
{
	draw_menubar();

	if (delayed_selection_node.has_value())
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
		ImNodes::SelectNode(*delayed_selection_node);
		delayed_selection_node.reset();

		open_node_context_menu_tries = 5;
	}
	else if (delayed_selection_link.has_value())
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
		ImNodes::SelectLink(*delayed_selection_link);
		delayed_selection_link.reset();

		open_node_context_menu_tries = 5;
	}

	// 因为未知原因，需要反复尝试打开菜单，5次应该足够了
	if (open_node_context_menu_tries > 0)
	{
		ImGui::OpenPopup("node-editor-context-menu");
		open_node_context_menu_tries--;
	}

	draw_node_editor_context_menu();

	const auto side_panel_width_pixel = app_settings.ui.side_panel_width * runtime_config::ui_scale;
	const auto [display_size_x, display_size_y] = ImGui::GetIO().DisplaySize;
	const bool show_side_panel = ImNodes::NumSelectedNodes() == 1;

	// 计算主面板宽度
	float main_panel_width = show_side_panel ? display_size_x - side_panel_width_pixel : display_size_x;

	// 侧面板
	if (show_side_panel)
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
	if (app_settings.ui.show_toolbar)
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

	if (show_diagnostics) draw_diagnostics_overlay();
	if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

	handle_keyboard_shortcuts();
}

// =============================================================================
/* 菜单栏绘制 */

// 主菜单绘制
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
	ImGui::BeginDisabled(state != State::Editing);
	{
		if (ImGui::MenuItem("New", "Ctrl+N")) new_project_async();
		if (ImGui::MenuItem("Open", "Ctrl+O")) open_project();
		if (ImGui::MenuItem("Save", "Ctrl+S")) save_project();
		ImGui::Separator();

		if (ImGui::MenuItem("Exit", "Ctrl+Q"))
		{
			// 判断是否有未保存更改
			if (graph.modified)
			{
				add_exit_confirm_window();
			}
			else
			{
				SDL_Event quit_event;
				quit_event.type = SDL_QUIT;
				SDL_PushEvent(&quit_event);
			}
		}
	}
	ImGui::EndDisabled();
}

// 绘制主菜单栏的编辑(EDIT)选项
void App::draw_menubar_edit()
{
	ImGui::BeginDisabled(state != State::Editing);
	{
		if (ImGui::MenuItem("Select All", "Ctrl+A"))
		{
			ImNodes::ClearNodeSelection();
			ImNodes::ClearLinkSelection();
			for (const auto& [id, _] : graph.nodes) ImNodes::SelectNode(id);
			for (const auto& [id, _] : graph.links) ImNodes::SelectLink(id);
		}

		if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !undo_stack.empty())) undo();
		if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !redo_stack.empty())) redo();

		if (ImGui::MenuItem(
				"Remove Selected",
				"Del",
				false,
				ImNodes::NumSelectedNodes() + ImNodes::NumSelectedLinks() > 0
			))
		{
			save_undo_state();
			remove_selected_nodes();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Settings")) popup_manager.open_window(Settings_window::create(app_settings));
	}
	ImGui::EndDisabled();
}

// 绘制主菜单栏的视图(VIEW)选项
void App::draw_menubar_view()
{
	draw_view_panels_menu();
	ImGui::Separator();
	draw_view_center_menu();
}

// 绘制主菜单栏的帮助(HELP)选项
void App::draw_menubar_help()
{
	if (ImGui::MenuItem("View Documentation")) popup_manager.open_window(help_documentation_window());
	if (ImGui::MenuItem("Visit Source Page")) open_url(config::app::source_page);
	if (ImGui::MenuItem("About")) popup_manager.open_window(about_window());
}

//==============================================================================
/* UI面板绘制 */

// 绘制侧边栏
void App::draw_side_panel()
{
	// 如果有选中的节点，显示节点信息
	if (ImNodes::NumSelectedNodes() == 1)
	{
		infra::Id_t selected_node_id;
		ImNodes::GetSelectedNodes(&selected_node_id);

		if (!graph.nodes.contains(selected_node_id))
			THROW_LOGIC_ERROR("Selected node ID {} does not exist in the graph.", selected_node_id);

		const auto& selected_node = graph.nodes.at(selected_node_id);
		ImGui::SeparatorText(selected_node.processor->get_processor_info_non_static().display_name.c_str());
        imgui_utility::display_processor_description(selected_node.processor->get_processor_info_non_static().description, false);
		if (selected_node.processor->draw_content(state != State::Editing))
			graph.update_node_pin(selected_node_id);
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

	ImGui::BeginDisabled(state != State::Editing && state != State::Previewing);
	if (state == State::Editing)
	{
		if (ImGui::Button(ICON_PLAY "##toolbar-play", {area_width, area_width}))
		{
			if (runner == nullptr)
				state = State::Preview_requested;
			else
				THROW_LOGIC_ERROR("Cannot start preview, runner is already running.");
		}

		if (ImGui::BeginItemTooltip()) ImGui::Text("Start Preview"), ImGui::EndTooltip();
	}
	else if (state == State::Previewing)
	{
		if (ImGui::Button(ICON_STOP "##toolbar-stop", {area_width, area_width}))
		{
			if (runner)
				state = State::Preview_cancelling;
			else
				THROW_LOGIC_ERROR("Cannot stop preview, runner is not running.");
		}

		if (ImGui::BeginItemTooltip()) ImGui::Text("Stop Preview"), ImGui::EndTooltip();
	}
	ImGui::EndDisabled();

	ImGui::Separator();

	// 撤销按钮
	ImGui::BeginDisabled(undo_stack.empty() || state != State::Editing);
	if (ImGui::Button(ICON_UNDO "##toolbar-undo", {area_width, area_width})) undo();
	if (ImGui::BeginItemTooltip()) ImGui::Text("Undo Action"), ImGui::EndTooltip();
	ImGui::EndDisabled();

	// 重做按钮
	ImGui::BeginDisabled(redo_stack.empty() || state != State::Editing);
	if (ImGui::Button(ICON_REDO "##toolbar-redo", {area_width, area_width})) redo();
	if (ImGui::BeginItemTooltip()) ImGui::Text("Redo Action"), ImGui::EndTooltip();
	ImGui::EndDisabled();

	ImGui::Separator();

	// 复制按钮
	ImGui::BeginDisabled((ImNodes::NumSelectedNodes() == 0) || state != State::Editing);
	if (ImGui::Button(ICON_COPY "##toolbar-copy", {area_width, area_width})) copy_selected_nodes();
	if (ImGui::BeginItemTooltip()) ImGui::Text("Copy"), ImGui::EndTooltip();
	ImGui::EndDisabled();

	// 粘贴按钮
	ImGui::BeginDisabled(copied_graph_json.empty() || state != State::Editing);
	if (ImGui::Button(ICON_PASTE "##toolbar-paste", {area_width, area_width})) paste_nodes();
	if (ImGui::BeginItemTooltip()) ImGui::Text("Paste"), ImGui::EndTooltip();
	ImGui::EndDisabled();
}

// =============================================================================
/* 应用主循环 */

void App::run()
{
	app_settings.load_from_file("settings.json");

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

		sync_ui_settings();

		imgui_context.new_frame();
		{
			draw();
			popup_manager.draw();
			// ImGui::ShowDemoWindow();
		}
		imgui_context.render(sdl_context.get_renderer_ptr());

		poll_state();

		SDL_SetWindowTitle(
			sdl_context.get_window_ptr(),
			std::format(
				"{} {} {}",
				config::appearance::window_title,
				graph.modified ? "(Unsaved)" : "",
				state == State::Previewing ? "(Previewing)" : ""
			)
				.c_str()
		);
	}

	if (!app_settings.save_to_file("settings.json"))
		std::println(std::cerr, "Failed to save settings to file");
}

// =============================================================================
/* 弹窗管理 */

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
void App::add_info_popup_window(std::string title, std::string explanation, std::string detail)
{
	class Info_window
	{
		std::string title;
		std::string explanation;
		std::string detail;

	  public:

		Info_window(std::string title, std::string explanation, std::string detail) :
			title(std::move(title)),
			explanation(std::move(explanation)),
			detail(std::move(detail))
		{
		}

		bool operator()(bool close_button_pressed)
		{
			ImGui::Text("%s", title.c_str());

			if (!explanation.empty())
			{
				ImGui::SeparatorText("Explanation");
				ImGui::BeginChild(
					"Explanation",
					{300 * runtime_config::ui_scale, 140 * runtime_config::ui_scale},
					ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize
				);
				ImGui::TextWrapped("%s", explanation.c_str());
				ImGui::EndChild();
			}
			else
			{
				ImGui::Dummy({1, 5 * runtime_config::ui_scale});
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
		{.title = std::move(title),
		 .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		 .has_close_button = true,
		 .keep_centered = true,
		 .render_func = Info_window(std::move(title), std::move(explanation), std::move(detail))}
	);
}

// 新建项目确认弹窗
std::future<App::New_project_window_result> App::add_new_project_confirm_window_async()
{
	class New_project_confirm_window
	{
		std::promise<New_project_window_result> result;

	  public:

		bool operator()(bool close_button_pressed)
		{
			ImGui::Text("Are you sure you want to create a new project?");

			ImGui::Dummy({1, 5 * runtime_config::ui_scale});

			if (ImGui::Button("Save"))
			{
				result.set_value(New_project_window_result::Save);
				return true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard"))
			{
				result.set_value(New_project_window_result::Discard);
				return true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel") || close_button_pressed)
			{
				result.set_value(New_project_window_result::Cancel);
				return true;
			}

			return false;
		}

		auto get_future() { return result.get_future(); }

	} window;

	auto future = window.get_future();

	popup_manager.open_window(
		{.title = ICON_WARNING " New Project",
		 .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		 .has_close_button = true,
		 .keep_centered = true,
		 .render_func = std::move(window)}
	);

	return future;
}

// =============================================================================
/* 项目管理 */

// 保存项目
void App::save_project()
{
	if (graph_path.empty() || !std::filesystem::exists(graph_path)
		|| !std::filesystem::is_regular_file(graph_path))
	{
		// 如果没有指定路径，则弹出保存对话框
		const auto filename = save_file_dialog("Save Project", {"Project File", "*.json"});
		if (!filename.has_value()) return;  // 用户取消了保存

		save_project_with_path(filename.value());

		return;
	}

	save_project_with_path(graph_path);
}

// 打开项目
void App::open_project()
{
	class Open_confirm_window
	{
		App& app_ptr;

		bool open()
		{
			const auto path = open_file_dialog("Select Project File", {"Project File", "*.json"});

			if (!path.has_value()) return false;
			return app_ptr.load_project_from_file(path.value());
		}

	  public:

		Open_confirm_window(App& app) :
			app_ptr(app)
		{
		}

		bool operator()(bool close_button_pressed)
		{
			if (!app_ptr.graph.modified)
			{
				open();
				return true;
			}

			ImGui::Text("Opening a new project will discard current changes.");
			ImGui::Text("Do you want to continue?");

			ImGui::Dummy({1, 15 * runtime_config::ui_scale});

			if (ImGui::Button(
					"Save Current",
					{120 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}
				))
			{
				app_ptr.save_project();
				open();
				return true;
			}

			ImGui::SameLine();

			if (ImGui::Button("Discard", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}))
			{
				open();
				return true;
			}

			ImGui::SameLine();

			return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale})
				|| close_button_pressed;
		}
	};

	popup_manager.open_window(
		{.title = "Open Project",
		 .flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove,
		 .has_close_button = true,
		 .keep_centered = true,
		 .render_func = Open_confirm_window(*this)}
	);
}

//创建新的项目
void App::new_project_async()
{
	background_thread = std::jthread(
		[this](const std::stop_token& stop_token)
		{
			try
			{
				if (!graph.modified)  // 没有修改，直接创建
				{
					clear_project();
					return;
				}

				auto future = add_new_project_confirm_window_async();

				while (true)
				{
					if (stop_token.stop_requested()) return;
					if (future.wait_for(std::chrono::milliseconds(50)) == std::future_status::ready) break;
					std::this_thread::yield();
				}

				const auto result = future.get();

				if (result != New_project_window_result::Cancel)
				{
					if (result == New_project_window_result::Save) save_project();
					clear_project();
				}
			}
			catch (const std::exception& e)
			{
				add_error_popup_window(
					"Error Creating New Project",
					"Unexpected error occurred while creating a new project.",
					e.what()
				);
			}
		}
	);
}

// 清空当前项目
void App::clear_project()
{
	save_undo_state();

	if (state == State::Previewing)
	{
		if (runner == nullptr) THROW_LOGIC_ERROR("Runner is null while trying to stop preview.");
		runner.reset();
	}

	graph = infra::Graph();
	graph_path.clear();
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
	graph.modified = false;

	for (const auto& [id, node] : graph.nodes) ImNodes::SetNodeGridSpacePos(id, node.position);
}

// 使用指定路径和名称保存项目
bool App::save_project_with_path(const std::string& filename)
{
	// 获取当前图的JSON字符串
	std::string json_string = save_graph_as_string();

	// 写入文件
	std::ofstream file(filename);
	if (!file.is_open())
	{
		add_error_popup_window(
			"Failed to Save Project",
			"Could not open file for writing.",
			"File: " + filename
		);
		return false;
	}

	file << json_string;

	graph.modified = false;
	graph_path = filename;

	return true;
}

// 加载项目文件
bool App::load_project_from_file(const std::string& filepath)
{
	// 保存当前状态到撤销栈
	save_undo_state();

	// 如果正在预览，先停止预览
	if (state == State::Previewing && runner)
	{
		runner.reset();
		state = State::Editing;
	}

	std::ifstream file(filepath);
	if (!file.is_open())
	{
		add_error_popup_window(
			"Failed to Open Project File",
			"Could not open the selected file.",
			"File: " + filepath
		);
		return false;
	}

	redo_stack.clear();
	graph_path = filepath;

	load_graph_from_string(
		std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>())
	);

	return true;
}

//=============================================================================
/* 撤销重做状态实现 */

// 保存当前图到撤销栈
void App::save_undo_state()
{
	graph.modified = true;

	// 保存当前图到撤销栈
	undo_stack.push_back(graph);

	// 如果超过限制，移除最老的状态
	if (undo_stack.size() > size_t(app_settings.editor.max_undo_levels)) undo_stack.pop_front();

	// 清空重做栈（因为新操作会使之前的重做路径失效）
	redo_stack.clear();

	//定期压缩内存
	if (undo_stack.size() % 10 == 0) compress_undo_stack();
}

// 保存当前图到重做栈
void App::save_redo_state()
{
	// 限制栈大小
	const size_t max_redo_size = 30;

	// 保存当前图到重做栈
	redo_stack.push_back(graph);

	// 如果超过限制，移除最老的状态
	if (redo_stack.size() > max_redo_size)
	{
		redo_stack.pop_front();
	}

	// 清空撤销栈
	undo_stack.clear();
}

// 撤销和重做操作
void App::undo()
{
	if (!undo_stack.empty())
	{
		// 限制重做栈大小
		const size_t max_redo_size = 30;

		redo_stack.push_back(graph);

		// 如果重做栈超过限制，移除最老的状态
		if (redo_stack.size() > max_redo_size)
		{
			redo_stack.pop_front();
		}

		graph = undo_stack.back();
		undo_stack.pop_back();

		restore_node_positions();
	}
}

void App::redo()
{
	if (!redo_stack.empty())
	{
		// 限制撤销栈大小
		const size_t max_undo_size = 30;

		undo_stack.push_back(graph);

		// 如果撤销栈超过限制，移除最老的状态
		if (undo_stack.size() > max_undo_size)
		{
			undo_stack.pop_front();
		}

		graph = redo_stack.back();
		redo_stack.pop_back();

		restore_node_positions();
	}
}

// 压缩撤销栈
void App::compress_undo_stack()
{
	// 内存压缩策略：每10个状态合并为一个检查点
	if (undo_stack.size() < 20) return;  // 少于20个状态时不压缩

	// 保留最近的10个状态，每隔5个状态保留一个检查点
	std::list<infra::Graph> compressed_stack;

	auto it = undo_stack.begin();

	// 从较早的状态中每隔5个保留一个检查点
	size_t recent_start = undo_stack.size() >= 10 ? undo_stack.size() - 10 : 0;
	for (size_t i = 0; i < recent_start && it != undo_stack.end(); i += 5)
	{
		auto checkpoint_it = it;
		std::advance(it, std::min(5UL, static_cast<size_t>(std::distance(it, undo_stack.end()))));
		compressed_stack.push_back(std::move(*checkpoint_it));
	}

	// 保留最近的10个状态
	auto recent_it = undo_stack.begin();
	std::advance(recent_it, recent_start);
	for (auto iter = recent_it; iter != undo_stack.end(); ++iter)
	{
		compressed_stack.push_back(std::move(*iter));
	}

	undo_stack = std::move(compressed_stack);
}

// 恢复节点位置
void App::restore_node_positions()
{
	for (const auto& [id, node] : graph.nodes)
	{
		if (node.position.x != 0.0f || node.position.y != 0.0f)
		{
			ImNodes::SetNodeGridSpacePos(id, node.position);
		}
	}
}

// =============================================================================
/* 复制粘贴操作 */

// 复制选中的节点和连线
void App::copy_selected_nodes()
{
	const auto selected_node_count = ImNodes::NumSelectedNodes();
	const auto selected_link_count = ImNodes::NumSelectedLinks();

	if (selected_node_count == 0 && selected_link_count == 0) return;

	std::vector<infra::Id_t> selected_nodes(selected_node_count);
	std::vector<infra::Id_t> selected_links(selected_link_count);

	if (selected_node_count > 0) ImNodes::GetSelectedNodes(selected_nodes.data());
	if (selected_link_count > 0) ImNodes::GetSelectedLinks(selected_links.data());

	// 创建一个临时图来保存选中的内容
	infra::Graph temp_graph;
	std::map<infra::Id_t, infra::Id_t> node_id_mapping;  // 原ID -> 新ID映射

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

					if (from_pin_map.contains(from_pin.attribute.identifier)
						&& to_pin_map.contains(to_pin.attribute.identifier))
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

					if (from_pin_map.contains(from_pin.attribute.identifier)
						&& to_pin_map.contains(to_pin.attribute.identifier))
					{
						const auto new_from_pin_id = from_pin_map.at(from_pin.attribute.identifier);
						const auto new_to_pin_id = to_pin_map.at(to_pin.attribute.identifier);

						temp_graph.add_link(new_from_pin_id, new_to_pin_id);
					}
				}
			}
		}

		// 序列化临时图
		const auto json = temp_graph.serialize();
		Json::StreamWriterBuilder writer;
		writer["indentation"] = "";  // 紧凑格式
		copied_graph_json = Json::writeString(writer, json);

		// 显示复制成功的提示
		std::string message;
		if (selected_node_count > 0 && selected_link_count > 0)
		{
			message = std::format(
				"Copied {} node{} and {} link{} to clipboard.",
				selected_node_count,
				selected_node_count > 1 ? "s" : "",
				selected_link_count,
				selected_link_count > 1 ? "s" : ""
			);
		}
		else if (selected_node_count > 0)
		{
			const auto internal_links = temp_graph.links.size();
			if (internal_links > 0)
			{
				message = std::format(
					"Copied {} node{} and {} internal link{} to clipboard.",
					selected_node_count,
					selected_node_count > 1 ? "s" : "",
					internal_links,
					internal_links > 1 ? "s" : ""
				);
			}
			else
			{
				message = std::format(
					"Copied {} node{} to clipboard.",
					selected_node_count,
					selected_node_count > 1 ? "s" : ""
				);
			}
		}
		else if (selected_link_count > 0)
		{
			message = std::format(
				"Copied {} link{} to clipboard.",
				selected_link_count,
				selected_link_count > 1 ? "s" : ""
			);
		}
	}
	catch (const std::exception& e)
	{
		add_error_popup_window("Copy Failed", "Failed to copy selected nodes and links.", e.what());
	}
}

// 粘贴节点和连线
void App::paste_nodes()
{
	if (copied_graph_json.empty()) return;

	save_undo_state();

	try
	{
		// 反序列化复制的图
		Json::Value json;
		const auto parse_result = Json::Reader().parse(copied_graph_json, json, false);
		if (!parse_result)
		{
			throw std::runtime_error("Failed to parse copied graph JSON");
		}

		infra::Graph temp_graph = infra::Graph::deserialize(json);

		// 如果临时图为空，提前返回
		if (temp_graph.nodes.empty())
		{
			add_info_popup_window("Paste Failed", "No nodes to paste.", "");
			return;
		}

		// 计算粘贴偏移量
		const ImVec2 paste_offset = ImGui::GetMousePos();
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

		std::map<infra::Id_t, infra::Id_t> node_id_mapping;  // 临时图ID -> 目标图ID映射
		std::vector<infra::Id_t> pasted_node_ids;
		size_t skipped_singleton_count = 0;

		// 粘贴节点
		for (const auto& [temp_node_id, temp_node] : temp_graph.nodes)
		{
			const auto processor_info = temp_node.processor->get_processor_info_non_static();

			// 检查是否为单例处理器且已存在
			if (processor_info.singleton && graph.singleton_node_map.contains(processor_info.identifier))
			{
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
		}

		// 粘贴连线
		size_t pasted_link_count = 0;
		for (const auto& [temp_link_id, temp_link] : temp_graph.links)
		{
			const auto& temp_from_pin = temp_graph.pins.at(temp_link.from);
			const auto& temp_to_pin = temp_graph.pins.at(temp_link.to);

			// 检查两端节点是否都已成功粘贴
			if (node_id_mapping.contains(temp_from_pin.parent)
				&& node_id_mapping.contains(temp_to_pin.parent))
			{
				const auto new_from_node_id = node_id_mapping[temp_from_pin.parent];
				const auto new_to_node_id = node_id_mapping[temp_to_pin.parent];

				// 在目标图中找到对应的引脚
				const auto& from_pin_map = graph.nodes.at(new_from_node_id).pin_name_map;
				const auto& to_pin_map = graph.nodes.at(new_to_node_id).pin_name_map;

				if (from_pin_map.contains(temp_from_pin.attribute.identifier)
					&& to_pin_map.contains(temp_to_pin.attribute.identifier))
				{
					const auto new_from_pin_id = from_pin_map.at(temp_from_pin.attribute.identifier);
					const auto new_to_pin_id = to_pin_map.at(temp_to_pin.attribute.identifier);

					try
					{
						graph.add_link(new_from_pin_id, new_to_pin_id);
						pasted_link_count++;
					}
					catch (const std::exception& e)
					{
						{
						}
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

		// 显示粘贴结果的提示
		std::string message;
		if (pasted_node_ids.empty() && pasted_link_count == 0)
		{
			if (skipped_singleton_count > 0)
			{
				message = std::format(
					"No nodes were pasted. {} singleton node{} already exist{} in the graph.",
					skipped_singleton_count,
					skipped_singleton_count > 1 ? "s" : "",
					skipped_singleton_count > 1 ? "" : "s"
				);
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
				message = std::format(
					"Pasted {} node{} and {} link{} from clipboard.",
					pasted_node_ids.size(),
					pasted_node_ids.size() > 1 ? "s" : "",
					pasted_link_count,
					pasted_link_count > 1 ? "s" : ""
				);
			}
			else
			{
				message = std::format(
					"Pasted {} node{} from clipboard.",
					pasted_node_ids.size(),
					pasted_node_ids.size() > 1 ? "s" : ""
				);
			}

			if (skipped_singleton_count > 0)
			{
				message += std::format(
					" ({} singleton node{} skipped)",
					skipped_singleton_count,
					skipped_singleton_count > 1 ? "s" : ""
				);
			}
		}
	}
	catch (const std::exception& e)
	{
		add_error_popup_window("Paste Failed", "Failed to paste nodes and links from clipboard.", e.what());
	}
}

// =============================================================================
/* 视图和缩放窗口绘制 */

// 绘制面板显示控制菜单
void App::draw_view_panels_menu()
{
	ImGui::MenuItem("Toolbar", nullptr, &app_settings.ui.show_toolbar);
	ImGui::MenuItem("Node Editor Minimap", nullptr, &app_settings.ui.show_minimap);
	ImGui::MenuItem("Diagnostics Overlay", nullptr, &show_diagnostics);
	ImGui::MenuItem("ImGUI Demo", nullptr, &show_demo_window);
}

// 绘制节点编辑器视图控制菜单
void App::draw_view_center_menu()
{
	// 网格设置子菜单
	if (ImGui::BeginMenu("Grid"))
	{
		ImGui::MenuItem("Snap to Grid", nullptr, &app_settings.ui.snap_to_grid);
		ImGui::MenuItem("Show Grid Lines", nullptr, &app_settings.ui.show_grid);

		ImGui::Separator();

		// 网格大小预设 - 显示基础网格大小，不考虑缩放
		constexpr auto grid_sizes = std::to_array({10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f});

		ImGui::Text("Grid Size:");
		for (auto grid_size : grid_sizes)
		{
			if (ImGui::MenuItem(
					std::format("{}px", (int)grid_size).c_str(),
					nullptr,
					std::abs(app_settings.ui.grid_size - grid_size) < 0.1f
				))
				app_settings.ui.grid_size = grid_size;
		}

		ImGui::EndMenu();
	}
}

void App::sync_ui_settings() const
{
	ImNodesStyle& style = ImNodes::GetStyle();

	style.GridSpacing = app_settings.ui.grid_size;

	if (app_settings.ui.show_grid)
		style.Flags |= ImNodesStyleFlags_GridLines;
	else
		style.Flags &= ~ImNodesStyleFlags_GridLines;

	if (app_settings.ui.snap_to_grid)
		style.Flags |= ImNodesStyleFlags_GridSnapping;
	else
		style.Flags &= ~ImNodesStyleFlags_GridSnapping;
}

// 辅助函数：获取当前状态文本
std::string App::get_current_state_text(App::State state)
{
	switch (state)
	{
	case App::State::Editing:
		return "Editing";
	case App::State::Previewing:
		return "Previewing";
	case App::State::Preview_requested:
		return "Starting Preview";
	case App::State::Preview_cancelling:
		return "Stopping Preview";
	default:
		return "Unknown";
	}
}

// 辅助函数：统计处理器状态数量
std::tuple<size_t, size_t, size_t> App::count_processor_states(const auto& resources) const
{
	size_t running_count = 0;
	size_t finished_count = 0;
	size_t error_count = 0;

	for (const auto& [id, resource] : resources)
	{
		switch (resource->state)
		{
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
/*性能窗口*/

// 绘制悬浮性能指标覆盖层
void App::draw_diagnostics_overlay()
{
	if (!show_diagnostics) return;

	// 设置覆盖层默认位置（左下角）
	const auto [display_size_x, display_size_y] = ImGui::GetIO().DisplaySize;
	const float overlay_width = 280 * runtime_config::ui_scale;
	const float overlay_margin = config::appearance::toolbar_margin * runtime_config::ui_scale;

	// 左下角位置
	const float overlay_pos_x = overlay_margin;
	const float overlay_pos_y = display_size_y - overlay_margin;

	// 只在第一次使用时设置位置，之后允许用户自由移动
	ImGui::SetNextWindowPos(ImVec2(overlay_pos_x, overlay_pos_y), ImGuiCond_Always, {0, 1});
	ImGui::SetNextWindowSize(ImVec2(overlay_width, 0), ImGuiCond_Always);

	// 创建可移动的覆盖层窗口
	const ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse
										 | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
										 | ImGuiWindowFlags_NoMove;

	if (ImGui::Begin("Diagnostics", &show_diagnostics, overlay_flags))
	{
		// 基本性能指标
		ImGui::SeparatorText("Performance");
		{

			const ImGuiIO& io = ImGui::GetIO();
			ImGui::Text("FPS: %.1f (%.2fms)", io.Framerate, io.DeltaTime * 1000.0f);

			const auto working_set_size = get_working_set_size();

			if (working_set_size.has_value())
				ImGui::Text("Memory: %.2f MB", *working_set_size / (1024.0f * 1024.0f));
			else
				ImGui::Text("Memory: N/A");
		}

		// 图形统计
		ImGui::SeparatorText("Graph");
		{
			ImGui::Text("Nodes: %zu", graph.nodes.size());
			ImGui::Text("Links: %zu", graph.links.size());

			// 选中状态
			const int selected_nodes = ImNodes::NumSelectedNodes();
			const int selected_links = ImNodes::NumSelectedLinks();
			if (selected_nodes > 0 || selected_links > 0)
				ImGui::Text("Selected: %dN %dL", selected_nodes, selected_links);
		}

		// 编辑器状态
		ImGui::SeparatorText("Editor State");
		{
			std::string state_text = get_current_state_text(state);
			ImGui::Text("%s", state_text.c_str());

			// 撤销/重做状态
			ImGui::Text("Undo: %zu | Redo: %zu", undo_stack.size(), redo_stack.size());
		}

		// 如果在预览状态，显示处理器统计
		if (state == State::Previewing && runner)
		{
			ImGui::SeparatorText("Audio");

			auto& processor_resources = runner->get_processor_resources();
			auto [running_count, finished_count, error_count] = count_processor_states(processor_resources);

			ImGui::Text(
				"%d Running | %d Finished | %d Errors",
				(int)running_count,
				(int)finished_count,
				(int)error_count
			);

			// 显示音频链路状态（简化版）
			auto& link_products = runner->get_link_products();
			if (!link_products.empty())
			{
				for (const auto& [id, link] : link_products)
				{
					try
					{
						const auto& product = dynamic_cast<const processor::Audio_stream&>(*link);
						const float fill_ratio
							= (float)product.buffered_count() / config::processor::audio_stream::buffer_size;

						// < 60%: 红色
						// 60% - 80%: 黄色
						// > 80%: 绿色
						const ImVec4 buffer_color
							= ImVec4(fill_ratio < 0.8 ? 1 : 0, fill_ratio > 0.6 ? 1 : 0, 0, 1);

						ImGui::TextColored(buffer_color, "L%d: %.0f%%", id, fill_ratio * 100.0f);
					}
					catch (const std::bad_cast&)
					{
						{
						}
					}
				}
			}
		}
	}
	ImGui::End();
}

// =============================================================================
/*节点编辑器和渲染*/

// 绘制节点编辑器
void App::draw_node_editor()
{
	ImNodes::BeginNodeEditor();
	{
		for (auto& [i, node] : graph.nodes) draw_node(i, node);

		for (auto [i, link] : graph.links) ImNodes::Link(i, link.from, link.to);

		if (app_settings.ui.show_minimap)
		{
			ImNodes::MiniMap(
				config::appearance::node_editor_minimap_fraction,
				ImNodesMiniMapLocation_TopRight
			);
		}
	}
	ImNodes::EndNodeEditor();

	//保存节点位置
	for (auto& [id, item] : graph.nodes)
	{
		const auto new_position = ImNodes::GetNodeGridSpacePos(id);
		item.position = new_position;
	}

	// 直接右键节点/连结选中并打开菜单
	if (state == State::Editing && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		infra::Id_t hovered_node, hovered_link;
		if (ImNodes::IsNodeHovered(&hovered_node))
		{
			delayed_selection_node = hovered_node;
		}
		else if (ImNodes::IsLinkHovered(&hovered_link))
		{
			delayed_selection_link = hovered_link;
		}
		else
			open_node_context_menu_tries = 5;
	}

	if (state != State::Editing)
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
	}

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
	if (ImGui::BeginPopup("node-editor-context-menu", ImGuiWindowFlags_Popup))
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

			if (ImGui::MenuItem("Delete", number_description_text.c_str())) remove_selected_nodes();
		}

		// 复制按钮
		if (const auto selected_nodes = ImNodes::NumSelectedNodes(); selected_nodes > 0)
		{
			std::string copy_description
				= std::format("{} Node{}", selected_nodes, selected_nodes > 1 ? "s" : "");
			if (ImGui::MenuItem("Copy", copy_description.c_str()))
			{
				copy_selected_nodes();
			}
		}

		ImGui::BeginDisabled(copied_graph_json.empty());
		if (ImGui::MenuItem("Paste", "Ctrl+V"))
		{
			paste_nodes();
		}
		ImGui::EndDisabled();

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
	save_undo_state();

	const auto selected_node_count = ImNodes::NumSelectedNodes();
	if (selected_node_count != 0)
	{
		std::vector<infra::Id_t> selected_nodes(selected_node_count);
		ImNodes::GetSelectedNodes(selected_nodes.data());

		for (auto i : selected_nodes
						  | std::views::filter([this](auto i) { return i >= 0 && graph.nodes.contains(i); }))
			graph.remove_node(i);
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

	ImNodes::ClearNodeSelection();
}

// 处理节点编辑器的操作
void App::handle_node_actions()
{
	// 只能在编辑模式下改变图
	if (state != State::Editing) return;

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
}

void App::handle_keyboard_shortcuts()
{
	// Ctrl+C 复制
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_C) && state == State::Editing)
		copy_selected_nodes();

	// Ctrl+V 粘贴
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_V) && state == State::Editing) paste_nodes();

	// Ctrl+Z 撤销
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_Z) && state == State::Editing) undo();

	// Ctrl+Y 或 Ctrl+Shift+Z 重做
	if ((ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_Y)
		 || ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_ModShift | ImGuiKey_Z))
		&& state == State::Editing)
		redo();

	// Ctrl+A 全选
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_A) && state == State::Editing)
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
		for (const auto& [id, _] : graph.nodes) ImNodes::SelectNode(id);
		for (const auto& [id, _] : graph.links) ImNodes::SelectLink(id);
	}

	// Ctrl-S 保存
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_S) && state == State::Editing) save_project();

	// Ctrl-O 打开项目
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_O) && state == State::Editing) open_project();

	// Ctrl-N 新建项目
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_N) && state == State::Editing)
		new_project_async();

	// Ctrl-Q 退出
	if (ImGui::IsKeyChordPressed(ImGuiKey_ModCtrl | ImGuiKey_Q) && state == State::Editing)
	{
		if (graph.modified)
			add_exit_confirm_window();
		else
		{
			SDL_Event quit_event;
			quit_event.type = SDL_QUIT;
			SDL_PushEvent(&quit_event);
		}
	}

	// Esc 键取消选中节点和连线
	if (ImGui::IsKeyChordPressed(ImGuiKey_Escape) && state == State::Editing
		&& ImNodes::NumSelectedLinks() + ImNodes::NumSelectedNodes() > 0)
	{
		ImNodes::ClearNodeSelection();
		ImNodes::ClearLinkSelection();
	}

	// Delete 删除选中节点和连线
	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && state == State::Editing) remove_selected_nodes();
}

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

	default:
		break;
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

// 退出确认弹窗
void App::add_exit_confirm_window()
{
	class Exit_confirm_window
	{
		App* app_ptr;

	  public:

		Exit_confirm_window(App* app) :
			app_ptr(app)
		{
		}
		bool operator()(bool close_button_pressed)
		{
			ImGui::Text("You have unsaved changes. Do you want to save before exiting?");
			ImGui::Dummy({1, 10 * runtime_config::ui_scale});
			ImGui::Separator();

			if (ImGui::Button(
					"Save and Exit",
					{120 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}
				))
			{
				app_ptr->save_project();
				return true;
			}

			ImGui::SameLine();
			if (ImGui::Button(
					"Exit Without Saving",
					{160 * runtime_config::ui_scale, 30 * runtime_config::ui_scale}
				))
			{
				// 直接退出
				SDL_Event quit_event;
				quit_event.type = SDL_QUIT;
				SDL_PushEvent(&quit_event);
				return true;
			}

			ImGui::SameLine();

			return ImGui::Button("Cancel", {100 * runtime_config::ui_scale, 30 * runtime_config::ui_scale})
				|| close_button_pressed;
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