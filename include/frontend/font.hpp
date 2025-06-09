// font.hpp: 保存字体二进制

#pragma once
#include <cstdint>

extern "C"
{
	extern const uint8_t font_data[];
	extern const uint32_t font_data_size;
	extern const uint8_t icon_data[];
	extern const uint32_t icon_data_size;
}
