#include <iostream>

#include "testScene.h"

///test
#include <fstream>




int main()
{

	///
	Debug::Log("scene");

	std::string fileName = "default";

	std::ifstream file;
	file.open(("./res/scenes/" + fileName + ".mscene").c_str());


	std::string output;
	std::string line;

	if (file.is_open())
	{
		while (file.good())
		{
			getline(file, line);

			//burda parse islemi baslicak
			//shane dosyasini ayarladiktan sonra burasyi yaz
			//
			std::cout << line << std::endl;
		}
	}
	else
	{
		std::cerr << "Unable to load scene: " << fileName << std::endl;
	}


	system("PAUSE");
	///


	TestScene scene;

	//window kisminin detaylarinida ayarrladan cekcez sonra
	//default  1280x720
	Window window(1280, 720, "Malmy Engine !");
	renderEngine renderer(window);
	
	Engine engine(&window, &renderer, &scene);
	engine.Start();
	
	return 0;
}
