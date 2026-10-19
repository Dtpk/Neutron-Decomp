#include "piece.h"

piece::piece()
    : ident(-1), x(0.0f), y(0.0f), z(0.0f), size(0.3f),
      printer(new glinterface()), type(0), state(false),
      owner(-2), temporal(false), prints_counter(0.0),
      effect_size(0.3f), effect_alpha(180.0f), status(0) {
    material[0] = 0xff;
    material[1] = 0xff;
    material[2] = 0xff;
    material[3] = 200;
}

piece::~piece() {
    delete printer;
}

void piece::change_state(const int* mat, int t) {
    material[0] = mat[0];
    material[1] = mat[1];
    material[2] = mat[2];
    material[3] = mat[3];
    state = true;
    status = 1;
    type = t;
}

void piece::restart() {
    owner = -2;
    material[0] = 0xff;
    material[1] = 0xff;
    material[2] = 0xff;
    material[3] = 200;
    effect_size = 0.3f;
    effect_alpha = 180.0f;
    size = 0.3f;
    state = false;
    prints_counter = 0.0;
    type = 0;
    status = 0;
}

void piece::print(int axis, bool low_quality) {
    if (!printer) return;

    if (!temporal) {
        if (status != 2) {
            if (low_quality) {
                printer->cube(x, y, z, material[0], material[1], material[2], 0xff, size);
            } else {
                printer->solid_cube(x, y, z, material[0], material[1], material[2], material[3], size);
            }
            return;
        }

        printer->solid_cube(x, y, z, material[0], material[1], material[2], (int)effect_alpha, effect_size);
        if (effect_size < 0.6f) {
            effect_size += 0.1f;
        }
        if (effect_alpha <= 0.0f) {
            restart();
            return;
        }
        effect_alpha -= 5.0f;
    } else {
        if (prints_counter <= 6.0) {
            prints_counter += 1.0;
            printer->solid_cube(x, y, z, material[0], material[1], material[2], material[3], size);
            return;
        }
        state = false;
        prints_counter = 0.0;
    }
}
