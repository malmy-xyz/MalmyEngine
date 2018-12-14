#include "coreEngine.h"
#include "timing.h"
#include "../rendering/window.h"
#include "input.h"
#include "util.h"
#include "game.h"

#include <stdio.h>

CoreEngine::CoreEngine(Window* window, RenderingEngine* renderingEngine, Game* game) :
	m_isRunning(false),
	//frame rate default 50 bunlari yarlardan cekcen sonra onu yazinca
	m_frameTime(1.0 / 50),
	m_window(window),
	m_renderingEngine(renderingEngine),
	m_game(game)
{
	//motoru yarlama

	m_game->SetEngine(this);
	
	//oyunun baslatildigi yer

	m_game->Init(*m_window);
}

void CoreEngine::Start()
{
	if(m_isRunning)
	{
		return;
	}
		
	m_isRunning = true;

	double lastTime = Time::GetTime(); //son frame baslrkenki zman
	double frameCounter = 0;           //gecen sure
	double unprocessedTime = 0;        //hasaba katilmayn sure (hata gibi bisey)
	int frames = 0;                    //islenen karelerin sayisi

	ProfileTimer sleepTimer;
	ProfileTimer swapBufferTimer;
	ProfileTimer windowUpdateTimer;
	while(m_isRunning)
	{
		bool render = false;           //yeniden rendera gerk varmi

		double startTime = Time::GetTime();       //baslangic zamnau
		double passedTime = startTime - lastTime; //son kareden  u ana gecen sure
		lastTime = startTime;

		unprocessedTime += passedTime;
		frameCounter += passedTime;

		//motor her saniyeden sonra bilgilari gunceller
		//fps vs iste
		//zamn vs de sifirliyo
		if(frameCounter >= 1.0)
		{
			double totalTime = ((1000.0 * frameCounter)/((double)frames));

			//toplam olculen zamn
			double totalMeasuredTime = 0.0;
			
			totalMeasuredTime += m_game->DisplayInputTime((double)frames);
			totalMeasuredTime += m_game->DisplayUpdateTime((double)frames);
			totalMeasuredTime += m_renderingEngine->DisplayRenderTime((double)frames);
			totalMeasuredTime += sleepTimer.DisplayAndReset("Sleep Time: ", (double)frames);
			totalMeasuredTime += windowUpdateTimer.DisplayAndReset("Window Update Time: ", (double)frames);
			totalMeasuredTime += swapBufferTimer.DisplayAndReset("Buffer Swap Time: ", (double)frames);
			totalMeasuredTime += m_renderingEngine->DisplayWindowSyncTime((double)frames);
			
			printf("Other Time:                             %f ms\n", (totalTime - totalMeasuredTime));
			printf("Total Time:                             %f ms\n\n", totalTime);
			frames = 0;
			frameCounter = 0;
		}

		//her update 1 / framecount
		while(unprocessedTime > m_frameTime)
		{
			windowUpdateTimer.StartInvocation();
			m_window->Update();
			
			if(m_window->IsCloseRequested())
			{
				Stop();
			}
			windowUpdateTimer.StopInvocation();
		
			//inputlari burda aliyoz
			//yeni eylemlker vs iste
			m_game->ProcessInput(m_window->GetInput(), (float)m_frameTime);
			m_game->Update((float)m_frameTime);
			
			//tekrar enderi ac
			render = true;

			unprocessedTime -= m_frameTime;
		}

		if(render)
		{
			m_game->Render(m_renderingEngine);
			
			//yeni olusan gorsel bufferda
			//bufferlari degistir 

			swapBufferTimer.StartInvocation();
			m_window->SwapBuffers();
			swapBufferTimer.StopInvocation();
			frames++;
		}
		else
		{
			//goruntuyu ekrana cizmeye gerek yoksa sleep  yap
			//uyusun iste ya bosuna yorma cihazi
			sleepTimer.StartInvocation();
			Util::Sleep(1);
			sleepTimer.StopInvocation();
		}
	}
}

void CoreEngine::Stop()
{
	m_isRunning = false;
}

