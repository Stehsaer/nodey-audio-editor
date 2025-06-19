// audio-input.hpp
// 提供音频输入流处理器

#include "infra/processor.hpp"
#include "processor/audio-stream.hpp"
#include "third-party/ui.hpp"

#include <SDL_audio.h>

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace processor
{
	// 音频输入处理器
	// - 负责读取与解码音频文件
	class Audio_input : public infra::Processor
	{
		size_t file_count = 1;
		std::optional<size_t> remove_index;
		std::vector<std::string> file_paths = {""};

	  public:

		Audio_input() = default;
		virtual ~Audio_input() = default;

		Audio_input(const Audio_input&) = delete;
		Audio_input(Audio_input&&) = delete;
		Audio_input& operator=(const Audio_input&) = delete;
		Audio_input& operator=(Audio_input&&) = delete;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		);

		virtual Json::Value serialize() const;
		virtual void deserialize(const Json::Value& value);

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};

	// 音频输出处理器
	// - 负责将解码后的音频数据输出到音频设备
	class Audio_output : public infra::Processor
	{
	  public:

		// 音频处理上下文
		// - 该处理器所需的上下文信息，通过std::any传递
		struct Process_context
		{
			SDL_AudioDeviceID audio_device;
		};

		Audio_output() = default;
		virtual ~Audio_output() = default;

		Audio_output(const Audio_output&) = delete;
		Audio_output(Audio_output&&) = delete;
		Audio_output& operator=(const Audio_output&) = delete;
		Audio_output& operator=(Audio_output&&) = delete;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		);

		virtual Json::Value serialize() const { return {}; }
		virtual void deserialize(const Json::Value& value) {}

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};
}
