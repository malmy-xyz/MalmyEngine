#include "testGame.h"

void TestGame::Init(const Window& window)
{

	/*Ssahnedeki Global seyler 
	bi tane camera
	bi tane sun
	*/

	//Global Camera
	AddToScene((
		new Entity(
			Vector3f(0.0f, 5.0f, -10.0f),
			Quaternion(Vector3f(1, 0, 0), ToRadians(25)),
			1.0f
		))	
		->AddComponent(
			new CameraComponent(
				Matrix4f().InitPerspective(ToRadians(70.0f), 
					window.GetAspect(), 
					0.1f, 
					100.0f)
			)
		)		
		->AddComponent(
			new EditorCameraMove(
				window.GetCenter()
			)
		)

	);
	

	//Global Light
	AddToScene((new Entity(Vector3f(0.0f, 0.0f, 0.0f), Quaternion(Vector3f(0.0f, 0.0f, 0.0f), ToRadians(0))))
		->AddComponent(new DirectionalLight(Vector3f(1, 1, 1), 0.4f, 10, 80.0f, 1.0f)));



	/*sahneye harici eklenecek seyler*/

	//materyalleri once olustur
	Material bricks("plane", Texture("test.png"), 0.0f, 0, Texture("test.png"), Texture("test.png"), 0.03f, -0.5f);

	//plane
	AddToScene((new Entity(Vector3f(), Quaternion(0.0f, 0.0f, 0.0f, 1.0f)))
		->AddComponent(new MeshRenderer(Mesh("plane3.obj"), Material("plane"))));

	

	//Red Point Light
	/*AddToScene((new Entity(Vector3f(0, 1, -5)))
		->AddComponent(new PointLight(Vector3f(1.0f, 0, 0), 10.0f, Attenuation(1, 0, 1))));*/


	// 3 tane spot isik olustur
	AddToScene((new Entity(Vector3f(0, 2, 0), Quaternion(Vector3f(1, 0, 0), ToRadians(90.0f)) ))
		->AddComponent(new SpotLight(Vector3f(0, 0, 1), 0.4f, Attenuation(0, 0, 0.02f), ToRadians(91.1f), 7, 1.0f, 0.5f)));


	// 3 tane obje olustur bunla modellencek
	//ve dondurme kodu eklencek harici scriptlerde



}


