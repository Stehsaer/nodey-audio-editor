// 注册处理器类
// 每次添加新类时都需要在此处注册

#include "infra/processor.hpp"

#include "processor/audio-amix.hpp"
#include "processor/audio-bimix.hpp"
#include "processor/audio-detach.hpp"
#include "processor/audio-echo.hpp"
#include "processor/audio-io.hpp"
#include "processor/audio-velocity.hpp"
#include "processor/audio-vol.hpp"
#include "processor/display-spectrum.hpp"
#include "processor/display-waveform.hpp"
#include "processor/ease-in.hpp"
#include "processor/ease-out.hpp"
#include "processor/time-shift.hpp"

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
		Processor::register_processor<processor::Audio_bimix_v2>();
		Processor::register_processor<processor::Audio_echo>();
		Processor::register_processor<processor::Audio_detach>();
		Processor::register_processor<processor::Display_waveform>();
		Processor::register_processor<processor::Display_spectrum>();
		Processor::register_processor<processor::Time_shift>();
		Processor::register_processor<processor::Ease_in>();
		Processor::register_processor<processor::Ease_out>();
	}
}