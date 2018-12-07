#include "common.glh"
#include "forwardlighting.glh"

#if defined(VS_BUILD)
#include "forwardlighting.vsh"
#elif defined(FS_BUILD)

#include "lighting.glh"

uniform vec3 C_eyePos;
uniform float specularIntensity;
uniform float specularPower;

uniform PointLight R_pointLight;

vec4 CalcLightingEffect(vec3 normal, vec3 worldPos)
{
	return CalcPointLight(R_pointLight, normal, worldPos,
	                      specularIntensity, specularPower, C_eyePos);
}

#include "lightingMain.fsh"
#endif
