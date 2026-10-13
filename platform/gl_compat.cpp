// Inert OpenGL: the 3D mode is not supported by this port.
#define NOMINMAX
#include "platform/gl_compat.h"

void APIENTRY glBegin(GLenum) { }
void APIENTRY glBindTexture(GLenum, GLuint) { }
void APIENTRY glBlendFunc(GLenum, GLenum) { }
void APIENTRY glClear(GLbitfield) { }
void APIENTRY glClearColor(GLclampf, GLclampf, GLclampf, GLclampf) { }
void APIENTRY glClearDepth(GLclampd) { }
void APIENTRY glColor4f(GLfloat, GLfloat, GLfloat, GLfloat) { }
void APIENTRY glCullFace(GLenum) { }
void APIENTRY glDeleteTextures(GLsizei, const GLuint*) { }
void APIENTRY glDisable(GLenum) { }
void APIENTRY glEnable(GLenum) { }
void APIENTRY glEnd() { }
void APIENTRY glFrontFace(GLenum) { }

void APIENTRY glGenTextures(GLsizei n, GLuint* textures)
{
    for (GLsizei i = 0; textures && i < n; i++)
        textures[i] = 0;
}

GLenum APIENTRY glGetError() { return GL_NO_ERROR; }

const GLubyte* APIENTRY glGetString(GLenum)
{
    static const GLubyte empty[1] = { 0 };
    return empty;
}

void APIENTRY glLineWidth(GLfloat) { }
void APIENTRY glLoadIdentity() { }
void APIENTRY glMatrixMode(GLenum) { }
void APIENTRY glOrtho(GLdouble, GLdouble, GLdouble, GLdouble, GLdouble, GLdouble) { }
void APIENTRY glPixelStorei(GLenum, GLint) { }
void APIENTRY glPointSize(GLfloat) { }
void APIENTRY glPolygonStipple(const GLubyte*) { }
void APIENTRY glPushMatrix() { }
void APIENTRY glScissor(GLint, GLint, GLsizei, GLsizei) { }
void APIENTRY glShadeModel(GLenum) { }
void APIENTRY glTexCoord2f(GLfloat, GLfloat) { }
void APIENTRY glTexEnvf(GLenum, GLenum, GLfloat) { }
void APIENTRY glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*) { }
void APIENTRY glTexParameterf(GLenum, GLenum, GLfloat) { }
void APIENTRY glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid*) { }
void APIENTRY glVertex3f(GLfloat, GLfloat, GLfloat) { }
void APIENTRY glViewport(GLint, GLint, GLsizei, GLsizei) { }

HGLRC WINAPI wglCreateContext(HDC) { return NULL; }
BOOL WINAPI wglDeleteContext(HGLRC) { return TRUE; }
BOOL WINAPI wglMakeCurrent(HDC, HGLRC) { return FALSE; }
PROC WINAPI wglGetProcAddress(LPCSTR) { return NULL; }

int WINAPI ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR*) { return 0; }
BOOL WINAPI SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR*) { return FALSE; }
BOOL WINAPI SwapBuffers(HDC) { return FALSE; }
