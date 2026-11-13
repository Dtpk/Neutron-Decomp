#pragma once

#include "glinterface.h"

// Xbox-only optimization: accumulates multiple solid_cube()-shaped quads
// (trail pieces, see piece.cpp's print()) into as few glBegin/glEnd draw
// calls as possible instead of one full glPushMatrix/glTranslatef/
// glColor4ub/glBegin..glEnd/glPopMatrix bundle PER CUBE. Real cost this
// addresses: plane::print() only draws active trail pieces, so the
// number of these per-cube bundles grows directly with how much trail is
// laid down across the level (and multiplies by player count in split-
// screen, since each viewport redraws the whole active-piece set) --
// NV2A's real per-draw-call overhead made this measurably expensive
// already for the much smaller HUD-text case (see stage_interface.cpp's
// draw_hud_text(), the same fix category, already confirmed on real
// hardware). This is the same idea applied to trail-tile rendering,
// which scales far larger over a long match.
//
// Flat-colored only, same constraint gles_compat.cpp's glBegin/glEnd
// shim already has (no per-vertex color, no GL_COLOR_ARRAY) -- add()
// computes world-space vertices directly (no per-cube glPushMatrix/
// glTranslatef needed) and automatically flushes the current batch
// before starting a new color, or before the buffer would exceed
// gles_compat's own 256-vertex glBegin/glEnd cap.
class cube_batch {
public:
    explicit cube_batch(glinterface* printer);

    // Appends one cube's 24 vertices (6 faces); flushes first if the
    // color differs from whatever's currently buffered, or if adding
    // this cube would overflow the shared 256-vertex limit.
    void add(GLfloat x, GLfloat y, GLfloat z, int r, int g, int b, int a, GLfloat size);

    // Draws whatever's currently buffered (one glColor4ub + one
    // glBegin/glEnd) and resets. Call once after the last add() in a
    // plane's piece loop -- otherwise the final batch never actually
    // reaches the screen.
    void flush();

private:
    glinterface* printer_;
    static constexpr int kMaxVerts = 256; // must match gles_compat.cpp's own cap
    GLfloat verts_[kMaxVerts][3];
    int count_;
    int r_, g_, b_, a_;
};
