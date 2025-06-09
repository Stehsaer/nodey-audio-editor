#include "processor/audio-stream.hpp"

#include <boost/fiber/operations.hpp>

#define ASSERT_FRAME_VALID assert(frame != nullptr && "Audio_frame is not initialized")

namespace processor
{
	Audio_frame::Audio_frame() :
		frame(nullptr, nullptr)
	{
		AVFrame* new_frame = av_frame_alloc();

		if (new_frame == nullptr) throw std::bad_alloc();

		frame = std::unique_ptr<AVFrame, void (*)(AVFrame*)>(
			new_frame,
			[](AVFrame* frame) { av_frame_free(&frame); }
		);
	}

	Audio_frame::~Audio_frame() = default;

	AVFrame* Audio_frame::data()
	{
		ASSERT_FRAME_VALID;
		return frame.get();
	}

	const AVFrame* Audio_frame::data() const
	{
		ASSERT_FRAME_VALID;
		return frame.get();
	}

	AVFrame* Audio_frame::operator->()
	{
		ASSERT_FRAME_VALID;
		return frame.get();
	}

	AVFrame* Audio_frame::operator->() const
	{
		ASSERT_FRAME_VALID;
		return frame.get();
	}

	AVFrame& Audio_frame::operator*()
	{
		ASSERT_FRAME_VALID;
		return *frame;
	}

	const AVFrame& Audio_frame::operator*() const
	{
		ASSERT_FRAME_VALID;
		return *frame;
	}

	boost::fibers::channel_op_status Audio_stream::try_push(std::shared_ptr<Audio_frame> frame)
	{
		const auto status = channel.try_push(std::move(frame));
		if (status == boost::fibers::channel_op_status::success) ++buffered_frames;

		return status;
	}

	std::expected<std::shared_ptr<Audio_frame>, boost::fibers::channel_op_status> Audio_stream::try_pop()
	{
		std::shared_ptr<Audio_frame> frame;
		const auto status = channel.try_pop(frame);
		if (status == boost::fibers::channel_op_status::success)
		{
			--buffered_frames;
			return frame;
		}

		return std::unexpected(status);
	}
}