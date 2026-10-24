
#include "ship.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#ifdef SHIP_DEBUG
#include <cstdio>
#endif
#include "plane.h"

static bool float_eq(float a, float b, float tol = 0.05f) {
    return std::abs(a - b) < tol;
}






static float get_ref_z(plane* p) {
    if (!p) return 0.0f;
    int idx = p->side_size + 1;
    if (idx >= 0 && idx < 400 && p->pieces[idx]) return p->pieces[idx]->z;
    if (p->pieces[0]) return p->pieces[0]->z;
    return 0.0f;
}

static float get_ref_y(plane* p) {
    if (!p) return 0.0f;
    int idx = p->side_size + 1;
    if (idx >= 0 && idx < 400 && p->pieces[idx]) return p->pieces[idx]->y;
    if (p->pieces[0]) return p->pieces[0]->y;
    return 0.0f;
}

static float get_ref_x(plane* p) {
    if (!p) return 0.0f;
    int idx = p->side_size + 1;
    if (idx >= 0 && idx < 400 && p->pieces[idx]) return p->pieces[idx]->x;
    if (p->pieces[0]) return p->pieces[0]->x;
    return 0.0f;
}

// ------------------------------------------------------------------
// plane field accessors (layout confirmed from plane.cpp constructor order):
//   +0x18 face_number, +0x20 id_union_top, +0x24 id_union_left,
//   +0x28 id_union_bottom, +0x2c id_union_right, +0x30 visited (bool)
// look_for_pos / tell_me_the_cost walk the neighbours in the order
//   +0x20, +0x24, +0x2c, +0x28  =  top, left, right, bottom
// ------------------------------------------------------------------
static inline int plane_neighbor(const plane* p, int i) {
    switch (i) {
        case 0:  return p->id_union_top;     // +0x20
        case 1:  return p->id_union_left;    // +0x24
        case 2:  return p->id_union_right;   // +0x2c
        default: return p->id_union_bottom;  // +0x28
    }
}
static inline bool& plane_visited(plane* p) { return p->visited; }  // +0x30

// ------------------------------------------------------------------
// Neighbour-plane lookup with validation.  The original binary would index
// map[-1] and crash on an unlinked plane; here we bail out instead (the ship
// just doesn't move that tick).  Build with -DSHIP_DEBUG to log every time
// that happens - it tells you exactly which union link is bad.
// ------------------------------------------------------------------
#ifdef SHIP_DEBUG
#include <cstdio>
#define SHIP_LOG(...) std::fprintf(stderr, __VA_ARGS__)
#else
#define SHIP_LOG(...) ((void)0)
#endif
static plane* link_plane(const ship* s, plane** map, int id, const char* which) {
    if (id < 0 || !map[id]) {
        SHIP_LOG("[ship] bad '%s' union link id=%d  (plane=%d face=%d prev_face=%d x=%.0f y=%.0f z=%.0f)\n",
                 which, id, s->actual_plane, s->actual_face, s->previous_face, s->x, s->y, s->z);
        return nullptr;
    }
    return map[id];
}

// Recursive plane-graph cost (ship::tell_me_the_cost @0x0890659c).
// Number of plane hops from 'cur' to 'dest' (starting count 'cost'); 90 = already visited.
static int plane_cost(plane** map, int cur, int dest, int cost) {
    if (cur == dest) {
        plane_visited(map[cur]) = 0;
        return cost;
    }
    plane* p = map[cur];
    int result = cost + 1;
    bool adjacent = false;
    for (int i = 0; i < 4; i++)
        if (plane_neighbor(p, i) == dest) { adjacent = true; break; }
    if (!adjacent) {
        plane_visited(p) = 1;
        int costs[4];
        for (int i = 0; i < 4; i++) {
            int n = plane_neighbor(p, i);
            if (plane_visited(map[n]) == 0)
                costs[i] = plane_cost(map, n, dest, cost + 1);
            else
                costs[i] = 90;
        }
        // Ghidra shows these summed, but the DWARF locals are "costs[4]" + "min":
        // the original is a minimum over the four neighbours.
        int mn = costs[0];
        for (int i = 1; i < 4; i++) if (costs[i] < mn) mn = costs[i];
        result = mn;
    }
    plane_visited(p) = 0;
    return result;
}

ship::ship()
    : x(0.0f), y(0.0f), z(0.0f),
      x_predict(0.0f), y_predict(0.0f), z_predict(0.0f),
      size(0.3f), parts(1),
      cam_ship(new camera()), printer(new glinterface()),
      advance(1), last_advance(1),
      previous_face(1), actual_face(1),
      actual_plane(0), last_plane(0),
      cue_counter(0), ident(0),
      path_found(false), status(1),
      step_counter(0), step_speed(0),
      effect_size(0.3f), effect_alpha(200.0f) {
    material[0] = 0;
    material[1] = 100;
    material[2] = 250;
    material[3] = 200;
    std::memset(plane_path, 0, sizeof(plane_path));
}

ship::~ship() {
    delete cam_ship;
    delete printer;
}

void ship::set_position(GLfloat nx, GLfloat ny, GLfloat nz) {
    x = nx;
    y = ny;
    z = nz;
}

// ==========================================
// START: arrange_direction_to
// Handles orienting / aligning previous_face when targeting a specific face
// ==========================================
void ship::arrange_direction_to(int face_forward) {
    int iVar1;
    switch (actual_face) {
        default:
            goto switchD_08906848_caseD_0;
        case 1:
        case 4:
            if (face_forward == 3) {
            LAB_08906920:
                previous_face = 6;
                return;
            }
            if (face_forward < 4) {
                if (face_forward != 2) {
                    return;
                }
                goto LAB_089068a0;
            }
            if (face_forward == 5) {
                previous_face = 2;
                return;
            }
            break;
        case 2:
        case 5:
            if (face_forward == 3) goto LAB_08906920;
            if (face_forward < 4) goto LAB_08906910;
            if (face_forward == 4) goto LAB_08906944;
            break;
        case 3:
            iVar1 = 2;
            if (face_forward == 2) {
            LAB_089068a0:
                previous_face = 5;
                return;
            }
            if (2 < face_forward) {
                if (face_forward == 4) goto LAB_08906944;
                if (face_forward != 5) {
                    return;
                }
                goto LAB_08906908;
            }
            goto LAB_08906910;
        case 6:
            if (face_forward == 2) goto LAB_089068a0;
            if (2 < face_forward) {
                if (face_forward != 4) {
                    if (face_forward != 5) {
                        return;
                    }
                    previous_face = 2;
                    return;
                }
            LAB_08906944:
                previous_face = 1;
                return;
            }
        LAB_08906910:
            if (face_forward == 1) {
                previous_face = 4;
                return;
            }
        switchD_08906848_caseD_0:
            return;
    }
    iVar1 = 3;
    if (face_forward != 6) {
        return;
    }
LAB_08906908:
    previous_face = iVar1;
    return;
}
// ==========================================
// END: arrange_direction_to
// ==========================================

// ==========================================
// START: change_direction
// Handles turning and updating previous_face for movement orientation
// ==========================================
void ship::change_direction(int turn) {
    int iVar1;
    int iVar2;
    int iVar3;

    if (turn != 0) {
        switch (actual_face) {
            default:
                goto switchD_089069a8_caseD_0;
            case 1:
                goto switchD_089069a8_caseD_4;
            case 2:
                goto switchD_089069a8_caseD_5;
            case 3:
                goto switchD_089069a8_caseD_6;
            case 4:
                goto switchD_089069a8_caseD_1;
            case 5:
                goto switchD_089069a8_caseD_2;
            case 6:
                iVar1 = previous_face;
                iVar2 = 2;
                if (iVar1 == 2) goto LAB_08906b74;
                if (2 < iVar1) {
                    if (iVar1 != 4) {
                        if (iVar1 != 5) {
                            return;
                        }
                        previous_face = 1;
                        return;
                    }
                    goto LAB_08906b58;
                }
                goto LAB_08906ab0;
        }
    }
    switch (actual_face) {
        default:
        switchD_089069a8_caseD_0:
            return;
        case 1:
            goto switchD_089069a8_caseD_1;
        case 2:
        switchD_089069a8_caseD_2:
            iVar1 = previous_face;
            iVar2 = 3;
            if (iVar1 == 3) {
            LAB_08906b24:
                previous_face = 1;
                return;
            }
            if (iVar1 < 4) {
                if (iVar1 != 1) {
                    return;
                }
                goto LAB_08906b48;
            }
            iVar3 = 4;
            if (iVar1 == 4) goto LAB_08906b34;
            break;
        case 3:
            iVar1 = previous_face;
            iVar2 = 2;
            if (iVar1 == 2) goto LAB_08906b74;
            if (2 < iVar1) {
                if (iVar1 == 4) goto LAB_08906b58;
                if (iVar1 != 5) {
                    return;
                }
                goto LAB_08906b24;
            }
            goto LAB_08906ab0;
        case 4:
        switchD_089069a8_caseD_4:
            iVar1 = previous_face;
            if (iVar1 == 3) goto LAB_08906a70;
            if (iVar1 < 4) {
                if (iVar1 != 2) {
                    return;
                }
                goto LAB_08906b48;
            }
            iVar3 = 5;
            if (iVar1 == 5) {
                previous_face = 3;
                return;
            }
            break;
        case 5:
        switchD_089069a8_caseD_5:
            iVar1 = previous_face;
            iVar2 = 3;
            if (iVar1 == 3) {
            LAB_08906b74:
                previous_face = 4;
                return;
            }
            if (3 < iVar1) {
                if (iVar1 != 4) {
                    if (iVar1 != 6) {
                        return;
                    }
                    goto LAB_08906b24;
                }
                goto LAB_08906b48;
            }
        LAB_08906ab0:
            if (iVar1 != 1) {
                return;
            }
        LAB_08906b34:
            previous_face = iVar2;
            return;
        case 6:
        switchD_089069a8_caseD_6:
            iVar1 = previous_face;
            iVar2 = 2;
            if (iVar1 == 2) goto LAB_08906b24;
            if (iVar1 < 3) {
                if (iVar1 != 1) {
                    return;
                }
                goto LAB_08906b58;
            }
            iVar3 = 4;
            if (iVar1 != 4) {
                if (iVar1 != 5) {
                    return;
                }
                goto LAB_08906aa8;
            }
            goto LAB_08906b34;
    }
    if (iVar1 != 6) {
        return;
    }
LAB_08906aa8:
    previous_face = iVar3;
    return;
switchD_089069a8_caseD_1:
    iVar1 = previous_face;
    iVar2 = 3;
    if (iVar1 == 3) {
    LAB_08906b58:
        previous_face = 5;
        return;
    }
    if (3 < iVar1) {
        if (iVar1 != 5) {
            if (iVar1 != 6) {
                return;
            }
        LAB_08906a70:
            previous_face = 2;
            return;
        }
    LAB_08906b48:
        previous_face = 6;
        return;
    }
    if (iVar1 != 2) {
        return;
    }
    goto LAB_08906b34;
}

// ==========================================
// START: do_step
// Core movement logic: handles ship stepping across grid coordinates,
// boundary checks, and face/plane transitions (faces 1-6)
// ==========================================
void ship::do_step(plane** map) {
    if (!map || actual_plane < 0 || !map[actual_plane]) return;
#ifdef SHIP_DEBUG
    // Build with -DSHIP_DEBUG to log plane transitions and unlinked/invalid planes.
    {
        plane* cp = map[actual_plane];
        const int u[4] = { cp->id_union_top, cp->id_union_left, cp->id_union_bottom, cp->id_union_right };
        static const char* nm[4] = { "top", "left", "bottom", "right" };
        for (int k = 0; k < 4; k++)
            if (u[k] < 0 || !map[u[k]])
                std::fprintf(stderr, "[ship] plane %d (face %d, dead_axis %d) has INVALID id_union_%s = %d\n",
                             actual_plane, cp->face_number, cp->dead_axis, nm[k], u[k]);
    }
    struct StepLog {
        ship* s; int p, f, pf; float x, y, z;
        ~StepLog() {
            if (s->actual_plane != p)
                std::fprintf(stderr,
                    "[ship] plane %d->%d face %d->%d prev_face %d->%d pos (%.0f %.0f %.0f)->(%.0f %.0f %.0f) advance %d\n",
                    p, s->actual_plane, f, s->actual_face, pf, s->previous_face,
                    x, y, z, s->x, s->y, s->z, s->advance);
        }
    } step_log = { this, actual_plane, actual_face, previous_face, x, y, z };
#endif
    last_plane = actual_plane;
    last_advance = advance;
    float fVar9 = 1.0f;

    switch (actual_face) {
        default:
            return;
        case 1:
        case 4:
            if (previous_face == 3) {
                if (!map[actual_plane]->pieces[0]) return;
                if (x != map[actual_plane]->pieces[0]->x) {
                    advance = -1;
                    x -= fVar9;
                    return;
                }
                int nxt = map[actual_plane]->id_union_left;
                if (!link_plane(this, map, nxt, "left")) return;
                plane* ppVar3 = map[nxt];
                actual_plane = nxt;
                float fVar8 = get_ref_z(ppVar3);
                if (z < fVar8) {
                    previous_face = 4;
                    advance = 3;
                    actual_face = ppVar3->face_number;
                    z += fVar9;
                    return;
                }
                if (float_eq(fVar8, z)) {
                    advance = -1;
                    x -= fVar9;
                    return;
                }
                previous_face = 1;
                advance = -3;
                z -= fVar9;
                actual_face = ppVar3->face_number;
                return;
            } else {
                if (previous_face < 4) {
                    if (previous_face != 2) return;
                    plane* ppVar2 = map[actual_plane];
                    if (!ppVar2->pieces[0]) return;
                    if (y != ((float)ppVar2->side_size + ppVar2->pieces[0]->y) - fVar9) {
                        advance = 2;
                        y += fVar9;
                        return;
                    }
                    plane* ppVar3 = link_plane(this, map, ppVar2->id_union_top, "top");
                    if (!ppVar3) return;
                    actual_plane = ppVar2->id_union_top;
                    float fVar5 = get_ref_z(ppVar3);
                    if (z < fVar5) {
                        previous_face = 4;
                        advance = 3;
                        actual_face = ppVar3->face_number;
                        z += fVar9;
                        return;
                    }
                    if (float_eq(fVar5, z)) {
                        advance = 2;
                        y += fVar9;
                        return;
                    }
                    previous_face = 1;
                    advance = -3;
                    z -= fVar9;
                    actual_face = ppVar3->face_number;
                    return;
                } else {
                    if (previous_face == 5) {
                        if (!map[actual_plane]->pieces[0]) return;
                        if (y != map[actual_plane]->pieces[0]->y) {
                            advance = -2;
                            y -= fVar9;
                            return;
                        }
                        int nxt = map[actual_plane]->id_union_bottom;
                        if (!link_plane(this, map, nxt, "bottom")) return;
                        plane* ppVar3 = map[nxt];
                        actual_plane = nxt;
                        float fVar8 = get_ref_z(ppVar3);
                        if (z < fVar8) {
                            previous_face = 4;
                            advance = 3;
                            actual_face = ppVar3->face_number;
                            z += fVar9;
                            return;
                        }
                        if (float_eq(fVar8, z)) {
                            advance = -2;
                            y -= fVar9;
                            return;
                        }
                        previous_face = 1;
                        advance = -3;
                        z -= fVar9;
                        actual_face = ppVar3->face_number;
                        return;
                    }
                    if (previous_face != 6) return;
                    plane* ppVar2 = map[actual_plane];
                    if (!ppVar2->pieces[0]) return;
                    if (x != ((float)ppVar2->side_size + ppVar2->pieces[0]->x) - fVar9) {
                        advance = 1;
                        x += fVar9;
                        return;
                    }
                    plane* ppVar3 = link_plane(this, map, ppVar2->id_union_right, "right");
                    if (!ppVar3) return;
                    actual_plane = ppVar2->id_union_right;
                    float fVar5 = get_ref_z(ppVar3);
                    if (z < fVar5) {
                        previous_face = 4;
                        advance = 3;
                        actual_face = ppVar3->face_number;
                        z += fVar9;
                        return;
                    }
                    if (float_eq(fVar5, z)) {
                        advance = 1;
                        x += fVar9;
                        return;
                    }
                    previous_face = 1;
                    advance = -3;
                    z -= fVar9;
                    actual_face = ppVar3->face_number;
                    return;
                }
            }
            break;

        case 2:
        case 5:
            if (previous_face == 3) {
                if (!map[actual_plane]->pieces[0]) return;
                if (x != map[actual_plane]->pieces[0]->x) {
                    advance = -1;
                    x -= fVar9;
                    return;
                }
                int nxt = map[actual_plane]->id_union_left;
                if (!link_plane(this, map, nxt, "left")) return;
                plane* ppVar2 = map[nxt];
                actual_plane = nxt;
                float fVar8 = get_ref_y(ppVar2);
                if (y < fVar8) {
                    advance = 2;
                    previous_face = 2;
                    actual_face = ppVar2->face_number;
                    y += fVar9;
                    return;
                }
                if (float_eq(fVar8, y)) {
                    advance = -1;
                    x -= fVar9;
                    return;
                }
                previous_face = 5;
                advance = -2;
                y -= fVar9;
                actual_face = ppVar2->face_number;
                return;
            } else {
                if (previous_face > 3) {
                    if (previous_face == 4) {
                        plane* ppVar3 = map[actual_plane];
                        if (!ppVar3->pieces[0]) return;
                        if (z != ((float)ppVar3->side_size + ppVar3->pieces[0]->z) - fVar9) {
                            advance = 3;
                            z += fVar9;
                            return;
                        }
                        plane* ppVar2 = link_plane(this, map, ppVar3->id_union_bottom, "bottom");
                        if (!ppVar2) return;
                        actual_plane = ppVar3->id_union_bottom;
                        float fVar6 = get_ref_y(ppVar2);
                        if (y < fVar6) {
                            previous_face = 2;
                            advance = 2;
                            actual_face = ppVar2->face_number;
                            y += fVar9;
                            return;
                        }
                        if (float_eq(fVar6, y)) {
                            advance = 3;
                            z += fVar9;
                            return;
                        }
                        previous_face = 5;
                        advance = -2;
                        y -= fVar9;
                        actual_face = ppVar2->face_number;
                        return;
                    }
                    if (previous_face != 6) return;
                    plane* ppVar3 = map[actual_plane];
                    if (!ppVar3->pieces[0]) return;
                    if (x != ((float)ppVar3->side_size + ppVar3->pieces[0]->x) - fVar9) {
                        advance = 1;
                        x += fVar9;
                        return;
                    }
                    plane* ppVar2 = link_plane(this, map, ppVar3->id_union_right, "right");
                    if (!ppVar2) return;
                    actual_plane = ppVar3->id_union_right;
                    float fVar5 = get_ref_y(ppVar2);
                    if (y < fVar5) {
                        previous_face = 2;
                        advance = 2;
                        actual_face = ppVar2->face_number;
                        y += fVar9;
                        return;
                    }
                    if (float_eq(fVar5, y)) {
                        advance = 1;
                        x += fVar9;
                        return;
                    }
                    previous_face = 5;
                    advance = -2;
                    y -= fVar9;
                    actual_face = ppVar2->face_number;
                    return;
                }
                if (previous_face != 1) return;
                if (!map[actual_plane]->pieces[0]) return;
                if (z != map[actual_plane]->pieces[0]->z) {
                    advance = -3;
                    z -= fVar9;
                    return;
                }
                int nxt = map[actual_plane]->id_union_top;
                if (!link_plane(this, map, nxt, "top")) return;
                plane* ppVar2 = map[nxt];
                actual_plane = nxt;
                float fVar9_ref = get_ref_y(ppVar2);
                if (y < fVar9_ref) {
                    previous_face = 2;
                    advance = 2;
                    actual_face = ppVar2->face_number;
                    y += 1.0f;
                    return;
                }
                if (float_eq(fVar9_ref, y)) {
                    advance = -3;
                    z -= 1.0f;
                    return;
                }
                previous_face = 5;
                advance = -2;
                y -= 1.0f;
                actual_face = ppVar2->face_number;
                return;
            }
            break;

        case 3:
        case 6:
            if (previous_face != 2) {
                if (previous_face < 3) {
                    if (previous_face != 1) return;
                    if (!map[actual_plane]->pieces[0]) return;
                    if (z != map[actual_plane]->pieces[0]->z) {
                        advance = -3;
                        z -= fVar9;
                        return;
                    }
                    int nxt = map[actual_plane]->id_union_left;
                    if (!link_plane(this, map, nxt, "left")) return;
                    plane* ppVar3 = map[nxt];
                    actual_plane = nxt;
                    float fVar5 = get_ref_x(ppVar3);
                    if (x < fVar5) {
                        previous_face = 6;
                        advance = 1;
                        x += fVar9;
                        actual_face = ppVar3->face_number;
                        return;
                    }
                    if (float_eq(fVar5, x)) {
                        advance = -3;
                        z -= fVar9;
                        return;
                    }
                    previous_face = 3;
                    advance = -1;
                    x -= fVar9;
                    actual_face = ppVar3->face_number;
                    return;
                } else {
                    if (previous_face == 4) {
                        plane* ppVar2 = map[actual_plane];
                        if (!ppVar2->pieces[0]) return;
                        if (z != ((float)ppVar2->side_size + ppVar2->pieces[0]->z) - fVar9) {
                            advance = 3;
                            z += fVar9;
                            return;
                        }
                        plane* ppVar3 = link_plane(this, map, ppVar2->id_union_right, "right");
                        if (!ppVar3) return;
                        actual_plane = ppVar2->id_union_right;
                        float fVar8 = get_ref_x(ppVar3);
                        if (fVar8 <= x) {
                            if (float_eq(fVar8, x)) {
                                advance = 3;
                                z += fVar9;
                                return;
                            }
                            previous_face = 3;
                            advance = -1;
                            x -= fVar9;
                            actual_face = ppVar3->face_number;
                            return;
                        }
                        previous_face = 6;
                        advance = 1;
                        x += fVar9;
                        actual_face = ppVar3->face_number;
                        return;
                    }
                    if (previous_face != 5) return;
                    if (!map[actual_plane]->pieces[0]) return;
                    if (y != map[actual_plane]->pieces[0]->y) {
                        advance = -2;
                        y -= fVar9;
                        return;
                    }
                    int nxt = map[actual_plane]->id_union_bottom;
                    if (!link_plane(this, map, nxt, "bottom")) return;
                    plane* ppVar3 = map[nxt];
                    actual_plane = nxt;
                    float fVar5 = get_ref_x(ppVar3);
                    if (x < fVar5) {
                        previous_face = 6;
                        advance = 1;
                        x += fVar9;
                        actual_face = ppVar3->face_number;
                        return;
                    }
                    if (float_eq(fVar5, x)) {
                        advance = -2;
                        y -= fVar9;
                        return;
                    }
                    previous_face = 3;
                    advance = -1;
                    x -= fVar9;
                    actual_face = ppVar3->face_number;
                    return;
                }
            } else {
                plane* ppVar2 = map[actual_plane];
                if (!ppVar2->pieces[0]) return;
                if (y != ((float)ppVar2->side_size + ppVar2->pieces[0]->y) - fVar9) {
                    advance = 2;
                    y += fVar9;
                    return;
                }
                plane* ppVar3 = link_plane(this, map, ppVar2->id_union_top, "top");
                if (!ppVar3) return;
                actual_plane = ppVar2->id_union_top;
                float fVar5 = get_ref_x(ppVar3);
                if (fVar5 <= x) {
                    if (float_eq(fVar5, x)) {
                        advance = 2;
                        y += fVar9;
                        return;
                    }
                    previous_face = 3;
                    advance = -1;
                    x -= fVar9;
                    actual_face = ppVar3->face_number;
                    return;
                }
                previous_face = 6;
                advance = 1;
                x += fVar9;
                actual_face = ppVar3->face_number;
                return;
            }
            break;
    }
}
// ==========================================
// END: do_step
// ==========================================

// ==========================================
// START: Initialization (init_pos and init)
// Handles setting initial camera position and ship orientation on map
// ==========================================
void ship::init_pos(int ix, int iy, int iz) {
    if (cam_ship) {
        cam_ship->update_pos(ix, iy, iz, 0.0f, 0.0f, 0.0f);
        cam_ship->last_x = (float)ix;
        cam_ship->last_y = (float)iy;
        cam_ship->last_z = (float)iz;
    }
}

void ship::init(plane** map) {
    if (!map || !map[actual_plane]) return;
    int idx = map[actual_plane]->find_piece_index(x, y, z);
    if (idx >= 0 && cam_ship) {
        cam_ship->update_pos(map[actual_plane]->cam_positions[idx][0],
                             map[actual_plane]->cam_positions[idx][1],
                             map[actual_plane]->cam_positions[idx][2],
                             0.0f, 0.0f, 0.0f);
    }
}

// ==========================================
// START: find_path   (ship::find_path @0x08907600)
// Ghidra decoded: three stages per dead_axis
//   A) aligned on the "other" axis      -> turn toward target, found = true
//   B) aligned on the second axis       -> turn toward target, found = true, return
//   C) aligned on neither (diagonal)    -> turn toward target, found stays false
// 'polarity' is Ghidra's in_t1 (> 0 / <= 0 selects turn direction).
// ==========================================
bool ship::find_path(int tx, int ty, int tz, int dead_axis, int polarity) {
    const float fx = (float)tx, fy = (float)ty, fz = (float)tz;
    const int P = polarity;
    bool found = false;

    if (dead_axis == 3) {
        // ---- A: x aligned, y not ----
        if (x == fx && y != fy) {
            found = true;
            const int adv = advance;
            if (fy <= y) {
                if (adv == 1)       change_direction(P > 0 ? 1 : 0);
                else if (adv == 2)  change_direction(std::rand() % 2);
                else if (adv == -1) change_direction(P < 1 ? 1 : 0);
            } else {
                if (adv == -1)      change_direction(P > 0 ? 1 : 0);
                else if (adv == 1)  change_direction(P < 1 ? 1 : 0);
                else if (adv == -2) change_direction(std::rand() % 2);
            }
        }
        // ---- B: y aligned ----
        if (y == fy) {
            if (fx != x) {
                found = true;
                const int adv = advance;
                if (fx <= x) {
                    if (adv == 1)       change_direction(std::rand() % 2);
                    else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
                    else if (adv == -2) change_direction(P > 0 ? 1 : 0);
                } else {
                    if (adv == -1)      change_direction(std::rand() % 2);
                    else if (adv == 2)  change_direction(P > 0 ? 1 : 0);
                    else if (adv == -2) change_direction(P > 0 ? 0 : 1);
                }
            }
            return found;
        }
        // ---- C: neither aligned (x != tx and y != ty) ----
        if (fx == x) return found;
        const int adv = advance;
        if (x < fx) {
            if (adv == -1)      change_direction(std::rand() % 2);
            else if (adv == -2) change_direction(P > 0 ? 0 : 1);
            else if (adv == 2)  change_direction(P > 0 ? 1 : 0);
        } else {
            if (adv == 1)       change_direction(std::rand() % 2);
            else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
            else if (adv == -2) change_direction(P > 0 ? 1 : 0);
        }
        return found;
    }

    if (dead_axis == 2) {
        // ---- A: z aligned, x not ----
        if (z == fz && x != fx) {
            found = true;
            const int adv = advance;
            if (fx <= x) {
                if (adv == 1)       change_direction(std::rand() % 2);
                else if (adv == 3)  change_direction(P > 0 ? 1 : 0);
                else if (adv == -3) change_direction(P > 0 ? 0 : 1);
            } else {
                if (adv == -1)      change_direction(std::rand() % 2);
                else if (adv == 3)  change_direction(P > 0 ? 0 : 1);
                else if (adv == -3) change_direction(P > 0 ? 1 : 0);
            }
        }
        // ---- B: x aligned ----
        if (x == fx) {
            if (fz != z) {
                found = true;
                const int adv = advance;
                if (fz <= z) {
                    if (adv == 2)       change_direction(P > 0 ? 1 : 0);
                    else if (adv == 3)  change_direction(std::rand() % 2);
                    else if (adv == -2) change_direction(P > 0 ? 0 : 1);
                } else {
                    if (adv == -2)      change_direction(P > 0 ? 1 : 0);
                    else if (adv == -3) change_direction(std::rand() % 2);
                    else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
                }
            }
            return found;
        }
        // ---- C: neither aligned (x != tx and z != tz) ----
        if (fz == z) return found;
        const int adv = advance;
        if (z < fz) {
            if (adv == -2)      change_direction(P > 0 ? 1 : 0);
            else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
            else if (adv == -3) change_direction(std::rand() % 2);
        } else {
            if (adv == 2)       change_direction(P > 0 ? 1 : 0);
            else if (adv == 3)  change_direction(std::rand() % 2);
            else if (adv == -2) change_direction(P > 0 ? 0 : 1);
        }
        return found;
    }

    if (dead_axis == 1) {
        // ---- A: z aligned, y not ----
        if (z == fz && y != fy) {
            found = true;
            const int adv = advance;
            if (fy <= y) {
                if (adv == 2)       change_direction(std::rand() % 2);
                else if (adv == 3)  change_direction(P > 0 ? 0 : 1);
                else if (adv == -3) change_direction(P > 0 ? 1 : 0);
            } else {
                if (adv == -2)      change_direction(std::rand() % 2);
                else if (adv == 3)  change_direction(P > 0 ? 1 : 0);
                else if (adv == -3) change_direction(P > 0 ? 0 : 1);
            }
        }
        // ---- B: y aligned ----
        if (y == fy) {
            if (fz != z) {
                found = true;
                const int adv = advance;
                if (fz <= z) {
                    if (adv == 2)       change_direction(P > 0 ? 1 : 0);
                    else if (adv == 3)  change_direction(std::rand() % 2);
                    else if (adv == -2) change_direction(P > 0 ? 0 : 1);
                } else {
                    if (adv == -2)      change_direction(P > 0 ? 1 : 0);
                    else if (adv == -3) change_direction(std::rand() % 2);
                    else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
                }
            }
            return found;
        }
        // ---- C: neither aligned (y != ty and z != tz) ----
        if (fz == z) return found;
        const int adv = advance;
        if (fz <= z) {
            if (adv == 2)       change_direction(P > 0 ? 1 : 0);
            else if (adv == 3)  change_direction(std::rand() % 2);
            else if (adv == -2) change_direction(P > 0 ? 0 : 1);
        } else {
            if (adv == -2)      change_direction(P > 0 ? 1 : 0);
            else if (adv == 2)  change_direction(P > 0 ? 0 : 1);
            else if (adv == -3) change_direction(std::rand() % 2);
        }
        return found;
    }

    return false;
}

// ==========================================
// START: Navigation & Destination Planning
// ==========================================
int ship::where_is(float qx, float qy, float qz, plane** map, int n_planes) {
    if (!map) return 0;
    for (int k = 0; k < n_planes; k++) {
        if (map[k] && map[k]->has_piece(qx, qy, qz)) {
            return k;
        }
    }
    return 0;
}

// Header-compatible wrapper around plane_cost().
// The binary's real signature is (x,y,z floats, map, n_planes, plane_src, plane_dest, cost);
// here:  cx = current plane index, cy = destination plane index, cz = cost so far.
int ship::tell_me_the_cost(float cx, float cy, float cz, plane** map, int adv, int steps, int tx, int ty, int tz) {
    (void)adv; (void)steps; (void)tx; (void)ty; (void)tz;
    if (!map) return 0;
    return plane_cost(map, (int)cx, (int)cy, (int)cz);
}

// ship::look_for_pos @0x08907f4c
// Finds the plane that holds the target, then picks the cheapest NEIGHBOUR of the
// current plane (min of 4 costs, strict '<' so ties keep the earlier index) as the
// next hop.  Turns toward that hop's face if the ship isn't already on it.
bool ship::look_for_pos(float qx, float qy, float qz, plane** map, int n_planes) {
    int target_plane = where_is(qx, qy, qz, map, n_planes);
    plane_path[0] = target_plane;
    if (target_plane == actual_plane) {
        plane_visited(map[target_plane]) = 0;
        return true;
    }

    plane* cur = map[actual_plane];
    int costs[4];
    for (int i = 0; i < 4; i++)
        costs[i] = plane_cost(map, plane_neighbor(cur, i), target_plane, 0);

    int best = (costs[1] < costs[0]) ? 1 : 0;
    int mn   = (costs[1] < costs[0]) ? costs[1] : costs[0];
    if (costs[2] < mn) { best = 2; mn = costs[2]; }
    if (costs[3] < mn) { best = 3; }

    plane_path[0] = plane_neighbor(cur, best);
    path_found = false;

    int hop_face = map[plane_path[0]]->face_number;
    if (hop_face != actual_face) {
        arrange_direction_to(hop_face);
        return false;
    }
    plane_visited(map[actual_plane]) = 0;
    return true;
}

// ship::go_to @0x08908260
bool ship::go_to(float tx, float ty, float tz, plane** map, int n_planes) {
    if (x == tx && ty == y && tz == z) {
        plane_path[0] = -1;
        path_found = false;
        return true;
    }
    bool onPlane = look_for_pos(tx, ty, tz, map, n_planes);
    if (onPlane && !path_found) {
        path_found = find_path((int)tx, (int)ty, (int)tz,
                               map[actual_plane]->dead_axis,
                               map[actual_plane]->axis_polarity);
        return false;
    }
    return false;
}
