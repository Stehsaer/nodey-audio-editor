// 注册处理器类
// 每次添加新类时都需要在此处注册

#include "infra/processor.hpp"

#include "processor/audio-io.hpp"
#include "processor/audio-vol.hpp"

namespace infra
{
	void register_all_processors()
	{
		Processor::register_processor<processor::Audio_input>();
		Processor::register_processor<processor::Audio_output>();
		Processor::register_processor<processor::Audio_vol>();
	}
}