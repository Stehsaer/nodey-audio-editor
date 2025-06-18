#include "processor/audio-bimix.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "utility/free-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <vector>

namespace processor
{

	infra::Processor::Info Audio_bimix::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_bimix",
			.display_name = "Audio Bimix",
			.singleton = false,
			.generate = std::make_unique<Audio_bimix>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_bimix::get_pin_attributes() const
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
			{.identifier = "input_l",
			 .display_name = "Left",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func =
				 []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
			{.identifier = "input_r",
			 .display_name = "Right",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }}
		};
	}

	void Audio_bimix::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{
		std::vector<std::shared_ptr<const Audio_frame>> buf_l, buf_r;
		std::unique_ptr<bool> initial;

		// 初始化软件重采样器
		SwrContext* resampler_l = nullptr;
		SwrContext* resampler_r = nullptr;

		bool left_eof = false;
		bool right_eof = false;

		Free_utility resampler_l_free{std::bind(swr_free, &resampler_l)};
		Free_utility resampler_r_free{std::bind(swr_free, &resampler_r)};

		double time_seconds;

		const auto input_item_optional_l = get_input_item<Audio_stream>(input, "input_l");
		const auto input_item_optional_r = get_input_item<Audio_stream>(input, "input_r");
		const auto output_item = get_output_item<Audio_stream>(output, "output");

		if (!input_item_optional_l.has_value() || !input_item_optional_r.has_value())
			throw Runtime_error(
				"Audio Channel mix processor has no input",
				"Audio channel mix processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_item_l = input_item_optional_l.value().get();
		auto& input_item_r = input_item_optional_r.value().get();

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

		int input_num = 2;

		while (!stop_token)
		{
			// 获取数据

			const auto pop_result_l = input_item_l.try_pop();
			if (!pop_result_l.has_value())
			{
				if (pop_result_l.error() == boost::fibers::channel_op_status::empty)
					if (input_item_l.eof()) left_eof = true;
				if (pop_result_l.error() == boost::fibers::channel_op_status::closed)
					THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
			}
			else
				buf_l.push_back(pop_result_l.value());

			const auto pop_result_r = input_item_r.try_pop();
			if (!pop_result_r.has_value())
			{
				if (pop_result_r.error() == boost::fibers::channel_op_status::empty)
					if (input_item_r.eof()) right_eof = true;
				if (pop_result_r.error() == boost::fibers::channel_op_status::closed)
					THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
			}
			else
				buf_r.push_back(pop_result_r.value());

			if ((buf_r.empty() && !right_eof) || (buf_l.empty() && !left_eof))
			{
				boost::this_fiber::yield();
				continue;
			}

			// 获取帧参数
			std::shared_ptr<Audio_frame> new_frame = std::make_shared<Audio_frame>();

			const auto& frame_l = buf_l.empty() ? nullptr : buf_l.front()->data();
			const auto& frame_r = buf_r.empty() ? nullptr : buf_r.front()->data();

			AVFrame* out_frame = new_frame->data();
			if (frame_r && frame_l)
				out_frame->nb_samples = std::min(frame_r->nb_samples, frame_l->nb_samples);
			if (!frame_r && frame_l)
				out_frame->nb_samples = frame_l->nb_samples;
			else if (frame_r && !frame_l)
				out_frame->nb_samples = frame_r->nb_samples;
			else
				out_frame->nb_samples = 1152;
			out_frame->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
			out_frame->sample_rate = 48000;
			out_frame->format = AV_SAMPLE_FMT_FLTP;

			time_seconds += out_frame->nb_samples / double(out_frame->sample_rate);

			out_frame->pts = time_seconds * 1000000;
			out_frame->time_base = {.num = 1, .den = 1000000};

			av_frame_get_buffer(out_frame, 32);
			av_frame_make_writable(out_frame);

			if (!initial)
			{
				resampler_l = swr_alloc();
				resampler_r = swr_alloc();

				const AVChannelLayout dst_layout = AV_CHANNEL_LAYOUT_STEREO;

				const AVChannelLayout input_layout_r = frame_r->ch_layout.nb_channels == 2
														 ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
														 : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);
				const AVChannelLayout input_layout_l = frame_l->ch_layout.nb_channels == 2
														 ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
														 : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

				av_opt_set_chlayout(resampler_r, "in_chlayout", &input_layout_r, 0);
				av_opt_set_int(resampler_r, "in_sample_rate", frame_r->sample_rate, 0);
				av_opt_set_sample_fmt(resampler_r, "in_sample_fmt", (AVSampleFormat)frame_r->format, 0);

				av_opt_set_chlayout(resampler_r, "out_chlayout", &dst_layout, 0);
				av_opt_set_int(resampler_r, "out_sample_rate", 48000, 0);
				av_opt_set_sample_fmt(resampler_r, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

				av_opt_set_chlayout(resampler_l, "in_chlayout", &input_layout_l, 0);
				av_opt_set_int(resampler_l, "in_sample_rate", frame_l->sample_rate, 0);
				av_opt_set_sample_fmt(resampler_l, "in_sample_fmt", (AVSampleFormat)frame_l->format, 0);

				av_opt_set_chlayout(resampler_l, "out_chlayout", &dst_layout, 0);
				av_opt_set_int(resampler_l, "out_sample_rate", 48000, 0);
				av_opt_set_sample_fmt(resampler_l, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);

				if (swr_init(resampler_r) < 0)
					throw Runtime_error(
						"Failed to initialize software resampler",
						"Cannot start the audio resampling process. Internal error may have "
						"occurred.",
						"swr_init() returned error"
					);

				if (swr_init(resampler_l) < 0)
					throw Runtime_error(
						"Failed to initialize software resampler",
						"Cannot start the audio resampling process. Internal error may have "
						"occurred.",
						"swr_init() returned error"
					);

				initial = std::make_unique<bool>();
			}

			uint8_t** data1 = nullptr;
			uint8_t** data2 = nullptr;

			int linesize_l;
			av_samples_alloc_array_and_samples(
				&data1,
				&linesize_l,
				2,
				out_frame->nb_samples,
				static_cast<AVSampleFormat>(out_frame->format),
				0
			);
			int convert_count_l = 0;
			if (frame_l)
				convert_count_l = swr_convert(
					resampler_l,
					data1,
					out_frame->nb_samples,
					(const uint8_t**)frame_l->data,
					frame_l->nb_samples
				);
			else
				convert_count_l = swr_convert(resampler_l, data1, out_frame->nb_samples, 0, 0);
			if (convert_count_l < 0)
				throw Runtime_error(
					"Software resampler failed",
					"Cannot convert audio sample rate or format. Internal error may have occurred.",
					"swr_convert() returned error"
				);

			int linesize_r;
			av_samples_alloc_array_and_samples(
				&data2,
				&linesize_r,
				2,
				out_frame->nb_samples,
				static_cast<AVSampleFormat>(out_frame->format),
				0
			);
			int convert_count_r = 0;
			if (frame_r)
				convert_count_r = swr_convert(
					resampler_r,
					data2,
					out_frame->nb_samples,
					(const uint8_t**)frame_r->data,
					frame_r->nb_samples
				);
			else
				convert_count_l = swr_convert(resampler_r, data2, out_frame->nb_samples, 0, 0);
			if (convert_count_r < 0)
				throw Runtime_error(
					"Software resampler failed",
					"Cannot convert audio sample rate or format. Internal error may have occurred.",
					"swr_convert() returned error"
				);

			const auto* float_data_ll = (const float*)data1[0];
			const auto* float_data_lr = (const float*)data1[1];
			const auto* float_data_rl = (const float*)data2[0];
			const auto* float_data_rr = (const float*)data2[1];

			auto* out_left = (float*)out_frame->data[0];
			auto* out_right = (float*)out_frame->data[1];

			const float bias_minus = (1 - bias);
			const float bias_plus = (1 + bias);

			for (size_t i = 0; i < out_frame->nb_samples; i++)
			{
				out_left[i] = (float_data_ll[i] / 2 + float_data_lr[i] / 2) * bias_minus;
				out_right[i] = (float_data_rl[i] / 2 + float_data_rr[i] / 2) * bias_plus;
			}

			if (frame_l) buf_l.erase(buf_l.begin());
			if (frame_r) buf_r.erase(buf_r.begin());

			push_frame(new_frame);

			if (data1) av_freep(&data1[0]);
			if (data2) av_freep(&data2[0]);

			if (convert_count_r == 0 && convert_count_l == 0) break;
		}

		for (auto& output : output_item) output->set_eof();
	}

	void Audio_bimix::draw_title()
	{
		imgui_utility::shadowed_text("Audio Bimixer");
	}

	bool Audio_bimix::draw_content(bool readonly)
	{
		ImGui::SetNextItemWidth(200);
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			ImGui::DragFloat("Bias", &this->bias, 0.005, -1.0, 1.0, "%.3f");
			bias = std::clamp<float>(bias, -1, 1);
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	Json::Value Audio_bimix::serialize() const
	{
		Json::Value value;
		value["bias"] = bias;
		return value;
	}

	void Audio_bimix::deserialize(const Json::Value& value)
	{
		if (!value.isMember("bias"))
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: bias"
			);

		if (!value["bias"].isDouble())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_bimix failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: bias"
			);

		bias = value["bias"].asDouble();
		bias = std::clamp<float>(bias, -1, 1);
	}

}
