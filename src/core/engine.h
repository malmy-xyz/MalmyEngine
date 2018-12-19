#pragma once
#include "../rendering/renderEngine.h"
#include <string>

class Scene;

//ana motor kismi etkilesimleri vs yonetmek icin
//render + physic den olusuyo ilerde baska seylerde eklencek
//inputu da ayirabilirz
class Engine
{
public:
	Engine(Window* window, renderEngine* renderEngine, Scene* scene);
	
	void Start(); //Update icinde //motoru baslatnakcicn
	void Stop();  //oyunu ve diger altsistemleri durdur
	
	inline renderEngine* GetrenderEngine() { return m_renderEngine; }
protected:
private:
	bool             m_isRunning;       //motor calsiiyomu
	double           m_frameTime;       //frame ne kadar suruyo fps = 1 / frameTime
	Window*          m_window;          //oyunun cizildigi window
	renderEngine* m_renderEngine; //render motoru
	Scene*            m_scene;            //ekran cizile oyun iste
};
