// ease-out.hpp
// 包装了ffmpeg中的缓入

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
	class Ease_out : public infra::Processor
	{
		float start_volume = 1.f;
		float volume_rate = 0.000001f;
		float time = 0.0f;
		std::unique_ptr<bool> initial;

	  public:

		Ease_out() = default;
		virtual ~Ease_out() = default;

		Ease_out(const Ease_out&) = delete;
		Ease_out(Ease_out&&) = default;
		Ease_out& operator=(const Ease_out&) = delete;
		Ease_out& operator=(Ease_out&&) = default;

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
		void change_volume(
			uint8_t* const* dst,
			const uint8_t* const* src,
			int channel_count,
			int element_count
		);
	};

}