#include "frontend/app/popup.hpp"

#include <format>
#include <ranges>

Popup_manager::Internal_window::Internal_window(Window&& window) :
	Window(std::move(window)),
	opened(false),
	close_requested(false),
	imgui_popen(true)
{
	title = std::format("{}###{:p}", title, (const void*)this);
}

void Popup_manager::draw()
{
	while (!add_window_queue.empty())
	{
		windows.emplace_back(std::move(add_window_queue.front()));
		add_window_queue.pop();
	}

	for (auto& window : windows)
	{
		// 打开尚未打开的弹窗
		if (!window->opened)
		{
			ImGui::OpenPopup(window->title.c_str());
			window->opened = true;
		}

		// 保持窗口居中
		if (window->keep_centered)
		{
			const auto display_size = ImGui::GetIO().DisplaySize;
			ImGui::SetNextWindowPos({display_size.x / 2, display_size.y / 2}, ImGuiCond_Always, {0.5f, 0.5f});
		}

		if (ImGui::BeginPopupModal(
				window->title.c_str(),
				window->has_close_button ? &window->imgui_popen : nullptr,
				window->flags
			))
			window->close_requested = window->render_func(!window->imgui_popen);
	}

	for (auto& window : windows | std::views::reverse)
	{
		if (window->close_requested) ImGui::CloseCurrentPopup();
		if (window->imgui_popen) ImGui::EndPopup();
	}

	std::erase_if(windows, [](const auto& window) { return window->close_requested; });
}

void Popup_manager::open_window(Window window)
{
	std::lock_guard lock(add_window_mutex);
	add_window_queue.emplace(std::make_unique<Internal_window>(std::move(window)));
}