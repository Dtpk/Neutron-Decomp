#include "terrain.h"
#include <cstring>

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
    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p || p->side_size < 1) continue;
        piece* p0 = p->pieces[0];
        if (!p0) continue;

        int ss = p->side_size;
        int p0x = (int)p0->x;
        int p0y = (int)p0->y;
        int p0z = (int)p0->z;

        if (p->dead_axis == 1) {
            unions[i][0][0] = p0x; unions[i][0][1] = p0y + ss - 1; unions[i][0][2] = p0z + ss / 2;
            unions[i][1][0] = p0x; unions[i][1][1] = p0y + ss / 2; unions[i][1][2] = p0z;
            unions[i][2][0] = p0x; unions[i][2][1] = p0y + ss / 2; unions[i][2][2] = p0z + ss - 1;
            unions[i][3][0] = p0x; unions[i][3][1] = p0y;          unions[i][3][2] = p0z + ss / 2;
        } else if (p->dead_axis == 2) {
            unions[i][0][0] = p0x + ss / 2; unions[i][0][1] = p0y; unions[i][0][2] = p0z;
            unions[i][1][0] = p0x;          unions[i][1][1] = p0y; unions[i][1][2] = p0z + ss / 2;
            unions[i][2][0] = p0x + ss - 1; unions[i][2][1] = p0y; unions[i][2][2] = p0z + ss / 2;
            unions[i][3][0] = p0x + ss / 2; unions[i][3][1] = p0y; unions[i][3][2] = p0z + ss - 1;
        } else {
            unions[i][0][0] = p0x + ss / 2; unions[i][0][1] = p0y + ss - 1; unions[i][0][2] = p0z;
            unions[i][1][0] = p0x;          unions[i][1][1] = p0y + ss / 2; unions[i][1][2] = p0z;
            unions[i][2][0] = p0x + ss - 1; unions[i][2][1] = p0y + ss / 2; unions[i][2][2] = p0z;
            unions[i][3][0] = p0x + ss / 2; unions[i][3][1] = p0y;          unions[i][3][2] = p0z;
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
    return c;
}

void terrain::set_unions_of_plane(int n_plane) {
    if (n_plane < 0 || n_plane >= N_PLANES || !map[n_plane]) return;

    int top_unions[4] = {-1, -1, -1, -1};
    int left_unions[4] = {-1, -1, -1, -1};
    int right_unions[4] = {-1, -1, -1, -1};
    int bottom_unions[4] = {-1, -1, -1, -1};

    int top_count = 0, left_count = 0, right_count = 0, bottom_count = 0;

    for (int other = 0; other < N_PLANES; other++) {
        if (other == n_plane) continue;

        for (int e = 0; e < 4; e++) {
            // Check top
            if (unions[n_plane][0][0] == unions[other][e][0] &&
                unions[n_plane][0][1] == unions[other][e][1] &&
                unions[n_plane][0][2] == unions[other][e][2]) {
                if (top_count < 3) top_unions[top_count++] = other;
            }
            // Check left
            if (unions[n_plane][1][0] == unions[other][e][0] &&
                unions[n_plane][1][1] == unions[other][e][1] &&
                unions[n_plane][1][2] == unions[other][e][2]) {
                if (left_count < 3) left_unions[left_count++] = other;
            }
            // Check right
            if (unions[n_plane][2][0] == unions[other][e][0] &&
                unions[n_plane][2][1] == unions[other][e][1] &&
                unions[n_plane][2][2] == unions[other][e][2]) {
                if (right_count < 3) right_unions[right_count++] = other;
            }
            // Check bottom
            if (unions[n_plane][3][0] == unions[other][e][0] &&
                unions[n_plane][3][1] == unions[other][e][1] &&
                unions[n_plane][3][2] == unions[other][e][2]) {
                if (bottom_count < 3) bottom_unions[bottom_count++] = other;
            }
        }
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

    for (int c = 0; c < mini_map->cubes; c++) {
        for (int f = 1; f <= 6; f++) {
            int cur_id = count;
            temp_map[count] = mini_map->convert_to_plane(c, f, cur_id, 10);
            count++;
        }
    }

    int valid = 0;
    for (int i = 0; i < count; i++) {
        plane* pi = temp_map[i];
        bool duplicate = false;

        for (int j = 0; j < count; j++) {
            if (i == j) continue;
            plane* pj = temp_map[j];
            if (pi->i == pj->i && pi->j == pj->j &&
                pi->dead_axis == pj->dead_axis &&
                pi->dead_offset == pj->dead_offset) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            map[valid] = pi;
            if (pi->face_number == 4 && start_on_plane == -1) {
                start_on_plane = valid;
            }
            valid++;
        } else {
            delete pi;
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

void terrain::print_skeleton() {
    if (!printer) return;

    for (int i = 0; i < N_PLANES; i++) {
        plane* p = map[i];
        if (!p) continue;

        // Inside face: Light Grey quad sitting flat on the plane with depth masking disabled for true transparency
        glDepthMask(GL_FALSE);
        glBegin(GL_QUADS);
        glColor4ub((GLubyte)ground_material[0], (GLubyte)ground_material[1], (GLubyte)ground_material[2], (GLubyte)ground_material[3]);
        glVertex3f((float)p->a_vertex[0], (float)p->a_vertex[1], (float)p->a_vertex[2]);
        glVertex3f((float)p->b_vertex[0], (float)p->b_vertex[1], (float)p->b_vertex[2]);
        glVertex3f((float)p->d_vertex[0], (float)p->d_vertex[1], (float)p->d_vertex[2]);
        glVertex3f((float)p->c_vertex[0], (float)p->c_vertex[1], (float)p->c_vertex[2]);
        glEnd();
        glDepthMask(GL_TRUE);

        // Wireframe border lines: White in navigation mode, Red in combat
        glLineWidth(2.5f);
        printer->line((float)p->a_vertex[0], (float)p->a_vertex[1], (float)p->a_vertex[2],
                      (float)p->b_vertex[0], (float)p->b_vertex[1], (float)p->b_vertex[2], material);
        printer->line((float)p->b_vertex[0], (float)p->b_vertex[1], (float)p->b_vertex[2],
                      (float)p->d_vertex[0], (float)p->d_vertex[1], (float)p->d_vertex[2], material);
        printer->line((float)p->d_vertex[0], (float)p->d_vertex[1], (float)p->d_vertex[2],
                      (float)p->d_vertex[0], (float)p->d_vertex[1], (float)p->d_vertex[2], material);
        printer->line((float)p->c_vertex[0], (float)p->c_vertex[1], (float)p->c_vertex[2],
                      (float)p->a_vertex[0], (float)p->a_vertex[1], (float)p->a_vertex[2], material);
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
    print_skeleton();
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
