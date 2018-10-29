#include "testScene.h"


TestScene::TestScene()
{
	//test scene const
	//burda bi tane test sahnesi olsurcam
	//ve buna bazi game objeler ekleyip
	//bunlarada bazi componentler eklicem

	//sonra bunlari render negine vericem ekrana cizcek


	Scene* m_Scene = new Scene();

	//first gameobject
	GameObject* m_GameObject_00 = new GameObject();
	
	Transform* m_Transform_00 = new Transform();

	MeshRenderer* m_MeshRenderer_00 = new MeshRenderer(new Mesh("cube.obj"), new Material("Material.mmat"));

	m_GameObject_00->AddComponent(m_Transform_00);
	m_GameObject_00->AddComponent(m_MeshRenderer_00);

	//second game object
	GameObject* m_GameObject_01 = new GameObject();

	Transform* m_Transform_01 = new Transform();

	MeshRenderer* m_MeshRenderer_01 = new MeshRenderer(new Mesh("cube.obj"), new Material("Material.mmat"));

	m_GameObject_00->AddComponent(m_Transform_01);
	m_GameObject_00->AddComponent(m_MeshRenderer_01);

	//parent child deneme kodu
	m_Transform_00->AddChild(m_Transform_01);

	m_Scene->AddGameObject(m_GameObject_00);
	m_Scene->AddGameObject(m_GameObject_01);

	m_Scene->Update();

	Window* m_Window = new Window();
		
	RenderEngine* m_RenderEngine = new RenderEngine();
	Engine* m_Engine = new Engine();

	//engine (window renderengine physicengine scene)

	m_Engine->Start();

	//parent child dene burda





}


TestScene::~TestScene()
{
	//desct
}
