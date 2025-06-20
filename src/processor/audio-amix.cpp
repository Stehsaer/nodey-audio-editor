#include "processor/audio-amix.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "infra/processor.hpp"
#include "utility/free-utility.hpp"

#include <algorithm>
#include <boost/fiber/operations.hpp>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <vector>

namespace processor
{

	Audio_amix::Audio_amix()
	{
		input_pins.push_back(
			{.identifier = "output",
			 .display_name = "Output",
			 .type = typeid(Audio_stream),
			 .is_input = false,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }}
		);
	};

	infra::Processor::Info Audio_amix::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_amix",
			.display_name = "Audio Amix",
			.singleton = false,
			.generate = std::make_unique<Audio_amix>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_amix::get_pin_attributes() const
	{
		return input_pins;
	}

	void Audio_amix::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{

		std::unique_ptr<bool> initial;
		const auto input_num = this->input_num;
		int ret;
		int count = 0;
		double time_seconds = 0;
		std::vector<bool> eofs;
		eofs.resize(input_num);
		std::ranges::fill(eofs, false);

		std::vector<std::vector<std::shared_ptr<const Audio_frame>>> buffers;
		buffers.resize(input_num);

		std::vector<std::reference_wrapper<Audio_stream>> input_items;
		input_items.reserve(input_num);

		std::vector<SwrContext*> resamplers;
		resamplers.resize(input_num);
		std::ranges::fill(resamplers, nullptr);

		std::vector<Free_utility> resamplers_free;
		resamplers_free.reserve(input_num);
		for (auto& i : resamplers) resamplers_free.emplace_back(std::bind(swr_free, &i));

		for (int i = 0; i < input_num; i++)
		{
			const auto try_item = get_input_item<Audio_stream>(input, std::format("input_{}", i + 1));

			if (!try_item.has_value())
				throw Runtime_error(
					"Audio Mixer processor has no input",
					"Audio Mixer processor requires an audio stream input to function properly.",
					std::format("Input item 'input_{}' not found", i + 1)
				);

			input_items.emplace_back(try_item.value());
		}

		const auto output_item = get_output_item<Audio_stream>(output, "output");

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
			for (int i = 0; i < input_num; i++)
			{
				auto pop_result = input_items[i].get().try_pop();
				if (!pop_result.has_value())
				{
					if (pop_result.error() == boost::fibers::channel_op_status::empty)
					{
						if (input_items[i].get().eof()) eofs[i] = true;
					}
					else if (pop_result.error() == boost::fibers::channel_op_status::closed)
						THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
				}
				else
				{
					buffers[i].push_back(pop_result.value());
				}
			}

			bool check = false;

			count = 0;

			for (int i = 0; i < input_num; i++)
			{
				if (buffers[i].empty() && !eofs[i])
				{
					boost::this_fiber::yield();
					check = true;
					break;
				}
			}
			if (check) continue;

			const auto front_view
				= buffers
				| std::views::transform([](const auto& buffer)
										{ return buffer.empty() ? nullptr : buffer.front()->data(); });
			const std::vector<const AVFrame*> frames(front_view.begin(), front_view.end());

			std::shared_ptr<Audio_frame> new_frame = std::make_shared<Audio_frame>();
			AVFrame* out_frame = new_frame->data();
			out_frame->nb_samples = std::numeric_limits<int>::max();
			for (auto& i : frames)
				if (i) out_frame->nb_samples = std::min(out_frame->nb_samples, i->nb_samples);
			if (out_frame->nb_samples == std::numeric_limits<int>::max()) out_frame->nb_samples = 1152;
			out_frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
			out_frame->sample_rate = config::processor::audio_amix::std_sample_rate;
			out_frame->format = AV_SAMPLE_FMT_FLTP;
			time_seconds += out_frame->nb_samples / double(out_frame->sample_rate);
			out_frame->pts = time_seconds * 1000000;
			out_frame->time_base = {.num = 1, .den = 1000000};

			av_frame_get_buffer(out_frame, 32);
			av_frame_make_writable(out_frame);

			if (!initial)
			{
				const AVChannelLayout dst_layout = AV_CHANNEL_LAYOUT_STEREO;

				for (int i = 0; i < input_num; i++)
				{
					resamplers[i] = swr_alloc();
					const AVChannelLayout input_layout_r = frames[i]->ch_layout.nb_channels == 2
															 ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
															 : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

					av_opt_set_chlayout(resamplers[i], "in_chlayout", &input_layout_r, 0);
					av_opt_set_int(resamplers[i], "in_sample_rate", frames[i]->sample_rate, 0);
					av_opt_set_sample_fmt(
						resamplers[i],
						"in_sample_fmt",
						(AVSampleFormat)frames[i]->format,
						0
					);
					av_opt_set_chlayout(resamplers[i], "out_chlayout", &dst_layout, 0);
					av_opt_set_int(
						resamplers[i],
						"out_sample_rate",
						config::processor::audio_amix::std_sample_rate,
						0
					);
					av_opt_set_sample_fmt(resamplers[i], "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

					if (swr_init(resamplers[i]) < 0)
						throw Runtime_error(
							"Failed to initialize software resampler",
							"Cannot start the audio resampling process. Internal error may have "
							"occurred.",
							"swr_init() returned error"
						);
				}
				initial = std::make_unique<bool>();
			}
			std::vector<uint8_t**> datas;
			std::vector<int> linesizes;
			datas.resize(input_num);
			linesizes.resize(input_num);
			std::ranges::fill(datas, nullptr);
			std::ranges::fill(linesizes, 0);

			for (int i = 0; i < input_num; i++)
			{
				av_samples_alloc_array_and_samples(
					&datas[i],
					&linesizes[i],
					2,
					out_frame->nb_samples,
					static_cast<AVSampleFormat>(out_frame->format),
					0
				);
				if (frames[i])
				{
					const auto convert_count = swr_convert(
						resamplers[i],
						datas[i],
						out_frame->nb_samples,
						(const uint8_t**)frames[i]->data,
						frames[i]->nb_samples
					);

					if (convert_count < 0)
						throw Runtime_error(
							"Software resampler failed",
							"Cannot convert audio sample rate or format. Internal error may have "
							"occurred.",
							std::format("swr_convert() returned error {} at input channel {}", ret, i)
						);
				}
				else if (!frames[i])
				{
					const auto convert_count
						= swr_convert(resamplers[i], datas[i], out_frame->nb_samples, 0, 0);

					if (convert_count < 0)
						throw Runtime_error(
							"Software resampler failed",
							"Cannot convert audio sample rate or format. Internal error may have occurred.",
							std::format("swr_convert() returned error {} at input channel {}", ret, i)
						);
					if (convert_count < out_frame->nb_samples) count++;
				}
			}
			auto* out_left = (float*)out_frame->data[0];
			auto* out_right = (float*)out_frame->data[1];

			for (int j = 0; j < out_frame->nb_samples; j++)
			{
				float temp_l = 0.0f;
				float temp_r = 0.0f;
				for (int i = 0; i < input_num; i++)
				{
					temp_l += ((const float*)datas[i][0])[j] * volumes[i];
					temp_r += ((const float*)datas[i][1])[j] * volumes[i];
				}
				out_left[j] = temp_l;
				out_right[j] = temp_r;
			}

			for (auto& i : buffers)
				if (!i.empty()) i.erase(i.begin());

			push_frame(new_frame);

			for (auto& i : datas)
			{
				if (i) av_freep(&i[0]);
				av_freep(i);
			}

			if (count == input_num) break;
		}

		for (auto& output : output_item) output->set_eof();
	}

	void Audio_amix::draw_title()
	{
		imgui_utility::shadowed_text("Audio Mixer");
	}

	bool Audio_amix::draw_content(bool readonly)
	{
		bool change = false;
		ImGui::PushItemWidth(200);
		ImGui::BeginDisabled(readonly);
		{
			if (ImGui::InputInt("Input Channels", &input_num, 1, 100, 0))
				input_num = std::clamp(input_num, 1, 16);

			if (input_pins.size() != input_num + 1) change = true;

			int temp_size = input_pins.size() - 1;
			while (input_num > input_pins.size() - 1)
			{
				input_pins.push_back(
					{.identifier = std::format("input_{}", input_pins.size()),
					 .display_name = std::format("Input {}", input_pins.size()),
					 .type = typeid(Audio_stream),
					 .is_input = true,
					 .generate_func = []
					 {
						 return std::make_shared<Audio_stream>();
					 }}
				);
				volumes.push_back(1.0f / input_num);
				locks.push_back(false);
			}
			if (temp_size)
				for (int i = 0; i < temp_size - 1; i++) volumes[i] *= (float)(temp_size) / (input_num);

			float temp_volume = 0.0f;
			while (input_num < input_pins.size() - 1)
			{
				input_pins.pop_back();

				temp_volume += volumes.back();
				volumes.pop_back();
				input_pins.pop_back();
				locks.pop_back();
			}

			if (input_num > 1)
				for (float& volume : volumes) volume *= 1.0f / (1.0f - temp_volume);
			else if (input_num == 1)
				volumes[0] = 1.0f;

			for (int i = 0; i < input_num; i++)
			{
				if (ImGui::SliderFloat(
						std::format("Input {} Volume", i + 1).c_str(),
						&volumes[i],
						0.001f,
						0.999f,
						"%.3f",
						0
					))
				{
					float lock_sum = 0.0f;
					float unlock_sum = 0.0f;
					for (int j = 0; j < input_num; j++)
					{
						if (locks[j])
							lock_sum += volumes[j];
						else
							unlock_sum += volumes[j];
					}
					for (int j = 0; j < input_num; j++)
						if (!locks[j] && unlock_sum > 0.001f) volumes[j] *= (1.0f - lock_sum) / unlock_sum;
				}

				ImGui::SameLine();

				bool temp_lock = locks[i];
				ImGui::Checkbox(std::format("Locked##locked_{}", i).c_str(), &temp_lock);
				locks[i] = temp_lock;
			}
		}
		ImGui::EndDisabled();
		ImGui::PopItemWidth();

		return change;
	}

	Json::Value Audio_amix::serialize() const
	{
		Json::Value value;
		value["input_num"] = input_num;
		for (int i = 0; i < input_num; i++)
		{
			value[std::format("volumes{}", i)] = volumes[i];
			value[std::format("locks{}", i)] = locks[i];
		}
		return value;
	}
<<<<<<< Updated upstream

	== == == =
>>>>>>> Stashed changes
		void Audio_amix::deserialize(const Json::Value& value)
	{
		if (!value.isMember("input_num"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: input_num"
			);
		input_num = value["input_num"].asInt();
		locks.clear();
		volumes.clear();
		input_pins.clear();
		input_pins.push_back(
			{.identifier = "output",
			 .display_name = "Output",
			 .type = typeid(Audio_stream),
			 .is_input = false,
<<<<<<< Updated upstream
			 .generate_func = [] == == == =.generate_func = []
>>>>>>> Stashed changes
			 {
				 return std::make_shared<Audio_stream>();
			 }}
		);
		for (int i = 0; i < input_num; i++)
		{
			input_pins.push_back(
				{.identifier = std::format("input_{}", i + 1),
				 .display_name = std::format("Input_{}", i + 1),
				 .type = typeid(Audio_stream),
				 .is_input = true,
<<<<<<< Updated upstream
				 .generate_func = [] == == == =.generate_func = []
>>>>>>> Stashed changes
				 {
					 return std::make_shared<Audio_stream>();
				 }}
			);
			volumes.push_back(value[std::format("volumes{}", i)].asFloat());
			locks.push_back(value[std::format("locks{}", i)].asBool());
		}
	}
}
