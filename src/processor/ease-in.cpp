#include "processor/ease-in.hpp"
#include "config.hpp"
#include "imgui.h"
#include "utility/free-utility.hpp"
#include "utility/imgui-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <iostream>
#include <limits>
#include <print>
#include <stdlib.h>
#include <vector>

namespace processor
{
	infra::Processor::Info Ease_in::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "Ease_in",
			.display_name = "Ease in",
			.singleton = false,
			.generate = std::make_unique<Ease_in>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Ease_in::get_pin_attributes() const
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
	void Ease_in::change_volume(
		uint8_t* const* dst,
		const uint8_t* const* src,
		int channel_count,
		int element_count
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
			for (int i = 0; i < element_count; i++)
			{
				typed_dst[ch][i] *= start_volume;
				if (start_volume <= 1.0f) start_volume += volume_rate;
			}
		}
	};

	void Ease_in::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Ease in processor has no input",
				"Ease in processor requires an audio stream input to function properly.",
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
				change_volume<float>(dst_data, src_data, 1, frame_sample_element_count * frame_channels);
				break;
			case AV_SAMPLE_FMT_FLTP:
				change_volume<float>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S16:
				change_volume<int16_t>(dst_data, src_data, 1, frame_sample_element_count * frame_channels);
				break;
			case AV_SAMPLE_FMT_S16P:
				change_volume<int16_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S32:
				change_volume<int32_t>(dst_data, src_data, 1, frame_sample_element_count * frame_channels);
				break;
			case AV_SAMPLE_FMT_S32P:
				change_volume<int32_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
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

	void Ease_in::draw_title()
	{
		imgui_utility::shadowed_text("Esae in");
	}

	bool Ease_in::draw_content(bool readonly)
	{
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::SetNextItemWidth(200);
			ImGui::DragFloat("Temp Volume", &this->start_volume, 0.001, 0.0, 1.0f, "%.2f");
			ImGui::SetNextItemWidth(200);
			ImGui::DragFloat("Volume Rate", &this->volume_rate, 0.000001, 0.0, 1.0f, "%.6f");
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	Json::Value Ease_in::serialize() const
	{
		Json::Value value;
		value["start volume"] = start_volume;
		value["volume rate"] = volume_rate;
		return value;
	}

	void Ease_in::deserialize(const Json::Value& value)
	{
		if (!value.isMember("start volume"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: start volume"
			);

		if (!value["start volume"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: start volume"
			);

		start_volume = value["start volume"].asFloat();

		if (!value.isMember("volume rate"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: volume rate"
			);

		if (!value["volume rate"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: volume rate"
			);

		volume_rate = value["volume rate"].asFloat();
	}
}