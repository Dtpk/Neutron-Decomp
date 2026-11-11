#include "save_data.h"
#include <cstdio>
#include <cctype>

static const char* SAVE_FILE_PATH = "neutron_save.txt";

std::string save_key_for_stage_name(const char* name) {
    std::string key;
    if (!name) return key;
    for (const char* p = name; *p; p++) {
        char c = *p;
        if (c == ' ') key += '_';
        else key += (char)std::tolower((unsigned char)c);
    }
    return key;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

SaveData load_save_data() {
    SaveData data; // defaults: version/color/difficulty 0, sound on, stage 1, no scores

    std::FILE* f = std::fopen(SAVE_FILE_PATH, "r");
    if (!f) return data; // no save yet -- defaults are correct for a fresh install

    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        std::string s(line);
        if (s.empty() || s[0] == '#') continue;
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(s.substr(0, eq));
        std::string val = trim(s.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        // Unknown keys (from a newer save format) are silently ignored --
        // forward compatibility. Keys this version doesn't see just keep
        // their struct defaults -- backward compatibility.
        if (key == "version") {
            data.version = std::atoi(val.c_str());
        } else if (key == "user_color") {
            data.user_color = std::atoi(val.c_str());
        } else if (key == "user_difficulty") {
            data.user_difficulty = std::atoi(val.c_str());
        } else if (key == "user_sound") {
            data.user_sound = std::atoi(val.c_str()) != 0;
        } else if (key == "user_volume") {
            data.user_volume = std::atoi(val.c_str());
        } else if (key == "user_stage") {
            data.user_stage = std::atoi(val.c_str());
        } else if (key == "fullscreen") {
            data.fullscreen = std::atoi(val.c_str()) != 0;
        } else if (key.rfind("score.", 0) == 0) {
            data.high_scores[key.substr(6)] = std::atoi(val.c_str());
        }
    }
    std::fclose(f);

    if (data.user_stage < 1) data.user_stage = 1;
    if (data.user_volume < 0) data.user_volume = 0;
    if (data.user_volume > 100) data.user_volume = 100;
    return data;
}

void write_save_data(const SaveData& data) {
    std::FILE* f = std::fopen(SAVE_FILE_PATH, "w");
    if (!f) return; // read-only directory or similar -- fail silently, don't crash play

    std::fprintf(f, "# Neutron save data -- plain text, safe to hand-edit\n");
    std::fprintf(f, "version=%d\n", data.version);
    std::fprintf(f, "user_color=%d\n", data.user_color);
    std::fprintf(f, "user_difficulty=%d\n", data.user_difficulty);
    std::fprintf(f, "user_sound=%d\n", data.user_sound ? 1 : 0);
    std::fprintf(f, "user_volume=%d\n", data.user_volume);
    std::fprintf(f, "user_stage=%d\n", data.user_stage);
    std::fprintf(f, "fullscreen=%d\n", data.fullscreen ? 1 : 0);
    for (const auto& kv : data.high_scores) {
        std::fprintf(f, "score.%s=%d\n", kv.first.c_str(), kv.second);
    }
    std::fclose(f);
}
