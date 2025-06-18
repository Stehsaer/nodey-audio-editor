// 注册处理器类
// 每次添加新类时都需要在此处注册

#include "infra/processor.hpp"

#include "processor/audio-amix.hpp"
#include "processor/audio-bimix.hpp"
#include "processor/audio-io.hpp"
#include "processor/audio-velocity.hpp"
#include "processor/audio-vol.hpp"

namespace infra
{
	void register_all_processors()
	{
		Processor::register_processor<processor::Audio_input>();
		Processor::register_processor<processor::Audio_output>();
		Processor::register_processor<processor::Audio_vol>();
		Processor::register_processor<processor::Velocity_modifier>();
		Processor::register_processor<processor::Pitch_modifier>();
		Processor::register_processor<processor::Audio_amix>();
		Processor::register_processor<processor::Audio_bimix>();
	}
}