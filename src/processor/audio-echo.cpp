#include "processor/audio-echo.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "utility/free-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <stdlib.h>
#include <vector>

namespace processor
{
	infra::Processor::Info Audio_echo::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_echo",
			.display_name = "Audio Echo",
			.singleton = false,
			.generate = std::make_unique<Audio_echo>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_echo::get_pin_attributes() const
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
	void Audio_echo::apply_echo_plannar(
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
		}

		if (echo_buffer_1.size() < echo_distance) return;
		if (channel_count == 1)
		{
			while (element_count > echo_distance)
			{
				for (int i = 0; i < echo_distance; i++) typed_dst[0][i] += echo_volume * echo_buffer_1[i];
				echo_buffer_1.clear();
				for (int i = 0; i < echo_distance; i++) echo_buffer_1.push_back(typed_src[0][i]);
				element_count -= echo_distance;
			}
			for (int i = 0; i < element_count; i++) typed_dst[0][i] += echo_volume * echo_buffer_1[i];
			echo_buffer_1.erase(echo_buffer_1.begin(), echo_buffer_1.begin() + element_count);
			for (int i = 0; i < element_count; i++) echo_buffer_1.push_back(typed_src[0][i]);
			return;
		}
		else if (channel_count == 2)
		{
			while (element_count > echo_distance)
			{
				for (int i = 0; i < echo_distance; i++)
				{
					typed_dst[0][i] += echo_volume * echo_buffer_1[i];
					typed_dst[1][i] += echo_volume * echo_buffer_2[i];
				}
				echo_buffer_1.clear();
				echo_buffer_2.clear();
				for (int i = 0; i < echo_distance; i++)
				{
					echo_buffer_1.push_back(typed_src[0][i]);
					echo_buffer_2.push_back(typed_src[1][i]);
				}
				element_count -= echo_distance;
			}
			for (int i = 0; i < element_count; i++)
			{
				typed_dst[0][i] += echo_volume * echo_buffer_1[i];
				typed_dst[1][i] += echo_volume * echo_buffer_2[i];
			}
			echo_buffer_1.erase(echo_buffer_1.begin(), echo_buffer_1.begin() + element_count);
			echo_buffer_2.erase(echo_buffer_2.begin(), echo_buffer_2.begin() + element_count);
			for (int i = 0; i < element_count; i++)
			{
				echo_buffer_1.push_back(typed_src[0][i]);
				echo_buffer_2.push_back(typed_src[1][i]);
			}
		}
	}

	template <typename T>
	void Audio_echo::apply_echo_packed(
		uint8_t* const* dst,
		const uint8_t* const* src,
		int channel_count,
		int element_count
	)
	{
		const auto typed_dst = reinterpret_cast<T* const*>(dst);
		const auto typed_src = reinterpret_cast<const T* const*>(src);

		[[assume(typed_dst[0] != nullptr)]];
		[[assume(typed_src[0] != nullptr)]];
		[[assume(element_count > 0)]];
		[[assume(uintptr_t(typed_dst[0]) % 32 == 0)]];

		// 把原数据先拷贝到目标数据中
		std::copy(typed_src[0], typed_src[0] + element_count * channel_count, typed_dst[0]);

		if (echo_buffer_1.size() < echo_distance) return;
		if (channel_count == 1)
		{
			while (element_count > echo_distance)
			{
				for (int i = 0; i < echo_distance; i++) typed_dst[0][i] += echo_volume * echo_buffer_1[i];
				echo_buffer_1.clear();
				for (int i = 0; i < echo_distance; i++) echo_buffer_1.push_back(typed_src[0][i]);
				element_count -= echo_distance;
			}
			for (int i = 0; i < element_count; i++) typed_dst[0][i] += echo_volume * echo_buffer_1[i];
			echo_buffer_1.erase(echo_buffer_1.begin(), echo_buffer_1.begin() + element_count);
			for (int i = 0; i < element_count; i++) echo_buffer_1.push_back(typed_src[0][i]);
			return;
		}
		else if (channel_count == 2)
		{
			while (element_count > echo_distance)
			{
				for (int i = 0; i < echo_distance; i++)
				{
					typed_dst[0][2 * i] += echo_volume * echo_buffer_1[i];
					typed_dst[0][2 * i + 1] += echo_volume * echo_buffer_2[i];
				}
				echo_buffer_1.clear();
				echo_buffer_2.clear();
				for (int i = 0; i < echo_distance; i++)
				{
					echo_buffer_1.push_back(typed_src[0][2 * i]);
					echo_buffer_2.push_back(typed_src[0][2 * i + 1]);
				}
				element_count -= echo_distance;
			}
			for (int i = 0; i < element_count; i++)
			{
				typed_dst[0][2 * i] += echo_volume * echo_buffer_1[i];
				typed_dst[0][2 * i + 1] += echo_volume * echo_buffer_2[i];
			}
			echo_buffer_1.erase(echo_buffer_1.begin(), echo_buffer_1.begin() + element_count);
			echo_buffer_2.erase(echo_buffer_2.begin(), echo_buffer_2.begin() + element_count);
			for (int i = 0; i < element_count; i++)
			{
				echo_buffer_1.push_back(typed_src[0][2 * i]);
				echo_buffer_2.push_back(typed_src[0][2 * i + 1]);
			}
		}
	}

	void Audio_echo::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		std::unique_ptr<bool> initial;

		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				" Audio echo processor has no input",
				"Audio echo adjust processor requires an audio stream input to function properly.",
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
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((float*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((float*)src_frame.data[0])[2 * i]);
							echo_buffer_2.push_back(((float*)src_frame.data[0])[2 * i + 1]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_packed<float>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_FLTP:
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((float*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((float*)src_frame.data[0])[i]);
							echo_buffer_2.push_back(((float*)src_frame.data[1])[i]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_plannar<float>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S16:
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((int16_t*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((int16_t*)src_frame.data[0])[2 * i]);
							echo_buffer_2.push_back(((int16_t*)src_frame.data[0])[2 * i + 1]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_packed<int16_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S16P:
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((int16_t*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((int16_t*)src_frame.data[0])[i]);
							echo_buffer_2.push_back(((int16_t*)src_frame.data[1])[i]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_plannar<int16_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S32:
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((int32_t*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((int32_t*)src_frame.data[0])[2 * i]);
							echo_buffer_2.push_back(((int32_t*)src_frame.data[0])[2 * i + 1]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_packed<int32_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			case AV_SAMPLE_FMT_S32P:
				if (!initial)
				{
					if (frame_channels == 1)
					{
						for (int i = 0; i < frame_sample_element_count; i++)
							echo_buffer_1.push_back(((int32_t*)src_frame.data[0])[i]);
					}
					else
					{
						for (int i = 0; i < frame_sample_element_count; i++)
						{
							echo_buffer_1.push_back(((int32_t*)src_frame.data[0])[i]);
							echo_buffer_2.push_back(((int32_t*)src_frame.data[1])[i]);
						}
					}
					if (echo_buffer_1.size() > echo_distance) initial = std::make_unique<bool>();
				}
				apply_echo_plannar<int32_t>(dst_data, src_data, frame_channels, frame_sample_element_count);
				break;
			default:
				throw Runtime_error(
					"Audio format is not support",
					"Audio echoume processor requires an audio format properly.",
					"Include FLT, S16, S32"
				);
			}

			push_frame(dst_frame);
		}

		for (auto& channel : output_item) channel->set_eof();
	}

	void Audio_echo::draw_title()
	{
		imgui_utility::shadowed_text("Audio echo");
	}

	bool Audio_echo::draw_content(bool readonly)
	{
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::SetNextItemWidth(200);
			ImGui::DragInt("echo distance", &this->echo_distance, 500, 5000, 100000);
			ImGui::SetNextItemWidth(200);
			ImGui::DragFloat("echo volume", &this->echo_volume, 0.01f, 0.0f, 1.0f);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	Json::Value Audio_echo::serialize() const
	{
		Json::Value value;
		value["echo_distance"] = echo_distance;
		value["echo_volume"] = echo_volume;
		return value;
	}

	void Audio_echo::deserialize(const Json::Value& value)
	{
		if (!value.isMember("echo_distance"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: echo distance"
			);

		if (!value["echo_distance"].isInt())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: echo distance"
			);

		echo_distance = value["echo_distance"].asInt();

		if (!value.isMember("echo_volume"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: echo volume"
			);

		if (!value["echo_volume"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: echo volume"
			);

		echo_volume = value["echo_volume"].asFloat();
	}
}