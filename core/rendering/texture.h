#pragma once

#include <iostream>
#include <string>

//burda haatvar duzelt

free image dahil et o calsiiyo

#define STB_IMAGE_IMPLEMENTATION
#include "../../thirdparty/stb/stb_image.h"

#include "../../thirdparty/stb/stb_image.h"
#include "../../thirdparty/glew-2.1.0/include/GL/glew.h"

class Texture
{
public:
	Texture(const std::string& fileName);

	void Bind();

	virtual ~Texture();
protected:
private:
	Texture(const Texture& texture) {}
	void operator=(const Texture& texture) {}

	GLuint m_texture;
};


