#pragma once

#include "infra/processor.hpp"
#include "processor/audio-stream.hpp"
#include "third-party/ui.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
}

namespace processor
{
	class Display_waveform : public infra::Processor
	{

		int display_sample_count = 50000;
		std::mutex waveform_lock;
		std::vector<float> waveform1;
		std::vector<float> waveform2;

	  public:

		Display_waveform() = default;
		virtual ~Display_waveform() = default;

		Display_waveform(const Display_waveform&) = delete;
		Display_waveform(Display_waveform&&) = delete;
		Display_waveform& operator=(const Display_waveform&) = delete;
		Display_waveform& operator=(Display_waveform&&) = delete;

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
	};
}