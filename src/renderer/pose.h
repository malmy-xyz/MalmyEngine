#pragma once


#include "engine/malmy.h"


namespace Malmy
{


struct IAllocator;
struct Matrix;
class Model;
struct Quat;
struct Vec3;


struct MALMY_RENDERER_API Pose
{
	explicit Pose(IAllocator& allocator);
	~Pose();

	void resize(int count);
	void computeAbsolute(Model& model);
	void computeRelative(Model& model);
	void blend(Pose& rhs, float weight);

	IAllocator& allocator;
	bool is_absolute;
	i32 count;
	Vec3* positions;
	Quat* rotations;
	
	private:
		Pose(const Pose&);
		void operator =(const Pose&);
};


} // namespace Malmy