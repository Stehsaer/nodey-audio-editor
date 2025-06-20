#include "frontend/app/popup.hpp"
#include "infra/processor.hpp"

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
	std::function<void(
		std::vector<std::unique_ptr<Internal_window>>&,
		std::vector<std::unique_ptr<Internal_window>>::iterator
	)>
		draw_window;

	draw_window = [&draw_window](
					  std::vector<std::unique_ptr<Internal_window>>& windows,
					  std::vector<std::unique_ptr<Internal_window>>::iterator it
				  )
	{
		if (it == windows.end()) return;

		auto& window = **it;

		if (!ImGui::IsPopupOpen(window.title.c_str())) ImGui::OpenPopup(window.title.c_str());

		if (window.keep_centered)
		{
			ImGui::SetNextWindowPos(
				ImGui::GetMainViewport()->GetCenter(),
				ImGuiCond_Always,
				ImVec2(0.5f, 0.5f)
			);
		}

		if (ImGui::BeginPopupModal(
				window.title.c_str(),
				window.has_close_button && window.imgui_popen ? &window.imgui_popen : nullptr,
				window.flags
			))
		{
			if (window.render_func(!window.imgui_popen)) window.close_requested = true;

			draw_window(windows, std::next(it));

			if (window.close_requested) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}
	};

	while (!add_window_queue.empty())
	{
		std::lock_guard lock(add_window_mutex);
		windows.emplace_back(std::move(add_window_queue.front()));
		add_window_queue.pop();
	}

	draw_window(windows, windows.begin());

	std::erase_if(windows, [](const auto& window) { return window->close_requested; });
}

void Popup_manager::open_window(Window window)
{
	std::lock_guard lock(add_window_mutex);
	add_window_queue.emplace(std::make_unique<Internal_window>(std::move(window)));
}