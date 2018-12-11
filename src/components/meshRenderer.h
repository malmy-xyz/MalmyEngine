#ifndef MESHRENDERER_H_INCLUDED
#define MESHRENDERER_H_INCLUDED

#include "../core/gameObject.h"
#include "../rendering/mesh.h"

class MeshRenderer : public GameObject
{
public:
	MeshRenderer(const Mesh& mesh, const Material& material) :
		m_mesh(mesh),
		m_material(material) {}

	virtual void Render(const Shader& shader, const RenderingEngine& renderingEngine, const Camera& camera) const
	{
		shader.Bind();
		shader.UpdateUniforms(GetTransform(), m_material, renderingEngine, camera);
		m_mesh.Draw();
	}
protected:
private:
	Mesh m_mesh;
	Material m_material;
};

#endif // MESHRENDERER_H_INCLUDED

/*

render funksiyonlari Update ile degiscek
*/