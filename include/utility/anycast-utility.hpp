#pragma once

#include <any>
#include <optional>

// 尝试将std::any转换为指定类型T
// - 如果转换成功，返回std::optional<T>，否则返回std::nullopt
// - 避免了在主函数中使用try来捕获（太丑了！）
template <typename T>
std::optional<T> try_anycast(const std::any& value)
{
	try
	{
		return std::any_cast<T>(value);
	}
	catch (const std::bad_any_cast&)
	{
		return std::nullopt;
	}
}