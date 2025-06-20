#include "processor/audio-detach.hpp"
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

	infra::Processor::Info Audio_detach::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_detach",
			.display_name = "Audio Detach",
			.singleton = false,
			.generate = std::make_unique<Audio_detach>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_detach::get_pin_attributes() const
	{

		return std::vector<infra::Processor::Pin_attribute>{
			{.identifier = "output_l",
			 .display_name = "Left Channel",
			 .type = typeid(Audio_stream),
			 .is_input = false,
			 .generate_func =
				 []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
			{.identifier = "output_r",
			 .display_name = "Right Channel",
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
	static void copy_data_planar(
		uint8_t* const* dst,
		const uint8_t* const* src,
		int input_ch_idx,
		int output_ch_idx,
		int element_count
	)
	{
		const auto typed_dst = reinterpret_cast<T* const*>(dst);
		const auto typed_src = reinterpret_cast<const T* const*>(src);

		[[assume(typed_dst[output_ch_idx] != nullptr)]];
		[[assume(typed_src[input_ch_idx] != nullptr)]];
		[[assume(element_count > 0)]];
		[[assume(uintptr_t(typed_dst[0]) % 32 == 0)]];

		// 把原数据先拷贝到目标数据中
		std::copy(typed_src[input_ch_idx], typed_src[input_ch_idx] + element_count, typed_dst[output_ch_idx]);
		std::fill(typed_dst[(1 + output_ch_idx) % 2], typed_dst[(1 + output_ch_idx) % 2] + element_count, 0);
	};

	template <typename T>
	static void copy_data_packed(
		uint8_t* const* dst,
		const uint8_t* const* src,
		int input_ch_num,
		int output_ch_idx,
		int element_count
	)
	{
		const auto typed_dst = reinterpret_cast<T* const*>(dst);
		const auto typed_src = reinterpret_cast<const T* const*>(src);

		[[assume(typed_dst[output_ch_idx] != nullptr)]];
		[[assume(typed_src[0] != nullptr)]];
		[[assume(element_count > 0)]];
		[[assume(uintptr_t(typed_dst[0]) % 32 == 0)]];

		// 把原数据先拷贝到目标数据中

		if (input_ch_num == 1)
			for (int i = 0; i < element_count * 2; i++)
				typed_dst[0][i] = (output_ch_idx == 0) ? (i % 2 == 1) ? 0 : typed_src[0][i / 2]
								: (i % 2 == 0) ? 0
													   : typed_src[0][i / 2];
		if (input_ch_num == 2)
			for (int i = 0; i < element_count * 2; i++)
				typed_dst[0][i] = (output_ch_idx == 0) ? (i % 2 == 1) ? 0 : typed_src[0][i]
								: (i % 2 == 0) ? 0
													   : typed_src[0][i];
	};

	void Audio_detach::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{

		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");
		const auto output_item_l = get_output_item<Audio_stream>(output, "output_l");
		const auto output_item_r = get_output_item<Audio_stream>(output, "output_r");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Audio Channel mix processor has no input",
				"Audio channel mix processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_item = input_item_optional.value().get();

		/* 接受数据帧 */

		auto push_frame =
			[&stop_token, &output_item_r, &output_item_l](const std::shared_ptr<Audio_frame>& frame, int idx)
		{
			if (!idx)
			{
				for (auto& channel : output_item_l)
				{
					if (stop_token) return;

					while (channel->try_push(frame) != boost::fibers::channel_op_status::success)
					{
						if (stop_token) return;
						boost::this_fiber::yield();
					}
				}
				return;
			}
			else
			{
				for (auto& channel : output_item_r)
				{
					if (stop_token) return;

					while (channel->try_push(frame) != boost::fibers::channel_op_status::success)
					{
						if (stop_token) return;
						boost::this_fiber::yield();
					}
				}
				return;
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
				if (pop_result.error() == boost::fibers::channel_op_status::closed)
					THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
			}

			const auto& frame_shared_ptr = pop_result.value();
			const auto& src_frame = *frame_shared_ptr->data();

			const auto frame_channels = src_frame.ch_layout.nb_channels;
			const auto frame_sample_element_count = src_frame.nb_samples;
			const auto format = static_cast<AVSampleFormat>(src_frame.format);

			std::shared_ptr<Audio_frame> new_frame_l = std::make_shared<Audio_frame>();
			std::shared_ptr<Audio_frame> new_frame_r = std::make_shared<Audio_frame>();

			AVFrame* out_frame_l = new_frame_l->data();
			AVFrame* out_frame_r = new_frame_r->data();

			out_frame_l->nb_samples = src_frame.nb_samples;
			out_frame_r->nb_samples = src_frame.nb_samples;

			out_frame_l->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
			out_frame_r->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
			out_frame_l->sample_rate = src_frame.sample_rate;
			out_frame_r->sample_rate = src_frame.sample_rate;
			out_frame_l->format = src_frame.format;
			out_frame_r->format = src_frame.format;
			out_frame_l->pts = src_frame.pts;
			out_frame_r->pts = src_frame.pts;

			av_frame_get_buffer(out_frame_l, 32);
			av_frame_get_buffer(out_frame_r, 32);
			av_frame_make_writable(out_frame_l);
			av_frame_make_writable(out_frame_r);

			const uint8_t* const* src_data = src_frame.data;
			uint8_t* const* dst_data_l = out_frame_l->data;
			uint8_t* const* dst_data_r = out_frame_r->data;

			if (frame_channels == 1)
			{
				switch (format)
				{
				case AV_SAMPLE_FMT_FLT:
					copy_data_packed<float>(dst_data_l, src_data, 1, 0, frame_sample_element_count);
					copy_data_packed<float>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_FLTP:
					copy_data_planar<float>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<float>(dst_data_r, src_data, 0, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S16:
					copy_data_packed<int16_t>(dst_data_l, src_data, 1, 0, frame_sample_element_count);
					copy_data_packed<int16_t>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S16P:
					copy_data_planar<int16_t>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<int16_t>(dst_data_r, src_data, 0, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S32:
					copy_data_packed<int32_t>(dst_data_l, src_data, 1, 0, frame_sample_element_count);
					copy_data_packed<int32_t>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S32P:
					copy_data_planar<int32_t>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<int32_t>(dst_data_r, src_data, 0, 1, frame_sample_element_count);
					break;
				default:
					throw Runtime_error(
						"Audio format is not support",
						"Audio volume processor requires an audio format properly.",
						"Include FLT, S16, S32"
					);
				}
			}
			else if (frame_channels == 2)
			{

				switch (format)
				{
				case AV_SAMPLE_FMT_FLT:
					copy_data_packed<float>(dst_data_l, src_data, 2, 0, frame_sample_element_count);
					copy_data_packed<float>(dst_data_r, src_data, 2, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_FLTP:
					copy_data_planar<float>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<float>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S16:
					copy_data_packed<int16_t>(dst_data_l, src_data, 2, 0, frame_sample_element_count);
					copy_data_packed<int16_t>(dst_data_r, src_data, 2, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S16P:
					copy_data_planar<int16_t>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<int16_t>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S32:
					copy_data_packed<int32_t>(dst_data_l, src_data, 2, 0, frame_sample_element_count);
					copy_data_packed<int32_t>(dst_data_r, src_data, 2, 1, frame_sample_element_count);
					break;
				case AV_SAMPLE_FMT_S32P:
					copy_data_planar<int32_t>(dst_data_l, src_data, 0, 0, frame_sample_element_count);
					copy_data_planar<int32_t>(dst_data_r, src_data, 1, 1, frame_sample_element_count);
					break;
				default:
					throw Runtime_error(
						"Audio format is not support",
						"Audio volume processor requires an audio format properly.",
						"Include FLT, S16, S32"
					);
				}
			}
			else
			{
				throw Runtime_error(
					"Invalid channel count",
					"Only mono and stereo audio are supported.",
					std::format("Got {} channels", frame_channels)
				);
			}

			push_frame(new_frame_l, 0);
			push_frame(new_frame_r, 1);
			boost::this_fiber::yield();
		}

		for (auto& output : output_item_l) output->set_eof();
		for (auto& output : output_item_r) output->set_eof();
	}

	void Audio_detach::draw_title()
	{
		imgui_utility::shadowed_text("Audio detacher");
	}

	bool Audio_detach::draw_content(bool readonly)
	{
		return false;
	}

	Json::Value Audio_detach::serialize() const
	{
		return {};
	}

	void Audio_detach::deserialize(const Json::Value& value) {}

}
