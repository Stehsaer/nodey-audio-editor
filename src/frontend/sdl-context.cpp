#include "frontend/sdl-context.hpp"
#include "config.hpp"

#include <format>

// 运行时参数定义
namespace runtime_config
{
	// UI参数
	float ui_scale = 1.0f;
}

SDL_context::SDL_context()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
		throw std::runtime_error(std::format("Failed to initialize SDL: {}", SDL_GetError()));

	window = SDL_CreateWindow(
		config::appearance::window_title,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		800,
		600,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED
	);
	if (window == nullptr)
	{
		SDL_Quit();
		throw std::runtime_error(std::format("Failed to create window: {}", SDL_GetError()));
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == nullptr)
	{
		SDL_DestroyWindow(window);
		SDL_Quit();
		throw std::runtime_error(std::format("Failed to create renderer: {}", SDL_GetError()));
	}

	SDL_AudioSpec audio_spec = {
		.freq = config::audio::sample_rate,
		.format = config::audio::buffer_format,
		.channels = config::audio::channels,
		.silence = 0,
		.samples = config::audio::buffer_size,
		.padding = 0,
		.size = sizeof(config::audio::Buffer_type) * (config::audio::buffer_size * 3 / 2),
		.callback = nullptr,
		.userdata = nullptr,
	};

	audio_device = SDL_OpenAudioDevice(nullptr, false, &audio_spec, nullptr, SDL_AUDIO_ALLOW_ANY_CHANGE);
	if (audio_device < 2)
	{
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
		throw std::runtime_error(std::format("Failed to open audio device: {}", SDL_GetError()));
	}

	// determine scaling
	// float dpi = 96;
	// if (SDL_GetDisplayDPI(0, &dpi, nullptr, nullptr) == 0 && dpi > 0) runtime_config::ui_scale = dpi / 96.0f;
	runtime_config::ui_scale = 1.25;

	SDL_SetWindowMinimumSize(
		window,
		config::appearance::min_window_width * runtime_config::ui_scale,
		config::appearance::min_window_height * runtime_config::ui_scale
	);
}

SDL_context::~SDL_context()
{
	if (audio_device >= 2)
	{
		SDL_ClearQueuedAudio(audio_device);
		SDL_CloseAudioDevice(audio_device);
	}

	if (renderer != nullptr) SDL_DestroyRenderer(renderer);
	if (window != nullptr) SDL_DestroyWindow(window);
	if (sdl_initialized) SDL_Quit();
}
