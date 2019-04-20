#pragma once


#include "engine/malmy.h"


namespace Malmy
{
namespace PathUtils
{
	MALMY_ENGINE_API void normalize(const char* path, char* out, u32 max_size);
	MALMY_ENGINE_API void getDir(char* dir, int max_length, const char* src);
	MALMY_ENGINE_API void getBasename(char* basename, int max_length, const char* src);
	MALMY_ENGINE_API void getFilename(char* filename, int max_length, const char* src);
	MALMY_ENGINE_API void getExtension(char* extension, int max_length, const char* src);
	MALMY_ENGINE_API bool hasExtension(const char* filename, const char* ext);
	MALMY_ENGINE_API bool replaceExtension(char* path, const char* ext);
	MALMY_ENGINE_API bool isAbsolute(const char* path);
		

	struct MALMY_ENGINE_API PathDirectory
	{
		explicit PathDirectory(const char* path)
		{
			getDir(m_dir, sizeof(m_dir), path);
		}

		operator const char*()
		{
			return m_dir;
		}

		char m_dir[MAX_PATH_LENGTH];
	};


	struct MALMY_ENGINE_API FileInfo
	{
		explicit FileInfo(const char* path)
		{
			getExtension(m_extension, sizeof(m_extension), path);
			getBasename(m_basename, sizeof(m_basename), path);
			getDir(m_dir, sizeof(m_dir), path);
		}

		char m_extension[10];
		char m_basename[MAX_PATH_LENGTH];
		char m_dir[MAX_PATH_LENGTH];
	};
} // namespace PathUtils
} // namespace Malmy