add_rules("mode.debug", "mode.release")

add_requires("imgui v1.91.9",			{system=false, configs={sdl2=true, sdl2_renderer=true, wchar32=true}})
add_requires("jsoncpp 1.9.6",			{system=false})

if is_plat("windows") then
	add_requires("libsdl2 2.32.2",		{system=false, configs={shared=false, sdl2_image=false, sdl2_mixer=true, sdl2_ttf=false}})
else
	add_requires("libsdl2 2.32.2",		{system=false, configs={shared=true, sdl2_image=false, sdl2_mixer=true, sdl2_ttf=false}})
end

add_requires("ffmpeg 7.1",				{system=false, configs={shared=true, ffmpeg=false, gpl=false}})
add_requires("lame",				    {system=false})	
add_requires("boost 1.88.0",			{system=false, configs={cmake=false, fiber=true}})
add_requires("fftw 3.3.10",				{system=false, configs={precision="float"}})
add_requires("soundtouch 2.3.2",		{system=false, configs={shared=true}})

includes("third-party")

target("nodey_audio")
	set_kind("binary")
	set_languages("c++23")
	set_version("0.1")
	set_license("GPL-2.0-or-later")

	add_deps("imnodes", "portable-file-dialogs")
	add_packages(
		"libsdl2", 
		"imgui", 
		"jsoncpp", 
		"ffmpeg", 
		"boost", 
		"fftw",
		"soundtouch",
		"lame"
	)
	
	add_files("src/**.cpp")
	add_files("src/asset/*.c")
	add_includedirs("include")
	add_files("assets/font.ttf", "assets/icon.ttf", {rule="utils.bin2c"})

	-- 修复Windows UTF-8
	if is_plat("windows") then
		add_cxflags("/utf-8")
		add_defines("NOMINMAX")
	end

	if is_mode("debug") then
		add_defines("_DEBUG")
	end
target_end()

includes("@builtin/xpack")

xpack("nodey_audio")
	set_title("Nodey Audio")
	set_description("A node-based audio processing application")
	set_formats("nsis", "zip", "deb")
	add_targets("nodey_audio")