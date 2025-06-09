#include "frontend/imgui-context.hpp"
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "config.hpp"
#include "frontend/font.hpp"
#include "frontend/nerdfont.hpp"

// 加载字体
static void load_font(float scale)
{
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig config;
	config.FontDataOwnedByAtlas = false;

	config.MergeMode = false;
	io.Fonts->AddFontFromMemoryTTF((void*)font_data, font_data_size, 18.0f * scale, &config);

	const ImWchar icon_range[] = {NF_FA_REGION0, NF_FA_REGION1, NF_MATERIAL_REGION, 0};

	config.MergeMode = true;
	config.OversampleH = 1;
	config.OversampleV = 1;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromMemoryTTF((void*)icon_data, icon_data_size, 18.0f * scale, &config, icon_range);

	io.Fonts->Build();
}

// 设置IMGUI和ImNodes的样式
static void set_style()
{
	ImGui::StyleColorsDark();
	ImNodes::StyleColorsDark();
	load_font(runtime_config::ui_scale);

	ImGuiStyle& style = ImGui::GetStyle();
	style.ChildRounding = 4;
	style.FrameRounding = 4;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 4;
	style.TabRounding = 4;
	style.WindowRounding = 4;
	style.GrabRounding = 4;
	style.Colors[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.5};
	style.ChildBorderSize = 1;

	ImNodesStyle& imnodes_style = ImNodes::GetStyle();
	ImNodes::GetIO().AltMouseButton = ImGuiMouseButton_Middle;  // 鼠标中键也可以移动

	imnodes_style.Flags &= ~ImNodesStyleFlags_GridSnapping;
	imnodes_style.Flags |= ImNodesStyleFlags_GridLines;
	imnodes_style.Flags |= ImNodesStyleFlags_GridLinesPrimary;

	imnodes_style.Colors[ImNodesCol_NodeBackgroundHovered] = imnodes_style.Colors[ImNodesCol_NodeBackground];
	imnodes_style.Colors[ImNodesCol_NodeBackgroundSelected] = imnodes_style.Colors[ImNodesCol_NodeBackground];

	imnodes_style.NodeCornerRounding = 4 * runtime_config::ui_scale;
	imnodes_style.PinCircleRadius = 4 * runtime_config::ui_scale;
	imnodes_style.PinQuadSideLength = 4 * runtime_config::ui_scale;
	imnodes_style.PinTriangleSideLength = 4 * runtime_config::ui_scale;
	imnodes_style.LinkThickness *= runtime_config::ui_scale;
	imnodes_style.PinLineThickness *= runtime_config::ui_scale;
	imnodes_style.NodeBorderThickness = 2 * runtime_config::ui_scale;
	style.ScaleAllSizes(runtime_config::ui_scale);
}

Imgui_context::Imgui_context(SDL_Window* window, SDL_Renderer* renderer)
{
	ImGui::CreateContext();
	ImNodes::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	set_style();

	imgui_sdl2_context_valid = ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	if (!imgui_sdl2_context_valid) throw std::runtime_error("Failed to initialize ImGui SDL2 context");

	imgui_sdlrenderer2_context_valid = ImGui_ImplSDLRenderer2_Init(renderer);
	if (!imgui_sdlrenderer2_context_valid)
		throw std::runtime_error("Failed to initialize ImGui SDLRenderer2 context");
}

void Imgui_context::new_frame()
{
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplSDLRenderer2_NewFrame();
	ImGui::NewFrame();
}

void Imgui_context::render(SDL_Renderer* renderer)
{
	auto& io = ImGui::GetIO();

	ImGui::Render();
	SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);
	ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
	SDL_RenderPresent(renderer);
}

bool Imgui_context::process_event(SDL_Event& event)
{
	return ImGui_ImplSDL2_ProcessEvent(&event);
}

Imgui_context::~Imgui_context()
{
	if (imgui_sdlrenderer2_context_valid) ImGui_ImplSDLRenderer2_Shutdown();
	if (imgui_sdl2_context_valid) ImGui_ImplSDL2_Shutdown();

	ImNodes::DestroyContext();
	ImGui::DestroyContext();
}