#include <stdio.h>
#include <string.h>

#include "bettercamera.h"
#include "first_person_cam.h"
#include "hardcoded.h"
#include "level_info.h"
#include "level_update.h"
#include "pc/configfile.h"
#include "engine/surface_collision.h"

NewCamera gNewCamera = { 0 };
struct FirstPersonCamera gFirstPersonCamera = { .enabled = false, .fov = FIRST_PERSON_DEFAULT_FOV };
struct LevelValues gLevelValues = {
    .fixCollisionBugs = 0,
    .fixCollisionBugsRoundedCorners = 1,
    .entryLevel = LEVEL_CASTLE_GROUNDS,
    .pssSlideStarTime = 630,
    .metalCapDuration = 600,
    .metalCapDurationCotmc = 600,
    .vanishCapDurationVcutm = 600,
    .floorLowerLimit = FLOOR_LOWER_LIMIT,
    .floorLowerLimitMisc = FLOOR_LOWER_LIMIT_MISC,
    .floorLowerLimitShadow = (s16)FLOOR_LOWER_LIMIT_SHADOW,
};
s16 gDelayedInitSound = -1;

void newcam_init_settings(void) {
    gNewCamera.isActive = configEnableFreeCamera;
    gNewCamera.LCentering = configFreeCameraLCentering;
}

void romhack_camera_init_settings(void) {
    // Keep deterministic state with current compatibility camera implementation.
    if (configEnableRomhackCamera != 0) {
        gNewCamera.isActive = false;
    }
}

bool first_person_check_cancels(struct MarioState *m) {
    (void)m;
    return false;
}

bool get_first_person_enabled(void) {
    return gFirstPersonCamera.enabled;
}

void set_first_person_enabled(bool enable) {
    gFirstPersonCamera.enabled = enable;
}

void first_person_update(void) {
}

void first_person_reset(void) {
    gFirstPersonCamera.enabled = false;
    gFirstPersonCamera.fov = FIRST_PERSON_DEFAULT_FOV;
}

void stop_demo(void *caller) {
    (void)caller;
}

void fake_lvl_init_from_save_file(void) {
    lvl_init_from_save_file(0, gLevelValues.entryLevel);
}

void play_character_sound(struct MarioState *m, s32 soundId) {
    (void)m;
    (void)soundId;
}

void **get_course_name_table(void) { return NULL; }
void **get_course_name_table_original(void) { return NULL; }
void **get_act_name_table(void) { return NULL; }
void **get_act_name_table_original(void) { return NULL; }

u8 *convert_string_ascii_to_sm64(u8 *str64, const char *strAscii, bool menu) {
    (void)menu;
    if (str64 == NULL || strAscii == NULL) { return str64; }
    size_t len = strlen(strAscii);
    for (size_t i = 0; i < len; i++) { str64[i] = (u8)strAscii[i]; }
    str64[len] = 0xFF;
    return str64;
}

char *convert_string_sm64_to_ascii(char *strAscii, const u8 *str64) {
    if (strAscii == NULL || str64 == NULL) { return strAscii; }
    size_t i = 0;
    while (str64[i] != 0xFF && i < 255) {
        strAscii[i] = (char)str64[i];
        i++;
    }
    strAscii[i] = '\0';
    return strAscii;
}

const char *get_level_name_ascii(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase) {
    (void)courseNum; (void)levelNum; (void)areaIndex; (void)charCase;
    return "Level";
}

const u8 *get_level_name_sm64(s16 courseNum, s16 levelNum, s16 areaIndex, s16 charCase) {
    (void)courseNum; (void)levelNum; (void)areaIndex; (void)charCase;
    static u8 sName[] = { 'L', 'e', 'v', 'e', 'l', 0xFF };
    return sName;
}

const char *get_level_name(s16 courseNum, s16 levelNum, s16 areaIndex) {
    (void)courseNum; (void)levelNum; (void)areaIndex;
    return "level";
}

const char *get_star_name_ascii(s16 courseNum, s16 starNum, s16 charCase) {
    (void)courseNum; (void)starNum; (void)charCase;
    return "Star";
}

const u8 *get_star_name_sm64(s16 courseNum, s16 starNum, s16 charCase) {
    (void)courseNum; (void)starNum; (void)charCase;
    static u8 sName[] = { 'S', 't', 'a', 'r', 0xFF };
    return sName;
}

const char *get_star_name(s16 courseNum, s16 starNum) {
    (void)courseNum; (void)starNum;
    return "star";
}
