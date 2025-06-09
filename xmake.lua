add_rules("mode.debug", "mode.release")

add_requires("imgui", {alias="imgui", configs={sdl2=true, sdl2_renderer=true, wchar32=true}})
add_requires("jsoncpp")
add_requires("libsdl2")
add_requires("ffmpeg", {configs={shared=true, ffmpeg=false}})
add_requires("boost", {configs={cmake=false, fiber=true}})
add_requires("nativefiledialog")

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