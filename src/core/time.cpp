#include "time.h"
#include <time.h>

#include <Windows.h>
#include <iostream>
static double g_freq;
static bool g_timerInitialized = false;

double Time::GetTime()
{
	//fps hesabi bu sn ler ile yapilyo
	//bilgisayr yanilirsa motor da yanilir

		if(!g_timerInitialized)
		{
			LARGE_INTEGER li;
			if(!QueryPerformanceFrequency(&li))
				std::cerr << "performas sikliginda sikinti var"  << std::endl;
			
			g_freq = double(li.QuadPart);
			g_timerInitialized = true;
		}
	
		LARGE_INTEGER li;
		if(!QueryPerformanceCounter(&li))
			std::cerr << "performas sayaci hatali" << std::endl;
		
		return double(li.QuadPart)/g_freq;

}
