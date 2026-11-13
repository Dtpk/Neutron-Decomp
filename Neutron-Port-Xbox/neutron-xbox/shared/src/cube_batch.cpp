#include "cube_batch.h"

namespace {

// Same 6-face vertex layout as glinterface::solid_cube(), just emitted
// into a caller-provided world-space buffer instead of going through
// glPushMatrix/glTranslatef + an immediate glBegin/glEnd -- lets many
// cubes share one draw call. Winding/face order kept identical to
// solid_cube() on purpose, so this is a pure batching change, not a
// geometry change.
void append_cube_verts(GLfloat (*out)[3], int& count, GLfloat x, GLfloat y, GLfloat z, GLfloat size) {
    const float s = size, ns = -size;
    const GLfloat local[24][3] = {
        // Front
        {ns, ns,  s}, { s, ns,  s}, { s,  s,  s}, {ns,  s,  s},
        // Back
        {ns, ns, ns}, {ns,  s, ns}, { s,  s, ns}, { s, ns, ns},
        // Top
        {ns,  s, ns}, {ns,  s,  s}, { s,  s,  s}, { s,  s, ns},
        // Bottom
        {ns, ns, ns}, { s, ns, ns}, { s, ns,  s}, {ns, ns,  s},
        // Right
        { s, ns, ns}, { s,  s, ns}, { s,  s,  s}, { s, ns,  s},
        // Left
        {ns, ns, ns}, {ns, ns,  s}, {ns,  s,  s}, {ns,  s, ns},
    };
    for (int i = 0; i < 24; i++) {
        out[count + i][0] = local[i][0] + x;
        out[count + i][1] = local[i][1] + y;
        out[count + i][2] = local[i][2] + z;
    }
    count += 24;
}

} // namespace

cube_batch::cube_batch(glinterface* printer)
    : printer_(printer), count_(0), r_(0), g_(0), b_(0), a_(0) {}

void cube_batch::add(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size) {
    if (count_ > 0 && (r != r_ || g != g_ || b != b_ || a != a_)) {
        flush();
    }
    if (count_ + 24 > kMaxVerts) {
        flush();
    }
    if (count_ == 0) {
        r_ = r; g_ = g; b_ = b; a_ = a;
    }
    append_cube_verts(verts_, count_, x, y, z, size);
}

void cube_batch::flush() {
    if (count_ == 0) return;
    printer_->draw_batch(verts_, count_, r_, g_, b_, a_);
    count_ = 0;
}
