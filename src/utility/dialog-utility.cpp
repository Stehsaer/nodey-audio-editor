#include "utility/dialog-utility.hpp"
#include "portable-file-dialogs.h"
#include <SDL_events.h>

// 打开文件
// - 当没有选中文件时，返回std::nullopt
std::optional<std::string> open_file_dialog(
	const std::string& title,
	const std::vector<std::string>& filter,
	const std::string& default_path
)
{
	pfd::open_file open_file_dialog(title, default_path, filter);

	while (!open_file_dialog.ready(50))
	{
		SDL_Event event;
		while (SDL_PollEvent(&event));
		std::this_thread::yield();
	}

	const auto result = open_file_dialog.result();
	if (result.empty()) return std::nullopt;
	return result[0];
}

// 打开多选文件对话框
// - 当没有选中文件时，返回std::nullopt
std::optional<std::vector<std::string>> open_file_dialog_multiselect(
	const std::string& title,
	const std::vector<std::string>& filter,
	const std::string& default_path
)
{
	pfd::open_file open_file_dialog(title, default_path, filter, pfd::opt::multiselect);

	while (!open_file_dialog.ready(50))
	{
		SDL_Event event;
		while (SDL_PollEvent(&event));
		std::this_thread::yield();
	}

	auto result = open_file_dialog.result();
	if (result.empty()) return std::nullopt;
	return result;
}

// 保存文件对话框
// - 当没有选中文件时，返回std::nullopt
std::optional<std::string> save_file_dialog(
	const std::string& title,
	const std::vector<std::string>& filter,
	const std::string& default_path,
	bool force_overwrite_warning
)
{
	pfd::save_file save_file_dialog(
		title,
		default_path,
		filter,
		force_overwrite_warning ? pfd::opt::force_overwrite : pfd::opt::none
	);

	while (!save_file_dialog.ready(50))
	{
		SDL_Event event;
		while (SDL_PollEvent(&event));
		std::this_thread::yield();
	}

	auto result = save_file_dialog.result();
	if (result.empty()) return std::nullopt;
	return result;
}

// 打开文件夹对话框
// - 当没有选中文件夹时，返回std::nullopt
std::optional<std::string> open_folder_dialog(const std::string& title, const std::string& default_path)
{
	pfd::select_folder open_folder_dialog(title, default_path);
	if (!open_folder_dialog.ready(50))
	{
		SDL_Event event;
		while (SDL_PollEvent(&event));
		std::this_thread::yield();
	}

	auto result = open_folder_dialog.result();
	if (result.empty()) return std::nullopt;
	return result;
}
