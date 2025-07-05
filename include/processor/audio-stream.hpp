// audio-stream.hpp
// 包装了ffmpeg中的音频帧

#pragma once

#include "config.hpp"
#include "infra/processor.hpp"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <boost/fiber/buffered_channel.hpp>
#include <expected>
#include <list>

namespace processor
{
	// 音频帧
	// - 封装了libav中的AVFrame，保证内存安全
	class Audio_frame
	{
		std::unique_ptr<AVFrame, void (*)(AVFrame*)> frame;

	  public:

		Audio_frame();
		virtual ~Audio_frame();

		Audio_frame(const Audio_frame&) = delete;
		Audio_frame(Audio_frame&&) = default;
		Audio_frame& operator=(const Audio_frame&) = delete;
		Audio_frame& operator=(Audio_frame&&) = default;

		AVFrame* data();
		const AVFrame* data() const;
		AVFrame* operator->();
		AVFrame* operator->() const;
		AVFrame& operator*();
		const AVFrame& operator*() const;
	};

	// 音频流对象
	// - 管理与同步音频帧的生产和消费
	class Audio_stream : public infra::Processor::Product
	{
		boost::fibers::buffered_channel<std::shared_ptr<const Audio_frame>> channel;
		std::atomic<size_t> buffered_frames = 0;
		std::atomic<bool> end_of_stream;

	  public:

		Audio_stream() :
			channel(config::processor::audio_stream::buffer_size),
			end_of_stream(false)
		{
		}

		Audio_stream(const Audio_stream&) = delete;
		Audio_stream(Audio_stream&&) = delete;
		Audio_stream& operator=(const Audio_stream&) = delete;
		Audio_stream& operator=(Audio_stream&&) = delete;

		// 尝试向通道中推送音频帧
		// - 推送成功时，返回`boost::fibers::channel_op_status::success`
		boost::fibers::channel_op_status try_push(std::shared_ptr<const Audio_frame> frame);

		// 尝试从通道中取出音频帧
		// - 有数据时，通过std::expected返回音频帧
		// - 否则，通过`boost::fibers::channel_op_status`返回状态
		// - 注意，该操作类似queue.pop()，将删除缓冲区中的对应帧
		std::expected<std::shared_ptr<const Audio_frame>, boost::fibers::channel_op_status> try_pop();

		// 音频流是否到达末尾
		bool eof() const { return end_of_stream.load(); }

		// 通知音频流已经达到了末尾
		void set_eof() { end_of_stream.store(true); }

		// 音频流中暂存的音频帧数量
		size_t buffered_count() const { return buffered_frames.load(); }
	};
}