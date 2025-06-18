#include "processor/audio-io.hpp"
#include "config.hpp"
#include "frontend/imgui-utility.hpp"
#include "utility/dialog-utility.hpp"
#include "utility/free-utility.hpp"

#include <SDL_events.h>
#include <boost/fiber/operations.hpp>
#include <print>

extern "C"
{
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace processor
{
	infra::Processor::Info Audio_input::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_input",
			.display_name = "Audio Input",
			.singleton = true,
			.generate = std::make_unique<Audio_input>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_input::get_pin_attributes() const
	{
		return {
			{.identifier = "output",
			 .display_name = "Output",
			 .type = typeid(Audio_stream),
			 .is_input = false,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
		};
	}

	void Audio_input::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input [[maybe_unused]],
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data
	)
	{
		av_log_set_level(AV_LOG_QUIET);  // 禁用 FFmpeg 的日志输出

		const auto output_item = get_output_item<Audio_stream>(output, "output");

		/* 解码上下文 */

		AVFormatContext* format_context = nullptr;
		int audio_index;
		{
			const int open_ret = avformat_open_input(&format_context, file_path.c_str(), nullptr, nullptr);
			if (open_ret < 0)
				throw Runtime_error(
					"Failed to open input file",
					"The program fails to open the input file, check if the path is valid",
					std::format("File path: {}", file_path)
				);

			const int find_ret = avformat_find_stream_info(format_context, nullptr);
			if (find_ret < 0)
				throw Runtime_error(
					"Failed to find stream info",
					"The program cannot analyze the audio file structure, check the audio file",
					std::format("File path: {}", file_path)
				);

			audio_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
			if (audio_index < 0)
				throw Runtime_error(
					"No audio stream found",
					"The file does not contain any audio streams, check the audio file",
					std::format("File path: {}", file_path)
				);
		}
		const Free_utility free_format_context(std::bind(avformat_close_input, &format_context));

		/* 找解码器 */

		AVStream* const audio_stream = format_context->streams[audio_index];
		AVCodecParameters* const codec_params = audio_stream->codecpar;
		const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
		if (!codec)
			throw Runtime_error(
				"No decoder found",
				"The audio codec in the file is not supported",
				std::format("File path: {}", file_path)
			);

		/* 创建上下文 */

		AVCodecContext* codec_context = avcodec_alloc_context3(codec);
		if (!codec_context) throw std::bad_alloc();
		const Free_utility free_codec_context(std::bind(avcodec_free_context, &codec_context));

		if (avcodec_parameters_to_context(codec_context, codec_params) < 0)
			throw Runtime_error(
				"Failed to copy codec parameters",
				"Cannot initialize the audio decoder with the file parameters, check the audio file",
				std::format("File path: {}", file_path)
			);

		if (avcodec_open2(codec_context, codec, nullptr) < 0)
			throw Runtime_error(
				"Failed to open codec",
				"Cannot start the audio decoder",
				std::format("File path: {}", file_path)
			);

		AVPacket* packet = av_packet_alloc();
		if (packet == nullptr) throw std::bad_alloc();
		const Free_utility free_packet(std::bind(av_packet_free, &packet));

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
			const std::shared_ptr<Audio_frame> new_frame = std::make_shared<Audio_frame>();

			do {
				const int read_frame_result = av_read_frame(format_context, packet);
				if (read_frame_result < 0)
				{
					if (read_frame_result == AVERROR_EOF)
						break;
					else
						throw Runtime_error(
							"Error reading frame",
							"Failed to read audio data from the file. Internal error may have occurred.",
							std::format("File path: {}", file_path)
						);
				}
			} while (packet->stream_index != audio_index);  // 跳过非音频流

			const int send_packet_result = avcodec_send_packet(codec_context, packet);
			if (send_packet_result < 0)
			{
				if (send_packet_result == AVERROR(EAGAIN))
					continue;
				else
					throw Runtime_error(
						"Error sending packet to codec",
						"Failed to send audio packet to the decoder. Internal error may have occurred.",
						std::format("File path: {}", file_path)
					);
			}

			av_packet_unref(packet);

			const int receive_frame_result = avcodec_receive_frame(codec_context, new_frame->data());
			if (receive_frame_result < 0)
			{
				if (receive_frame_result == AVERROR(EAGAIN))
					continue;
				else if (receive_frame_result == AVERROR_EOF)
					break;
				else
					throw Runtime_error(
						"Error receiving frame from codec",
						"Failed to decode audio frame. Internal error may have occurred.",
						std::format("File path: {}", file_path)
					);
			}

			push_frame(new_frame);
		}

		for (auto& channel : output_item) channel->set_eof();
	}

	Json::Value Audio_input::serialize() const
	{
		Json::Value value(Json::ValueType::objectValue);
		value["file_path"] = file_path;
		return value;
	}

	void Audio_input::deserialize(const Json::Value& value)
	{
		file_path = value["file_path"].asString();
	}

	void Audio_input::draw_title()
	{
		imgui_utility::shadowed_text("Audio Input");
	}

	bool Audio_input::draw_content(bool readonly)
	{
		ImGui::SetNextItemWidth(200);
		ImGui::BeginGroup();
		ImGui::BeginDisabled(readonly);
		{
			// 选择文件
			if (ImGui::Button("Choose File"))
			{
				const auto open_file_result = open_file_dialog(
					"Select Audio File",
					{"Audio Files (*.wav, *.mp3, *.flac, *.ogg)",
					 "*.wav *.mp3 *.flac *.ogg",
					 "All Files",
					 "*.*"}
				);

				if (open_file_result.has_value()) file_path = open_file_result.value();
			}
		}
		ImGui::EndDisabled();
		ImGui::EndGroup();

		return false;
	}

	infra::Processor::Info Audio_output::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_output",
			.display_name = "Audio Output",
			.singleton = true,
			.generate = std::make_unique<Audio_output>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_output::get_pin_attributes() const
	{
		return {
			{.identifier = "input",
			 .display_name = "Input",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func = []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
		};
	}

	void Audio_output::draw_title()
	{
		imgui_utility::shadowed_text("Audio Output");
	}

	void Audio_output::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input [[maybe_unused]],
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output
		[[maybe_unused]],
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		const auto input_item_optional = get_input_item<Audio_stream>(input, "input");

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Audio output processor has no input",
				"Audio output processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		auto& input_item = input_item_optional.value().get();

		struct Stream_info
		{
			int sample_rate, element_count, channels;
			SwrContext* resampler;

			Stream_info(int sample_rate, int element_count, int channels, SwrContext* resampler) :
				sample_rate(sample_rate),
				element_count(element_count),
				channels(channels),
				resampler(resampler)
			{
				if (resampler == nullptr)
					throw Runtime_error(
						"Resampler initialization failed",
						"Cannot create audio resampler",
						"Resampler pointer is null"
					);
			}

			Stream_info(const Stream_info&) = delete;
			Stream_info(Stream_info&&) = delete;
			Stream_info& operator=(const Stream_info&) = delete;
			Stream_info& operator=(Stream_info&&) = delete;

			~Stream_info()
			{
				if (resampler != nullptr) swr_free(&resampler);
			}
		};

		std::optional<std::unique_ptr<Stream_info>> stream_info;

		auto frontend_context = std::any_cast<Process_context>(user_data);
		SDL_PauseAudioDevice(frontend_context.audio_device, 0);
		SDL_ClearQueuedAudio(frontend_context.audio_device);
		const Free_utility stop_audio(
			[frontend_context]
			{
				SDL_ClearQueuedAudio(frontend_context.audio_device);
				SDL_PauseAudioDevice(frontend_context.audio_device, 1);
			}
		);

		std::vector<config::audio::Buffer_type> output_buffer;

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

			// 获取帧参数

			const auto& frame_shared_ptr = pop_result.value();
			const auto& frame = *frame_shared_ptr->data();

			const auto frame_sample_rate = frame.sample_rate;
			const auto frame_sample_element_count = frame.nb_samples;
			const auto frame_channels = frame.ch_layout.nb_channels;

			// 假定流的采样率和通道数不变
			if (!stream_info.has_value())  // 此前没有数据
			{
				// 初始化软件重采样器
				SwrContext* resampler = swr_alloc();
				if (!resampler) throw std::bad_alloc();

				const AVChannelLayout input_layout = frame_channels == 2
													   ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
													   : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

				av_opt_set_chlayout(resampler, "in_chlayout", &input_layout, 0);
				av_opt_set_int(resampler, "in_sample_rate", frame_sample_rate, 0);
				av_opt_set_sample_fmt(resampler, "in_sample_fmt", (AVSampleFormat)frame.format, 0);

				av_opt_set_chlayout(resampler, "out_chlayout", &config::audio::av_channel_layout, 0);
				av_opt_set_int(resampler, "out_sample_rate", config::audio::sample_rate, 0);
				av_opt_set_sample_fmt(resampler, "out_sample_fmt", config::audio::av_format, 0);

				stream_info = std::make_unique<Stream_info>(
					frame_sample_rate,
					frame_sample_element_count,
					frame_channels,
					resampler
				);

				if (swr_init(resampler) < 0)
					throw Runtime_error(
						"Failed to initialize software resampler",
						"Cannot start the audio resampling process. Internal error may have occurred.",
						"swr_init() returned error"
					);
			}
			else
			{
				// 不匹配，抛出运行时异常

				if (frame_sample_rate != stream_info.value()->sample_rate)
					throw Runtime_error(
						"Sample rate changed",
						"Audio stream sample rate is inconsistent. Internal error may have occurred.",
						std::format(
							"Expected {}, got {}",
							stream_info.value()->sample_rate,
							frame_sample_rate
						)
					);

				if (frame_channels != stream_info.value()->channels)
					throw Runtime_error(
						"Channel count changed",
						"Audio stream channel count is inconsistent. Internal error may have occurred.",
						std::format("Expected {}, got {}", stream_info.value()->channels, frame_channels)
					);
			}

			if (frame_channels != 1 && frame_channels != 2)
				throw Runtime_error(
					"Invalid channel count",
					"Only mono and stereo audio are supported.",
					std::format("Got {} channels", frame_channels)
				);

			const int output_buffer_size
				= (float)frame_sample_element_count / frame_sample_rate * config::audio::sample_rate * 1.5;
			output_buffer.resize(output_buffer_size * 2);

			auto* const output_raw_pointer = reinterpret_cast<uint8_t*>(output_buffer.data());

			const auto convert_count = swr_convert(
				stream_info.value()->resampler,
				&output_raw_pointer,
				output_buffer_size * sizeof(config::audio::Buffer_type),
				frame.data,
				frame.nb_samples
			);
			if (convert_count < 0)
				throw Runtime_error(
					"Software resampler failed",
					"Cannot convert audio sample rate or format. Internal error may have occurred.",
					"swr_convert() returned error"
				);

			while (SDL_GetQueuedAudioSize(frontend_context.audio_device) > config::audio::max_buffer_size)
			{
				if (stop_token) return;
				boost::this_fiber::yield();
			}

			if (SDL_QueueAudio(
					frontend_context.audio_device,
					output_buffer.data(),
					convert_count * config::audio::channels * sizeof(config::audio::Buffer_type)
				)
				!= 0)
				throw Runtime_error(
					"Failed to queue audio data",
					"Cannot send audio data to the output device. Internal or IO error may have occurred.",
					std::format("SDL Error: {}", SDL_GetError())
				);
		}
	}
}