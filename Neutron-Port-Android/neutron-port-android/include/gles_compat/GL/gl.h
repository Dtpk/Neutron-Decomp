/* Drop-in replacement for <GL/gl.h> when targeting OpenGL ES 1.1 (e.g. Android).
 *
 * ES 1.1 keeps almost the entire desktop fixed-function pipeline under the
 * same function names (matrix stack, lighting, texture env, client-side
 * vertex arrays) -- GLES/gl.h already provides those. The one thing ES
 * dropped is immediate mode: glBegin/glVertex.../glEnd and GL_QUADS don't
 * exist. This header adds a small emulation of just those three calls (the
 * only ones this project actually uses -- glVertex2f/glVertex3f plus
 * whatever glColor4ub was last set, only GL_QUADS and GL_LINES as
 * primitives) so every existing call site keeps working unchanged: it
 * buffers vertices client-side and flushes them with glVertexPointer /
 * glColorPointer / glDrawArrays at glEnd(), splitting GL_QUADS into two
 * GL_TRIANGLES since ES has no quad primitive.
 *
 * Only include this by putting this directory ahead of the normal GL
 * include path for the Android build; the desktop build is untouched and
 * keeps using the real <GL/gl.h>.
 */
#pragma once
#include <GLES/gl.h>

/* ES has no quad primitive at all, so GLES/gl.h doesn't define this token --
 * glBegin()/glEnd() in gles_compat.cpp recognize this exact sentinel value
 * and split the batch into two GL_TRIANGLES. Picked a value well past any
 * real GLenum used by this project to avoid colliding with one. */
#define GL_QUADS 0x7FFF0007

#ifdef __cplusplus
extern "C" {
#endif

/* Real implementations in gles_compat.cpp. */
void glBegin(GLenum mode);
void glVertex2f(GLfloat x, GLfloat y);
void glVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glEnd(void);

/* Desktop-only double-precision orthographic projection -- ES 1.1 only has
 * the float version, glOrthof. */
void glOrtho(double left, double right, double bottom, double top, double zNear, double zFar);

#ifdef __cplusplus
}
#endif
