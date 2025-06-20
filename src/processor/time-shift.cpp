#include "processor/time-shift.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "imgui.h"
#include "libavutil/samplefmt.h"
#include "processor/audio-stream.hpp"
#include "utility/free-utility.hpp"

#include "json/value.h"
#include <boost/fiber/operations.hpp>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <stdlib.h>
#include <vector>

namespace processor
{
	infra::Processor::Info Time_shift::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "time shift",
			.display_name = "Shift Time",
			.singleton = false,
			.generate = std::make_unique<Time_shift>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Time_shift::get_pin_attributes() const
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

	void Time_shift::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		float temp_shift_time = shift_time;
		std::vector<std::shared_ptr<const Audio_frame>> buffers;
		double time_seconds = 0;
		std::unique_ptr<bool> initial;

		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Time Shift processor has no input",
				"Time Shift processor requires an audio stream input to function properly.",
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

			if (temp_shift_time > 0 && !initial)
			{
				std::shared_ptr<Audio_frame> dst_frame = std::make_shared<Audio_frame>();
				buffers.push_back(pop_result.value());
				AVFrame* out_frame = dst_frame->data();

				out_frame->sample_rate = buffers.front()->data()->sample_rate;
				out_frame->format = buffers.front()->data()->format;
				out_frame->nb_samples = 1152;
				out_frame->ch_layout = buffers.front()->data()->ch_layout;
				out_frame->time_base = buffers.front()->data()->time_base;
				out_frame->pts = time_seconds * out_frame->time_base.den / out_frame->time_base.num;
				time_seconds += 1152.0f / out_frame->sample_rate;
				temp_shift_time -= 1152.0f / out_frame->sample_rate;
				if (temp_shift_time <= 0)
				{
					initial = std::make_unique<bool>();
					temp_shift_time = 0;
				}
				av_frame_get_buffer(out_frame, 32);
				av_frame_make_writable(out_frame);
				if (out_frame->ch_layout.nb_channels == 1)
					memset(
						out_frame->data[0],
						0,
						out_frame->nb_samples
							* av_get_bytes_per_sample(static_cast<enum AVSampleFormat>(out_frame->format))
					);
				else
				{
					if (out_frame->format < 5 && out_frame->format > -1)
					{
						memset(
							out_frame->data[0],
							0,
							out_frame->nb_samples
								* av_get_bytes_per_sample(static_cast<enum AVSampleFormat>(out_frame->format))
								* 2
						);
					}
					else if (out_frame->format > 4 && out_frame->format < 12)
					{
						memset(
							out_frame->data[0],
							0,
							out_frame->nb_samples
								* av_get_bytes_per_sample(static_cast<enum AVSampleFormat>(out_frame->format))
						);
						memset(
							out_frame->data[1],
							0,
							out_frame->nb_samples
								* av_get_bytes_per_sample(static_cast<enum AVSampleFormat>(out_frame->format))
						);
					}
					else
					{
						throw Runtime_error(
							"Unsupported AVFormat!",
							"Audio AVFormat must bewteen 0~12",
							std::format("Got AVFormat {}", out_frame->format)
						);
					}
				}
				push_frame(dst_frame);
				continue;
			}
			else if (temp_shift_time < 0 && !initial)
			{
				const auto& temp_frame = pop_result.value()->data();
				temp_shift_time += (double)temp_frame->nb_samples / (double)temp_frame->sample_rate;
				if (temp_shift_time >= 0)
				{
					initial = std::make_unique<bool>();
					temp_shift_time = 0;
				}
				boost::this_fiber::yield();
				continue;
			}
			else
			{
				if (!buffers.empty())
				{
					std::shared_ptr<Audio_frame> dst_frame = std::make_shared<Audio_frame>();
					buffers.push_back(pop_result.value());
					AVFrame* out_frame = dst_frame->data();

					out_frame->sample_rate = buffers.front()->data()->sample_rate;
					out_frame->format = buffers.front()->data()->format;
					out_frame->nb_samples = buffers.front()->data()->nb_samples;
					out_frame->ch_layout = buffers.front()->data()->ch_layout;
					out_frame->time_base = buffers.front()->data()->time_base;
					out_frame->pts = time_seconds * out_frame->time_base.den / out_frame->time_base.num;
					time_seconds += (double)out_frame->nb_samples / out_frame->sample_rate;
					av_frame_get_buffer(out_frame, 32);
					av_frame_make_writable(out_frame);
					if (out_frame->ch_layout.nb_channels == 1)
						std::copy(
							buffers.front()->data()->data[0],
							buffers.front()->data()->data[0]
								+ out_frame->nb_samples
									  * av_get_bytes_per_sample(
										  static_cast<enum AVSampleFormat>(out_frame->format)
									  ),
							out_frame->data[0]
						);
					else
					{
						if (out_frame->format < 5 && out_frame->format > -1)
						{
							std::copy(
								buffers.front()->data()->data[0],
								buffers.front()->data()->data[0]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  )
										  * 2,
								out_frame->data[0]
							);
						}
						else if (out_frame->format > 4 && out_frame->format < 12)
						{
							std::copy(
								buffers.front()->data()->data[0],
								buffers.front()->data()->data[0]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  ),
								out_frame->data[0]
							);
							std::copy(
								buffers.front()->data()->data[1],
								buffers.front()->data()->data[1]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  ),
								out_frame->data[1]
							);
						}
						else
						{
							throw Runtime_error(
								"Unsupported AVFormat!",
								"Audio AVFormat must bewteen 0~12",
								std::format("Got AVFormat {}", out_frame->format)
							);
						}
					}
					push_frame(dst_frame);
					buffers.erase(buffers.begin());
					continue;
				}
				else
				{
					std::shared_ptr<Audio_frame> dst_frame = std::make_shared<Audio_frame>();
					const auto& temp_frame = pop_result.value()->data();
					AVFrame* out_frame = dst_frame->data();

					out_frame->sample_rate = temp_frame->sample_rate;
					out_frame->format = temp_frame->format;
					out_frame->nb_samples = temp_frame->nb_samples;
					out_frame->ch_layout = temp_frame->ch_layout;
					out_frame->time_base = temp_frame->time_base;
					out_frame->pts = time_seconds * out_frame->time_base.den / out_frame->time_base.num;
					time_seconds += (double)out_frame->nb_samples / out_frame->sample_rate;
					av_frame_get_buffer(out_frame, 32);
					av_frame_make_writable(out_frame);
					if (out_frame->ch_layout.nb_channels == 1)
						std::copy(
							temp_frame->data[0],
							temp_frame->data[0]
								+ out_frame->nb_samples
									  * av_get_bytes_per_sample(
										  static_cast<enum AVSampleFormat>(out_frame->format)
									  ),
							out_frame->data[0]
						);
					else
					{
						if (out_frame->format < 5 && out_frame->format > -1)
						{
							std::copy(
								temp_frame->data[0],
								temp_frame->data[0]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  )
										  * 2,
								out_frame->data[0]
							);
						}
						else if (out_frame->format > 4 && out_frame->format < 12)
						{
							std::copy(
								temp_frame->data[0],
								temp_frame->data[0]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  ),
								out_frame->data[0]
							);
							std::copy(
								temp_frame->data[1],
								temp_frame->data[1]
									+ out_frame->nb_samples
										  * av_get_bytes_per_sample(
											  static_cast<enum AVSampleFormat>(out_frame->format)
										  ),
								out_frame->data[1]
							);
						}
						else
						{
							throw Runtime_error(
								"Unsupported AVFormat!",
								"Audio AVFormat must bewteen 0~12",
								std::format("Got AVFormat {}", out_frame->format)
							);
						}
					}
					push_frame(dst_frame);
				}
			}
		}

		for (auto& channel : output_item) channel->set_eof();
	}

	void Time_shift::draw_title()
	{
		imgui_utility::shadowed_text("Time Shift");
	}

	bool Time_shift::draw_content(bool readonly)
	{
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::SetNextItemWidth(200);
			ImGui::DragFloat("shift times(sec)", &shift_time, 0.01f, 1000.0f, -1000.0f, "%.3f", 0);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	Json::Value Time_shift::serialize() const
	{
		Json::Value value;
		value["shift_time"] = shift_time;
		return value;
	}

	void Time_shift::deserialize(const Json::Value& value)
	{
		if (!value.isMember("shift_time"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: shift_time"
			);

		if (!value["shift_time"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: shift_time"
			);

		shift_time = value["shift_time"].asFloat();
		shift_time = std::min<float>(1000.f, std::max<float>(-1000.f, shift_time));
	}
}