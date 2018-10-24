#include "meshRenderer.h"


MeshRenderer::MeshRenderer(Mesh * m_Mesh, Material * m_Material)
{
	//mei vercen burda bide materiali 
	//tabi onlarin da olusturulmasi gerkiyo

	Debug::Log("Component::Update ");


}

MeshRenderer::~MeshRenderer()
{
	//
}


void MeshRenderer::Start()
{
	//enable ise
	if (m_Enable)
	{
		//
		Debug::Log("MeshRenderer::Start ");
	}
}

void MeshRenderer::Update()
{
	Debug::Log("MeshRenderer::Update ");
	//her framede cagrilcak
}
