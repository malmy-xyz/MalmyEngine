#include "debug.h"


//renk kodlari eklencek
//Log beyaz
//info mavi
//error kirimiz

void Debug::Log(std::string log)
{
	std::cout << "-- " << log << std::endl;
}

void Debug::LogInfo(std::string log)
{
	std::cout << "!! " << log << std::endl;
}

void Debug::LogError(std::string log)
{

	std::cout << "?? " << log << std::endl;
}