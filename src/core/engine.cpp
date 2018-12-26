#include "Engine.h"
#include "time.h"
#include "../rendering/window.h"
#include "input.h"
#include "util.h"
#include "scene.h"

#include <stdio.h>

Engine::Engine(Window* window, renderEngine* renderEngine, Scene* scene) :
	m_isRunning(false),
	//frame rate default 50 bunlari yarlardan cekcen sonra onu yazinca
	m_frameTime(1.0 / 50),
	m_window(window),
	m_renderEngine(renderEngine),
	m_scene(scene)
{
	//motoru yarlama

	m_scene->SetEngine(this);
	
	//oyunun baslatildigi yer
	m_scene->Init(*m_window);
}

void Engine::Start()
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
			
			totalMeasuredTime += m_scene->DisplayInputTime((double)frames);
			totalMeasuredTime += m_scene->DisplayUpdateTime((double)frames);
			totalMeasuredTime += m_renderEngine->DisplayRenderTime((double)frames);

			totalMeasuredTime += sleepTimer.DisplayAndReset("Sleep Time: ", (double)frames);
			totalMeasuredTime += windowUpdateTimer.DisplayAndReset("Window Update Time: ", (double)frames);
			totalMeasuredTime += swapBufferTimer.DisplayAndReset("Buffer Swap Time: ", (double)frames);

			totalMeasuredTime += m_renderEngine->DisplayWindowSyncTime((double)frames);
			
			//printf("Other Time:                             %f ms\n", (totalTime - totalMeasuredTime));
			//printf("Total Time:                             %f ms\n\n", totalTime);
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
			m_scene->ProcessInput(m_window->GetInput(), (float)m_frameTime);
			m_scene->Update((float)m_frameTime);
			
			//tekrar enderi ac
			render = true;

			unprocessedTime -= m_frameTime;
		}

		if(render)
		{
			m_scene->Render(m_renderEngine);
			
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

void Engine::Stop()
{
	m_isRunning = false;
}

