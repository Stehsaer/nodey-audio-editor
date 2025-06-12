#include "processor/audio-vol.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "utility/free-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <iostream>
#include <limits>
#include <nfd.h>
#include <print>
#include <stdlib.h>
#include <vector>

template <typename T>
static T apply_volume(T sample, float volume)
{
	constexpr T max_val = (std::numeric_limits<T>::max)();
	constexpr T min_val = (std::numeric_limits<T>::min)();
	float scaled = static_cast<float>(sample) * volume;
	if (scaled > static_cast<float>(max_val))
	{
		scaled = static_cast<float>(max_val);
	}
	else if (scaled < static_cast<float>(min_val))
	{
		scaled = static_cast<float>(min_val);
	}
	return static_cast<T>(scaled);
}

namespace processor
{
	infra::Processor::Info Audio_vol::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_vol",
			.display_name = "Audio Volume",
			.singleton = false,
			.generate = std::make_unique<Audio_vol>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_vol::get_pin_attributes() const
	{

		return std::vector<infra::Processor::Pin_attribute>{
			{.identifier = "output",
			 .display_name = "Output",
			 .type = typeid(Audio_stream),
			 .is_input = false,
			 .generate_func =
				 []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
			{.identifier = "input",
			 .display_name = "Input",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }}
		};
	}

	void Audio_vol::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	) const
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Audio output processor has no input",
				"Audio output processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_item = input_item_optional.value().get();

		/* 接受数据帧 */

		auto push_frame = [&stop_token, &output_item](const std::shared_ptr<Audio_frame>& frame)
		{
			for (auto& channel : output_item)
			{
				if (stop_token) return;

				while (channel->try_push(frame) != boost::fibers::channel_op_status::success)
				{
					if (stop_token) return;
					boost::this_fiber::yield();
				}
			}
		};

		while (!stop_token)
		{
			// 获取数据
			std::shared_ptr<Audio_frame> new_frame = std::make_shared<Audio_frame>();

			const auto pop_result = input_item.try_pop();

			if (!pop_result.has_value())
			{
				if (pop_result.error() == boost::fibers::channel_op_status::empty)
				{
					if (input_item.eof()) break;
					boost::this_fiber::yield();
					continue;
				}
				else if (pop_result.error() == boost::fibers::channel_op_status::closed)
					THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
			}

			// 获取帧参数

			const auto& frame_shared_ptr = pop_result.value();
			const auto& frame = *frame_shared_ptr->data();

			const auto frame_sample_rate = frame.sample_rate;
			const auto frame_sample_element_count = frame.nb_samples;
			const auto frame_channels = frame.ch_layout.nb_channels;

			AVFrame* out_frame = new_frame->data();

			out_frame->sample_rate = frame.sample_rate;
			out_frame->format = frame.format;
			out_frame->nb_samples = frame.nb_samples;
			out_frame->ch_layout = frame.ch_layout;
			out_frame->pts = frame.pts;

			av_frame_get_buffer(out_frame, 32);

			av_frame_make_writable(out_frame);

			if (frame_channels != 1 && frame_channels != 2)
				throw Runtime_error(
					"Invalid channel count",
					"Only mono and stereo audio are supported.",
					std::format("Got {} channels", frame_channels)
				);

			uint8_t** data = frame.extended_data;
			int bytes_per_sample = av_get_bytes_per_sample(static_cast<enum AVSampleFormat>(frame.format));
			AVSampleFormat format = static_cast<AVSampleFormat>(frame.format);

			for (int i = 0; i < out_frame->nb_samples; i++)
			{
				for (int ch = 0; ch < frame_channels; ch++)
				{
					switch (format)
					{
					case AV_SAMPLE_FMT_FLT:
					case AV_SAMPLE_FMT_FLTP:
					{
						float* sample_ptr = (float*)(data[ch] + i * bytes_per_sample);
						*sample_ptr *= vol;
						if (*sample_ptr < -1.0f) *sample_ptr = -1.0f;
						if (*sample_ptr > 1.0f) *sample_ptr = 1.0f;
						memcpy(out_frame->data[ch] + i * bytes_per_sample, sample_ptr, bytes_per_sample);
						break;
					}
					case AV_SAMPLE_FMT_S16:
					case AV_SAMPLE_FMT_S16P:
					{
						int16_t* sample_ptr = (int16_t*)(data[ch] + i * bytes_per_sample);
						*sample_ptr = apply_volume(*sample_ptr, vol);
						memcpy(out_frame->data[ch] + i * bytes_per_sample, sample_ptr, bytes_per_sample);
						break;
					}
					case AV_SAMPLE_FMT_S32:
					case AV_SAMPLE_FMT_S32P:
					{
						int32_t* sample_ptr = (int32_t*)(data[ch] + i * bytes_per_sample);
						*sample_ptr = apply_volume(*sample_ptr, vol);
						memcpy(out_frame->data[ch] + i * bytes_per_sample, sample_ptr, bytes_per_sample);
						break;
					}
					default:
						throw Runtime_error(
							"Audio format is not support",
							"Audio volume processor requires an audio format properly.",
							"Include FLT, S16, S32"
						);
					}
				}
			}
			push_frame(new_frame);
		}
	}

	void Audio_vol::draw_title()
	{
		imgui_utility::shadowed_text("Audio Volume");
	}

	bool Audio_vol::draw_content(bool readonly)
	{
		ImGui::SetNextItemWidth(200);
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::DragFloat(
				"Volume",
				&this->vol,
				0.01,
				0.0,
				config::processor::audio_volume::max_volume,
				"%.2f"
			);
			vol = std::clamp<float>(vol, 0, config::processor::audio_volume::max_volume);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}
}