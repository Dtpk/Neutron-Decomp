#pragma once

#include <string>
#include <map>

// Plain-text save file living next to the executable (same convention as
// music.wav and the planned map.txt/sound assets -- cwd-relative, no platform
// save-data APIs). Keys are explicit and unknown/missing ones are tolerated,
// so the format can gain fields later without breaking old or new saves.
// High scores are keyed by stage NAME (e.g. "cube", "crazy_cross"), not
// index, so custom stages added later via map.txt slot in without the save
// format needing to change.

#define SAVE_DATA_VERSION 1

struct SaveData {
    int version = SAVE_DATA_VERSION;
    int user_color = 0;
    int user_difficulty = 0;
    bool user_sound = true;
    int user_volume = 100;
    int user_stage = 1;
    bool fullscreen = false;
    std::map<std::string, int> high_scores; // sanitized stage name -> score
};

// Sanitizes a stage name into a save-key (lowercase, spaces -> underscores).
std::string save_key_for_stage_name(const char* name);

// Reads the save file if present; returns defaults (version/color/difficulty
// all 0, sound on, stage 1, no high scores) if it's missing or unreadable.
SaveData load_save_data();

// Writes the save file. Safe to call often -- it's a small local text file,
// not a platform memory-card API.
void write_save_data(const SaveData& data);
