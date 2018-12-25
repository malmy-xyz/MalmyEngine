#include "testScene.h"

#include "../res/scripts/demorot.h"

void TestScene::Init(const Window& window)
{

	/*Ssahnedeki Global seyler 
	bi tane camera
	bi tane sun
	*/

	//Global Camera
	AddToScene((
		new GameObject(
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
			new EditorCameraMove()
		)

	);
	

	/*sahneye harici eklenecek seyler*/

	//materyalleri once olustur
	Material bricks("plane", Texture("plane_diff.png"), 0.0f, 0, Texture("plane_spec.png"), Texture("plane_norm.png"), 0.03f, -0.5f);

	Material logo_pau("logo_pau", Texture("logo_pau_diff.png"), 0.0f, 0, Texture("logo_pau_spec.png"), Texture("logo_pau_norm.png"), 0.03f, -0.5f);
	Material logo_malmy("logo_malmy", Texture("logo_malmy_diff.png"), 0.0f, 0, Texture("logo_malmy_spec.png"), Texture("logo_malmy_norm.png"), 0.03f, -0.5f);


	//plane
	AddToScene((new GameObject(Vector3f(), Quaternion(0.0f, 0.0f, 0.0f, 1.0f)))
		->AddComponent(new MeshRenderer(Mesh("plane3.obj"), Material("plane"))));

	//3 tane obje olustur bunla modellencek
	//ve dondurme kodu eklencek harici scriptlerde
	AddToScene((new GameObject(Vector3f(-5, 2, 0), Quaternion(Vector3f(1, 0, 0), ToRadians(0.0f))))
		->AddComponent(new MeshRenderer(Mesh("logo_pau.obj"), Material("logo_pau")))
		->AddComponent(new DemoRot(-25))
	);

	AddToScene((new GameObject(Vector3f(0, 2, 0), Quaternion(Vector3f(1, 0, 0), ToRadians(0.0f))))
		->AddComponent(new MeshRenderer(Mesh("logo_malmy.obj"), Material("logo_malmy"))));

	AddToScene((new GameObject(Vector3f(5, 2, 0), Quaternion(Vector3f(1, 0, 0), ToRadians(0.0f))))
		->AddComponent(new MeshRenderer(Mesh("logo_pau.obj"), Material("logo_pau")))
		->AddComponent(new DemoRot(25))
	);


	//Global Light
	AddToScene((new GameObject(Vector3f(0.0f, 0.0f, 0.0f), Quaternion(Vector3f(1.0f, 0.0f, 0.0f), ToRadians(-135))))
		->AddComponent(new DirectionalLight(Vector3f(1, 1, 1), 0.75f, 10, 80.0f, 1.0f)));


	// 3 tane spot isik olustur
	AddToScene((new GameObject(Vector3f(-5, 5, -5), Quaternion(Vector3f(1, 0, 0), ToRadians(45)) ))
		->AddComponent(new SpotLight(Vector3f(0, 1, 1), 0.5f, Attenuation(0, 0, 0.02f), ToRadians(90), 7, 1.0f, 0.5f)));

	AddToScene((new GameObject(Vector3f(0, 5, -5), Quaternion(Vector3f(1, 0, 0), ToRadians(45)) ))
		->AddComponent(new SpotLight(Vector3f(0, 0, 0), 0.75f, Attenuation(0, 0, 0.02f), ToRadians(90), 7, 1.0f, 0.5f)));

	AddToScene((new GameObject(Vector3f(5, 5, -5), Quaternion(Vector3f(1, 0, 0), ToRadians(45)) ))
		->AddComponent(new SpotLight(Vector3f(0, 1, 1), 0.5f, Attenuation(0, 0, 0.02f), ToRadians(90), 7, 1.0f, 0.5f)));


	/*
	GameObject list yapilabilir
	*/
	

}


