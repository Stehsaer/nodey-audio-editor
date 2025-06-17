#include "processor/audio-vol.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "utility/free-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <iostream>
#include <limits>
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
			.identifier = "audio_volume_adjust",
			.display_name = "Adjust Volume",
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

	template <typename T>
	static void change_volume(
		uint8_t* const* dst,
		const uint8_t* const* src,
		int channel_count,
		int element_count,
		float volume
	)
	{
		const auto typed_dst = reinterpret_cast<T* const*>(dst);
		const auto typed_src = reinterpret_cast<const T* const*>(src);

		for (int ch = 0; ch < channel_count; ch++)
		{
			[[assume(typed_dst[ch] != nullptr)]];
			[[assume(typed_src[ch] != nullptr)]];
			[[assume(element_count > 0)]];
			[[assume(uintptr_t(typed_dst[ch]) % 32 == 0)]];

			// 把原数据先拷贝到目标数据中
			std::copy(typed_src[ch], typed_src[ch] + element_count, typed_dst[ch]);

			// 更改音量
			for (int i = 0; i < element_count; i++) typed_dst[ch][i] *= volume;
		}
	};

	void Audio_vol::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	) const
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Volume adjust processor has no input",
				"Volume adjust processor requires an audio stream input to function properly.",
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
			std::shared_ptr<Audio_frame> dst_frame = std::make_shared<Audio_frame>();

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
			const auto& src_frame = *frame_shared_ptr->data();

			const auto frame_sample_element_count = src_frame.nb_samples;
			const auto frame_channels = src_frame.ch_layout.nb_channels;

			AVFrame* out_frame = dst_frame->data();

			out_frame->sample_rate = src_frame.sample_rate;
			out_frame->format = src_frame.format;
			out_frame->nb_samples = src_frame.nb_samples;
			out_frame->ch_layout = src_frame.ch_layout;
			out_frame->pts = src_frame.pts;

			av_frame_get_buffer(out_frame, 32);

			av_frame_make_writable(out_frame);

			if (frame_channels != 1 && frame_channels != 2)
				throw Runtime_error(
					"Invalid channel count",
					"Only mono and stereo audio are supported.",
					std::format("Got {} channels", frame_channels)
				);

			const auto format = static_cast<AVSampleFormat>(src_frame.format);
			const uint8_t* const* src_data = src_frame.data;
			uint8_t* const* dst_data = out_frame->data;

			switch (format)
			{
			case AV_SAMPLE_FMT_FLT:
				change_volume<float>(
					dst_data,
					src_data,
					1,
					frame_sample_element_count * frame_channels,
					volume
				);
				break;
			case AV_SAMPLE_FMT_FLTP:
				change_volume<float>(dst_data, src_data, frame_channels, frame_sample_element_count, volume);
				break;
			case AV_SAMPLE_FMT_S16:
				change_volume<int16_t>(
					dst_data,
					src_data,
					1,
					frame_sample_element_count * frame_channels,
					volume
				);
				break;
			case AV_SAMPLE_FMT_S16P:
				change_volume<int16_t>(
					dst_data,
					src_data,
					frame_channels,
					frame_sample_element_count,
					volume
				);
				break;
			case AV_SAMPLE_FMT_S32:
				change_volume<int32_t>(
					dst_data,
					src_data,
					1,
					frame_sample_element_count * frame_channels,
					volume
				);
				break;
			case AV_SAMPLE_FMT_S32P:
				change_volume<int32_t>(
					dst_data,
					src_data,
					frame_channels,
					frame_sample_element_count,
					volume
				);
				break;
			default:
				throw Runtime_error(
					"Audio format is not support",
					"Audio volume processor requires an audio format properly.",
					"Include FLT, S16, S32"
				);
			}

			push_frame(dst_frame);
		}

		for (auto& channel : output_item) channel->set_eof();
	}

	void Audio_vol::draw_title()
	{
		imgui_utility::shadowed_text("Audio Volume");
	}

	bool Audio_vol::draw_content(bool readonly)
	{
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::SetNextItemWidth(200);
			ImGui::DragFloat(
				"Volume",
				&this->volume,
				0.01,
				0.0,
				config::processor::audio_volume::max_volume,
				"%.2f"
			);
			volume = std::clamp<float>(volume, 0, config::processor::audio_volume::max_volume);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}
}