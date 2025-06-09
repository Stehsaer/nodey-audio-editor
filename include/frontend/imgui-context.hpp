#pragma once

#include "sdl-context.hpp"
#include "third-party/ui.hpp"

// IMGUI上下文类
// - 负责IMGUI的初始化
// - 提供IMGUI的功能接口，抽象掉底层
class Imgui_context
{
	bool imgui_sdl2_context_valid = false;
	bool imgui_sdlrenderer2_context_valid = false;

  public:

	Imgui_context(SDL_Window* window, SDL_Renderer* renderer);
	~Imgui_context();

	void new_frame();
	void render(SDL_Renderer* renderer);
	bool process_event(SDL_Event& event);
};
