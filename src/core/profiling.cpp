#include "profiling.h"
#include "time.h"
#include <cassert>
#include <iostream>

void ProfileTimer::StartInvocation()
{
	m_startTime = Time::GetTime();
}

void ProfileTimer::StopInvocation()
{
	if(m_startTime == 0)
	{
		std::cout << "Hata baslmadan durduruyo" << std::endl;
		assert(m_startTime != 0);
	}
	
	m_numInvocations++;
	m_totalTime += (Time::GetTime() - m_startTime);
	m_startTime = 0;
}

double ProfileTimer::GetTimeAndReset(double divisor)
{
	divisor = (divisor == 0) ? m_numInvocations : divisor;
	double result = (m_totalTime == 0 && divisor == 0.0) ? 0.0 : (1000.0 * m_totalTime)/((double)divisor);
	m_totalTime = 0.0;
	m_numInvocations = 0;
	
	return result;
}

double ProfileTimer::DisplayAndReset(const std::string& message, double divisor, int displayedMessageLength)
{
	std::string whiteSpace = "";
	for(int i = message.length(); i < displayedMessageLength; i++)
	{
		whiteSpace += " ";
	}
	
	double time = GetTimeAndReset(divisor);
	//std::cout << message << whiteSpace << time << " ms" << std::endl;
	return time;
}
