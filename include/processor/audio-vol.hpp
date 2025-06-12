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
	class Audio_vol : public infra::Processor
	{
		float vol = 1.0;

	  public:

		Audio_vol() = default;
		virtual ~Audio_vol() = default;

		Audio_vol(const Audio_vol&) = delete;
		Audio_vol(Audio_vol&&) = default;
		Audio_vol& operator=(const Audio_vol&) = delete;
		Audio_vol& operator=(Audio_vol&&) = default;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		) const;

		virtual Json::Value serialize() const { return {}; }
		virtual void deserialize(const Json::Value& value) {}

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};

}