#include <Windows.h>
#include <Winuser.h>
#include <ShellScalingAPI.h>
#include "editor/studio_app.h"
#define MF_RESOURCE_DONT_INCLUDE_WINDOWS_H
#include "stb/mf_resource.h"

INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{
	/*SetProcessDPIAware();
	HMODULE shcore = LoadLibrary("shcore.dll");
	if (shcore)
	{
		auto setter = (decltype(&SetProcessDpiAwareness))GetProcAddress(shcore, "SetProcessDpiAwareness");
		if (setter) setter(PROCESS_PER_MONITOR_DPI_AWARE);
	}*/

	auto* app = Malmy::StudioApp::create();
	app->run();
	int exit_code = app->getExitCode();
	Malmy::StudioApp::destroy(*app);
	//if(shcore) FreeLibrary(shcore);
	return exit_code;
}



/*

#include "engine/malmy.h"
#include "engine/debug/floating_points.h"
#include "unit_tests/suite/unit_test_app.h"


int main(int argc, const char * argv[])
{
	Malmy::enableFloatingPointTraps(true);
	Malmy::UnitTest::App app;
	app.init();
	app.run(argc, argv);
	app.exit();
}


*/