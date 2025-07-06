#pragma once

#include <memory>
#include <ranges>
#include <span>
#include <vector>

extern "C"
{
#include <libswresample/swresample.h>
}

// 音频重采样器
class Audio_resampler
{
	SwrContext* resampler_context;

  public:

	Audio_resampler() :
		resampler_context(swr_alloc())
	{
	}

	Audio_resampler(const Audio_resampler&) = delete;
	Audio_resampler(Audio_resampler&&) = delete;
	Audio_resampler& operator=(const Audio_resampler&) = delete;
	Audio_resampler& operator=(Audio_resampler&&) = delete;
	~Audio_resampler();

	struct Format
	{
		AVSampleFormat format;
		int sample_rate;
		AVChannelLayout channel_layout;
	};

	// 创建重采样器，需要给出输入和输出的格式
	static std::optional<std::unique_ptr<Audio_resampler>> create(Format input, Format output);

	// 插入静音采样
	void inject_silence(int sample_count);

	// 丢弃输出采样
	void drop_samples(int sample_count);

	// 给出输入采样数，计算输出采样数
	int calc_samples(int input_samples) const;

	// 计算重采样
	// - input: 输入采样数据，必须是包含了指针的std::span
	// - input_samples: 输入采样数
	// - output: 输出采样数据，必须是包含了指针的std::span
	// - output_samples: 输出缓冲的最大容量
	template <typename Input_t, typename Output_t>
	int resample(
		std::span<const Input_t* const> input,
		size_t input_samples,
		std::span<Output_t* const> output,
		size_t output_samples
	)
	{
		return swr_convert(
			resampler_context,
			(uint8_t* const* const)output.data(),
			output_samples,
			(const uint8_t* const* const)input.data(),
			input_samples
		);
	}
};
