// audio-vol.hpp
// 包装了ffmpeg中的音量调节

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
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include <boost/fiber/buffered_channel.hpp>
#include <expected>
#include <list>

namespace processor
{

	// 音量调节处理器
	// - 负责更改音频音量
	class Audio_amix : public infra::Processor
	{
		
		int input_num = 2;
		std::vector<infra::Processor::Pin_attribute> input_pins;
		std::vector<float> volumes;
		std::vector<bool> locks;

	  public:

		Audio_amix();
		virtual ~Audio_amix() = default;

		Audio_amix(const Audio_amix&) = delete;
		Audio_amix(Audio_amix&&) = default;
		Audio_amix& operator=(const Audio_amix&) = delete;
		Audio_amix& operator=(Audio_amix&&) = default;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;

		void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		);

		virtual Json::Value serialize() const;
		virtual void deserialize(const Json::Value& value);

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};

}