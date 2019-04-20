#pragma once


#include "engine/malmy.h"
#include "engine/engine.h"


struct SDL_Window;


namespace Malmy
{

	
struct IAllocator;


namespace PlatformInterface
{
	struct FileInfo
	{
		bool is_directory;
		char filename[MAX_PATH_LENGTH];
	};

	struct FileIterator;

	MALMY_EDITOR_API FileIterator* createFileIterator(const char* path, IAllocator& allocator);
	MALMY_EDITOR_API void destroyFileIterator(FileIterator* iterator);
	MALMY_EDITOR_API bool getNextFile(FileIterator* iterator, FileInfo* info);

	MALMY_EDITOR_API void setCurrentDirectory(const char* path);
	MALMY_EDITOR_API void getCurrentDirectory(char* buffer, int buffer_size);
	MALMY_EDITOR_API bool getOpenFilename(char* out, int max_size, const char* filter, const char* starting_file);
	MALMY_EDITOR_API bool getSaveFilename(char* out, int max_size, const char* filter, const char* default_extension);
	MALMY_EDITOR_API bool getOpenDirectory(char* out, int max_size, const char* starting_dir);
	MALMY_EDITOR_API bool shellExecuteOpen(const char* path, const char* parameters);
	MALMY_EDITOR_API void copyToClipboard(const char* text);

	MALMY_EDITOR_API bool deleteFile(const char* path);
	MALMY_EDITOR_API bool moveFile(const char* from, const char* to);
	MALMY_EDITOR_API size_t getFileSize(const char* path);
	MALMY_EDITOR_API bool fileExists(const char* path);
	MALMY_EDITOR_API bool dirExists(const char* path);
	MALMY_EDITOR_API u64 getLastModified(const char* file);
	MALMY_EDITOR_API bool makePath(const char* path);

	MALMY_EDITOR_API void setWindow(SDL_Window* window);
	MALMY_EDITOR_API void clipCursor(int x, int y, int w, int h);
	MALMY_EDITOR_API void unclipCursor();

	struct Process;

	MALMY_EDITOR_API Process* createProcess(const char* cmd, const char* args, IAllocator& allocator);
	MALMY_EDITOR_API void destroyProcess(Process& process);
	MALMY_EDITOR_API bool isProcessFinished(Process& process);
	MALMY_EDITOR_API int getProcessExitCode(Process& process);
	MALMY_EDITOR_API int getProcessOutput(Process& process, char* buf, int buf_size);

} // namespace PlatformInterface


} // namespace Malmy