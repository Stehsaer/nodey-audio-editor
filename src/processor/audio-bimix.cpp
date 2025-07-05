#include "processor/audio-bimix.hpp"
#include "config.hpp"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "utility/free-utility.hpp"
#include "utility/imgui-utility.hpp"
#include "utility/sw-resample.hpp"

#include <algorithm>
#include <boost/fiber/operations.hpp>
#include <cstdlib>
#include <imgui.h>
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
			.generate = std::make_unique<Audio_bimix>,
			.description = "Advanced Stereo Channel Mixer (V2)\n\n"
						   "## Functionality\n"
						   "- Enhanced stereo mixing with precise time alignment\n"
						   "- Handles asynchronous left and right channel inputs\n"
						   "- Advanced resampling with buffer management\n"
						   "- Time-accurate channel synchronization\n"
						   "- Bias control for channel balance adjustment\n\n"
						   "## Output Format\n"
						   "- Sample Rate: 48kHz (configurable)\n"
						   "- Format: 32-bit Float Interleaved\n"
						   "- Channels: Stereo (Left/Right)\n\n"
						   "## Usage\n"
						   "- Connect audio sources to 'Left' and 'Right' input pins\n"
						   "- Adjust bias slider for channel balance (-1.0 to +1.0)\n"
						   "- Supports different sample rates and formats on inputs\n"
						   "- Automatically handles timing misalignment between channels\n\n"
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
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
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
		}

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

	infra::Processor::Info Audio_bimix_v2::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_bimix_v2",
			.display_name = "Audio Bimix V2",
			.singleton = false,
			.generate = std::make_unique<Audio_bimix_v2>,
			.description = "Advanced Stereo Channel Mixer (V2)\n\n"
						   "## Functionality\n"
						   "- Enhanced stereo mixing with precise time alignment\n"
						   "- Handles asynchronous left and right channel inputs\n"
						   "- Advanced resampling with buffer management\n"
						   "- Time-accurate channel synchronization\n"
						   "- Bias control for channel balance adjustment\n\n"
						   "## Output Format\n"
						   "- Sample Rate: 48kHz (configurable)\n"
						   "- Format: 32-bit Float Interleaved\n"
						   "- Channels: Stereo (Left/Right)\n\n"
						   "## Usage\n"
						   "- Connect audio sources to 'Left' and 'Right' input pins\n"
						   "- Adjust bias slider for channel balance (-1.0 to +1.0)\n"
						   "- Supports different sample rates and formats on inputs\n"
						   "- Automatically handles timing misalignment between channels\n\n"
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_bimix_v2::get_pin_attributes() const
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

	Json::Value Audio_bimix_v2::serialize() const
	{
		return {};
	}

	void Audio_bimix_v2::deserialize(const Json::Value& value) {}

	static std::shared_ptr<Audio_frame> make_audio_frame_flt_interleaved(
		std::span<float> samples,
		double time_seconds
	)
	{
		auto frame = std::make_shared<Audio_frame>();
		auto* data = frame->data();

		data->nb_samples = samples.size() / 2;
		data->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
		data->sample_rate = config::processor::audio_bimix::std_sample_rate;
		data->format = AV_SAMPLE_FMT_FLT;

		data->pts = time_seconds * 1000000;
		data->time_base = {.num = 1, .den = 1000000};

		av_frame_get_buffer(data, 32);
		av_frame_make_writable(data);

		std::ranges::copy(samples, reinterpret_cast<float*>(data->data[0]));

		return frame;
	}

	void Audio_bimix_v2::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{
		auto input_item_optional_l = get_input_item<Audio_stream>(input, "input_l");
		auto input_item_optional_r = get_input_item<Audio_stream>(input, "input_r");

		if (!input_item_optional_l.has_value() || !input_item_optional_r.has_value())
			throw Runtime_error(
				"Audio Channel mix processor has no input",
				"Audio channel mix processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_stream_l = input_item_optional_l.value().get();
		auto& input_stream_r = input_item_optional_r.value().get();
		auto output_stream = get_output_item<Audio_stream>(output, "output");

		auto push_frame = [&stop_token, &output_stream](const std::shared_ptr<Audio_frame>& frame)
		{
			for (auto& channel : output_stream)
			{
				if (stop_token) return;

				while (channel->try_push(frame) != boost::fibers::channel_op_status::success)
				{
					if (stop_token) return;
					boost::this_fiber::yield();
				}
			}
		};

		constexpr auto target_sample_rate = config::processor::audio_bimix::std_sample_rate;

		// 单帧，只记录单声道
		struct Frame
		{
			std::vector<float> samples;
			double time_seconds = 0.0;

			double elapsed_seconds() const { return double(samples.size()) / target_sample_rate; }
			double end_time() const { return time_seconds + elapsed_seconds(); }
			void drop_samples(size_t count)
			{
				assert(count <= samples.size());
				samples.erase(samples.begin(), samples.begin() + count);
				time_seconds += double(count) / target_sample_rate;
			}
		};

		std::list<Frame> frames_l, frames_r;                        // 左右声道的帧链表
		std::unique_ptr<Audio_resampler> resampler_l, resampler_r;  // 重采样器
		double time_l, time_r;                                      //  左右声道的时间戳
		bool eof_l = false, eof_r = false;                          // 左右声道的结束标志

		std::vector<float> shared_buffer[2];  // 暂存重采样器的结果
		std::vector<float> frame_samples;     // 生成新帧时的样本缓冲区

		while (!stop_token)
		{
			boost::this_fiber::yield();

			/* 获取左声道帧 */
			if (!eof_l)
			{
				const auto pop_result_l = input_stream_l.try_pop();
				if (!pop_result_l.has_value())
				{
					if (pop_result_l.error() == boost::fibers::channel_op_status::empty)
						if (input_stream_l.eof()) eof_l = true;
					if (pop_result_l.error() == boost::fibers::channel_op_status::closed)
						THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
				}
				else  //  成功获取新的帧，传递给重采样器
				{
					const auto& data = *pop_result_l.value()->data();

					if (data.ch_layout.nb_channels != 2 && data.ch_layout.nb_channels != 1)
						throw Runtime_error(
							"Invalid audio channel layout",
							"Audio channel layout must be stereo or mono.",
							std::format("Invalid channel layout: {}", data.ch_layout.nb_channels)
						);

					// 创建新的重采样器
					if (resampler_l == nullptr)
					{
						const AVChannelLayout input_layout = data.ch_layout.nb_channels == 2
															   ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
															   : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

						const Audio_resampler::Format input_format{
							.format = (AVSampleFormat)data.format,
							.sample_rate = data.sample_rate,
							.channel_layout = input_layout,
						};

						const Audio_resampler::Format output_format{
							.format = AV_SAMPLE_FMT_FLTP,
							.sample_rate = target_sample_rate,
							.channel_layout = AV_CHANNEL_LAYOUT_STEREO,
						};

						auto create_result = Audio_resampler::create(input_format, output_format);
						if (!create_result.has_value())
							throw Runtime_error(
								"Failed to create audio resampler",
								"Cannot start the audio resampling process. Internal error may have occurred."
							);

						resampler_l = std::move(create_result.value());
						time_l = data.pts * av_q2d(data.time_base);
					}

					/* 重采样并放到缓冲区 */

					shared_buffer[0].resize(data.nb_samples * 2);
					shared_buffer[1].resize(data.nb_samples * 2);

					float* shared_buffer_ptr[2] = {shared_buffer[0].data(), shared_buffer[1].data()};

					const int resampled_count = resampler_l->resample<float, float>(
						std::span((const float* const*)data.data, data.ch_layout.nb_channels),
						data.nb_samples,
						std::span<float*>(shared_buffer_ptr, 2),
						shared_buffer[0].size()
					);

					if (resampled_count < 0)
					{
						char err_str[256];
						av_make_error_string(err_str, sizeof(err_str), resampled_count);

						throw Runtime_error(
							"Failed to resample audio",
							"Cannot resample audio data. Internal error may have occurred.",
							std::format("swr_convert() returned error {}", err_str)
						);
					}

					time_l += double(resampled_count) / target_sample_rate;

					Frame new_frame;
					new_frame.time_seconds = time_l;
					new_frame.samples.resize(resampled_count);

					// 混合左右声道
					for (auto [dst, left, right] :
						 std::views::zip(new_frame.samples, shared_buffer[0], shared_buffer[1]))
						dst = (left + right) * 0.5;

					frames_l.emplace_back(std::move(new_frame));
				}
			}

			/* 获取右声道帧 */
			if (!eof_r)
			{
				const auto pop_result_r = input_stream_r.try_pop();
				if (!pop_result_r.has_value())
				{
					if (pop_result_r.error() == boost::fibers::channel_op_status::empty)
						if (input_stream_r.eof()) eof_r = true;
					if (pop_result_r.error() == boost::fibers::channel_op_status::closed)
						THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
				}
				else  //  成功获取新的帧，传递给重采样器
				{
					const auto& data = *pop_result_r.value()->data();

					if (data.ch_layout.nb_channels != 2 && data.ch_layout.nb_channels != 1)
						throw Runtime_error(
							"Invalid audio channel layout",
							"Audio channel layout must be stereo or mono.",
							std::format("Invalid channel layout: {}", data.ch_layout.nb_channels)
						);

					// 创建新的重采样器
					if (resampler_r == nullptr)
					{
						const AVChannelLayout input_layout = data.ch_layout.nb_channels == 2
															   ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
															   : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

						const Audio_resampler::Format input_format{
							.format = (AVSampleFormat)data.format,
							.sample_rate = data.sample_rate,
							.channel_layout = input_layout,
						};

						const Audio_resampler::Format output_format{
							.format = AV_SAMPLE_FMT_FLTP,
							.sample_rate = target_sample_rate,
							.channel_layout = AV_CHANNEL_LAYOUT_STEREO,
						};

						auto create_result = Audio_resampler::create(input_format, output_format);
						if (!create_result.has_value())
							throw Runtime_error(
								"Failed to create audio resampler",
								"Cannot start the audio resampling process. Internal error may have occurred."
							);

						resampler_r = std::move(create_result.value());
						time_r = data.pts * av_q2d(data.time_base);
					}

					/* 重采样并放到缓冲区 */

					shared_buffer[0].resize(data.nb_samples * 2);
					shared_buffer[1].resize(data.nb_samples * 2);

					float* const shared_buffer_ptr[2] = {shared_buffer[0].data(), shared_buffer[1].data()};

					const int resampled_count = resampler_r->resample(
						std::span((const float* const*)data.data, data.ch_layout.nb_channels),
						data.nb_samples,
						std::span<float* const>(shared_buffer_ptr, 2),
						data.nb_samples * 2
					);

					if (resampled_count < 0)
					{
						char err_str[256];
						av_make_error_string(err_str, sizeof(err_str), resampled_count);

						throw Runtime_error(
							"Failed to resample audio",
							"Cannot resample audio data. Internal error may have occurred.",
							std::format("swr_convert() returned error {}", err_str)
						);
					}

					time_r += double(resampled_count) / target_sample_rate;

					Frame new_frame;
					new_frame.time_seconds = time_r;
					new_frame.samples.resize(resampled_count);

					// 混合左右声道
					for (auto [dst, left, right] :
						 std::views::zip(new_frame.samples, shared_buffer[0], shared_buffer[1]))
						dst = (left + right) * 0.5;

					frames_r.emplace_back(std::move(new_frame));
				}
			}

			/* 生成新帧 */
			{
				// 转换完成
				if (frames_l.empty() && frames_r.empty() && eof_l && eof_r) break;

				// 右声道已经结束
				if (frames_r.empty() && eof_r)
				{
					const size_t remaining_samples = frames_l.front().samples.size();

					std::vector<float> remaining_samples_buffer(remaining_samples * 2);
					auto begin = remaining_samples_buffer.begin();
					for (float sample : frames_l.front().samples)
					{
						*begin++ = sample;
						*begin++ = 0;
					}

					push_frame(make_audio_frame_flt_interleaved(
						std::span(remaining_samples_buffer),
						frames_l.front().time_seconds
					));

					frames_l.pop_front();

					continue;
				}

				// 左声道已经结束
				if (frames_l.empty() && eof_l)
				{
					const size_t remaining_samples = frames_r.front().samples.size();

					std::vector<float> remaining_samples_buffer(remaining_samples * 2);
					auto begin = remaining_samples_buffer.begin();
					for (float sample : frames_r.front().samples)
					{
						*begin++ = 0;
						*begin++ = sample;
					}

					push_frame(make_audio_frame_flt_interleaved(
						std::span(remaining_samples_buffer),
						frames_r.front().time_seconds
					));

					frames_r.pop_front();

					continue;
				}

				while (!frames_l.empty() && !frames_r.empty() && !stop_token)
				{
					const bool left_eariler
						= frames_l.front().time_seconds < frames_r.front().time_seconds;  // 左声道更早

					// 流对应的偏移（偏移0为左声道，偏移1为右声道）
					const size_t eariler_offset = left_eariler ? 0 : 1;
					const size_t later_offset = left_eariler ? 1 : 0;

					auto& eariler_stream = left_eariler ? frames_l : frames_r;
					auto& later_stream = left_eariler ? frames_r : frames_l;

					const double eariler_begin_time = eariler_stream.front().time_seconds;
					const double later_begin_time = later_stream.front().time_seconds;
					const double eariler_end_time = eariler_stream.front().end_time();
					const double later_end_time = later_stream.front().end_time();

					if (eariler_end_time <= later_begin_time)
					{
						frame_samples.clear();
						frame_samples.resize(eariler_stream.front().samples.size() * 2);

						for (size_t i = 0; i < eariler_stream.front().samples.size(); i++)
						{
							frame_samples[i * 2 + eariler_offset] = eariler_stream.front().samples[i];
							frame_samples[i * 2 + later_offset] = 0;
						}

						push_frame(
							make_audio_frame_flt_interleaved(std::span(frame_samples), eariler_begin_time)
						);

						eariler_stream.pop_front();
						continue;
					}

					// 本次帧生成的结束时间点
					const double frame_end_time = std::min(eariler_end_time, later_end_time);

					// 未对齐的样本数量，需要填0
					const auto unaligned_samples = static_cast<size_t>(
						std::round((later_begin_time - eariler_begin_time) * target_sample_rate)
					);

					// 对齐的样本数量，两个分别填入对应声道
					auto aligned_samples = static_cast<size_t>(
						std::round((frame_end_time - later_begin_time) * target_sample_rate)
					);

					// 防止浮点误差，可能会舍弃一两个样本，无伤大雅
					aligned_samples = std::min(
						aligned_samples,
						eariler_stream.front().samples.size() - unaligned_samples
					);
					aligned_samples = std::min(aligned_samples, later_stream.front().samples.size());

					frame_samples.clear();
					frame_samples.resize((unaligned_samples + aligned_samples) * 2);

					// 填入未对齐的样本
					for (size_t i = 0; i < unaligned_samples; i++)
					{
						frame_samples[i * 2 + eariler_offset] = eariler_stream.front().samples[i];
						frame_samples[i * 2 + later_offset] = 0;
					}

					// 填入对齐样本
					for (size_t i = 0; i < aligned_samples; i++)
					{
						frame_samples[(i + unaligned_samples) * 2 + eariler_offset]
							= eariler_stream.front().samples[i + unaligned_samples];
						frame_samples[(i + unaligned_samples) * 2 + later_offset]
							= later_stream.front().samples[i];
					}

					// 根据结束情况移除对应帧
					if (eariler_end_time <= later_end_time)
					{
						eariler_stream.pop_front();
						later_stream.front().drop_samples(aligned_samples);
					}
					else
					{
						later_stream.pop_front();
						eariler_stream.front().drop_samples(unaligned_samples + aligned_samples);
					}

					// 判断是否需要移除空帧
					if (!eariler_stream.empty() && eariler_stream.front().samples.empty())
						eariler_stream.pop_front();
					if (!later_stream.empty() && later_stream.front().samples.empty())
						later_stream.pop_front();

					push_frame(make_audio_frame_flt_interleaved(std::span(frame_samples), eariler_begin_time)
					);
				}
			}
		}

		for (auto& stream : output_stream) stream->set_eof();
	}

	void Audio_bimix_v2::draw_title()
	{
		imgui_utility::shadowed_text("Audio Bimixer V2");
	}

	bool Audio_bimix_v2::draw_content(bool readonly)
	{
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("No Configuration Available");
		}

		return false;
	}
}
