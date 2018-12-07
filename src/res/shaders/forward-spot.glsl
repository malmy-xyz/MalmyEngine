#include "common.glh"
#include "forwardlighting.glh"

#if defined(VS_BUILD)
#include "forwardlighting.vsh"
#elif defined(FS_BUILD)

#include "lighting.glh"

uniform vec3 C_eyePos;
uniform float specularIntensity;
uniform float specularPower;

uniform SpotLight R_spotLight;

vec4 CalcLightingEffect(vec3 normal, vec3 worldPos)
{
	vec3 lightDirection = normalize(worldPos - R_spotLight.pointLight.position);
    float spotFactor = dot(lightDirection, R_spotLight.direction);
    
    vec4 color = vec4(0,0,0,0);
    
    if(spotFactor > R_spotLight.cutoff)
    {
        color = CalcPointLight(R_spotLight.pointLight, normal, worldPos, 
                               specularIntensity, specularPower, C_eyePos) *
                (1.0 - (1.0 - spotFactor)/(1.0 - R_spotLight.cutoff));
    }
    
    return color;
}

#include "lightingMain.fsh"
#endif
