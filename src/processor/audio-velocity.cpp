#ifdef _WIN32
#define NOMINMAX
#endif

#include "processor/audio-velocity.hpp"
#include "frontend/imgui-utility.hpp"

#include <boost/fiber/operations.hpp>
#include <soundtouch/SoundTouch.h>

namespace processor
{
	infra::Processor::Info Velocity_modifier::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "velocity_modifier",
			.display_name = "Velocity Modifier",
			.singleton = false,
			.generate = std::make_unique<Velocity_modifier>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Velocity_modifier::get_pin_attributes() const
	{
		return {
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

	void Velocity_modifier::draw_title()
	{
		imgui_utility::shadowed_text("Velocity Modifier");
	}

	infra::Processor::Info Pitch_modifier::get_processor_info()
	{
		return infra::Processor::Info{
			.identifier = "pitch_modifier",
			.display_name = "Pitch Modifier",
			.singleton = false,
			.generate = std::make_unique<Pitch_modifier>
		};
	}

	std::vector<infra::Processor::Pin_attribute> Pitch_modifier::get_pin_attributes() const
	{
		return {
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

	void Pitch_modifier::draw_title()
	{
		imgui_utility::shadowed_text("Pitch Modifier");
	}

	bool Velocity_modifier::draw_content(bool readonly)
	{
		ImGui::PushItemWidth(200);
		ImGui::BeginDisabled(readonly);
		{
			ImGui::DragFloat(
				"Velocity",
				&velocity,
				0.01,
				0.5,
				3.0,
				"%.2fx",
				ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic
			);

			ImGui::Checkbox("Keep Pitch", &keep_pitch);
		}
		ImGui::EndDisabled();
		ImGui::PopItemWidth();

		return false;
	}

	bool Pitch_modifier::draw_content(bool readonly)
	{
		ImGui::PushItemWidth(200);
		ImGui::BeginDisabled(readonly);
		{
			ImGui::InputFloat("Pitch (Note)", &pitch, 0.5, 1.0, "%+.1f");
		}
		ImGui::EndDisabled();
		ImGui::PopItemWidth();

		return false;
	}

	static std::vector<float> extract_samples_interleaved(const AVFrame* frame)
	{
		std::vector<float> samples;

		const int channel_count = frame->ch_layout.nb_channels;
		const int sample_count = frame->nb_samples;
		const auto sample_format = (AVSampleFormat)frame->format;

		samples.resize(sample_count * channel_count);

		switch (sample_format)
		{
		case AV_SAMPLE_FMT_FLT:
			std::copy(
				reinterpret_cast<const float*>(frame->data[0]),
				reinterpret_cast<const float*>(frame->data[0]) + sample_count * channel_count,
				samples.data()
			);
			break;
		case AV_SAMPLE_FMT_FLTP:
			for (int ch = 0; ch < channel_count; ++ch)
			{
				float* sample_ptr = samples.data() + ch;

				for (int i = 0; i < sample_count; ++i)
				{
					*sample_ptr = reinterpret_cast<const float*>(frame->data[ch])[i];
					sample_ptr += channel_count;
				}
			}
			break;
		case AV_SAMPLE_FMT_S16:
			std::transform(
				(const int16_t*)frame->data[0],
				(const int16_t*)frame->data[0] + sample_count * channel_count,
				samples.begin(),
				[](int16_t sample) { return static_cast<float>(sample) / 32768.0f; }
			);
			break;
		case AV_SAMPLE_FMT_S16P:
			for (int ch = 0; ch < channel_count; ++ch)
			{
				float* sample_ptr = samples.data() + ch;

				for (int i = 0; i < sample_count; ++i)
				{
					*sample_ptr = (float)reinterpret_cast<const int16_t*>(frame->data[ch])[i]
								/ std::numeric_limits<int16_t>::max();
					sample_ptr += channel_count;
				}
			}
			break;
		case AV_SAMPLE_FMT_S32:
			std::transform(
				(const int32_t*)frame->data[0],
				(const int32_t*)frame->data[0] + sample_count * channel_count,
				samples.begin(),
				[](int32_t sample) { return static_cast<float>(sample) / 2147483648.0f; }
			);
			break;
		case AV_SAMPLE_FMT_S32P:
			for (int ch = 0; ch < channel_count; ++ch)
			{
				float* sample_ptr = samples.data() + ch;

				for (int i = 0; i < sample_count; ++i)
				{
					*sample_ptr = (double)reinterpret_cast<const int32_t*>(frame->data[ch])[i]
								/ std::numeric_limits<int32_t>::max();
					sample_ptr += channel_count;
				}
			}
			break;
		default:
			throw infra::Processor::Runtime_error(
				"Unsupported sample format",
				"The processors do not support the given sample format.",
				std::format("Sample format: {}", av_get_sample_fmt_name((AVSampleFormat)frame->format))
			);
		}

		return samples;
	}

	static std::shared_ptr<Audio_frame> construct_audio_frame_float(
		const std::vector<float>& samples,
		int sample_rate,
		int channel_count,
		float time_ms
	)
	{
		std::shared_ptr<Audio_frame> new_frame = std::make_shared<Audio_frame>();
		AVFrame* frame = new_frame->data();

		frame->sample_rate = sample_rate;
		frame->ch_layout.nb_channels = channel_count;
		frame->nb_samples = static_cast<int>(samples.size() / channel_count);
		frame->format = AV_SAMPLE_FMT_FLT;
		frame->time_base = {.num = 1, .den = 1000};
		frame->pts = static_cast<int64_t>(time_ms);

		av_frame_make_writable(frame);
		av_frame_get_buffer(frame, 0);
		if (frame->data[0] == nullptr)
			throw infra::Processor::Runtime_error(
				"Failed to allocate audio frame buffer",
				"Cannot allocate buffer for audio frame.",
				"Audio frame data pointer is null"
			);

		std::copy(samples.begin(), samples.end(), reinterpret_cast<float*>(frame->data[0]));

		return new_frame;
	}

	static void soundtouch_process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		float velocity,
		float pitch,
		const std::string& processor_name
	)
	{
		const auto input_item = get_input_item<Audio_stream>(input, "input");
		const auto output_stream = get_output_item<Audio_stream>(output, "output");

		if (!input_item.has_value())
			throw infra::Processor::Runtime_error(
				std::format("{} has no input", processor_name),
				std::format("{} requires an audio stream input to function properly.", processor_name),
				"Input item 'input' not found"
			);

		Audio_stream& input_stream = input_item.value().get();

		std::unique_ptr<soundtouch::SoundTouch> soundtouch;

		bool input_stream_eof = false;

		const float time_ratio = 1.0f / velocity;
		int channel_count, sample_rate;
		float pts_begin = 0.0f;

		auto acquire_func = [&](int count)
		{
			std::vector<float> output_samples(count * channel_count);

			const int samples_read = soundtouch->receiveSamples(output_samples.data(), count);

			if (samples_read < 0)
				throw infra::Processor::Runtime_error(
					"SoundTouch receive samples failed",
					"Cannot receive audio samples from SoundTouch.",
					std::format("Received {} samples, expected at least {}", samples_read, count)
				);

			std::shared_ptr<Audio_frame> new_frame = construct_audio_frame_float(
				output_samples,
				sample_rate,
				channel_count,
				pts_begin + soundtouch->numSamples() * 1000.0f / sample_rate
			);

			pts_begin += static_cast<float>(samples_read) * 1000.0f / sample_rate;

			for (auto& stream : output_stream)
			{
				while (!stop_token)
				{
					const auto status = stream->try_push(new_frame);

					switch (status)
					{
					case boost::fibers::channel_op_status::success:
						break;
					case boost::fibers::channel_op_status::full:
						boost::this_fiber::yield();
						continue;
					default:
						throw infra::Processor::Runtime_error(
							"Failed to push audio frame to output stream",
							std::format("{} encountered an error when pushing audio frame.", processor_name),
							std::format("Channel push error: {}", (int)status)
						);
					}

					break;
				}
			}
		};

		while (!stop_token)
		{
			// 获取输入
			if (!input_stream_eof)
			{
				const auto pop_result = input_stream.try_pop();
				if (!pop_result.has_value())
				{
					if (pop_result.error() != boost::fibers::channel_op_status::empty)
						throw infra::Processor::Runtime_error(
							"Unexpected error when fetching audio frame",
							std::format("{} encountered an unexpected error.", processor_name),
							std::format("Channel fetch error: {}", (int)pop_result.error())
						);

					if (input_stream.eof()) input_stream_eof = true;
				}
				else
				{
					constexpr int max_queued_samples = 65536;

					const AVFrame* frame = pop_result.value()->data();

					if (soundtouch == nullptr)
					{
						soundtouch = std::make_unique<soundtouch::SoundTouch>();

						if (frame->sample_rate < 8000 || frame->sample_rate > 48000)
							throw infra::Processor::Runtime_error(
								"Unsupported sample rate",
								std::format(
									"{} requires a sample rate between 8000 and 48000 Hz.",
									frame->sample_rate
								),
								std::format("Sample rate: {}", frame->sample_rate)
							);

						soundtouch->setSampleRate(frame->sample_rate);
						soundtouch->setChannels(frame->ch_layout.nb_channels);

						soundtouch->setRate(velocity);
						soundtouch->setPitch(pitch);

						channel_count = frame->ch_layout.nb_channels;
						pts_begin = frame->pts * av_q2d(frame->time_base);
						sample_rate = frame->sample_rate;
					}

					if (soundtouch == nullptr)
						throw infra::Processor::Runtime_error(
							"SoundTouch initialization failed",
							"Cannot create SoundTouch instance for audio velocity processing.",
							"SoundTouch pointer is null"
						);

					while (!stop_token && soundtouch->numSamples() > max_queued_samples)
						boost::this_fiber::yield();

					const auto samples = extract_samples_interleaved(frame);
					soundtouch->putSamples(
						samples.data(),
						static_cast<int>(samples.size() / frame->ch_layout.nb_channels)
					);
				}
			}

			// 获取输出
			if (soundtouch != nullptr)
			{
				// 处理完成
				if (soundtouch->numSamples() == 0 && input_stream_eof) break;

				const uint32_t min_samples = time_ratio * 1152;
				const uint32_t max_samples = time_ratio * 1152 * 3;

				if (soundtouch->numSamples() > min_samples)
				{
					const int target_sample_count = std::min(soundtouch->numSamples(), max_samples);

					acquire_func(target_sample_count);
				}
				else if (input_stream_eof)
				{
					soundtouch->flush();
					const int remaining_samples = soundtouch->numSamples();

					if (remaining_samples > 0)
					{
						acquire_func(remaining_samples);
					}

					break;
				}
			}

			boost::this_fiber::yield();
		}

		for (auto& stream : output_stream) stream->set_eof();
	}

	void Velocity_modifier::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		soundtouch_process_payload(
			input,
			output,
			stop_token,
			velocity,
			keep_pitch ? 1 / velocity : 1,
			get_processor_info().display_name
		);
	}

	void Pitch_modifier::process_payload(
		const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
		const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
		const std::atomic<bool>& stop_token,
		std::any& user_data [[maybe_unused]]
	)
	{
		soundtouch_process_payload(
			input,
			output,
			stop_token,
			1,
			std::pow(2.0f, pitch / 12.0f),
			get_processor_info().display_name
		);
	}
}