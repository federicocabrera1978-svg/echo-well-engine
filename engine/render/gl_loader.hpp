#pragma once
// Loader GL 3.3 core mínimo (sin GLAD): solo las funciones que usa el motor.
// Se resuelven con glfwGetProcAddress tras crear el contexto.
#include <GL/gl.h>
#include <GL/glext.h>

namespace ew::gl {
#define EW_GL_FUNCS(X) \
  X(PFNGLGENFRAMEBUFFERSPROC, GenFramebuffers) \
  X(PFNGLBINDFRAMEBUFFERPROC, BindFramebuffer) \
  X(PFNGLDELETEFRAMEBUFFERSPROC, DeleteFramebuffers) \
  X(PFNGLFRAMEBUFFERTEXTURE2DPROC, FramebufferTexture2D) \
  X(PFNGLCHECKFRAMEBUFFERSTATUSPROC, CheckFramebufferStatus) \
  X(PFNGLDRAWBUFFERSPROC, DrawBuffers) \
  X(PFNGLBLITFRAMEBUFFERPROC, BlitFramebuffer) \
  X(PFNGLCREATESHADERPROC, CreateShader) \
  X(PFNGLSHADERSOURCEPROC, ShaderSource) \
  X(PFNGLCOMPILESHADERPROC, CompileShader) \
  X(PFNGLGETSHADERIVPROC, GetShaderiv) \
  X(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog) \
  X(PFNGLDELETESHADERPROC, DeleteShader) \
  X(PFNGLCREATEPROGRAMPROC, CreateProgram) \
  X(PFNGLATTACHSHADERPROC, AttachShader) \
  X(PFNGLLINKPROGRAMPROC, LinkProgram) \
  X(PFNGLGETPROGRAMIVPROC, GetProgramiv) \
  X(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog) \
  X(PFNGLUSEPROGRAMPROC, UseProgram) \
  X(PFNGLDELETEPROGRAMPROC, DeleteProgram) \
  X(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation) \
  X(PFNGLUNIFORM1IPROC, Uniform1i) \
  X(PFNGLUNIFORM1FPROC, Uniform1f) \
  X(PFNGLUNIFORM2FPROC, Uniform2f) \
  X(PFNGLUNIFORM4FPROC, Uniform4f) \
  X(PFNGLUNIFORM1FVPROC, Uniform1fv) \
  X(PFNGLUNIFORM1IVPROC, Uniform1iv) \
  X(PFNGLUNIFORM3FVPROC, Uniform3fv) \
  X(PFNGLUNIFORM4FVPROC, Uniform4fv) \
  X(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays) \
  X(PFNGLBINDVERTEXARRAYPROC, BindVertexArray) \
  X(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays) \
  X(PFNGLACTIVETEXTUREPROC, ActiveTexture)

#define EW_GL_DECL(type, name) extern type name;
EW_GL_FUNCS(EW_GL_DECL)
#undef EW_GL_DECL

using ProcLoader = void* (*)(const char*);
// Devuelve el nombre de la primera función faltante, o nullptr si todo cargó.
const char* load(ProcLoader loader);
}  // namespace ew::gl
