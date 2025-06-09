#pragma once

#include <SDL2/SDL.h>
#include <span>
#include <stdexcept>

// SDL上下文类
// - 负责SDL库的初始化和上下文管理
class SDL_context
{
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	SDL_AudioDeviceID audio_device;
	bool sdl_initialized = false;

  public:

	SDL_context();

	SDL_Window* get_window_ptr() const { return window; }
	SDL_Renderer* get_renderer_ptr() const { return renderer; }
	SDL_AudioDeviceID get_audio_device() const { return audio_device; }

	~SDL_context();
};
