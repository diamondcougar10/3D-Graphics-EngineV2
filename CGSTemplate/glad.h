// Minimal OpenGL 4.3 Compute Shader Loader for Windows
// Auto-generated subset of GLAD
#pragma once

#include <Windows.h>
#include <GL/gl.h>

// OpenGL types
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

// OpenGL constants
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_DRAW                    0x88E0
#define GL_STATIC_DRAW                    0x88E4
#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_BUFFER_UPDATE_BARRIER_BIT      0x00000200
#define GL_SHADER_STORAGE_BARRIER_BIT     0x00002000
#define GL_ALL_BARRIER_BITS               0xFFFFFFFF

// Function pointer types
typedef GLuint(APIENTRY* PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void (APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRY* PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLuint(APIENTRY* PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY* PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void (APIENTRY* PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY* PFNGLDELETESHADERPROC)(GLuint shader);
typedef void (APIENTRY* PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY* PFNGLDISPATCHCOMPUTEPROC)(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void (APIENTRY* PFNGLMEMORYBARRIERPROC)(GLbitfield barriers);
typedef void (APIENTRY* PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (APIENTRY* PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void (APIENTRY* PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY* PFNGLBINDBUFFERBASEPROC)(GLenum target, GLuint index, GLuint buffer);
typedef void (APIENTRY* PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (APIENTRY* PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void (APIENTRY* PFNGLGETBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, void* data);
typedef void* (APIENTRY* PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean(APIENTRY* PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void (APIENTRY* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRY* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY* PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY* PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY* PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef GLint(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);

// Function pointers
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
extern PFNGLMEMORYBARRIERPROC glMemoryBarrier;
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBINDBUFFERBASEPROC glBindBufferBase;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
extern PFNGLMAPBUFFERPROC glMapBuffer;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLUNIFORM3FPROC glUniform3f;
extern PFNGLUNIFORM4FPROC glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;

// Initialize OpenGL functions - call after creating GL context
inline bool gladLoadGL() {
    HMODULE opengl32 = LoadLibraryA("opengl32.dll");
    if (!opengl32) return false;

    auto wglGetProcAddress_ptr = (PROC(WINAPI*)(LPCSTR))GetProcAddress(opengl32, "wglGetProcAddress");
    if (!wglGetProcAddress_ptr) return false;

    #define LOAD_GL_FUNC(name) name = (decltype(name))wglGetProcAddress_ptr(#name); if(!name) return false;
    
    LOAD_GL_FUNC(glCreateShader);
    LOAD_GL_FUNC(glShaderSource);
    LOAD_GL_FUNC(glCompileShader);
    LOAD_GL_FUNC(glGetShaderiv);
    LOAD_GL_FUNC(glGetShaderInfoLog);
    LOAD_GL_FUNC(glCreateProgram);
    LOAD_GL_FUNC(glAttachShader);
    LOAD_GL_FUNC(glLinkProgram);
    LOAD_GL_FUNC(glGetProgramiv);
    LOAD_GL_FUNC(glGetProgramInfoLog);
    LOAD_GL_FUNC(glUseProgram);
    LOAD_GL_FUNC(glDeleteShader);
    LOAD_GL_FUNC(glDeleteProgram);
    LOAD_GL_FUNC(glDispatchCompute);
    LOAD_GL_FUNC(glMemoryBarrier);
    LOAD_GL_FUNC(glGenBuffers);
    LOAD_GL_FUNC(glDeleteBuffers);
    LOAD_GL_FUNC(glBindBuffer);
    LOAD_GL_FUNC(glBindBufferBase);
    LOAD_GL_FUNC(glBufferData);
    LOAD_GL_FUNC(glBufferSubData);
    LOAD_GL_FUNC(glGetBufferSubData);
    LOAD_GL_FUNC(glMapBuffer);
    LOAD_GL_FUNC(glUnmapBuffer);
    LOAD_GL_FUNC(glUniform1i);
    LOAD_GL_FUNC(glUniform1f);
    LOAD_GL_FUNC(glUniform2f);
    LOAD_GL_FUNC(glUniform3f);
    LOAD_GL_FUNC(glUniform4f);
    LOAD_GL_FUNC(glUniformMatrix4fv);
    LOAD_GL_FUNC(glGetUniformLocation);
    
    #undef LOAD_GL_FUNC
    return true;
}

// Function pointer definitions (in one translation unit)
#ifdef GLAD_IMPLEMENTATION
PFNGLCREATESHADERPROC glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
PFNGLDELETESHADERPROC glDeleteShader = nullptr;
PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = nullptr;
PFNGLMEMORYBARRIERPROC glMemoryBarrier = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
PFNGLBINDBUFFERBASEPROC glBindBufferBase = nullptr;
PFNGLBUFFERDATAPROC glBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC glBufferSubData = nullptr;
PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData = nullptr;
PFNGLMAPBUFFERPROC glMapBuffer = nullptr;
PFNGLUNMAPBUFFERPROC glUnmapBuffer = nullptr;
PFNGLUNIFORM1IPROC glUniform1i = nullptr;
PFNGLUNIFORM1FPROC glUniform1f = nullptr;
PFNGLUNIFORM2FPROC glUniform2f = nullptr;
PFNGLUNIFORM3FPROC glUniform3f = nullptr;
PFNGLUNIFORM4FPROC glUniform4f = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
#endif
