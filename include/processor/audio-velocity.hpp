#pragma once

#include "infra/processor.hpp"
#include "processor/audio-stream.hpp"
#include "third-party/ui.hpp"

namespace processor
{
	class Velocity_modifier : public infra::Processor
	{
		float velocity = 1;
		bool keep_pitch = false;

	  public:

		Velocity_modifier() = default;
		virtual ~Velocity_modifier() = default;

		Velocity_modifier(const Velocity_modifier&) = delete;
		Velocity_modifier(Velocity_modifier&&) = delete;
		Velocity_modifier& operator=(const Velocity_modifier&) = delete;
		Velocity_modifier& operator=(Velocity_modifier&&) = delete;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		) const;

		virtual Json::Value serialize() const { return {}; }
		virtual void deserialize(const Json::Value& value) {}

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};

	class Pitch_modifier : public infra::Processor
	{
		float pitch = 0;

	  public:

		Pitch_modifier() = default;
		virtual ~Pitch_modifier() = default;

		Pitch_modifier(const Pitch_modifier&) = delete;
		Pitch_modifier(Pitch_modifier&&) = delete;
		Pitch_modifier& operator=(const Pitch_modifier&) = delete;
		Pitch_modifier& operator=(Pitch_modifier&&) = delete;

		static infra::Processor::Info get_processor_info();
		virtual Processor::Info get_processor_info_non_static() const { return get_processor_info(); }

		virtual std::vector<infra::Processor::Pin_attribute> get_pin_attributes() const;
		virtual void process_payload(
			const std::map<std::string, std::shared_ptr<infra::Processor::Product>>& input,
			const std::map<std::string, std::set<std::shared_ptr<infra::Processor::Product>>>& output,
			const std::atomic<bool>& stop_token,
			std::any& user_data
		) const;

		virtual Json::Value serialize() const { return {}; }
		virtual void deserialize(const Json::Value& value) {}

		virtual void draw_title();
		virtual bool draw_content(bool readonly);
	};
}