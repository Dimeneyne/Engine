#include "stdafx.h"
#include "bench.h"
#include "../xrengine/device.h"

CRenderDevice* xBench::device = nullptr;

xBench::xBench(bool init) : m_start_time((init) ? device->GetTimerGlobal()->GetElapsed_sec() : -1.f)
{
}

void xBench::setupDevice(CRenderDevice* d)
{
	device = d;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

xr_vector<xptr<aBench::measures>> aBench::statistics = {};

aBench::aBench(measures* data) : xBench(device->isGameProcess()), m_data(data)
{
}

aBench::~aBench()
{
	if (m_start_time >= 0.f)
	{
		m_data->sum_time				+= device->GetTimerGlobal()->GetElapsed_sec() - m_start_time;
		++m_data->cnt;
		if (m_data->last_frame != device->dwFrame)
		{
			m_data->last_frame			= device->dwFrame;
			++m_data->frames;
		}
	}
}

aBench::measures* aBench::createMeasures(LPCSTR tag)
{
	return								statistics.emplace_back(tag).get();
}

void aBench::flushStatistics()
{
	for (auto& data : statistics)
	{
		float sum_time					= data->sum_time * 1000.f;
		float fcnt						= static_cast<float>(data->cnt);
		float fframes					= static_cast<float>(data->frames);

		float calls_per_frame			= fcnt / fframes;
		float time_per_call				= sum_time / fcnt;
		float time_per_frame			= sum_time / fframes;

		Msg								(
			"--benchmark [%s] sum [%.3f] cnt [%d] frames [%d] calls per frame [%.3f] time per call [%.3f] time per frame [%.3f]",
			data->tag.c_str(), sum_time, data->cnt, data->frames, calls_per_frame, time_per_call, time_per_frame
		);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

iBench::iBench(LPCSTR tag) : xBench(true), m_tag(tag)
{
}

iBench::~iBench()
{
	float result						= device->GetTimerGlobal()->GetElapsed_sec() - m_start_time;
	Msg									("--benchmark [%s] result [%.3f]", m_tag.c_str(), result * 1000.f);
}
