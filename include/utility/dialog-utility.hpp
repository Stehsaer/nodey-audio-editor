#pragma once

#include <optional>
#include <string>
#include <vector>

// 注：Filter格式为
// { "JPEG File", "*.jpg", "PNG File", "*.png", "Two types", "*.cpp *.c" }
// 名称和后缀名交替出现，用空格分割多个不同的后缀名

// 打开文件
// - 当没有选中文件时，返回std::nullopt
std::optional<std::string> open_file_dialog(
	const std::string& title,
	const std::vector<std::string>& filter = {"All Files", "*.*"},
	const std::string& default_path = ""
);

// 打开多选文件对话框
// - 当没有选中文件时，返回std::nullopt
std::optional<std::vector<std::string>> open_file_dialog_multiselect(
	const std::string& title,
	const std::vector<std::string>& filter = {"All Files", "*.*"},
	const std::string& default_path = ""
);

// 保存文件对话框
// - 当没有选中文件时，返回std::nullopt
std::optional<std::string> save_file_dialog(
	const std::string& title,
	const std::vector<std::string>& filter = {"All Files", "*.*"},
	const std::string& default_path = "",
	bool force_overwrite_warning = true
);

// 打开文件夹对话框
// - 当没有选中文件夹时，返回std::nullopt
std::optional<std::string> open_folder_dialog(const std::string& title, const std::string& default_path = "");
