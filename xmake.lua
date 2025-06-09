add_rules("mode.debug", "mode.release")

add_requires("imgui v1.91.9",			{system=false, configs={sdl2=true, sdl2_renderer=true, wchar32=true}})
add_requires("jsoncpp 1.9.6",			{system=false})
add_requires("libsdl2 2.32.2",			{system=false, configs={shared=true, sdl2_image=false, sdl2_mixer=true, sdl2_ttf=false}})
add_requires("ffmpeg 7.1",				{system=false, configs={shared=true, ffmpeg=false}})
add_requires("boost 1.88.0",			{system=false, configs={cmake=false, fiber=true}})
add_requires("nativefiledialog 1.1.6",	{system=false})

includes("third-party")

target("nodey_audio")
	set_kind("binary")
	set_languages("c++23")
	set_version("0.0")

	add_deps("imgui-knobs", "imnodes")
	add_packages(
		"libsdl2", 
		"imgui", 
		"jsoncpp", 
		"ffmpeg", 
		"boost", 
		"nativefiledialog"
	)
	
	add_files("src/**.cpp")
	add_files("src/asset/*.c")
	add_includedirs("include")
	add_files("assets/font.ttf", "assets/icon.ttf", {rule="utils.bin2c"})

	-- 修复Windows UTF-8
	if is_plat("windows") then
		add_cxflags("/utf-8")
	end
target_end()

includes("@builtin/xpack")

xpack("nodey_audio")
	set_title("Nodey Audio")
	set_description("A node-based audio processing application")
	set_formats("nsis", "zip", "deb")
	add_targets("nodey_audio")