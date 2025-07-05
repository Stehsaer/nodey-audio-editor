#include "utility/sw-resample.hpp"

extern "C"
{
#include <libavutil/opt.h>
}

std::optional<std::unique_ptr<Audio_resampler>> Audio_resampler::create(Format input, Format output)
{
	std::unique_ptr<Audio_resampler> resampler = std::make_unique<Audio_resampler>();
	if (!resampler->resampler_context) throw std::bad_alloc();

	av_opt_set_int(resampler->resampler_context, "in_sample_rate", input.sample_rate, 0);
	av_opt_set_sample_fmt(resampler->resampler_context, "in_sample_fmt", input.format, 0);
	av_opt_set_chlayout(resampler->resampler_context, "in_chlayout", &input.channel_layout, 0);
	av_opt_set_int(resampler->resampler_context, "out_sample_rate", output.sample_rate, 0);
	av_opt_set_sample_fmt(resampler->resampler_context, "out_sample_fmt", output.format, 0);
	av_opt_set_chlayout(resampler->resampler_context, "out_chlayout", &output.channel_layout, 0);

	if (swr_init(resampler->resampler_context) < 0) return std::nullopt;

	return resampler;
}

void Audio_resampler::inject_silence(int sample_count)
{
	swr_inject_silence(resampler_context, sample_count);
}

void Audio_resampler::drop_samples(int sample_count)
{
	swr_drop_output(resampler_context, sample_count);
}

int Audio_resampler::calc_samples(int input_samples) const
{
	return swr_get_out_samples(resampler_context, input_samples);
}

Audio_resampler::~Audio_resampler()
{
	if (resampler_context != nullptr) swr_free(&resampler_context);
}
