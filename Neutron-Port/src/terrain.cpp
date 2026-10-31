#include "terrain.h"
#include <cstring>
#ifdef TERRAIN_DEBUG
#include <cstdio>
#endif

terrain::terrain()
    : N_PLANES(0),
      cam_terrain(new camera()),
      printer(new glinterface()),
      is_clear(true),
      painted_bones(0),
      painted_planes(0),
      time(0.0f),
      status(0),
      start_on_plane(-1) {
    material[0] = 255;
    material[1] = 255;
    material[2] = 255;
    material[3] = 255; // White border lines in navigation mode

    ground_material[0] = 190;
    ground_material[1] = 190;
    ground_material[2] = 200;
    ground_material[3] = 25; // Light transparent grey inside faces

    std::memset(map, 0, sizeof(map));
    std::memset(unions, 0, sizeof(unions));
    std::memset(max_vertex, 0, sizeof(max_vertex));
    std::memset(painted, 0, sizeof(painted));
}

terrain::~terrain() {
    delete cam_terrain;
    delete printer;
    for (int i = 0; i < N_PLANES; i++) {
        delete map[i];
        map[i] = nullptr;
    }
}

void terrain::create_unions() {
    int iVar6 = N_PLANES;
    if (iVar6 <= 0) return;
    for (int i = 0; i < iVar6; i++) {
        plane* ppVar3 = map[i];
        if (!ppVar3 || ppVar3->side_size < 1) continue;
        piece* ppVar1 = ppVar3->pieces[0];
        if (!ppVar1) continue;

        int iVar2 = ppVar3->side_size;
        int iVar7 = (int)ppVar1->x;
        int iVar9 = (int)ppVar1->y;
        int iVar11 = (int)ppVar1->z;

        if (ppVar3->dead_axis == 1) {
            unions[i][0][0] = iVar7;
            unions[i][0][1] = iVar9 + iVar2 - 1;
            unions[i][0][2] = iVar11 + iVar2 / 2;

            unions[i][1][0] = iVar7;
            unions[i][1][1] = iVar9 + iVar2 / 2;
            unions[i][1][2] = iVar11;

            unions[i][2][0] = iVar7;
            unions[i][2][1] = iVar9 + iVar2 / 2;
            unions[i][2][2] = iVar11 + iVar2 - 1;

            unions[i][3][0] = iVar7;
            unions[i][3][1] = iVar9;
            unions[i][3][2] = iVar11 + iVar2 / 2;
        } else if (ppVar3->dead_axis == 2) {
            unions[i][0][0] = iVar7 + iVar2 / 2;
            unions[i][0][1] = iVar9;
            unions[i][0][2] = iVar11;

            unions[i][1][0] = iVar7;
            unions[i][1][1] = iVar9;
            unions[i][1][2] = iVar11 + iVar2 / 2;

            unions[i][2][0] = iVar7 + iVar2 - 1;
            unions[i][2][1] = iVar9;
            unions[i][2][2] = iVar11 + iVar2 / 2;

            unions[i][3][0] = iVar7 + iVar2 / 2;
            unions[i][3][1] = iVar9;
            unions[i][3][2] = iVar11 + iVar2 - 1;
        } else {
            unions[i][0][0] = iVar7 + iVar2 / 2;
            unions[i][0][1] = iVar9 + iVar2 - 1;
            unions[i][0][2] = iVar11;

            unions[i][1][0] = iVar7;
            unions[i][1][1] = iVar9 + iVar2 / 2;
            unions[i][1][2] = iVar11;

            unions[i][2][0] = iVar7 + iVar2 - 1;
            unions[i][2][1] = iVar9 + iVar2 / 2;
            unions[i][2][2] = iVar11;

            unions[i][3][0] = iVar7 + iVar2 / 2;
            unions[i][3][1] = iVar9;
            unions[i][3][2] = iVar11;
        }
    }
}

int terrain::wich_union(int a, int b, int c, int n_plane, int union_type) {
    if (n_plane < 0 || n_plane >= N_PLANES || !map[n_plane]) return a;
    int target_face = 0;
    int current_face = map[n_plane]->face_number;

    switch (current_face) {
        case 1:
        case 4:
            if (union_type == 1) target_face = 3;
            else if (union_type == 0) target_face = 2;
            else if (union_type == 2) target_face = 6;
            else target_face = 5;
            break;
        case 2:
        case 5:
            if (union_type == 1) target_face = 3;
            else if (union_type == 0) target_face = 1;
            else if (union_type == 2) target_face = 6;
            else target_face = 4;
            break;
        case 3:
        case 6:
            if (union_type == 1) target_face = 1;
            else if (union_type == 0) target_face = 2;
            else if (union_type == 2) target_face = 4;
            else target_face = 5;
            break;
        default:
            target_face = 0;
            break;
    }

    if (a >= 0 && a < N_PLANES && map[a] && map[a]->face_number == target_face) return a;
    if (b >= 0 && b < N_PLANES && map[b] && map[b]->face_number == target_face) return b;
    if (c >= 0 && c < N_PLANES && map[c] && map[c]->face_number == target_face) return c;
    if (a >= 0 && a < N_PLANES && map[a] && a != n_plane) return a;
    if (b >= 0 && b < N_PLANES && map[b] && b != n_plane) return b;
    if (c >= 0 && c < N_PLANES && map[c] && c != n_plane) return c;
    return a;
}

void terrain::set_unions_of_plane(int n_plane) {
    if (n_plane < 0 || n_plane >= N_PLANES || !map[n_plane]) return;

    int top_unions[4] = {-1, -1, -1, -1};
    int left_unions[4] = {-1, -1, -1, -1};
    int right_unions[4] = {-1, -1, -1, -1};
    int bottom_unions[4] = {-1, -1, -1, -1};

    int top_count = 0, left_count = 0, right_count = 0, bottom_count = 0;

    // Edge midpoints of this plane: 0 = top, 1 = left, 2 = right, 3 = bottom.
    // Two planes are neighbours across an edge when that edge midpoint coincides with
    // ANY of the other plane's edge midpoints (the shared edge is a different slot on
    // each plane: e.g. a front face's "right" edge is the side face's "left" edge).
    // Comparing only the same slot index (as before) misses most links, leaving -1.
    for (int other = 0; other < N_PLANES; other++) {
        if (other == n_plane || !map[other]) continue;

        bool shares[4] = {false, false, false, false};
        for (int k = 0; k < 4; k++) {
            for (int m = 0; m < 4; m++) {
                if (unions[n_plane][k][0] == unions[other][m][0] &&
                    unions[n_plane][k][1] == unions[other][m][1] &&
                    unions[n_plane][k][2] == unions[other][m][2]) {
                    shares[k] = true;
                    break;
                }
            }
        }

        if (shares[0] && top_count < 4)    top_unions[top_count++] = other;
        if (shares[1] && left_count < 4)   left_unions[left_count++] = other;
        if (shares[2] && right_count < 4)  right_unions[right_count++] = other;
        if (shares[3] && bottom_count < 4) bottom_unions[bottom_count++] = other;
    }

    // Assign top
    if (top_count == 1) map[n_plane]->id_union_top = top_unions[0];
    else map[n_plane]->id_union_top = wich_union(top_unions[0], top_unions[1], top_unions[2], n_plane, 0);

    // Assign left
    if (left_count == 1) map[n_plane]->id_union_left = left_unions[0];
    else map[n_plane]->id_union_left = wich_union(left_unions[0], left_unions[1], left_unions[2], n_plane, 1);

    // Assign right
    if (right_count == 1) map[n_plane]->id_union_right = right_unions[0];
    else map[n_plane]->id_union_right = wich_union(right_unions[0], right_unions[1], right_unions[2], n_plane, 2);

    // Assign bottom
    if (bottom_count == 1) map[n_plane]->id_union_bottom = bottom_unions[0];
    else map[n_plane]->id_union_bottom = wich_union(bottom_unions[0], bottom_unions[1], bottom_unions[2], n_plane, 3);
}

bool terrain::am_i_clear() {
    is_clear = true;
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            if (p->pieces[j] && p->pieces[j]->state && p->pieces[j]->owner > 0) {
                is_clear = false;
                return false;
            }
        }
    }
    return is_clear;
}

void terrain::fire_up_cue(int ship_ident) {
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc && pc->owner == ship_ident) {
                pc->owner = -2;
                pc->status = 2;
                pc->state = false;
            }
        }
    }
}

void terrain::clear_cue(int ship_ident) {
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc && pc->owner == ship_ident) {
                pc->temporal = true;
            }
        }
    }
}

void terrain::restart() {
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc) {
                pc->temporal = true;
                pc->type = 0;
                pc->state = false;
                pc->status = 0;
            }
        }
    }
    time = 0.0f;
}

void terrain::combat_mode() {
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc) {
                pc->owner = -2;
                pc->state = false;
                pc->temporal = false;
                pc->prints_counter = 0.0;
            }
        }
    }
}

void terrain::navigation_mode() {
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc) {
                pc->owner = -2;
                pc->state = false;
                if (pc->status != 2) {
                    pc->temporal = true;
                }
                pc->prints_counter = 0.0;
            }
        }
    }
}

void terrain::change_color(int r, int g, int b, int a) {
    material[0] = r;
    material[1] = g;
    material[2] = b;
    material[3] = a;
}

void terrain::create_from_miniature(miniature* mini_map) {
    if (!mini_map || mini_map->cubes < 1) {
        N_PLANES = 0;
        return;
    }

    plane* temp_map[150];
    int count = 0;

    for (int c = 0; c < mini_map->cubes && count + 6 <= 150; c++) {
        for (int f = 1; f <= 6; f++) {
            int cur_id = count;
            // miniature::convert_to_plane @0x0890b844: side_size is passed as a literal
            // 20 at the one real call site (confirmed via MIPS disassembly -- Ghidra lost
            // this as a corrupted stack/register arg in the decompiled C). The port's own
            // plane arrays (pieces[400], index[20][20]) were already sized for this; only
            // this call site had the wrong value (10, a quarter the real map area).
            temp_map[count] = mini_map->convert_to_plane(c, f, cur_id, 20);
            count++;
        }
    }

    // Ghidra (0x089039ac): a plane is dropped when ANOTHER plane has the same
    // i, j, dead_axis and dead_offset but a DIFFERENT face_number (the two faces of
    // a wall shared by neighbouring cubes - both disappear).  The scan must finish
    // before anything is deleted: the old code deleted planes mid-scan, so later
    // comparisons read freed memory and shared walls were only half removed.
    bool keep[150];
    for (int i = 0; i < count; i++) {
        keep[i] = true;
        plane* pi = temp_map[i];
        for (int j = 0; j < count; j++) {
            if (i == j) continue;
            plane* pj = temp_map[j];
            if (pi->i == pj->i && pi->j == pj->j &&
                pi->dead_axis == pj->dead_axis &&
                pi->dead_offset == pj->dead_offset &&
                pi->face_number != pj->face_number) {
                keep[i] = false;
                break;
            }
        }
    }

    int valid = 0;
    for (int i = 0; i < count; i++) {
        if (keep[i]) {
            map[valid] = temp_map[i];
            if (temp_map[i]->face_number == 4 && start_on_plane == -1) {
                start_on_plane = valid;
            }
            valid++;
        } else {
            delete temp_map[i];
            temp_map[i] = nullptr;
        }
    }

    N_PLANES = valid;
    if (start_on_plane == -1) start_on_plane = 0;
}

void terrain::create_from_array(int count, const int (*PLANES)[7]) {
    N_PLANES = count;
    for (int k = 0; k < count; k++) {
        plane* p = new plane();
        p->ident = k;
        p->i = PLANES[k][0];
        p->j = PLANES[k][1];
        p->side_size = PLANES[k][2];
        p->dead_axis = PLANES[k][3];
        p->dead_offset = PLANES[k][4];
        p->face_number = PLANES[k][5];
        p->axis_polarity = PLANES[k][6];
        p->id_union_right = -1;
        p->id_union_top = -1;
        p->id_union_left = -1;
        p->id_union_bottom = -1;
        map[k] = p;
    }
    if (start_on_plane == -1) start_on_plane = 0;
}

// ---------------------------------------------------------------------------
// Skeleton drawing, per Ghidra (print_skeleton @0x08903d28, build_skeleton @0x089041ac).
// An edge of a plane is drawn only when the plane across that edge has a DIFFERENT
// dead_axis (i.e. the surface folds there).  Between two coplanar planes (e.g. two cubes
// side by side) the seam is NOT drawn - that is why the in-game map has no line down the
// middle even though the stage-select miniature (which draws each cube) does.
//
// Edge ids on a plane's corners: 0 = a-b, 1 = a-c, 2 = b-d, 3 = c-d.
//   a = (row 0, col 0)  b = (row 0, col last)  c = (row last, col 0)  d = (row last, col last)
// ---------------------------------------------------------------------------
enum { U_TOP = 0, U_LEFT = 1, U_RIGHT = 2, U_BOTTOM = 3 };

// which corner-edge each union (top/left/right/bottom) is, per dead_axis (from create_unions)
static const int skel_edge_of_union[4][4] = {
    /* unused  */ { 0, 0, 0, 0 },
    /* axis 1  */ { 3, 1, 2, 0 },   // top=c-d  left=a-c  right=b-d  bottom=a-b
    /* axis 2  */ { 1, 0, 3, 2 },   // top=a-c  left=a-b  right=c-d  bottom=b-d
    /* axis 3  */ { 2, 0, 3, 1 },   // top=b-d  left=a-b  right=c-d  bottom=a-c
};
// order in which the original visits the four unions (also the order of painted[plane][0..3])
static const int skel_union_order[4][4] = {
    /* unused  */ { 0, 0, 0, 0 },
    /* axis 1  */ { U_BOTTOM, U_RIGHT, U_TOP, U_LEFT },
    /* axis 2  */ { U_LEFT, U_BOTTOM, U_RIGHT, U_TOP },
    /* axis 3  */ { U_LEFT, U_TOP, U_RIGHT, U_BOTTOM },
};

static int skel_union_id(const plane* p, int u) {
    switch (u) {
        case U_TOP:    return p->id_union_top;
        case U_LEFT:   return p->id_union_left;
        case U_RIGHT:  return p->id_union_right;
        default:       return p->id_union_bottom;
    }
}

template <class M>
static void skel_draw_edge(glinterface* g, const plane* p, int edge, M material) {
    const int* s = p->a_vertex;
    const int* t = p->b_vertex;
    switch (edge) {
        case 0: s = p->a_vertex; t = p->b_vertex; break;
        case 1: s = p->a_vertex; t = p->c_vertex; break;
        case 2: s = p->b_vertex; t = p->d_vertex; break;
        default: s = p->c_vertex; t = p->d_vertex; break;
    }
    g->line((float)s[0], (float)s[1], (float)s[2],
            (float)t[0], (float)t[1], (float)t[2], material);
}

// true when the plane across union 'u' is NOT coplanar with p (so the edge is a visible fold)
static bool skel_is_fold(plane** map, int n_planes, const plane* p, int u) {
    int id = skel_union_id(p, u);
    if (id < 0 || id >= n_planes || !map[id]) return true;
    return map[id]->dead_axis != p->dead_axis;
}

static void skel_draw_quad(const plane* p, const int* ground_material) {
    // Inside face: light quad sitting flat on the plane, depth mask off for transparency
    glDepthMask(GL_FALSE);
    glBegin(GL_QUADS);
    glColor4ub((GLubyte)ground_material[0], (GLubyte)ground_material[1],
               (GLubyte)ground_material[2], (GLubyte)ground_material[3]);
    glVertex3f((float)p->a_vertex[0], (float)p->a_vertex[1], (float)p->a_vertex[2]);
    glVertex3f((float)p->b_vertex[0], (float)p->b_vertex[1], (float)p->b_vertex[2]);
    glVertex3f((float)p->d_vertex[0], (float)p->d_vertex[1], (float)p->d_vertex[2]);
    glVertex3f((float)p->c_vertex[0], (float)p->c_vertex[1], (float)p->c_vertex[2]);
    glEnd();
    glDepthMask(GL_TRUE);
}

void terrain::print_skeleton() {
    if (!printer) return;

    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p || p->dead_axis < 1 || p->dead_axis > 3) continue;

        glLineWidth(2.5f);
        for (int k = 0; k < 4; k++) {
            int u = skel_union_order[p->dead_axis][k];
            if (skel_is_fold(map, N_PLANES, p, u)) {
                skel_draw_edge(printer, p, skel_edge_of_union[p->dead_axis][u], material);
            }
        }
        skel_draw_quad(p, ground_material);
    }
}

void terrain::build_skeleton(bool build) {
    if (painted_planes < N_PLANES) {
        if (painted_bones < 5) {
            painted[painted_planes][painted_bones] = build;
            painted_bones++;
        } else {
            painted_planes++;
            painted_bones = 0;
        }
    } else {
        status = 1;
        painted_bones = 0;
        painted_planes = 0;
    }

    if (!printer) return;

    // Only the edges whose painted flag has been switched on so far are drawn.
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p || p->dead_axis < 1 || p->dead_axis > 3) continue;

        glLineWidth(2.5f);
        for (int k = 0; k < 4; k++) {
            if (!painted[i][k]) continue;
            int u = skel_union_order[p->dead_axis][k];
            if (skel_is_fold(map, N_PLANES, p, u)) {
                skel_draw_edge(printer, p, skel_edge_of_union[p->dead_axis][u], material);
            }
        }
        skel_draw_quad(p, ground_material);
    }
}

int terrain::print(bool skeleton, int actual_plane, bool low_quality) {
    if (skeleton) {
        if (status == 4) {
            build_skeleton(true);
        } else if (status == 5) {
            build_skeleton(false);
        } else {
            print_skeleton();
        }
    }

    int total_prints = 0;
    for (int i = 0; i < N_PLANES; i++) {
        if (map[i]) {
            total_prints += map[i]->print(low_quality);
        }
    }
    return total_prints;
}

void terrain::init(int offset_x, int offset_y, int offset_z, int cam_distance) {
    for (int i = 0; i < N_PLANES; i++) {
        if (map[i]) {
            map[i]->ident = i;
            map[i]->generate_positions(offset_x, offset_y, offset_z, cam_distance);
            if (map[i]->max_vertex[0] > max_vertex[0]) max_vertex[0] = map[i]->max_vertex[0];
            if (map[i]->max_vertex[1] > max_vertex[1]) max_vertex[1] = map[i]->max_vertex[1];
            if (map[i]->max_vertex[2] > max_vertex[2]) max_vertex[2] = map[i]->max_vertex[2];
        }
    }
    create_unions();
    for (int i = 0; i < N_PLANES; i++) {
        set_unions_of_plane(i);
    }

#ifdef TERRAIN_DEBUG
    // Dump the plane layout and links (build with -DTERRAIN_DEBUG, run a level, send me stderr).
    std::fprintf(stderr, "[terrain] %d planes\n", N_PLANES);
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) { std::fprintf(stderr, "[terrain] #%d NULL\n", i); continue; }
        std::fprintf(stderr,
            "[terrain] #%d face=%d dead_axis=%d offset=%d i=%d j=%d side=%d  top=%d left=%d right=%d bottom=%d  p0=(%.0f,%.0f,%.0f)\n",
            i, p->face_number, p->dead_axis, p->dead_offset, p->i, p->j, p->side_size,
            p->id_union_top, p->id_union_left, p->id_union_right, p->id_union_bottom,
            p->pieces[0] ? p->pieces[0]->x : -1.0f, p->pieces[0] ? p->pieces[0]->y : -1.0f,
            p->pieces[0] ? p->pieces[0]->z : -1.0f);
    }
#endif

    if (cam_terrain) {
        float cx = (float)max_vertex[0] * 0.5f;
        float cy = (float)max_vertex[1] * 0.5f;
        float cz = (float)max_vertex[2] * 0.5f;
        cam_terrain->x = (float)max_vertex[0] + 18.0f;
        cam_terrain->y = (float)max_vertex[1] + 18.0f;
        cam_terrain->z = (float)max_vertex[2] + 18.0f;
        cam_terrain->x_dir = cx;
        cam_terrain->y_dir = cy;
        cam_terrain->z_dir = cz;
        cam_terrain->x_ori = 0.0f;
        cam_terrain->y_ori = 1.0f;
        cam_terrain->z_ori = 0.0f;
    }
}
