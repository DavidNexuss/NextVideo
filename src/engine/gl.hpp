#pragma once
#include "engine.hpp"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  define GL_GLEXT_PROTOTYPES
#  define EGL_EGLEXT_PROTOTYPES
#else
#  include <GL/glew.h>
#endif
#include <GL/gl.h>

namespace NextVideo {
ENGINE_API void   glUtilRenderScreenQuad();
ENGINE_API void   glUtilsSetVertexAttribs(int index);
ENGINE_API void   glUtilRenderQuad(GLuint vbo, GLuint ebo, GLuint worldMat, GLuint viewMat, GLuint projMat);
ENGINE_API GLuint glUtilLoadProgram(const char* vs, const char* fs);

bool checkScene(Scene* scene);
} // namespace NextVideo
