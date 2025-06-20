// 依赖于具体系统的实现

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

// 获取占用内存的大小
std::optional<size_t> get_working_set_size();

// 打开网页链接
void open_url(std::string_view url);