#include "frontend/app.hpp"
#include "frontend/nerdfont.hpp"
#include "processor/audio-io.hpp"
#include "utility/anycast-utility.hpp"

#include <utility>

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

void App::draw_node(infra::Id_t id, const infra::Graph::Node& node)
{
	ImNodes::BeginNode(id);
	{
		// 绘制标题栏
		ImNodes::BeginNodeTitleBar();
		node.processor->draw_title();
		ImNodes::EndNodeTitleBar();

		// 绘制节点本体
		if (node.processor->draw_content(false) && state == State::Editing) graph.update_node_pin(id);

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
			const auto new_id = graph.add_node(item.second.generate());
			new_node_id = new_id;
		}
	}
}

void App::draw_menubar()
{
	if (ImGui::BeginMainMenuBar())
	{
		main_menu_bar_height = ImGui::GetWindowSize().y;
		ImGui::EndMainMenuBar();
	}
}

void App::draw_node_editor()
{
	ImNodes::BeginNodeEditor();
	{
		// 把新节点放置在鼠标处
		if (new_node_id.has_value())
		{
			ImNodes::SetNodeScreenSpacePos(new_node_id.value(), ImGui::GetMousePos());
			new_node_id.reset();
		}

		for (auto& [i, item] : graph.nodes) draw_node(i, item);
		for (auto [i, link] : graph.links) ImNodes::Link(i, link.from, link.to);

		ImNodes::MiniMap(config::appearance::node_editor_minimap_fraction, ImNodesMiniMapLocation_TopRight);

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

			if (ImGui::MenuItem("Delete", number_description_text.c_str())) remove_selected_nodes();
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

void App::draw_side_panel()
{
	const auto full_width = config::appearance::side_panel_width * runtime_config::ui_scale
						  - 2 * ImGui::GetStyle().WindowPadding.x;

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

	ImGui::SeparatorText("Diagnostics");
	ImGui::BulletText("%.2f FPS", ImGui::GetIO().Framerate);

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

void App::draw_main_panel()
{
	draw_node_editor();
}

void App::draw_toolbar()
{
	const auto area_width = config::appearance::toolbar_internal_width * runtime_config::ui_scale;

	ImGui::Button(ICON_CHECK "##toolbar-placeholder-check", {area_width, area_width});
	if (ImGui::BeginItemTooltip())
	{
		ImGui::Text("Placeholder 1 (check icon). Place descriptive text here.");
		ImGui::EndTooltip();
	}

	ImGui::Button(ICON_COPY "##toolbar-placeholder-copy", {area_width, area_width});
	if (ImGui::BeginItemTooltip())
	{
		ImGui::Text("Placeholder 2 (copy icon). Place descriptive text here.");
		ImGui::EndTooltip();
	}
}

void App::draw()
{
	draw_menubar();

	const auto side_panel_width_pixel = config::appearance::side_panel_width * runtime_config::ui_scale;
	const auto [display_size_x, display_size_y] = ImGui::GetIO().DisplaySize;

	// 侧面板
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
		ImGui::SetNextWindowSize({display_size_x - side_panel_width_pixel, ImGui::GetIO().DisplaySize.y});
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

	ImGui::ShowDemoWindow();
}

void App::remove_selected_nodes()
{
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
}

void App::handle_node_actions()
{
	// 只能在编辑模式下改变图
	if (state != State::Editing) return;

	int start, end;
	bool from_snap;
	if (ImNodes::IsLinkCreated(&start, &end, &from_snap))
	{
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

	if (ImGui::IsKeyDown(ImGuiKey_Delete))
	{
		remove_selected_nodes();
		ImNodes::ClearNodeSelection();
	}
}

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