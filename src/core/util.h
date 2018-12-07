#ifndef UTIL_H
#define UTIL_H
#define SNPRINTF _snprintf_s

#define ARRAY_SIZE_IN_ELEMENTS(a) (sizeof(a)/sizeof(a[0]))
#define INVALID_VALUE 0xFFFFFFFF

#include <vector>
#include <string>

//cesitli arcalr yazilcak burad
//cok elzem degil sunalik ama dursun


namespace Util
{
	void Sleep(int milliseconds);
	std::vector<std::string> Split(const std::string &s, char delim);
};

#endif
