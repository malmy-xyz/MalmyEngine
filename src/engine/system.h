#pragma once


#include "engine/malmy.h"


namespace Malmy
{
	MALMY_ENGINE_API bool copyFile(const char* from, const char* to);
	MALMY_ENGINE_API void getExecutablePath(char* buffer, int buffer_size);
	MALMY_ENGINE_API void messageBox(const char* text);
	MALMY_ENGINE_API void setCommandLine(int argc, char* argv[]);
	MALMY_ENGINE_API bool getCommandLine(char* output, int max_size);
	MALMY_ENGINE_API void* loadLibrary(const char* path);
	MALMY_ENGINE_API void unloadLibrary(void* handle);
	MALMY_ENGINE_API void* getLibrarySymbol(void* handle, const char* name);
}