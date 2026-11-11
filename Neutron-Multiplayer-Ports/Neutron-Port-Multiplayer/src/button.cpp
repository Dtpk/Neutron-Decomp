#include "button.h"
#include "game_data.h"
#include <cstring>
#include <cctype>

button::button()
    : printer(new glinterface()),
      x(0.0f), y(0.0f), z(0.0f),
      size(0.2f),
      left(0), top(0), right(0), bottom(0),
      width(0.0f), height(6.0f),
      visible(false), enabled(false),
      action(-1), action_type(0),
      effect_alpha(0), isFadeing(false),
      key(0) {
    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
    color[3] = 255;
    std::memset(characters, -1, sizeof(characters));
}

button::~button() {
    delete printer;
}

void button::change_color(int r, int g, int b, int a) {
    color[0] = r;
    color[1] = g;
    color[2] = b;
    color[3] = a;
}

void button::show_skeleton() {
    if (!printer) return;
    float l = (float)left, t = (float)top, r = (float)right, b = (float)bottom;
    printer->line(l, t, 0.0f, r, t, 0.0f, color);
    printer->line(r, t, 0.0f, r, b, 0.0f, color);
    printer->line(r, b, 0.0f, l, b, 0.0f, color);
    printer->line(l, b, 0.0f, l, t, 0.0f, color);
}

void button::print_3D(float qx, float qy, float qz, int axis, int polarity) {
    if (!printer) return;
    this->x = qx;
    this->y = qy;
    this->z = qz;

    float char_step = 3.5f * size;
    float cur_offset = 0.0f;

    for (int i = 0; i < 64 && characters[i] >= 0; i++) {
        int ch = characters[i];
        if (ch >= 0 && ch < 49) {
            for (int r = 0; r < 6; r++) {
                for (int c = 0; c < 6; c++) {
                    if (FONT[ch][r][c] != 0) {
                        float px = qx, py = qy, pz = qz;
                        if (axis == 1) {
                            py += (polarity > 0 ? (cur_offset + c * size) : -(cur_offset + c * size));
                            pz += (5 - r) * size;
                        } else if (axis == 2) {
                            px += (polarity > 0 ? (cur_offset + c * size) : -(cur_offset + c * size));
                            pz += (5 - r) * size;
                        } else {
                            px += (polarity > 0 ? (cur_offset + c * size) : -(cur_offset + c * size));
                            py += (5 - r) * size;
                        }
                        printer->square_3D(px, py, pz, color[0], color[1], color[2], color[3], size, axis);
                    }
                }
            }
        }
        cur_offset += char_step;
    }
    this->width = cur_offset;
}

void button::print_2D(float qx, float qy, float qz) {
    if (!printer) return;
    this->x = qx;
    this->y = qy;
    this->z = qz;

    if (isFadeing) {
        int next_a = color[3] + 10;
        if (next_a < effect_alpha) {
            color[3] = next_a;
        } else {
            color[3] = effect_alpha;
            isFadeing = false;
        }
    }

    float char_step = 4.5f * size;
    float cur_offset = 0.0f;

    for (int i = 0; i < 64 && characters[i] >= 0; i++) {
        int ch = characters[i];
        if (ch >= 0 && ch < 49) {
            for (int r = 0; r < 6; r++) {
                for (int c = 0; c < 6; c++) {
                    if (FONT[ch][r][c] != 0) {
                        printer->square(qx + cur_offset + c * size, qy - r * size, qz,
                                        color[0], color[1], color[2], color[3], size * 0.5f);
                    }
                }
            }
        }
        cur_offset += char_step;
    }
    this->width = cur_offset;
}

void button::print(float qx, float qy, float qz, int method) {
    if (method == 0) {
        print_2D(qx, qy, qz);
    } else {
        print_3D(qx, qy, qz, method, 1);
    }
}

void button::load_text(const char* word, int fade_alpha) {
    size_t len = strlen(word);
    height = 6.0f;
    width = (float)(len * 5);

    std::memset(characters, -1, sizeof(characters));

    for (size_t s = 0; s < len && s < 63; s++) {
        char ch = word[s];
        characters[s] = -1;

        if (ch >= 'a' && ch <= 'z') ch = (char)std::toupper(ch);

        switch (ch - 0x20) {
            case 0:    characters[s] = 0x24; break; // ' '
            case 3:    characters[s] = 0x2c; break; // '#'
            case 12:   characters[s] = 0x29; break; // ','
            case 14:   characters[s] = 0x28; break; // '.'
            case 15:   characters[s] = 0x2d; break; // '/'
            case 16: case 17: case 18: case 19: case 20:
            case 21: case 22: case 23: case 24: case 25:
                       characters[s] = 0x1a + (ch - '0'); break; // '0'-'9'
            case 26:   characters[s] = 0x25; break; // ':'
            case 27: case 60: characters[s] = 0x2a; break; // ';' or '\'
            case 28:   characters[s] = 0x26; break; // '<'
            case 29:   characters[s] = 0x2b; break; // '='
            case 30:   characters[s] = 0x27; break; // '>'
            default:
                if (ch >= 'A' && ch <= 'Z') {
                    characters[s] = ch - 'A';
                }
                break;
        }
    }

    if (fade_alpha > 0) {
        isFadeing = true;
        effect_alpha = fade_alpha;
        color[3] = 0;
    }
}
