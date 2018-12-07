#ifndef AABB_INCLUDED_H
#define AABB_INCLUDED_H

#include "../maths/math3d.h"
#include "intersectData.h"

/**
burasi fizik motirunda carpisma icin
 */
class AABB
{
public:
	
	//kullanilabir bi AABB olusutr
	//min ve mak koordinatlari ayarla
	//bunu asarsa fizik  ootru coker

	AABB(const Vector3f& minExtents, const Vector3f& maxExtents) :
		m_minExtents(minExtents),
		m_maxExtents(maxExtents) {}
	
	//iki AABB kesisirse haber ver
	//carpisma burasi iste
	//daha fazl hareket etmesini eneglle bu duurmda
	IntersectData IntersectAABB(const AABB& other) const;

	//min uzunluk
	inline const Vector3f& GetMinExtents() const { return m_minExtents; }
	//maks uzunluk
	inline const Vector3f& GetMaxExtents() const { return m_maxExtents; }

	//birim testi icin
	static void Test();
private:
	//en kucuk kkordinatlariini kosesi
	const Vector3f m_minExtents;
	//en buyuk koorinatlarinin kosesi
	const Vector3f m_maxExtents;
};

#endif
