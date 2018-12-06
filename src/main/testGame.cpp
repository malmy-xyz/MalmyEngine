#include "testGame.h"

void TestGame::Init(const Window& window)
{


	Material bricks("bricks", Texture("bricks.jpg"), 0.0f, 0,
		Texture("bricks_normal.jpg"), Texture("bricks_disp.png"), 0.03f, -0.5f);
	Material bricks2("bricks2", Texture("bricks2.jpg"), 0.0f, 0,
		Texture("bricks2_normal.png"), Texture("bricks2_disp.jpg"), 0.04f, -1.0f);

	//IndexedModel square;
	//{
	//	square.AddVertex(1.0f, -1.0f, 0.0f);  square.AddTexCoord(Vector2f(1.0f, 1.0f));
	//	square.AddVertex(1.0f, 1.0f, 0.0f);   square.AddTexCoord(Vector2f(1.0f, 0.0f));
	//	square.AddVertex(-1.0f, -1.0f, 0.0f); square.AddTexCoord(Vector2f(0.0f, 1.0f));
	//	square.AddVertex(-1.0f, 1.0f, 0.0f);  square.AddTexCoord(Vector2f(0.0f, 0.0f));
	//	square.AddFace(0, 1, 2); square.AddFace(2, 1, 3);
	//}
	//Mesh customMesh("square", square.Finalize());

	////	AddToScene((new Entity(Vector3f(0, -1, 5), Quaternion(), 32.0f))
	////		->AddComponent(new MeshRenderer(Mesh("terrain02.obj"), Material("bricks"))));
	//		
	////	AddToScene((new Entity(Vector3f(7,0,7)))
	////		->AddComponent(new PointLight(Vector3f(0,1,0), 0.4f, Attenuation(0,0,1))));
	////	
	////	AddToScene((new Entity(Vector3f(20,-11.0f,5), Quaternion(Vector3f(1,0,0), ToRadians(-60.0f)) * Quaternion(Vector3f(0,1,0), ToRadians(90.0f))))
	////		->AddComponent(new SpotLight(Vector3f(0,1,1), 0.4f, Attenuation(0,0,0.02f), ToRadians(91.1f), 7, 1.0f, 0.5f)));
	//	
	//	AddToScene((new Entity(Vector3f(), Quaternion(Vector3f(1,0,0), ToRadians(-45))))
	//		->AddComponent(new DirectionalLight(Vector3f(1,1,1), 0.4f, 10, 80.0f, 1.0f)));
	//	
	//	AddToScene((new Entity(Vector3f(0, 2, 0), Quaternion(Vector3f(0,1,0), 0.4f), 1.0f))
	//		->AddComponent(new MeshRenderer(Mesh("plane3.obj"), Material("bricks2")))
	//		->AddChild((new Entity(Vector3f(0, 0, 25)))
	//			->AddComponent(new MeshRenderer(Mesh("plane3.obj"), Material("bricks2")))
	//			->AddChild((new Entity())
	//				->AddComponent(new CameraComponent(Matrix4f().InitPerspective(ToRadians(70.0f), window.GetAspect(), 0.1f, 1000.0f)))
	//				->AddComponent(new FreeLook(window.GetCenter()))
	//				->AddComponent(new FreeMove(10.0f)))));
	//	
	////	AddToScene((new Entity(Vector3f(24,-12,5), Quaternion(Vector3f(0,1,0), ToRadians(30.0f))))
	////		->AddComponent(new MeshRenderer(Mesh("sphere.obj"), Material("bricks"))));
	//		
	////	AddToScene((new Entity(Vector3f(0,0,7), Quaternion(), 1.0f))
	////		->AddComponent(new MeshRenderer(Mesh("square"), Material("bricks2"))));
	//
	//
	//	//TODO: Temporary Physics Engine Code!
	//	PhysicsEngine physicsEngine;
	//	
	//	physicsEngine.AddObject(PhysicsObject(
	//			new BoundingSphere(Vector3f(0.0f, 0.0f, 0.0f), 1.0f),
	//		   	Vector3f(0.0f, 0.0f, 1.141f/2.0f)));
	//
	//	physicsEngine.AddObject(PhysicsObject(
	//			new BoundingSphere(Vector3f(1.414f/2.0f * 7.0f, 0.0f, 1.414f/2.0f * 7.0f), 1.0f),
	//			Vector3f(-1.414f/2.0f, 0.0f, -1.414f/2.0f))); 
	//
	//
	//	PhysicsEngineComponent* physicsEngineComponent 
	//		= new PhysicsEngineComponent(physicsEngine);
	//
	//	for(unsigned int i = 0; 
	//		i < physicsEngineComponent->GetPhysicsEngine().GetNumObjects(); 
	//		i++)
	//	{
	//		
	//		AddToScene((new Entity(Vector3f(0,0,0), Quaternion(), 
	//					1.0f))
	//			->AddComponent(new PhysicsObjectComponent(
	//					&physicsEngineComponent->GetPhysicsEngine().GetObject(i)))
	//			->AddComponent(new MeshRenderer(Mesh("sphere.obj"), Material("bricks"))));
	//	}
	//
	//	AddToScene((new Entity())
	//		->AddComponent(physicsEngineComponent));

	//	AddToScene((new Entity(Vector3f(), Quaternion(Vector3f(1,0,0), ToRadians(-45))))
	//		->AddComponent(new DirectionalLight(Vector3f(1,1,1), 0.4f, 10, 80.0f, 1.0f)));

	AddToScene((new Entity())
		->AddComponent(new CameraComponent(Matrix4f().InitPerspective(
			ToRadians(70.0f), window.GetAspect(), 0.1f, 1000.0f)))
		->AddComponent(new FreeLook(window.GetCenter()))
		->AddComponent(new FreeMove(10.0f)));

	static const int CUBE_SIZE = 3;

	AddToScene((new Entity())
		->AddComponent(new PointLight(Vector3f(1, 1, 1),
		(CUBE_SIZE * CUBE_SIZE) * 2, Attenuation(0, 0, 1))));

	for (int i = -CUBE_SIZE; i <= CUBE_SIZE; i++)
	{
		for (int j = -CUBE_SIZE; j <= CUBE_SIZE; j++)
		{
			for (int k = -CUBE_SIZE; k <= CUBE_SIZE; k++)
			{
				if (i == -CUBE_SIZE || i == CUBE_SIZE ||
					j == -CUBE_SIZE || j == CUBE_SIZE ||
					k == -CUBE_SIZE || k == CUBE_SIZE)
				{
					if (i == 0 || j == 0 || k == 0)
					{
						AddToScene((new Entity(Vector3f(i * 2, j * 2, k * 2)))
							->AddComponent(new MeshRenderer(Mesh("sphere.obj"),
								Material("bricks"))));
					}
					else
					{
						AddToScene((new Entity(Vector3f(i * 2, j * 2, k * 2)))
							->AddComponent(new MeshRenderer(Mesh("cube.obj"),
								Material("bricks2"))));
					}

				}
			}
		}
	}


}


