#include "processor/display-waveform.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "utility/free-utility.hpp"

#include "json/value.h"
#include <algorithm>
#include <boost/fiber/operations.hpp>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <vector>

namespace processor
{

	infra::Processor::Info Display_waveform::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "Display_waveform",
			.display_name = "Waveform",
			.singleton = false,
			.generate = std::make_unique<Display_waveform>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Display_waveform::get_pin_attributes() const
	{

		return std::vector<infra::Processor::Pin_attribute>{
			{.identifier = "input",
			 .display_name = "Input_Channel",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }}
		};
	}

	void Display_waveform::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Display waveform processor has no input",
				"Display waveform processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_item = input_item_optional.value().get();

		while (!stop_token)
		{
			// 获取数据

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
			const auto& frame_shared_ptr = pop_result.value();
			const auto& frame = *frame_shared_ptr->data();

			const auto frame_sample_rate = frame.sample_rate;
			const auto frame_sample_element_count = frame.nb_samples;
			const auto frame_channels = frame.ch_layout.nb_channels;
			uint8_t** data = frame.extended_data;

			if (frame_channels == 1)
			{
				waveform2.clear();
			}

			if (frame_channels > 2)
				throw Runtime_error(
					"Unsupported audio channel count!",
					"Audio channels above 2 are unsupported",
					std::format("Got {} channels", frame_channels)
				);

			if (frame_channels == 1)
			{
				waveform1.resize(display_sample_count);
				std::move(
					waveform1.begin(),
					waveform1.end() - frame_sample_element_count,
					waveform1.begin() + frame_sample_element_count
				);

				for (int i = 0; i < frame_sample_element_count; i++)
				{
					switch (frame.format)
					{
					case AV_SAMPLE_FMT_FLT:
					case AV_SAMPLE_FMT_FLTP:
					{
						std::copy(
							(float*)data[0],
							(float*)data[0] + frame_sample_element_count,
							waveform1.begin()
						);
						break;
					}
					case AV_SAMPLE_FMT_S16:
					case AV_SAMPLE_FMT_S16P:
					{
						std::transform(
							(int16_t*)data[0],
							(int16_t*)data[0] + frame_sample_element_count,
							waveform1.begin(),
							[](int16_t value) { return value / 32768.0f; }
						);
						break;
					}
					case AV_SAMPLE_FMT_S32:
					case AV_SAMPLE_FMT_S32P:
					{
						std::transform(
							(int32_t*)data[0],
							(int32_t*)data[0] + frame_sample_element_count,
							waveform1.begin(),
							[](int32_t value) { return value / (float)std::numeric_limits<int32_t>::max(); }
						);
						break;
					}
					default:
						throw Runtime_error(
							"Audio format is not supported",
							"Audio volume processor requires an audio format properly.",
							"Include FLT, S16, S32"
						);
					}
				}
			}
			else if (frame_channels == 2)
			{
				waveform1.resize(display_sample_count);
				std::move(
					waveform1.begin(),
					waveform1.end() - frame_sample_element_count,
					waveform1.begin() + frame_sample_element_count
				);

				waveform2.resize(display_sample_count);
				std::move(
					waveform2.begin(),
					waveform2.end() - frame_sample_element_count,
					waveform2.begin() + frame_sample_element_count
				);

				for (int i = 0; i < frame_sample_element_count; i++)
				{
					switch (frame.format)
					{
					case AV_SAMPLE_FMT_FLT:
					{
						auto audio_data
							= std::span((float*)data[0], (float*)data[0] + 2 * frame_sample_element_count);
						auto left_view = audio_data | std::views::stride(2);
						auto right_view = audio_data | std::views::drop(1) | std::views::stride(2);

						std::ranges::copy(left_view, waveform1.begin());
						std::ranges::copy(right_view, waveform2.begin());

						break;
					}
					case AV_SAMPLE_FMT_FLTP:
					{
						std::copy(
							(float*)data[0],
							(float*)data[0] + frame_sample_element_count,
							waveform1.begin()
						);
						std::copy(
							(float*)data[1],
							(float*)data[1] + frame_sample_element_count,
							waveform2.begin()
						);
						break;
					}
					case AV_SAMPLE_FMT_S16:
					{
						auto audio_data = std::span(
							(int16_t*)data[0],
							(int16_t*)data[0] + 2 * frame_sample_element_count
						);
						auto left_view = audio_data | std::views::stride(2);
						auto right_view = audio_data | std::views::drop(1) | std::views::stride(2);
						auto transform = std::views::transform(
							[](int16_t value) { return value / (float)std::numeric_limits<int16_t>::max(); }
						);

						std::ranges::copy(left_view | transform, waveform1.begin());
						std::ranges::copy(right_view | transform, waveform2.begin());

						break;
					}
					case AV_SAMPLE_FMT_S16P:
					{
						std::transform(
							(int16_t*)data[0],
							(int16_t*)data[0] + frame_sample_element_count,
							waveform1.begin(),
							[](int16_t value) { return value / 32768.0f; }
						);
						std::transform(
							(int16_t*)data[1],
							(int16_t*)data[1] + frame_sample_element_count,
							waveform2.begin(),
							[](int16_t value) { return value / 32768.0f; }
						);
						break;
					}
					case AV_SAMPLE_FMT_S32:
					{
						auto audio_data = std::span(
							(int32_t*)data[0],
							(int32_t*)data[0] + 2 * frame_sample_element_count
						);
						auto left_view = audio_data | std::views::stride(2);
						auto right_view = audio_data | std::views::drop(1) | std::views::stride(2);
						auto transform = std::views::transform(
							[](int32_t value) { return value / (float)std::numeric_limits<int32_t>::max(); }
						);

						std::ranges::copy(left_view | transform, waveform1.begin());
						std::ranges::copy(right_view | transform, waveform2.begin());

						break;
					}
					case AV_SAMPLE_FMT_S32P:
					{
						std::transform(
							(int32_t*)data[0],
							(int32_t*)data[0] + frame_sample_element_count,
							waveform1.begin(),
							[](int32_t value) { return value / (float)std::numeric_limits<int32_t>::max(); }
						);
						std::transform(
							(int32_t*)data[1],
							(int32_t*)data[1] + frame_sample_element_count,
							waveform2.begin(),
							[](int32_t value) { return value / (float)std::numeric_limits<int32_t>::max(); }
						);
						break;
					}
					default:
						throw Runtime_error(
							"Audio format is not supported",
							"Audio volume processor requires an audio format properly.",
							"Include FLT, S16, S32"
						);
					}
				}
			}
		}
	}

	void Display_waveform::draw_title()
	{
		imgui_utility::shadowed_text("Waveform");
	}

	bool Display_waveform::draw_content(bool readonly)
	{
		ImGui::SetNextItemWidth(200);
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			if (!waveform1.empty())
			{
				ImGui::PlotLines(
					waveform2.empty() ? "" : "Left Channel",
					waveform1.data(),
					waveform1.size(),
					0,
					0,
					-1,
					1,
					ImVec2(500, 100)
				);
			}

			if (!waveform2.empty())
			{
				ImGui::PlotLines(
					"Right Channel",
					waveform2.data(),
					waveform2.size(),
					0,
					0,
					-1,
					1,
					ImVec2(500, 100)
				);
			}

			if (ImGui::InputInt("Windows Size", &display_sample_count, 5000))
				display_sample_count = std::max(5000, display_sample_count);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	Json::Value Display_waveform::serialize() const
	{
		Json::Value value;
		value["display_sample_count"] = display_sample_count;
		return value;
	}

	void Display_waveform::deserialize(const Json::Value& value)
	{
		if (!value.isMember("display_sample_count"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: display_sample_count"
			);

		if (!value["display_sample_count"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: display_sample_count"
			);

		display_sample_count = value["display_sample_count"].asFloat();
		display_sample_count = std::max<float>(0.f, display_sample_count);
	}
}