// main.cpp
// 主函数

#include "frontend/app.hpp"
#include <boost/fiber/algo/work_stealing.hpp>
#include <boost/fiber/operations.hpp>
#include <print>

#undef main

int main(int argc, char** argv)
{
	try
	{
		App app;
		app.run();
	}
	catch (const std::logic_error& e)
	{
		std::println(std::cerr, "[ERROR] Logic error: {}", e.what());
		return 1;
	}
	catch (const std::runtime_error& e)
	{
		std::println(std::cerr, "[ERROR] Uncaught runtime error: {}", e.what());
		return 1;
	}
	catch (const std::bad_alloc& e)
	{
		std::println(std::cerr, "[ERROR] Out of Memory: {}", e.what());
		return 1;
	}
	catch (const std::exception& e)
	{
		std::println(std::cerr, "[ERROR] Uncaught exception: {}", e.what());
		return 1;
	}

	return 0;
}
