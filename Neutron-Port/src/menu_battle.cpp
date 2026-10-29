#include "menu_battle.h"
#include "neutron.h"
#include "enemy.h"
#include "terrain.h"
#include "plane.h"
#include "piece.h"
#include "sound_mixer.h"

#include <cstdlib>

namespace {

void respawn_ship(neutron* game, int slot) {
    if (slot < 0 || slot >= 2 || !game->ground || game->ground->N_PLANES < 1) return;
    enemy* e = game->electron[slot];
    if (!e) return;

    terrain* ground = game->ground;
    int pl = std::rand() % ground->N_PLANES;
    plane* p = ground->map[pl];
    if (!p) {
        for (int k = 0; k < ground->N_PLANES; k++) {
            if (ground->map[k]) { pl = k; p = ground->map[k]; break; }
        }
    }
    if (!p || !p->pieces[0]) return;

    e->start_plane = pl;
    e->actual_plane = pl;
    e->start_actual_face = p->face_number;
    int prev_face = (p->face_number < 6) ? p->face_number + 1 : p->face_number - 1;
    e->start_previous_face = prev_face;
    e->actual_face = p->face_number;
    e->previous_face = prev_face;

    int mid_idx = (p->side_size * p->side_size) / 2;
    piece* mp = p->pieces[mid_idx];
    if (mp) {
        e->x = mp->x;
        e->y = mp->y;
        e->z = mp->z;
    } else {
        float mid = (float)p->side_size * 0.5f;
        e->x = p->pieces[0]->x + mid;
        e->y = p->pieces[0]->y + mid;
        e->z = p->pieces[0]->z + mid;
        if (p->dead_axis == 1) e->x = (float)p->dead_offset;
        if (p->dead_axis == 2) e->y = (float)p->dead_offset;
        if (p->dead_axis == 3) e->z = (float)p->dead_offset;
    }
    e->start_x = (int)e->x;
    e->start_y = (int)e->y;
    e->start_z = (int)e->z;

    int idx = p->find_piece_index(e->x, e->y, e->z);
    if (idx >= 0 && p->pieces[idx]) p->pieces[idx]->restart();

    e->restart();
    e->init(ground->map);
    e->status = 1; // active immediately, no dormant trigger phase on the menu
    // type 0/1 never toggle `active` off in enemy::move() (that's types 2/3,
    // which blink their trail-claiming on and off every move) -- force type 0
    // so the trail claims every single step instead of leaving gaps, making
    // the growing trail read clearly instead of looking like a short,
    // constantly-recycling nav-mode trail.
    e->type = 0;
}

// Wipes every piece this ship currently owns, right away. Decomp's own
// fire_up_cue() just marks pieces dying (status=2) and lets them fade out
// over the normal per-piece aging schedule -- fine for a real stage, but
// for the menu's decorative loop we want the trail to vanish together with
// the explosion, not linger and fade afterward. Menu-only; terrain.cpp's
// fire_up_cue() stays decomp-faithful for real gameplay.
void clear_ship_trail(neutron* game, int ident) {
    if (!game->ground) return;
    for (int i = 0; i < game->ground->N_PLANES; i++) {
        plane* p = game->ground->map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        if (total > 400) total = 400;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc && pc->owner == ident) {
                pc->restart();
                pc->temporal = true; // undo the persistent-trail override below
            }
        }
    }
}

// Decomp's piece::print() (temporal branch) only ever does
// `state = false; prints_counter = 0.0;` once a claimed piece's trail
// segment ages out -- it never resets `status` back off 2 (the
// "dying/abandoned" marker fire_up_cue() sets). Since plane::print()'s
// call gate is `(state || status == 2)`, an abandoned piece just keeps
// re-entering the temporal branch forever: counter climbs 0..5 (visible),
// hits the cutoff, resets to 0, and repeats -- a piece nobody ever drives
// over again flickers indefinitely. In real combat stages this is masked
// because the whole terrain keeps getting reclaimed by moving ships; on
// the menu, with two ships wandering a mostly-empty board for minutes,
// these orphaned pieces pile up across every past ship's lifetime and
// the board fills with stray flickering squares. This finalizes a piece
// the moment its post-death fade naturally finishes (prints_counter has
// already crossed the render cutoff, so this is visually seamless) so it
// actually disappears for good instead of cycling. Menu-only cleanup --
// piece.cpp/terrain.cpp stay untouched and decomp-faithful for real play.
void cleanup_faded_pieces(neutron* game) {
    if (!game->ground) return;
    for (int i = 0; i < game->ground->N_PLANES; i++) {
        plane* p = game->ground->map[i];
        if (!p) continue;
        int total = p->side_size * p->side_size;
        if (total > 400) total = 400;
        for (int j = 0; j < total; j++) {
            piece* pc = p->pieces[j];
            if (pc && pc->status == 2 && !pc->state && pc->prints_counter >= 6.0) {
                pc->restart();
            }
        }
    }
}

}  // namespace

namespace menu_battle {

void spawn(neutron* game) {
    if (!game) return;
    game->autoload_eletrons(2);
    game->menu_battle_respawn[0] = -1;
    game->menu_battle_respawn[1] = -1;
    for (int i = 0; i < game->N_SHIPS - 1; i++) {
        if (game->electron[i]) {
            game->electron[i]->status = 1;
            game->electron[i]->type = 0; // continuous trail claiming, see respawn_ship()
        }
    }
}

void update(neutron* game) {
    if (!game || !game->ground || game->N_SHIPS < 2) return;

    cleanup_faded_pieces(game);

    for (int i = 0; i < game->N_SHIPS - 1 && i < 2; i++) {
        enemy* e = game->electron[i];
        if (!e) continue;

        if (e->status == 6) {
            // Mid-explosion -- enemy::print() drives the fade and flips status
            // to 2 on its own once it's done; nothing to do here but wait.
            continue;
        }
        if (e->status == 2) {
            // Faded out -- hold a beat before respawning so the crash actually
            // reads before a fresh ship pops in.
            if (game->menu_battle_respawn[i] < 0) game->menu_battle_respawn[i] = 45;
            if (game->menu_battle_respawn[i] > 0) {
                game->menu_battle_respawn[i]--;
                continue;
            }
            respawn_ship(game, i);
            game->menu_battle_respawn[i] = -1;
            continue;
        }
        if (e->status != 1) continue;

        if (e->step_counter < e->step_speed) {
            e->step_counter++;
            continue;
        }
        e->step_counter = 0;
        int res = e->move(game->ground->map, 0);
        if (res != 0) {
            e->status = 6;
            e->effect_size = e->size;
            e->effect_alpha = (float)e->material[3];
            clear_ship_trail(game, e->ident);
            game->ground->fire_up_cue(e->ident);
            if (game->mixer) game->mixer->play_effect(4);
        } else {
            // enemy::move()'s claimed piece is still "temporal" -- it ages out
            // on the normal prints_counter schedule like any other trail piece
            // (just delayed by the growing cue_counter offset). That means the
            // oldest segments near spawn still quietly expire while the ship
            // lives, so the trail reads as a long *sliding* navigation-style
            // trail instead of a genuinely persistent combat one. Since the
            // menu already clears the whole trail explicitly on death
            // (clear_ship_trail above), there's no need for per-piece aging at
            // all here -- mark the just-claimed piece non-temporal so it stays
            // solid for the ship's entire life and only goes away on death.
            plane* cur = game->ground->map[e->actual_plane];
            if (cur) {
                int idx = cur->find_piece_index(e->x, e->y, e->z);
                if (idx >= 0 && cur->pieces[idx]) cur->pieces[idx]->temporal = false;
            }
        }
    }
}

}  // namespace menu_battle
