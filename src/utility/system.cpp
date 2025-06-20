#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#elif defined(__linux__)
#endif

#include "utility/system.hpp"

#include <fstream>
#include <sstream>

std::optional<size_t> get_working_set_size()
{
#ifdef _WIN32

	PROCESS_MEMORY_COUNTERS pmc;
	if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return pmc.WorkingSetSize;
	return std::nullopt;

#elif defined(__linux__)

	static std::ifstream status("/proc/self/status");
	status.seekg(0);
	std::string line;
	while (std::getline(status, line))
	{
		if (line.starts_with("VmRSS:"))
		{
			std::istringstream iss(line);
			std::string dummy;
			int memory_kb;
			iss >> dummy >> memory_kb;
			return memory_kb * 1024;
		}
	}

	return std::nullopt;

#else

	return std::nullopt;

#endif
}

// 打开网页链接
void open_url(std::string_view url)
{
#ifdef _WIN32
	ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__linux__)
	const std::string cmd = std::format("xdg-open \"{}\" >/dev/null 2>&1 &", url);
	std::system(cmd.c_str());
#else
	// Not implemented for other platforms
#endif
}