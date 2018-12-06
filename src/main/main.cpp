#include <iostream>

#include "testGame.h"

///test
#include <fstream>




int main()
{

	///
	std::cout << "SCENE TEST " << std::endl;

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
			//shane dosyasini yarladiktan sonra burasyi yaz
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


	TestGame game;

	Window window(1280, 720, "Malmy Engine !");
	RenderingEngine renderer(window);
	
	CoreEngine engine(60, &window, &renderer, &game);
	engine.Start();
	
	return 0;
}
