#include "processor/display-spectrum.hpp"
#include "imgui.h"
#include "utility/imgui-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <cmath>
#include <cstdint>
#include <fftw3.h>
#include <format>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

static void fftf(const std::vector<float>& samples, std::vector<float>& spectrum)
{
	int N = samples.size();
	auto* in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * N);
	auto* out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * N);
	fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, (float*)samples.data(), out, FFTW_ESTIMATE);
	fftwf_execute(plan);
	for (int i = 0; i < N; ++i)
	{
		spectrum[i] = std::log2f(out[i][0] * out[i][0] + out[i][1] * out[i][1] + 0.00001);
	}
	fftwf_destroy_plan(plan);
	fftwf_free(in);
	fftwf_free(out);
}

namespace processor
{

	infra::Processor::Info Display_spectrum::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "Display_spectrum",
			.display_name = "Spectrum",
			.singleton = false,
			.generate = std::make_unique<Display_spectrum>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Display_spectrum::get_pin_attributes() const
	{

		return std::vector<infra::Processor::Pin_attribute>{
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

	void Display_spectrum::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Display spectrum processor has no input",
				"Display spectrum processor requires an audio stream input to function properly.",
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

			if (!sample_rate) sample_rate = frame.sample_rate;

			const auto frame_sample_rate = frame.sample_rate;
			const auto frame_sample_element_count = frame.nb_samples;
			const auto frame_channels = frame.ch_layout.nb_channels;
			uint8_t** data = frame.extended_data;

			std::lock_guard lock(spectrum_lock);

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
				waveform1.resize(config::processor::display_spectrum::spectrum_size);
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
				waveform1.resize(config::processor::display_spectrum::spectrum_size);
				std::move(
					waveform1.begin(),
					waveform1.end() - frame_sample_element_count,
					waveform1.begin() + frame_sample_element_count
				);

				waveform2.resize(config::processor::display_spectrum::spectrum_size);
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

	void Display_spectrum::draw_title()
	{
		imgui_utility::shadowed_text("Spectrum Display");
	}

	void Display_spectrum::draw_node_content()
	{
		ImGui::PushItemWidth(200);
		ImGui::BeginGroup();
		{
			if (waveform1.size() > config::processor::display_spectrum::spectrum_size / 2)
			{
				std::vector<float> spectrum_l(waveform1.size());
				std::lock_guard lock(spectrum_lock);
				fftf(waveform1, spectrum_l);

				ImGui::PlotLines(
					waveform2.empty() ? "" : "Left Channel",
					spectrum_l.data(),
					spectrum_l.size() / 2,
					0,
					0,
					FLT_MAX,
					FLT_MAX,
					ImVec2(400, 200)
				);
			}

			if (waveform2.size() > config::processor::display_spectrum::spectrum_size / 2)
			{
				std::vector<float> spectrum_r(waveform2.size());
				std::lock_guard lock(spectrum_lock);
				fftf(waveform2, spectrum_r);

				ImGui::PlotLines(
					"Right Channel",
					spectrum_r.data(),
					spectrum_r.size() / 2,
					0,
					0,
					FLT_MAX,
					FLT_MAX,
					ImVec2(400, 200)
				);
			}

			if (ImGui::InputInt("Windows Size", &display_sample_count, 5000))
				display_sample_count = std::max(5000, display_sample_count);
		}
		ImGui::EndGroup();
		ImGui::PopItemWidth();
	}

	Json::Value Display_spectrum::serialize() const
	{
		Json::Value value;
		value["display_sample_count"] = display_sample_count;
		return value;
	}

	void Display_spectrum::deserialize(const Json::Value& value)
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