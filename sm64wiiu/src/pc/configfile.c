// configfile.c - handles loading and saving the configuration options
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "configfile.h"
#include "fs/fs.h"
#include "mods/mods.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_STRING,
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        float *floatValue;
        char *stringValue;
    };
    int maxStringLength;
};

struct FunctionConfigOption {
    const char *name;
    void (*read)(const char *value);
    void (*write)(FILE *file);
};

/*
 *Config options and default values
 */
bool configFullscreen            = false;
ConfigWindow configWindow        = {
    .x = 0,
    .y = 0,
    .w = 640,
    .h = 480,
    .vsync = true,
    .reset = false,
    .fullscreen = false,
    .exiting_fullscreen = false,
    .settings_changed = false,
    .msaa = 0,
};
ConfigStick configStick = { 0 };
// Keyboard mappings (scancode values)
unsigned int configKeyA[MAX_BINDS]          = { 0x26, 0, 0 };
unsigned int configKeyB[MAX_BINDS]          = { 0x33, 0, 0 };
unsigned int configKeyStart[MAX_BINDS]      = { 0x39, 0, 0 };
unsigned int configKeyL[MAX_BINDS]          = { 0x2A, 0, 0 };
unsigned int configKeyR[MAX_BINDS]          = { 0x36, 0, 0 };
unsigned int configKeyZ[MAX_BINDS]          = { 0x25, 0, 0 };
unsigned int configKeyCUp[MAX_BINDS]        = { 0x148, 0, 0 };
unsigned int configKeyCDown[MAX_BINDS]      = { 0x150, 0, 0 };
unsigned int configKeyCLeft[MAX_BINDS]      = { 0x14B, 0, 0 };
unsigned int configKeyCRight[MAX_BINDS]     = { 0x14D, 0, 0 };
unsigned int configKeyStickUp[MAX_BINDS]    = { 0x11, 0, 0 };
unsigned int configKeyStickDown[MAX_BINDS]  = { 0x1F, 0, 0 };
unsigned int configKeyStickLeft[MAX_BINDS]  = { 0x1E, 0, 0 };
unsigned int configKeyStickRight[MAX_BINDS] = { 0x20, 0, 0 };
unsigned int configKeyX[MAX_BINDS]          = { 0x17, 0, 0 };
unsigned int configKeyY[MAX_BINDS]          = { 0x32, 0, 0 };
unsigned int configKeyChat[MAX_BINDS]       = { 0x1C, 0, 0 };
unsigned int configKeyPlayerList[MAX_BINDS] = { 0x0F, 0, 0 };
unsigned int configKeyDUp[MAX_BINDS]        = { 0x147, 0, 0 };
unsigned int configKeyDDown[MAX_BINDS]      = { 0x14F, 0, 0 };
unsigned int configKeyDLeft[MAX_BINDS]      = { 0x153, 0, 0 };
unsigned int configKeyDRight[MAX_BINDS]     = { 0x151, 0, 0 };
unsigned int configKeyConsole[MAX_BINDS]    = { 0x29, 0, 0 };
unsigned int configKeyPrevPage[MAX_BINDS]   = { 0x16, 0, 0 };
unsigned int configKeyNextPage[MAX_BINDS]   = { 0x18, 0, 0 };
unsigned int configKeyDisconnect[MAX_BINDS] = { 0, 0, 0 };
#ifdef TARGET_WII_U
bool configN64FaceButtons = 0;
#endif

static const struct ConfigOption options[] = {
    // Window/display
    {.name = "fullscreen",         .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.fullscreen},
    {.name = "vsync",              .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.vsync},
    {.name = "msaa",               .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.msaa},
    {.name = "texture_filtering",  .type = CONFIG_TYPE_UINT, .uintValue = &configFiltering},
    {.name = "show_fps",           .type = CONFIG_TYPE_BOOL, .boolValue = &configShowFPS},
    {.name = "show_ping",          .type = CONFIG_TYPE_BOOL, .boolValue = &configShowPing},
    {.name = "framerate_mode",     .type = CONFIG_TYPE_UINT, .uintValue = (unsigned int *)&configFramerateMode},
    {.name = "frame_limit",        .type = CONFIG_TYPE_UINT, .uintValue = &configFrameLimit},
    {.name = "interpolation_mode", .type = CONFIG_TYPE_UINT, .uintValue = &configInterpolationMode},
    {.name = "coop_draw_distance", .type = CONFIG_TYPE_UINT, .uintValue = &configDrawDistance},
    {.name = "force_4by3",         .type = CONFIG_TYPE_BOOL, .boolValue = &configForce4By3},

    // Sound
    {.name = "master_volume",       .type = CONFIG_TYPE_UINT, .uintValue = &configMasterVolume},
    {.name = "music_volume",        .type = CONFIG_TYPE_UINT, .uintValue = &configMusicVolume},
    {.name = "sfx_volume",          .type = CONFIG_TYPE_UINT, .uintValue = &configSfxVolume},
    {.name = "env_volume",          .type = CONFIG_TYPE_UINT, .uintValue = &configEnvVolume},
    {.name = "fade_distant_sounds", .type = CONFIG_TYPE_BOOL, .boolValue = &configFadeoutDistantSounds},
    {.name = "mute_focus_loss",     .type = CONFIG_TYPE_BOOL, .boolValue = &configMuteFocusLoss},

    // Controls / camera
    {.name = "stick_deadzone",                 .type = CONFIG_TYPE_UINT, .uintValue = &configStickDeadzone},
    {.name = "rumble_strength",                .type = CONFIG_TYPE_UINT, .uintValue = &configRumbleStrength},
    {.name = "gamepad_number",                 .type = CONFIG_TYPE_UINT, .uintValue = &configGamepadNumber},
    {.name = "background_gamepad",             .type = CONFIG_TYPE_BOOL, .boolValue = &configBackgroundGamepad},
    {.name = "disable_gamepads",               .type = CONFIG_TYPE_BOOL, .boolValue = &configDisableGamepads},
    {.name = "use_standard_key_bindings_chat", .type = CONFIG_TYPE_BOOL, .boolValue = &configUseStandardKeyBindingsChat},
    {.name = "stick_rotate_left",              .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.rotateLeft},
    {.name = "stick_invert_left_x",            .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.invertLeftX},
    {.name = "stick_invert_left_y",            .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.invertLeftY},
    {.name = "stick_rotate_right",             .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.rotateRight},
    {.name = "stick_invert_right_x",           .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.invertRightX},
    {.name = "stick_invert_right_y",           .type = CONFIG_TYPE_BOOL, .boolValue = &configStick.invertRightY},
    {.name = "smooth_scrolling",               .type = CONFIG_TYPE_BOOL, .boolValue = &configSmoothScrolling},
    {.name = "bettercam_enable",               .type = CONFIG_TYPE_BOOL, .boolValue = &configEnableFreeCamera},
    {.name = "bettercam_analog",               .type = CONFIG_TYPE_BOOL, .boolValue = &configFreeCameraAnalog},
    {.name = "bettercam_centering",            .type = CONFIG_TYPE_BOOL, .boolValue = &configFreeCameraLCentering},
    {.name = "bettercam_dpad",                 .type = CONFIG_TYPE_BOOL, .boolValue = &configFreeCameraDPadBehavior},
    {.name = "bettercam_collision",            .type = CONFIG_TYPE_BOOL, .boolValue = &configFreeCameraHasCollision},
    {.name = "bettercam_mouse_look",           .type = CONFIG_TYPE_BOOL, .boolValue = &configFreeCameraMouse},
    {.name = "bettercam_xsens",                .type = CONFIG_TYPE_UINT, .uintValue = &configFreeCameraXSens},
    {.name = "bettercam_ysens",                .type = CONFIG_TYPE_UINT, .uintValue = &configFreeCameraYSens},
    {.name = "bettercam_aggression",           .type = CONFIG_TYPE_UINT, .uintValue = &configFreeCameraAggr},
    {.name = "bettercam_pan_level",            .type = CONFIG_TYPE_UINT, .uintValue = &configFreeCameraPan},
    {.name = "bettercam_degrade",              .type = CONFIG_TYPE_UINT, .uintValue = &configFreeCameraDegrade},
    {.name = "romhackcam_enable",              .type = CONFIG_TYPE_UINT, .uintValue = &configEnableRomhackCamera},
    {.name = "romhackcam_bowser",              .type = CONFIG_TYPE_BOOL, .boolValue = &configRomhackCameraBowserFights},
    {.name = "romhackcam_collision",           .type = CONFIG_TYPE_BOOL, .boolValue = &configRomhackCameraHasCollision},
    {.name = "romhackcam_centering",           .type = CONFIG_TYPE_BOOL, .boolValue = &configRomhackCameraHasCentering},
    {.name = "romhackcam_dpad",                .type = CONFIG_TYPE_BOOL, .boolValue = &configRomhackCameraDPadBehavior},
    {.name = "romhackcam_slowfall",            .type = CONFIG_TYPE_BOOL, .boolValue = &configRomhackCameraSlowFall},
    {.name = "bettercam_invertx",              .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertX},
    {.name = "bettercam_inverty",              .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertY},
    {.name = "romhackcam_toxic_gas",           .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraToxicGas},

    // Player/host/join
    {.name = "coop_player_name",               .type = CONFIG_TYPE_STRING, .stringValue = configPlayerName, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "amount_of_players",              .type = CONFIG_TYPE_UINT,   .uintValue = &configAmountOfPlayers},
    {.name = "bubble_death",                   .type = CONFIG_TYPE_BOOL,   .boolValue = &configBubbleDeath},
    {.name = "coop_host_port",                 .type = CONFIG_TYPE_UINT,   .uintValue = &configHostPort},
    {.name = "coop_host_save_slot",            .type = CONFIG_TYPE_UINT,   .uintValue = &configHostSaveSlot},
    {.name = "coop_join_ip",                   .type = CONFIG_TYPE_STRING, .stringValue = configJoinIp, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "coop_join_port",                 .type = CONFIG_TYPE_UINT,   .uintValue = &configJoinPort},
    {.name = "coop_network_system",            .type = CONFIG_TYPE_UINT,   .uintValue = &configNetworkSystem},
    {.name = "coop_player_interaction",        .type = CONFIG_TYPE_UINT,   .uintValue = &configPlayerInteraction},
    {.name = "coop_player_knockback_strength", .type = CONFIG_TYPE_UINT,   .uintValue = &configPlayerKnockbackStrength},
    {.name = "coop_stay_in_level_after_star",  .type = CONFIG_TYPE_UINT,   .uintValue = &configStayInLevelAfterStar},
    {.name = "coop_nametags",                  .type = CONFIG_TYPE_BOOL,   .boolValue = &configNametags},
    {.name = "coop_mod_dev_mode",              .type = CONFIG_TYPE_BOOL,   .boolValue = &configModDevMode},
    {.name = "coop_bouncy_bounds",             .type = CONFIG_TYPE_UINT,   .uintValue = &configBouncyLevelBounds},
    {.name = "skip_intro",                     .type = CONFIG_TYPE_BOOL,   .boolValue = &configSkipIntro},
    {.name = "pause_anywhere",                 .type = CONFIG_TYPE_BOOL,   .boolValue = &configPauseAnywhere},
    {.name = "coop_menu_staff_roll",           .type = CONFIG_TYPE_BOOL,   .boolValue = &configMenuStaffRoll},
    {.name = "coop_menu_level",                .type = CONFIG_TYPE_UINT,   .uintValue = &configMenuLevel},
    {.name = "coop_menu_sound",                .type = CONFIG_TYPE_UINT,   .uintValue = &configMenuSound},
    {.name = "coop_menu_random",               .type = CONFIG_TYPE_BOOL,   .boolValue = &configMenuRandom},
    {.name = "coop_menu_demos",                .type = CONFIG_TYPE_BOOL,   .boolValue = &configMenuDemos},
    {.name = "disable_popups",                 .type = CONFIG_TYPE_BOOL,   .boolValue = &configDisablePopups},
    {.name = "language",                       .type = CONFIG_TYPE_STRING, .stringValue = configLanguage, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "player_pvp_mode",                .type = CONFIG_TYPE_UINT,   .uintValue = &configPvpType},
    {.name = "coopnet_password",               .type = CONFIG_TYPE_STRING, .stringValue = configPassword, .maxStringLength = MAX_CONFIG_STRING},
    {.name = "rules_version",                  .type = CONFIG_TYPE_UINT,   .uintValue = &configRulesVersion},

    // DJUI
    {.name = "djui_theme",           .type = CONFIG_TYPE_UINT, .uintValue = &configDjuiTheme},
    {.name = "djui_theme_center",    .type = CONFIG_TYPE_BOOL, .boolValue = &configDjuiThemeCenter},
    {.name = "djui_theme_gradients", .type = CONFIG_TYPE_BOOL, .boolValue = &configDjuiThemeGradients},
    {.name = "djui_theme_font",      .type = CONFIG_TYPE_UINT, .uintValue = &configDjuiThemeFont},
    {.name = "djui_scale",           .type = CONFIG_TYPE_UINT, .uintValue = &configDjuiScale},
    {.name = "ex_coop_theme",        .type = CONFIG_TYPE_BOOL, .boolValue = &configExCoopTheme},

#ifdef TARGET_WII_U
    {.name = "n64_face_buttons",     .type = CONFIG_TYPE_BOOL, .boolValue = &configN64FaceButtons},
#endif
};

static char *lstrip_whitespace(char *str) {
    while (str != NULL && *str != '\0' && isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

static void rstrip_whitespace(char *str) {
    size_t len;
    if (str == NULL) {
        return;
    }
    len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

// Splits one config line into key/value. Returns false for empty/comment lines.
static bool parse_key_value(char *line, char **out_key, char **out_value) {
    char *key;
    char *value;

    if (out_key == NULL || out_value == NULL) {
        return false;
    }

    key = lstrip_whitespace(line);
    if (key == NULL || key[0] == '\0' || key[0] == '#') {
        return false;
    }

    value = key;
    while (*value != '\0' && !isspace((unsigned char)*value)) {
        value++;
    }
    if (*value == '\0') {
        return false;
    }

    *(value++) = '\0';
    value = lstrip_whitespace(value);
    if (value == NULL || value[0] == '\0') {
        return false;
    }

    rstrip_whitespace(value);
    *out_key = key;
    *out_value = value;
    return true;
}

const char *configfile_name(void) {
    return CONFIGFILE_DEFAULT;
}

struct QueuedFile {
    char *path;
    struct QueuedFile *next;
};

static struct QueuedFile *sQueuedEnableModsHead = NULL;

static void queue_enabled_mod_path(const char *path) {
    struct QueuedFile *queued;
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return;
    }

    queued = malloc(sizeof(struct QueuedFile));
    if (queued == NULL) {
        return;
    }
    len = strlen(path) + 1;
    queued->path = malloc(len);
    if (queued->path == NULL) {
        free(queued);
        return;
    }
    memcpy(queued->path, path, len);
    queued->next = NULL;

    if (sQueuedEnableModsHead == NULL) {
        sQueuedEnableModsHead = queued;
    } else {
        struct QueuedFile *tail = sQueuedEnableModsHead;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = queued;
    }
}

void enable_queued_mods(void) {
    while (sQueuedEnableModsHead != NULL) {
        struct QueuedFile *next = sQueuedEnableModsHead->next;
        mods_enable(sQueuedEnableModsHead->path);
        free(sQueuedEnableModsHead->path);
        free(sQueuedEnableModsHead);
        sQueuedEnableModsHead = next;
    }
}

void enable_queued_dynos_packs(void) {
}

static void enable_mod_read(const char *value) {
    queue_enabled_mod_path(value);
}

static void enable_mod_write(FILE *file) {
    for (size_t i = 0; i < gLocalMods.entryCount; i++) {
        struct Mod *mod = gLocalMods.entries[i];
        if (mod == NULL || !mod->enabled) {
            continue;
        }
        fprintf(file, "enable-mod: %s\n", mod->relativePath);
    }
}

static const struct FunctionConfigOption function_options[] = {
    { .name = "enable-mod:", .read = enable_mod_read, .write = enable_mod_write },
};

static bool parse_bool_value(const char *value, bool *out_value) {
    if (value == NULL || out_value == NULL) {
        return false;
    }
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0) {
        *out_value = true;
        return true;
    }
    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

// Loads config values from the writable virtual filesystem location.
void configfile_load(void) {
    const char *filename = configfile_name();
    fs_file_t *file = NULL;
    char line[1024];

    printf("Loading configuration from '%s'\n", filename);
    file = fs_open(filename);
    if (file == NULL) {
        // Create a new config file and save defaults
        printf("Config file '%s' not found. Creating it.\n", filename);
        configfile_save();
        return;
    }

    // Go through each line in the file
    while (fs_readline(file, line, sizeof(line)) != NULL) {
        char *key = NULL;
        char *value = NULL;
        const struct ConfigOption *option = NULL;
        bool matched_function_option = false;

        if (!parse_key_value(line, &key, &value)) {
            continue;
        }

        for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
            if (strcmp(key, options[i].name) == 0) {
                option = &options[i];
                break;
            }
        }

        if (option != NULL) {
            switch (option->type) {
                case CONFIG_TYPE_BOOL:
                    parse_bool_value(value, option->boolValue);
                    break;
                case CONFIG_TYPE_UINT:
                    {
                        bool boolValue = false;
                        if (parse_bool_value(value, &boolValue)) {
                            // Backward compatibility: some older configs used true/false for numeric toggles.
                            *option->uintValue = boolValue ? 1U : 0U;
                        } else {
                            *option->uintValue = (unsigned int)strtoul(value, NULL, 10);
                        }
                    }
                    break;
                case CONFIG_TYPE_FLOAT:
                    *option->floatValue = strtof(value, NULL);
                    break;
                case CONFIG_TYPE_STRING:
                    if (option->maxStringLength > 0) {
                        snprintf(option->stringValue, (size_t)option->maxStringLength, "%s", value);
                    }
                    break;
                default:
                    assert(0); // bad type
            }
            continue;
        }

        for (unsigned int i = 0; i < ARRAY_LEN(function_options); i++) {
            if (strcmp(key, function_options[i].name) == 0) {
                function_options[i].read(value);
                matched_function_option = true;
                break;
            }
        }

        if (!matched_function_option) {
            printf("unknown option '%s'\n", key);
        }
    }

    fs_close(file);

    if (configFramerateMode > RRM_MAX) { configFramerateMode = RRM_AUTO; }
    if (configFrameLimit < 30) { configFrameLimit = 30; }
    if (configFrameLimit > 3000) { configFrameLimit = 3000; }

    if (configInterpolationMode > 1) { configInterpolationMode = 1; }
    if (configDrawDistance > 5) { configDrawDistance = 5; }
    if (configFiltering > 2) { configFiltering = 2; }
    if (configStickDeadzone > 100) { configStickDeadzone = 100; }
    if (configRumbleStrength > 100) { configRumbleStrength = 100; }
    if (configGamepadNumber > 4) { configGamepadNumber = 0; }
    if (configMenuLevel > 17) { configMenuLevel = 0; }
    if (configMenuSound > 13) { configMenuSound = 0; }
    if (configDjuiThemeFont > 1) { configDjuiThemeFont = 0; }

#ifdef TARGET_WII_U
    // Wii U safety defaults while donor frame-pacing/menu-scene parity is still in progress.
    configWindow.vsync = true;
    configFramerateMode = RRM_AUTO;
    configInterpolationMode = 1;
#endif
}

// Writes the config file to the writable virtual filesystem location.
void configfile_save(void) {
    FILE *file;
    const char *filename = configfile_name();
    const char *write_path = fs_get_write_path(filename);

    printf("Saving configuration to '%s'\n", filename);

    file = fopen(write_path, "w");
    if (file == NULL) {
        // error
        return;
    }

    for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
        const struct ConfigOption *option = &options[i];

        switch (option->type) {
            case CONFIG_TYPE_BOOL:
                fprintf(file, "%s %s\n", option->name, *option->boolValue ? "true" : "false");
                break;
            case CONFIG_TYPE_UINT:
                fprintf(file, "%s %u\n", option->name, *option->uintValue);
                break;
            case CONFIG_TYPE_FLOAT:
                fprintf(file, "%s %f\n", option->name, *option->floatValue);
                break;
            case CONFIG_TYPE_STRING:
                fprintf(file, "%s %s\n", option->name, option->stringValue);
                break;
            default:
                assert(0); // unknown type
        }
    }

    for (unsigned int i = 0; i < ARRAY_LEN(function_options); i++) {
        function_options[i].write(file);
    }

    fclose(file);
}
