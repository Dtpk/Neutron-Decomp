/* Implements the small immediate-mode + GLU emulation declared in
 * include/gles_compat/GL/{gl,glu}.h. See that header for why this exists.
 *
 * Immediate-mode emulation: every call site in this project sets color via
 * the real glColor4ub *before* glBegin and never calls glColor/glTexCoord
 * between vertices, so this only needs to buffer positions and draw with
 * GL_COLOR_ARRAY left disabled -- GL then uses whatever flat "current
 * color" was already set by the real (un-shimmed) glColor4ub call, which
 * is exactly the original behavior.
 */
#include "GL/gl.h"
#include "GL/glu.h"
#include <math.h>

namespace {

constexpr int kMaxVerts = 64;

GLenum g_mode = 0;
GLfloat g_verts[kMaxVerts][3];
int g_count = 0;
bool g_active = false;

} // namespace

extern "C" void glBegin(GLenum mode) {
    g_mode = mode;
    g_count = 0;
    g_active = true;
}

extern "C" void glVertex2f(GLfloat x, GLfloat y) {
    if (!g_active || g_count >= kMaxVerts) return;
    g_verts[g_count][0] = x;
    g_verts[g_count][1] = y;
    g_verts[g_count][2] = 0.0f;
    g_count++;
}

extern "C" void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!g_active || g_count >= kMaxVerts) return;
    g_verts[g_count][0] = x;
    g_verts[g_count][1] = y;
    g_verts[g_count][2] = z;
    g_count++;
}

extern "C" void glEnd(void) {
    g_active = false;
    if (g_count == 0) return;

    glEnableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);

    if (g_mode == GL_QUADS && g_count >= 4 && g_count % 4 == 0) {
        /* ES has no quad primitive: split EVERY run of 4 vertices into two
         * triangles sharing the 0-2 diagonal, same winding as the original
         * quad (0,1,2,3). A single glBegin(GL_QUADS)/glEnd() pair can (and,
         * e.g. glinterface::cube()'s 6-face cube, does) contain more than
         * one quad -- this used to only handle the exactly-4-vertex case,
         * silently drawing nothing at all for anything bigger (glDrawArrays
         * was reached with the GL_QUADS sentinel itself as the primitive
         * type, not a real ES enum), which is why cubes -- and everything
         * built from them: enemies, trail pieces -- never appeared while
         * the plain-GL_LINES draws elsewhere (a different, non-glBegin
         * code path) worked fine. */
        GLfloat tri[(kMaxVerts / 4) * 6][3];
        int triCount = 0;
        for (int q = 0; q + 4 <= g_count; q += 4) {
            static const int idx[6] = {0, 1, 2, 0, 2, 3};
            for (int i = 0; i < 6; i++) {
                tri[triCount][0] = g_verts[q + idx[i]][0];
                tri[triCount][1] = g_verts[q + idx[i]][1];
                tri[triCount][2] = g_verts[q + idx[i]][2];
                triCount++;
            }
        }
        glVertexPointer(3, GL_FLOAT, 0, tri);
        glDrawArrays(GL_TRIANGLES, 0, triCount);
    } else {
        /* GL_LINES (and anything else this project might add later that's
         * already a legal ES primitive) draws straight through. */
        glVertexPointer(3, GL_FLOAT, 0, g_verts);
        glDrawArrays(g_mode, 0, g_count);
    }
}

extern "C" void glOrtho(double left, double right, double bottom, double top, double zNear, double zFar) {
    glOrthof((GLfloat)left, (GLfloat)right, (GLfloat)bottom, (GLfloat)top, (GLfloat)zNear, (GLfloat)zFar);
}

/* --- GLU replacements: standard projection/view matrix construction,
 * built on ES 1.1's own glFrustumf/glMultMatrixf/glTranslatef. --- */

extern "C" void gluPerspective(double fovy, double aspect, double zNear, double zFar) {
    double top = zNear * tan(fovy * M_PI / 360.0);
    double bottom = -top;
    double right = top * aspect;
    double left = -right;
    glFrustumf((GLfloat)left, (GLfloat)right, (GLfloat)bottom, (GLfloat)top,
               (GLfloat)zNear, (GLfloat)zFar);
}

extern "C" void gluLookAt(double eyex, double eyey, double eyez,
                           double centerx, double centery, double centerz,
                           double upx, double upy, double upz) {
    double fx = centerx - eyex, fy = centery - eyey, fz = centerz - eyez;
    double flen = sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 0.0) { fx /= flen; fy /= flen; fz /= flen; }

    /* side = forward x up */
    double sx = fy * upz - fz * upy;
    double sy = fz * upx - fx * upz;
    double sz = fx * upy - fy * upx;
    double slen = sqrt(sx * sx + sy * sy + sz * sz);
    if (slen > 0.0) { sx /= slen; sy /= slen; sz /= slen; }

    /* recomputed up = side x forward, orthonormal */
    double ux = sy * fz - sz * fy;
    double uy = sz * fx - sx * fz;
    double uz = sx * fy - sy * fx;

    GLfloat m[16] = {
        (GLfloat)sx, (GLfloat)ux, (GLfloat)-fx, 0.0f,
        (GLfloat)sy, (GLfloat)uy, (GLfloat)-fy, 0.0f,
        (GLfloat)sz, (GLfloat)uz, (GLfloat)-fz, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    glMultMatrixf(m);
    glTranslatef((GLfloat)-eyex, (GLfloat)-eyey, (GLfloat)-eyez);
}
