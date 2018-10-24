#pragma once

#include "component.h"
#include "../core/rendering/mesh.h"
#include "../core/rendering/material.h"


class MeshRenderer : public Component
{
	//functions
	public:
		MeshRenderer(Mesh* m_Mesh, Material* m_Material);
		~MeshRenderer();

		void Start();
		void Update();

	//functions
	private:

	//variables
	public:


	//variables
	private:


};

