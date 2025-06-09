#pragma once
#include <functional>

// 辅助类，用于在作用域结束时自动调用指定的删除函数
// - 用于自动管理C库中使用库内自定义malloc所分配的对象/资源
class Free_utility
{
	std::function<void()> delete_func;

  public:

	Free_utility(std::function<void()> delete_func) :
		delete_func(std::move(delete_func))
	{
	}

	~Free_utility()
	{
		if (delete_func) delete_func();
	}

	Free_utility(const Free_utility&) = delete;
	Free_utility& operator=(const Free_utility&) = delete;

	Free_utility(Free_utility&&) = default;
	Free_utility& operator=(Free_utility&&) = default;
};