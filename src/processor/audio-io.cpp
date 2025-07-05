#include "processor/audio-io.hpp"
#include "config.hpp"
#include "frontend/nerdfont.hpp"
#include "utility/dialog-utility.hpp"
#include "utility/free-utility.hpp"
#include "utility/imgui-utility.hpp"
#include "utility/sw-resample.hpp"

#include <SDL_events.h>
#include <algorithm>
#include <boost/fiber/operations.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <print>

#include <lame/lame.h>

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
			.generate = std::make_unique<Audio_input>,
			.description = "Audio Input Processor\n\n"
						   "## Functionality\n"
						   "- Reads audio files and outputs audio streams\n"
						   "- Supports multiple file inputs with configurable paths\n"
						   "- Outputs audio in 48kHz, 32-bit float format\n\n"
						   "## Usage\n"
						   "- Add file paths to the input list\n"
						   "- Connect output pins to other audio processors or outputs\n"
						   "- Supports real-time audio playback from files",
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_input::get_pin_attributes() const
	{
		std::vector<infra::Processor::Pin_attribute> output;
		output.reserve(file_paths.size());

		for (auto i : std::views::iota(0zu, file_paths.size()))
		{
			output.push_back(
				{.identifier = std::format("output_{}", i),
				 .display_name = std::format("Output {}", i + 1),
				 .type = typeid(Audio_stream),
				 .is_input = false,
				 .generate_func =
					 []
				 {
					 return std::make_shared<Audio_stream>();
				 }}
			);
		}

		return output;
	}

	void Audio_input::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input [[maybe_unused]],
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
#ifndef _DEBUG
		av_log_set_level(AV_LOG_QUIET);  // 禁用 FFmpeg 的日志输出
#endif

		/* 解码上下文 */

		auto file_fiber = [](const std::set<std::shared_ptr<Audio_stream>> output_item,
							 const std::string& file_path,
							 const std::atomic<bool>& main_stop_token,
							 std::atomic<bool>& error_stop_token)
		{
			AVFormatContext* format_context = nullptr;
			int audio_index;
			{
				const int open_ret
					= avformat_open_input(&format_context, file_path.c_str(), nullptr, nullptr);
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

			auto push_frame =
				[&main_stop_token, &error_stop_token, &output_item](const std::shared_ptr<Audio_frame>& frame)
			{
				for (auto& channel : output_item)
				{
					if (main_stop_token || error_stop_token) return;

					while (channel->try_push(frame) != boost::fibers::channel_op_status::success)
					{
						if (main_stop_token || error_stop_token) return;
						boost::this_fiber::yield();
					}
				}

				boost::this_fiber::yield();
			};

			while (!main_stop_token && !error_stop_token)
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
		};

		std::atomic<bool> error_stop_token = false;
		std::any error_data;

		std::vector<boost::fibers::fiber> fibers;
		fibers.reserve(file_paths.size());

		for (const auto& [idx, file_path] : std::views::enumerate(file_paths))
			if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path))
				throw Runtime_error(
					std::format("Invalid file path in slot {}", idx + 1),
					"The specified audio file does not exist or is not a regular file.",
					std::format("File path: {}", file_path)
				);

		for (const auto& [idx, file_path] : std::views::enumerate(file_paths))
		{
			const auto output_item = get_output_item<Audio_stream>(output, std::format("output_{}", idx));

			fibers.emplace_back(
				boost::fibers::launch::dispatch,
				[&file_fiber, &error_data, file_path, output_item, &stop_token, &error_stop_token]
				{
					try
					{
						file_fiber(output_item, file_path, stop_token, error_stop_token);
					}
					catch (const Runtime_error& e)
					{
						error_stop_token = true;
						error_data = e;
					}
					catch (const std::exception& e)
					{
						error_stop_token = true;
						error_data = Runtime_error(
							"Unexpected error in audio input processing",
							"An unexpected error occurred while processing the audio input.",
							std::format("Error: {}", e.what())
						);
					}
					catch (...)
					{
						error_stop_token = true;
						error_data = Runtime_error(
							"Unknown error in audio input processing",
							"An unknown error occurred while processing the audio input.",
							"Unknown error"
						);
					}
				}
			);
		}

		for (auto& fiber : fibers)
			if (fiber.joinable()) fiber.join();

		if (error_data.has_value())
		{
			if (error_data.type() == typeid(Runtime_error))
			{
				const auto& error = std::any_cast<const Runtime_error&>(error_data);
				throw Runtime_error(error.message, error.explanation, error.detail);
			}
			else
			{
				throw Runtime_error(
					"Unknown error in audio input processing",
					"An unknown type of error occurred while processing the audio input.",
					std::format("Error: {}", error_data.type().name())
				);
			}
		}
	}

	Json::Value Audio_input::serialize() const
	{
		Json::Value file_path(Json::ValueType::arrayValue);
		for (const auto& path : file_paths) file_path.append(path);

		Json::Value value(Json::ValueType::objectValue);
		value["file_path"] = file_path;

		return value;
	}

	void Audio_input::deserialize(const Json::Value& value)
	{
		if (!value.isObject() || !value.isMember("file_path") || !value["file_path"].isArray())
			throw Runtime_error(
				"Failed to deserialize JSON file",
				"Audio_input failed to serialize the JSON input because of missing or invalid fields.",
				"Wrong field: file_path"
			);

		file_paths.clear();
		for (const auto& path : value["file_path"])
		{
			if (!path.isString())
				throw Runtime_error(
					"Failed to deserialize JSON file",
					"Audio_input failed to serialize the JSON input because of missing or invalid fields.",
					"Wrong field: file_path.path"
				);
			file_paths.push_back(path.asString());
		}

		file_count = file_paths.size();
		file_count = std::max<size_t>(file_count, 1);
		file_paths.resize(file_count);
		remove_index.reset();
	}

	void Audio_input::draw_title()
	{
		imgui_utility::shadowed_text("Audio Input");
	}

	bool Audio_input::draw_content(bool readonly)
	{
		bool modified = false;
		ImGui::Separator();

		if (remove_index.has_value())
		{
			file_paths.erase(file_paths.begin() + remove_index.value());
			remove_index.reset();
			file_count--;
			modified = true;
		}

		file_count = std::max<size_t>(file_count, 1);

		if (file_count > file_paths.size())
		{
			file_paths.resize(file_count);
			modified = true;
		}

		if (file_count < file_paths.size())
			THROW_LOGIC_ERROR("File count of ({}) smaller than file paths size ({})", file_count, file_paths);
		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SetNextItemWidth(200);
			ImGui::BeginGroup();
			ImGui::BeginDisabled(readonly);
			{
				for (size_t i = 0; i < file_count; i++)
				{
					ImGui::SeparatorText(std::format("Slot {}", i + 1).c_str());

					const auto normalize_path_offset = file_paths[i].find_last_of('/');

					ImGui::TextWrapped(
						"File Path: %s",
						file_paths[i].empty()
							? "None"
							: file_paths[i].c_str()
								  + ((normalize_path_offset != std::string::npos) ? normalize_path_offset + 1
																				  : 0)
					);
					if (ImGui::Button(std::format("Browse " ICON_EXT_LINK "##browse_button_{}", i).c_str()))
					{
						const auto& current_path = file_paths[i];
						std::string default_path = ".";

						if (std::filesystem::exists(current_path)
							&& std::filesystem::is_regular_file(current_path))
							default_path = std::filesystem::path(current_path).parent_path().string();

						const auto open_file_result = open_file_dialog(
							"Select Audio File",
							{"Audio Files (*.wav, *.mp3, *.flac, *.ogg)",
							 "*.wav *.mp3 *.flac *.ogg",
							 "All Files",
							 "*.*"},
							default_path
						);

						if (open_file_result.has_value()) file_paths[i] = open_file_result.value();
					}

					ImGui::SameLine();

					ImGui::BeginDisabled(file_count == 1);
					if (ImGui::Button(std::format(ICON_TRASH "##delete_button_{}", i).c_str()))
						remove_index = i;
					ImGui::EndDisabled();
				}

				ImGui::Separator();

				if (ImGui::Button(ICON_FILE_ADD)) file_count++;
			}
			ImGui::EndDisabled();
			ImGui::EndGroup();
		}

		return modified;
	}

	infra::Processor::Info Audio_output::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "audio_output",
			.display_name = "Audio Output",
			.singleton = true,
			.generate = std::make_unique<Audio_output>,
			.description = "Audio Output Processor\n\n"
						   "## Functionality\n"
						   "- Outputs audio streams to the system's audio device\n"
						   "- Supports real-time audio playback\n"
						   "- Outputs audio in 48kHz, 32-bit float format\n\n"
						   "## Usage\n"
						   "- Connect an audio stream input to the 'Input' pin\n"
						   "- The processor will play the audio through the system's default output device",
		};
	}

	std::vector<infra::Processor::Pin_attribute> Audio_output::get_pin_attributes() const
	{
		return {
			{.identifier = "input",
			 .display_name = "Input",
			 .type = typeid(Audio_stream),
			 .is_input = true,
			 .generate_func =
				 []
			 {
				 return std::make_shared<Audio_stream>();
			 }},
		};
	}

	void Audio_output::draw_title()
	{
		imgui_utility::shadowed_text("Audio Output");
	}
	bool Audio_output::draw_content(bool readonly)
	{
		ImGui::Separator();

		if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("This processor outputs audio to the system's default audio device.");
			ImGui::Text("No configurable properties available.");
		}

		return false;  // No modifications
	}

	void Audio_output::do_preview(
		Audio_stream& input_stream,
		SDL_AudioDeviceID audio_device,
		const std::atomic<bool>& stop_token
	)
	{
		struct Stream_info
		{
			int sample_rate, element_count, channels;
			std::unique_ptr<Audio_resampler> resampler;
		};

		std::optional<Stream_info> stream_info;

		SDL_PauseAudioDevice(audio_device, 0);
		SDL_ClearQueuedAudio(audio_device);
		const Free_utility stop_audio(
			[audio_device]
			{
				SDL_ClearQueuedAudio(audio_device);
				SDL_PauseAudioDevice(audio_device, 1);
			}
		);

		std::vector<config::audio::Buffer_type> output_buffer;

		while (!stop_token)
		{
			// 获取数据

			const auto pop_result = input_stream.try_pop();

			if (!pop_result.has_value())
			{
				if (pop_result.error() == boost::fibers::channel_op_status::empty)
				{
					if (input_stream.eof()) break;
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
				const AVChannelLayout input_layout = frame_channels == 2
													   ? AVChannelLayout(AV_CHANNEL_LAYOUT_STEREO)
													   : AVChannelLayout(AV_CHANNEL_LAYOUT_MONO);

				const Audio_resampler::Format input_format{
					.format = (AVSampleFormat)frame.format,
					.sample_rate = frame_sample_rate,
					.channel_layout = input_layout
				};

				const Audio_resampler::Format output_format{
					.format = config::audio::av_format,
					.sample_rate = config::audio::sample_rate,
					.channel_layout = config::audio::av_channel_layout
				};

				auto resampler_create = Audio_resampler::create(input_format, output_format);
				if (!resampler_create.has_value())
					throw infra::Processor::Runtime_error(
						"Failed to create audio resampler",
						"Cannot create audio resampler for the input audio format. Internal error may have "
						"occurred.",
						std::format(
							"Input format: {}, sample rate: {}, channels: {}",
							frame.format,
							frame_sample_rate,
							frame_channels
						)
					);

				stream_info = Stream_info{
					.sample_rate = frame_sample_rate,
					.element_count = frame_sample_element_count,
					.channels = frame_channels,
					.resampler = std::move(resampler_create.value())
				};
			}
			else
			{
				// 不匹配，抛出运行时异常

				if (frame_sample_rate != stream_info->sample_rate)
					throw Runtime_error(
						"Sample rate changed",
						"Audio stream sample rate is inconsistent. Internal error may have occurred.",
						std::format("Expected {}, got {}", stream_info->sample_rate, frame_sample_rate)
					);

				if (frame_channels != stream_info->channels)
					throw Runtime_error(
						"Channel count changed",
						"Audio stream channel count is inconsistent. Internal error may have occurred.",
						std::format("Expected {}, got {}", stream_info->channels, frame_channels)
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

			const auto output_ptr_array = std::to_array({output_buffer.data()});

			const auto convert_count = stream_info->resampler->resample<uint8_t, config::audio::Buffer_type>(
				std::span<const uint8_t* const>{frame.data, frame.data + frame_channels},
				frame_sample_element_count,
				std::span(output_ptr_array),
				output_buffer_size
			);

			if (convert_count < 0)
				throw Runtime_error(
					"Software resampler failed",
					"Cannot convert audio sample rate or format. Internal error may have occurred.",
					"swr_convert() returned error"
				);

			if constexpr (std::is_same_v<config::audio::Buffer_type, float>)
				for (auto& val : output_buffer) val = std::clamp<float>(val, -1.0, +1.0);

			while (SDL_GetQueuedAudioSize(audio_device) > config::audio::max_buffer_size)
			{
				if (stop_token) return;
				boost::this_fiber::yield();
			}

			if (SDL_QueueAudio(
					audio_device,
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

	void Audio_output::do_export(
		Audio_stream& input_stream,
		Process_context context,
		const std::atomic<bool>& stop_token
	)
	{
		/* 设置上下文 */

		std::ofstream output_file(context.export_path, std::ios::binary);
		if (!output_file.is_open())
			throw Runtime_error(
				"Failed to open output file",
				"Cannot open the output file for writing. Check if the path is valid and writable.",
				std::format("Output path: {}", context.export_path)
			);

		lame_t lame = lame_init();
		if (lame == nullptr) throw std::bad_alloc();
		const Free_utility free_lame(std::bind(lame_close, lame));

		std::vector<std::byte> file_buffer;

		bool lame_param_set = false;
		auto& time = *context.time;
		int sample_rate = 0;

		auto push_silence = [lame, &file_buffer, &sample_rate, &output_file](double time)
		{
			const int silence_samples = static_cast<int>(time * sample_rate);
			if (silence_samples <= 0) return;

			const int buffer_size = 1.25 * silence_samples + 7200;  // LAME 的缓冲区大小
			file_buffer.resize(buffer_size);

			std::vector<short> silence_buffer(silence_samples * 2, 0);  // 双声道静音缓冲区

			const int written = lame_encode_buffer_interleaved(
				lame,
				silence_buffer.data(),
				silence_samples,
				reinterpret_cast<unsigned char*>(file_buffer.data()),
				buffer_size
			);

			if (written < 0)
				throw Runtime_error(
					"Failed to encode silence",
					"Cannot encode silence audio data. Internal error may have occurred.",
					std::format("LAME Error: {}", written)
				);

			if (written == 0) return;

			output_file.write(reinterpret_cast<const char*>(file_buffer.data()), written);
		};

		auto parse_frame = [lame, &file_buffer, &output_file](const AVFrame& frame)
		{
			const int buffer_size = 4 * frame.nb_samples + 7200;  // LAME 的缓冲区大小
			file_buffer.resize(buffer_size);

			int written;

			switch ((AVSampleFormat)frame.format)
			{
			case AV_SAMPLE_FMT_S16:
			{
				written = lame_encode_buffer_interleaved(
					lame,
					reinterpret_cast<short*>(frame.data[0]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
				break;
			}
			case AV_SAMPLE_FMT_S16P:
			{
				written = lame_encode_buffer(
					lame,
					reinterpret_cast<short*>(frame.data[0]),
					reinterpret_cast<short*>(frame.data[1]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
			}
			case AV_SAMPLE_FMT_S32:
			{
				written = lame_encode_buffer_interleaved_int(
					lame,
					reinterpret_cast<int*>(frame.data[0]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
				break;
			}
			case AV_SAMPLE_FMT_S32P:
			{
				written = lame_encode_buffer_int(
					lame,
					reinterpret_cast<int*>(frame.data[0]),
					reinterpret_cast<int*>(frame.data[1]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
			}
			case AV_SAMPLE_FMT_FLT:
			{
				written = lame_encode_buffer_interleaved_ieee_float(
					lame,
					reinterpret_cast<const float*>(frame.data[0]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
				break;
			}
			case AV_SAMPLE_FMT_FLTP:
			{
				written = lame_encode_buffer_ieee_float(
					lame,
					reinterpret_cast<const float*>(frame.data[0]),
					reinterpret_cast<const float*>(frame.data[1]),
					frame.nb_samples,
					reinterpret_cast<unsigned char*>(file_buffer.data()),
					buffer_size
				);
				break;
			}
			default:
				throw Runtime_error(
					"Unsupported sample format",
					"The audio sample format is not supported for encoding.",
					std::format("Sample format: {}", frame.format)
				);
			}

			if (written < 0)
				throw Runtime_error(
					"Failed to encode audio frame",
					"Cannot encode the audio frame. Internal error may have occurred.",
					std::format("LAME Error: {}", written)
				);

			if (written == 0) return;
			output_file.write(reinterpret_cast<const char*>(file_buffer.data()), written);
		};

		while (!stop_token)
		{
			const auto pop_result = input_stream.try_pop();

			if (!pop_result.has_value())
			{
				if (pop_result.error() == boost::fibers::channel_op_status::empty)
				{
					if (input_stream.eof()) break;
				}
				else if (pop_result.error() == boost::fibers::channel_op_status::closed)
					THROW_LOGIC_ERROR("Unexpected channel closed in Audio_output::process_payload");
			}
			else
			{
				const std::shared_ptr<const Audio_frame>& audio_frame = pop_result.value();
				const AVFrame& frame = *audio_frame->data();

				if (!lame_param_set)  // 在第一帧初始化重采样器
				{
					lame_param_set = true;

					lame_set_in_samplerate(lame, frame.sample_rate);
					lame_set_num_channels(lame, frame.ch_layout.nb_channels);
					lame_set_quality(lame, 2);
					lame_set_mode(
						lame,
						frame.ch_layout.nb_channels == 2 ? MPEG_mode::STEREO : MPEG_mode::MONO
					);
					lame_set_out_samplerate(lame, config::audio::sample_rate);
					lame_set_VBR(lame, vbr_off);
					lame_set_brate(lame, context.kbps);

					if (lame_init_params(lame) == -1)
						throw Runtime_error(
							"Failed to initialize LAME parameters",
							"Cannot set LAME parameters for encoding. Internal error may have occurred."
						);

					sample_rate = frame.sample_rate;
				}

				const double frame_begin = frame.pts * av_q2d(frame.time_base);
				const double frame_end = frame_begin + frame.nb_samples / (double)frame.sample_rate;
				const double silence_time = frame_begin - time;

				push_silence(silence_time);
				parse_frame(frame);
				time = frame_end;
			}

			boost::this_fiber::yield();
		}
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
		auto frontend_context = std::any_cast<Process_context>(user_data);

		if (!input_item_optional.has_value())
			throw Runtime_error(
				"Audio output processor has no input",
				"Audio output processor requires an audio stream input to function properly.",
				"Input item 'input' not found"
			);

		if (!frontend_context.do_export)
			do_preview(input_item_optional.value().get(), frontend_context.audio_device, stop_token);
		else
			do_export(input_item_optional.value().get(), frontend_context, stop_token);
	}
}