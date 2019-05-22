#include <Windows.h>
#include <Winuser.h>
#include <ShellScalingAPI.h>
#include "editor/studio_app.h"
#define MF_RESOURCE_DONT_INCLUDE_WINDOWS_H
#include "stb/mf_resource.h"

INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	//motoru basalttik
	//winapi yerine main olmuyo
	//
	auto* app = Malmy::StudioApp::create();
	app->run();
	int exit_code = app->getExitCode();
	Malmy::StudioApp::destroy(*app);

	return exit_code;

}
