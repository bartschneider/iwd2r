// OpenGL 1.1, GLU and WGL declarations standing in for <gl/gl.h> and
// <gl/glu.h>. The engine loads its GL entry points through GetProcAddress,
// which returns NULL here, so its 3D mode never initializes; the functions in
// gl_compat.cpp are inert.
#pragma once

#include "compat/win_types.h"

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;

struct GLUnurbs;
struct GLUquadric;
struct GLUtesselator;

typedef INT_PTR(WINAPI* PROC)();

#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500
#define GL_INVALID_VALUE 0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW 0x0503
#define GL_STACK_UNDERFLOW 0x0504
#define GL_OUT_OF_MEMORY 0x0505

#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_QUADS 0x0007
#define GL_POLYGON 0x0009

#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307

#define GL_BACK 0x0405
#define GL_CCW 0x0901
#define GL_POLYGON_STIPPLE 0x0B42
#define GL_CULL_FACE 0x0B44
#define GL_LIGHTING 0x0B50
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_2D 0x0DE1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_PROJECTION 0x1701
#define GL_RGBA 0x1908
#define GL_SMOOTH 0x1D01
#define GL_REPLACE 0x1E01
#define GL_MODULATE 0x2100
#define GL_DECAL 0x2101
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_TEXTURE_ENV 0x2300
#define GL_NEAREST 0x2600
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_LIGHT0 0x4000

void APIENTRY glBegin(GLenum mode);
void APIENTRY glBindTexture(GLenum target, GLuint texture);
void APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor);
void APIENTRY glClear(GLbitfield mask);
void APIENTRY glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void APIENTRY glClearDepth(GLclampd depth);
void APIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void APIENTRY glCullFace(GLenum mode);
void APIENTRY glDeleteTextures(GLsizei n, const GLuint* textures);
void APIENTRY glDisable(GLenum cap);
void APIENTRY glEnable(GLenum cap);
void APIENTRY glEnd();
void APIENTRY glFrontFace(GLenum mode);
void APIENTRY glGenTextures(GLsizei n, GLuint* textures);
GLenum APIENTRY glGetError();
const GLubyte* APIENTRY glGetString(GLenum name);
void APIENTRY glLineWidth(GLfloat width);
void APIENTRY glLoadIdentity();
void APIENTRY glMatrixMode(GLenum mode);
void APIENTRY glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void APIENTRY glPixelStorei(GLenum pname, GLint param);
void APIENTRY glPointSize(GLfloat size);
void APIENTRY glPolygonStipple(const GLubyte* mask);
void APIENTRY glPushMatrix();
void APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
void APIENTRY glShadeModel(GLenum mode);
void APIENTRY glTexCoord2f(GLfloat s, GLfloat t);
void APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param);
void APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels);
void APIENTRY glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

HGLRC WINAPI wglCreateContext(HDC hdc);
BOOL WINAPI wglDeleteContext(HGLRC context);
BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC context);
PROC WINAPI wglGetProcAddress(LPCSTR name);

// GDI pixel formats, as in <wingdi.h>. No pixel format exists:
// ChoosePixelFormat returns 0, SetPixelFormat and SwapBuffers FALSE.
#define PFD_TYPE_RGBA 0
#define PFD_DOUBLEBUFFER 0x00000001
#define PFD_DRAW_TO_WINDOW 0x00000004
#define PFD_SUPPORT_OPENGL 0x00000020
#define PFD_MAIN_PLANE 0

struct PIXELFORMATDESCRIPTOR {
    WORD nSize;
    WORD nVersion;
    DWORD dwFlags;
    BYTE iPixelType;
    BYTE cColorBits;
    BYTE cRedBits;
    BYTE cRedShift;
    BYTE cGreenBits;
    BYTE cGreenShift;
    BYTE cBlueBits;
    BYTE cBlueShift;
    BYTE cAlphaBits;
    BYTE cAlphaShift;
    BYTE cAccumBits;
    BYTE cAccumRedBits;
    BYTE cAccumGreenBits;
    BYTE cAccumBlueBits;
    BYTE cAccumAlphaBits;
    BYTE cDepthBits;
    BYTE cStencilBits;
    BYTE cAuxBuffers;
    BYTE iLayerType;
    BYTE bReserved;
    DWORD dwLayerMask;
    DWORD dwVisibleMask;
    DWORD dwDamageMask;
};
typedef PIXELFORMATDESCRIPTOR* LPPIXELFORMATDESCRIPTOR;

struct LAYERPLANEDESCRIPTOR {
    WORD nSize;
    WORD nVersion;
    DWORD dwFlags;
    BYTE iPixelType;
    BYTE cColorBits;
    BYTE cRedBits;
    BYTE cRedShift;
    BYTE cGreenBits;
    BYTE cGreenShift;
    BYTE cBlueBits;
    BYTE cBlueShift;
    BYTE cAlphaBits;
    BYTE cAlphaShift;
    BYTE cAccumBits;
    BYTE cAccumRedBits;
    BYTE cAccumGreenBits;
    BYTE cAccumBlueBits;
    BYTE cAccumAlphaBits;
    BYTE cDepthBits;
    BYTE cStencilBits;
    BYTE cAuxBuffers;
    BYTE iLayerPlane;
    BYTE bReserved;
    COLORREF crTransparent;
};
typedef LAYERPLANEDESCRIPTOR* LPLAYERPLANEDESCRIPTOR;

struct POINTFLOAT {
    FLOAT x;
    FLOAT y;
};

struct GLYPHMETRICSFLOAT {
    FLOAT gmfBlackBoxX;
    FLOAT gmfBlackBoxY;
    POINTFLOAT gmfptGlyphOrigin;
    FLOAT gmfCellIncX;
    FLOAT gmfCellIncY;
};
typedef GLYPHMETRICSFLOAT* LPGLYPHMETRICSFLOAT;

int WINAPI ChoosePixelFormat(HDC hdc, const PIXELFORMATDESCRIPTOR* pfd);
BOOL WINAPI SetPixelFormat(HDC hdc, int format, const PIXELFORMATDESCRIPTOR* pfd);
BOOL WINAPI SwapBuffers(HDC hdc);
