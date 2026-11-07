/* Drop-in replacement for <GL/glu.h> on OpenGL ES 1.1 (no GLU at all there).
 * Only the two GLU calls this project actually uses: gluPerspective and
 * gluLookAt, same signatures as desktop GLU. Implemented in gles_compat.cpp
 * with the standard projection/view matrix formulas on top of ES 1.1's
 * glFrustumf/glMultMatrixf (both core ES 1.1 functions).
 */
#pragma once
#include <GLES/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void gluPerspective(double fovy, double aspect, double zNear, double zFar);
void gluLookAt(double eyex, double eyey, double eyez,
               double centerx, double centery, double centerz,
               double upx, double upy, double upz);

#ifdef __cplusplus
}
#endif
