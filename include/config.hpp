#pragma once

#include <SDL_audio.h>
#include <string_view>

extern "C"
{
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

// 静态配置参数
namespace config
{
	// 音频参数
	namespace audio
	{
		using Buffer_type = float;

		inline static constexpr auto sample_rate = 48000;        // 采样率
		inline static constexpr auto buffer_size = 2048;         // 缓冲区大小
		inline static constexpr auto buffer_format = AUDIO_F32;  // 格式
		inline static constexpr auto channels = 2;               // 双声道
		inline static constexpr auto max_buffer_items = 64;      //  最大可以暂存的音频包个数
		inline static constexpr auto max_buffer_size = buffer_size * max_buffer_items * sizeof(Buffer_type);

		inline static constexpr AVSampleFormat av_format = AV_SAMPLE_FMT_FLT;  // AVCODEC的对应格式
		inline static constexpr AVChannelLayout av_channel_layout = AV_CHANNEL_LAYOUT_STEREO;  // 双声道立体声
	}

	// UI参数
	namespace appearance
	{
		inline static constexpr auto window_title = "Nodey Audio Editor";              // 标题
		inline static constexpr auto toolbar_internal_width = 40;                      // 工具栏内部宽度
		inline static constexpr auto toolbar_margin = 30;                              // 工具栏边距
		inline static constexpr auto node_editor_minimap_fraction = 0.15;              // 节点编辑器小地图占比
		inline static constexpr auto min_window_width = 800, min_window_height = 600;  // 最小窗口大小
	};

	// 逻辑参数
	namespace logic
	{
		const std::string_view audio_input_node_name = "audio_input";
		const std::string_view audio_output_node_name = "audio_output";
	}

	namespace app
	{
		const std::string_view source_page = "https://github.com/Stehsaer/nodey-audio-editor";  // 源码页面
	}
}

// 运行时参数
namespace runtime_config
{
	// UI参数
	extern float ui_scale;
}
