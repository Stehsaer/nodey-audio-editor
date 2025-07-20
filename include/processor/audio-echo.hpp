// audio-echo.hpp
// 包装了ffmpeg中的回音节点

#pragma once

#include "infra/processor.hpp"
#include "processor/audio-stream.hpp"
#include "third-party/ui.hpp"

#include <SDL_audio.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
}

#include <boost/fiber/buffered_channel.hpp>
#include <expected>
#include <list>

namespace processor
{

	// 音量调节处理器
	// - 负责更改音频音量
	class Audio_echo : public infra::Processor
	{
		int echo_distance = 20000;
		float echo_volume = 0.3f;
		std::vector<double> echo_buffer_1, echo_buffer_2;

	  public:

		Audio_echo() = default;
		virtual ~Audio_echo() = default;

		Audio_echo(const Audio_echo&) = delete;
		Audio_echo(Audio_echo&&) = default;
		Audio_echo& operator=(const Audio_echo&) = delete;
		Audio_echo& operator=(Audio_echo&&) = default;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		);

		virtual Json::Value serialize() const;
		virtual void deserialize(const Json::Value& value);

		virtual void draw_title();
		virtual bool draw_content(bool readonly);

		template <typename T>
		void apply_echo_plannar(
			uint8_t* const* dst,
			const uint8_t* const* src,
			int channel_count,
			int element_count
		);

		template <typename T>
		void apply_echo_packed(
			uint8_t* const* dst,
			const uint8_t* const* src,
			int channel_count,
			int element_count
		);
	};

}