#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "editor.h"
#include "font.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Executable-directory lookup, so "saved next to it" means next to the .exe/
// binary regardless of the directory the user launched it from -- not the
// process's current working directory (which double-click launchers don't
// reliably set).
// ---------------------------------------------------------------------------
static std::string get_executable_dir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0) return ".";
    std::string path(buf, len);
    size_t pos = path.find_last_of("\\/");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return ".";
    buf[len] = '\0';
    std::string path(buf);
    size_t pos = path.find_last_of('/');
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
#endif
}

// ---------------------------------------------------------------------------
// Text rendering -- same 6x6 glyph layout/mapping as neutron-port's
// draw_hud_text(), just reimplemented standalone against this tool's own
// top-down (y increases downward) 2D ortho space.
// ---------------------------------------------------------------------------
static int glyph_index(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= '0' && ch <= '9') return 26 + (ch - '0');
    switch (ch) {
        case ' ': return 36;
        case ':': return 37;
        case '<': return 38;
        case '>': return 39;
        case '.': return 40;
        case ',': return 41;
        case ';': return 42;
        case '=': return 43;
        case '#': return 44;
        case '/': return 45;
        case '[': return 38;
        case ']': return 39;
        case '-': return 43;
        default: return -1;
    }
}

static void draw_text(const char* text, float x, float y, float size,
                       int r, int g, int b, int a) {
    float cur_x = x;
    float step = size * 6.0f;
    float pixel = size * 0.5f;
    for (int i = 0; text[i] != '\0'; i++) {
        int gi = glyph_index(text[i]);
        if (gi >= 0 && gi < 49) {
            for (int row = 0; row < 6; row++) {
                for (int col = 0; col < 6; col++) {
                    if (FONT[gi][row][col] == 0) continue;
                    float px = cur_x + col * size;
                    float py = y + row * size;
                    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
                    glBegin(GL_QUADS);
                    glVertex2f(px - pixel, py - pixel);
                    glVertex2f(px + pixel, py - pixel);
                    glVertex2f(px + pixel, py + pixel);
                    glVertex2f(px - pixel, py + pixel);
                    glEnd();
                }
            }
        }
        cur_x += step;
    }
}

static float text_width(const char* text, float size) {
    return (float)std::strlen(text) * size * 6.0f;
}

// ---------------------------------------------------------------------------
// Simple immediate-mode UI primitives (no external GUI library -- keeps the
// build dependency-free: just SDL2 + OpenGL, same stack the main game uses).
// ---------------------------------------------------------------------------
struct Rect {
    float x, y, w, h;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

static void draw_rect_filled(Rect r, int cr, int cg, int cb, int ca) {
    glColor4ub((GLubyte)cr, (GLubyte)cg, (GLubyte)cb, (GLubyte)ca);
    glBegin(GL_QUADS);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.w, r.y);
    glVertex2f(r.x + r.w, r.y + r.h);
    glVertex2f(r.x, r.y + r.h);
    glEnd();
}

static void draw_rect_outline(Rect r, int cr, int cg, int cb, int ca) {
    glColor4ub((GLubyte)cr, (GLubyte)cg, (GLubyte)cb, (GLubyte)ca);
    glBegin(GL_LINE_LOOP);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.w, r.y);
    glVertex2f(r.x + r.w, r.y + r.h);
    glVertex2f(r.x, r.y + r.h);
    glEnd();
}

struct Button {
    Rect rect;
    const char* label;
};

static bool button_draw_and_hit(const Button& b, bool mouse_down_event, float mx, float my) {
    bool hover = b.rect.contains(mx, my);
    draw_rect_filled(b.rect, hover ? 70 : 45, hover ? 70 : 45, hover ? 80 : 50, 255);
    draw_rect_outline(b.rect, 120, 200, 220, 255);
    float tw = text_width(b.label, 1.3f);
    draw_text(b.label, b.rect.x + (b.rect.w - tw) * 0.5f, b.rect.y + b.rect.h * 0.5f - 4.0f,
              1.3f, 230, 240, 255, 255);
    return hover && mouse_down_event;
}

// ---------------------------------------------------------------------------
// 3D cube rendering -- same shape/winding as neutron-port's
// glinterface::solid_cube()/cube_skeleton(), reimplemented standalone.
// ---------------------------------------------------------------------------
static void draw_cube_solid(float x, float y, float z, float s, int r, int g, int b, int a) {
    float n = -s;
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glBegin(GL_QUADS);
    glVertex3f(n, n, s); glVertex3f(s, n, s); glVertex3f(s, s, s); glVertex3f(n, s, s);
    glVertex3f(n, n, n); glVertex3f(n, s, n); glVertex3f(s, s, n); glVertex3f(s, n, n);
    glVertex3f(n, s, n); glVertex3f(n, s, s); glVertex3f(s, s, s); glVertex3f(s, s, n);
    glVertex3f(n, n, n); glVertex3f(s, n, n); glVertex3f(s, n, s); glVertex3f(n, n, s);
    glVertex3f(s, n, n); glVertex3f(s, s, n); glVertex3f(s, s, s); glVertex3f(s, n, s);
    glVertex3f(n, n, n); glVertex3f(n, n, s); glVertex3f(n, s, s); glVertex3f(n, s, n);
    glEnd();
    glPopMatrix();
}

static void draw_cube_wire(float x, float y, float z, float s, int r, int g, int b, int a) {
    float box[72] = {
        -s, -s, -s,  s, -s, -s,   s, -s, -s,  s, -s,  s,   s, -s,  s, -s, -s,  s,  -s, -s,  s, -s, -s, -s,
        -s,  s, -s,  s,  s, -s,   s,  s, -s,  s,  s,  s,   s,  s,  s, -s,  s,  s,  -s,  s,  s, -s,  s, -s,
        -s, -s, -s, -s,  s, -s,   s, -s, -s,  s,  s, -s,   s, -s,  s,  s,  s,  s,  -s, -s,  s, -s,  s,  s
    };
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor4ub((GLubyte)r, (GLubyte)g, (GLubyte)b, (GLubyte)a);
    glVertexPointer(3, GL_FLOAT, 0, box);
    glEnableClientState(GL_VERTEX_ARRAY);
    glDrawArrays(GL_LINES, 0, 24);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------
struct OrbitCamera {
    float yaw = -35.0f;
    float pitch = 25.0f;
    float distance = 34.0f;
};

static const int kGridCells = (2 * kGridHalfExtent / kGridStep) + 1; // 13

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    const int WIN_W = 1280, WIN_H = 800;
    SDL_Window* window = SDL_CreateWindow(
        "Neutron Custom Map Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.08f, 0.08f, 0.09f, 1.0f);

    MapEditor editor;
    std::string exe_dir = get_executable_dir();
    std::string map_path = exe_dir + "/map.txt";
    bool loaded_existing = editor.load(map_path);

    OrbitCamera cam;
    bool orbiting = false;
    int last_mouse_x = 0, last_mouse_y = 0;

    bool name_focused = false;
    std::string status_msg = loaded_existing ? "Loaded existing map.txt" : "New map";
    Uint32 status_until = SDL_GetTicks() + 2500;

    int win_w = WIN_W, win_h = WIN_H;

    // --- Static layout ---
    const float sidebar_w = 260.0f;
    const float grid_pane_w = 440.0f;

    bool running = true;
    while (running) {
        int mx = 0, my = 0;
        SDL_GetMouseState(&mx, &my);

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                win_w = ev.window.data1;
                win_h = ev.window.data2;
            }

            if (ev.type == SDL_TEXTINPUT && name_focused) {
                if (editor.name.size() < 31) editor.name += ev.text.text;
            }

            if (ev.type == SDL_KEYDOWN) {
                if (name_focused && ev.key.keysym.sym == SDLK_BACKSPACE && !editor.name.empty()) {
                    editor.name.pop_back();
                }
                if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_ESCAPE) {
                    if (name_focused) { name_focused = false; SDL_StopTextInput(); }
                }
                if (!name_focused) {
                    if (ev.key.keysym.sym == SDLK_PAGEUP || ev.key.keysym.sym == SDLK_RIGHTBRACKET) {
                        if (editor.current_layer_y < kGridHalfExtent) editor.current_layer_y += kGridStep;
                    }
                    if (ev.key.keysym.sym == SDLK_PAGEDOWN || ev.key.keysym.sym == SDLK_LEFTBRACKET) {
                        if (editor.current_layer_y > -kGridHalfExtent) editor.current_layer_y -= kGridStep;
                    }
                }
            }

            if (ev.type == SDL_MOUSEWHEEL) {
                if (mx > (int)(sidebar_w + grid_pane_w)) {
                    cam.distance -= (float)ev.wheel.y * 2.0f;
                    if (cam.distance < 8.0f) cam.distance = 8.0f;
                    if (cam.distance > 120.0f) cam.distance = 120.0f;
                }
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                float fx = (float)mx, fy = (float)my;
                bool hit_widget = false;

                // Name field
                Rect name_rect{20, 50, sidebar_w - 40, 32};
                if (name_rect.contains(fx, fy)) {
                    name_focused = true;
                    SDL_StartTextInput();
                    hit_widget = true;
                } else if (name_focused) {
                    name_focused = false;
                    SDL_StopTextInput();
                }

                // Sidebar buttons (rects recomputed to match the draw pass below)
                Button btn_save{{20, 110, sidebar_w - 40, 36}, "SAVE MAP.TXT"};
                Button btn_load{{20, 155, sidebar_w - 40, 36}, "RELOAD MAP.TXT"};
                Button btn_clear{{20, 200, sidebar_w - 40, 36}, "CLEAR ALL CUBES"};
                Button btn_layer_up{{20, 270, (sidebar_w - 50) * 0.5f, 32}, "LAYER +"};
                Button btn_layer_down{{30 + (sidebar_w - 50) * 0.5f, 270, (sidebar_w - 50) * 0.5f, 32}, "LAYER -"};

                if (btn_save.rect.contains(fx, fy)) {
                    hit_widget = true;
                    if (editor.cubes.empty()) {
                        status_msg = "Add at least one cube first";
                    } else {
                        bool ok = editor.save(map_path);
                        status_msg = ok ? "Saved map.txt" : "Save failed (check permissions)";
                    }
                    status_until = SDL_GetTicks() + 2500;
                }
                if (btn_load.rect.contains(fx, fy)) {
                    hit_widget = true;
                    bool ok = editor.load(map_path);
                    status_msg = ok ? "Loaded map.txt" : "No valid map.txt found";
                    status_until = SDL_GetTicks() + 2500;
                }
                if (btn_clear.rect.contains(fx, fy)) {
                    hit_widget = true;
                    editor.clear();
                    status_msg = "Cleared";
                    status_until = SDL_GetTicks() + 2500;
                }
                if (btn_layer_up.rect.contains(fx, fy)) {
                    hit_widget = true;
                    if (editor.current_layer_y < kGridHalfExtent) editor.current_layer_y += kGridStep;
                }
                if (btn_layer_down.rect.contains(fx, fy)) {
                    hit_widget = true;
                    if (editor.current_layer_y > -kGridHalfExtent) editor.current_layer_y -= kGridStep;
                }

                // 2D grid editor click -> toggle cube
                Rect grid_area{sidebar_w + 40, 70, 360, 360};
                if (!hit_widget && grid_area.contains(fx, fy)) {
                    float cell = grid_area.w / (float)kGridCells;
                    int col = (int)((fx - grid_area.x) / cell);
                    int row = (int)((fy - grid_area.y) / cell);
                    if (col >= 0 && col < kGridCells && row >= 0 && row < kGridCells) {
                        int wx = -kGridHalfExtent + col * kGridStep;
                        int wz = -kGridHalfExtent + row * kGridStep;
                        if (!editor.toggle_cube(wx, editor.current_layer_y, wz)) {
                            status_msg = "Cube limit reached (20 max)";
                            status_until = SDL_GetTicks() + 2500;
                        }
                    }
                    hit_widget = true;
                }

                // 3D pane -> start orbit drag
                if (!hit_widget && fx > sidebar_w + grid_pane_w) {
                    orbiting = true;
                    last_mouse_x = mx;
                    last_mouse_y = my;
                }
            }
            if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
                orbiting = false;
            }
            if (ev.type == SDL_MOUSEMOTION && orbiting) {
                int dx = mx - last_mouse_x;
                int dy = my - last_mouse_y;
                last_mouse_x = mx;
                last_mouse_y = my;
                cam.yaw += (float)dx * 0.4f;
                cam.pitch -= (float)dy * 0.4f;
                if (cam.pitch > 85.0f) cam.pitch = 85.0f;
                if (cam.pitch < -85.0f) cam.pitch = -85.0f;
            }
        }

        glViewport(0, 0, win_w, win_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---- 3D preview pane ----
        float preview_x = sidebar_w + grid_pane_w;
        float preview_w = (float)win_w - preview_x;
        if (preview_w > 1.0f) {
            glViewport((GLint)preview_x, 0, (GLsizei)preview_w, (GLsizei)win_h);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluPerspective(50.0, (double)preview_w / (double)win_h, 1.0, 400.0);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glTranslatef(0.0f, 0.0f, -cam.distance);
            glRotatef(cam.pitch, 1.0f, 0.0f, 0.0f);
            glRotatef(cam.yaw, 0.0f, 1.0f, 0.0f);

            glEnable(GL_DEPTH_TEST);

            // Reference grid on the floor for orientation.
            glColor4ub(60, 60, 70, 255);
            glBegin(GL_LINES);
            for (int i = -kGridHalfExtent; i <= kGridHalfExtent; i += kGridStep) {
                glVertex3f((float)i, -kGridHalfExtent - 2.0f, -(float)kGridHalfExtent);
                glVertex3f((float)i, -kGridHalfExtent - 2.0f, (float)kGridHalfExtent);
                glVertex3f(-(float)kGridHalfExtent, -kGridHalfExtent - 2.0f, (float)i);
                glVertex3f((float)kGridHalfExtent, -kGridHalfExtent - 2.0f, (float)i);
            }
            glEnd();

            // Translucent plane marking the layer currently shown in the 2D editor.
            // Skipped on an empty canvas -- at full grid extent it reads as a
            // solid wall rather than a faint guide when there's nothing to
            // compare it against yet.
            if (!editor.cubes.empty()) {
                glDisable(GL_DEPTH_TEST);
                glColor4ub(80, 180, 220, 18);
                glBegin(GL_QUADS);
                glVertex3f(-(float)kGridHalfExtent, (float)editor.current_layer_y, -(float)kGridHalfExtent);
                glVertex3f((float)kGridHalfExtent, (float)editor.current_layer_y, -(float)kGridHalfExtent);
                glVertex3f((float)kGridHalfExtent, (float)editor.current_layer_y, (float)kGridHalfExtent);
                glVertex3f(-(float)kGridHalfExtent, (float)editor.current_layer_y, (float)kGridHalfExtent);
                glEnd();
                glEnable(GL_DEPTH_TEST);
            }

            for (const Cube& c : editor.cubes) {
                bool on_layer = (c.y == editor.current_layer_y);
                draw_cube_solid((float)c.x, (float)c.y, (float)c.z, 1.0f,
                                 on_layer ? 255 : 150, on_layer ? 255 : 170, on_layer ? 255 : 220, 255);
                draw_cube_wire((float)c.x, (float)c.y, (float)c.z, 1.02f, 0, 230, 255, 255);
            }
        }

        // ---- 2D overlay (sidebar + grid editor + text) ----
        glViewport(0, 0, win_w, win_h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, win_w, win_h, 0.0, -1.0, 1.0); // y-down, matches SDL mouse space
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);

        // Sidebar background
        draw_rect_filled({0, 0, sidebar_w, (float)win_h}, 24, 24, 28, 255);
        draw_text("NEUTRON MAP EDITOR", 20, 20, 1.4f, 0, 255, 255, 255);

        // Name field
        Rect name_rect{20, 50, sidebar_w - 40, 32};
        draw_rect_filled(name_rect, name_focused ? 40 : 30, name_focused ? 55 : 30, name_focused ? 60 : 35, 255);
        draw_rect_outline(name_rect, name_focused ? 0 : 100, name_focused ? 255 : 100, name_focused ? 255 : 110, 255);
        {
            std::string shown = editor.name;
            if (name_focused && (SDL_GetTicks() / 500) % 2 == 0) shown += "_";
            draw_text(shown.c_str(), name_rect.x + 6, name_rect.y + 8, 1.2f, 255, 255, 255, 255);
        }

        bool clicked = false; // buttons already consumed their click in the event loop above;
                               // this pass only redraws them each frame.
        Button btn_save{{20, 110, sidebar_w - 40, 36}, "SAVE MAP.TXT"};
        Button btn_load{{20, 155, sidebar_w - 40, 36}, "RELOAD MAP.TXT"};
        Button btn_clear{{20, 200, sidebar_w - 40, 36}, "CLEAR ALL CUBES"};
        button_draw_and_hit(btn_save, clicked, (float)mx, (float)my);
        button_draw_and_hit(btn_load, clicked, (float)mx, (float)my);
        button_draw_and_hit(btn_clear, clicked, (float)mx, (float)my);

        char layer_buf[32];
        std::snprintf(layer_buf, sizeof(layer_buf), "LAYER Y = %d", editor.current_layer_y);
        draw_text(layer_buf, 20, 250, 1.2f, 255, 255, 0, 255);
        Button btn_layer_up{{20, 270, (sidebar_w - 50) * 0.5f, 32}, "LAYER +"};
        Button btn_layer_down{{30 + (sidebar_w - 50) * 0.5f, 270, (sidebar_w - 50) * 0.5f, 32}, "LAYER -"};
        button_draw_and_hit(btn_layer_up, clicked, (float)mx, (float)my);
        button_draw_and_hit(btn_layer_down, clicked, (float)mx, (float)my);

        char count_buf[32];
        std::snprintf(count_buf, sizeof(count_buf), "CUBES: %d OF %d", (int)editor.cubes.size(), kMaxCubes);
        draw_text(count_buf, 20, 320, 1.3f,
                  editor.cubes.empty() ? 255 : 220,
                  editor.cubes.empty() ? 80 : 255,
                  editor.cubes.empty() ? 80 : 220, 255);

        bool connected = editor.is_fully_connected();
        draw_text(connected ? "SHAPE: CONNECTED" : "SHAPE: DISCONNECTED",
                   20, 345, 1.1f,
                   connected ? 120 : 255, connected ? 255 : 120, 120, 255);

        draw_text("LEFT GRID: CLICK TO", 20, 400, 1.0f, 170, 170, 180, 255);
        draw_text("TOGGLE A CUBE ON THE", 20, 418, 1.0f, 170, 170, 180, 255);
        draw_text("CURRENT LAYER.", 20, 436, 1.0f, 170, 170, 180, 255);
        draw_text("RIGHT VIEW: DRAG TO", 20, 462, 1.0f, 170, 170, 180, 255);
        draw_text("ORBIT, SCROLL TO ZOOM.", 20, 480, 1.0f, 170, 170, 180, 255);
        draw_text("PGUP:PGDN CHANGE LAYER", 20, 506, 1.0f, 170, 170, 180, 255);

        if (SDL_GetTicks() < status_until) {
            draw_text(status_msg.c_str(), 20, (float)win_h - 40, 1.2f, 255, 255, 0, 255);
        }

        // 2D grid editor
        Rect grid_area{sidebar_w + 40, 70, 360, 360};
        draw_rect_filled(grid_area, 18, 18, 22, 255);
        float cell = grid_area.w / (float)kGridCells;
        for (int row = 0; row < kGridCells; row++) {
            for (int col = 0; col < kGridCells; col++) {
                int wx = -kGridHalfExtent + col * kGridStep;
                int wz = -kGridHalfExtent + row * kGridStep;
                Rect cr{grid_area.x + col * cell, grid_area.y + row * cell, cell - 1, cell - 1};
                bool filled = editor.has_cube(wx, editor.current_layer_y, wz);
                bool is_origin = (wx == 0 && wz == 0);
                if (filled) {
                    draw_rect_filled(cr, 0, 220, 255, 255);
                } else {
                    draw_rect_filled(cr, is_origin ? 35 : 26, is_origin ? 35 : 26, is_origin ? 45 : 30, 255);
                }
            }
        }
        draw_rect_outline(grid_area, 100, 200, 220, 255);
        draw_text("CURRENT LAYER (TOP-DOWN)", sidebar_w + 40, 48, 1.1f, 0, 255, 255, 255);
        draw_text("X -->", sidebar_w + 40, grid_area.y + grid_area.h + 10, 1.0f, 150, 150, 160, 255);
        draw_text("Z", sidebar_w + 12, grid_area.y + grid_area.h * 0.5f, 1.0f, 150, 150, 160, 255);

        SDL_GL_SwapWindow(window);
        SDL_Delay(1);
    }

    SDL_GL_DeleteContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
