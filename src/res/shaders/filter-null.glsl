#include "common.glh"

varying vec2 texCoord0;

#if defined(VS_BUILD)
#include "filter.vsh"
#elif defined(FS_BUILD)
uniform sampler2D R_filterTexture;

DeclareFragOutput(0, vec4);
void main()
{
	SetFragOutput(0, texture2D(R_filterTexture, texCoord0));
}
#endif
