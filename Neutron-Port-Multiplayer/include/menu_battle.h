#pragma once

// Decorative CPU battle shown in the main menu's background terrain -- not
// from decomp, a deliberate addition (project owner's request). Deliberately
// kept in its own file: these are the MENU-ONLY decisions (when ships spawn,
// move, die, and respawn). They call into the same shared enemy::move() AI
// and terrain/piece systems real gameplay uses -- duplicating those would
// risk the two drifting out of sync over time, which is a worse problem than
// sharing them -- but nothing in here should ever need a change to
// neutron.cpp's core dispatch, player.cpp, or piece.cpp again. If a future
// tweak to the menu battle's behavior seems to require touching one of those
// shared files instead of just this one, that's a sign the shared system
// itself has a bug worth fixing on its own merits, not something to route
// around from here.

class neutron;

namespace menu_battle {

// Called once from load_stage()'s stg==1000 branch, right after
// autoload_eletrons(2) spawns the two ships. Skips their normal dormant
// "sleeping trigger tile" phase and sets them moving immediately -- there's
// no player on the menu to charge them into combat the normal way.
void spawn(neutron* game);

// Called once per logic tick while the menu is idle (from both
// neutron::wait_option() and neutron::change_option() -- see their own
// comments for why it has to be both). Moves each ship, and on a collision
// (a wall, its own trail, or the other ship's trail) explodes it and starts
// its respawn countdown.
void update(neutron* game);

}  // namespace menu_battle
