#ifndef COREENGINE_H
#define COREENGINE_H

#include "../rendering/renderingEngine.h"
#include <string>
class Game;

//ana motor kismi etkilesimleri vs yonetmek icin
//render + physic den olusuyo ilerde baska seylerde eklencek
//inputu da ayirabilirz
class CoreEngine
{
public:
	CoreEngine(Window* window, RenderingEngine* renderingEngine, Game* game);
	
	void Start(); //Update icinde //motoru baslatnakcicn
	void Stop();  //oyunu ve diger altsistemleri durdur
	
	inline RenderingEngine* GetRenderingEngine() { return m_renderingEngine; }
protected:
private:
	bool             m_isRunning;       //motor calsiiyomu
	double           m_frameTime;       //frame ne kadar suruyo fps = 1 / frameTime
	Window*          m_window;          //oyunun cizildigi window
	RenderingEngine* m_renderingEngine; //render motoru
	Game*            m_game;            //ekran cizile oyun iste
};

#endif // COREENGINE_H
