// configfile.c - handles loading and saving the configuration options
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "configfile.h"
#include "fs/fs.h"

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_FLOAT,
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        float *floatValue;
    };
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
#ifdef TARGET_WII_U
    {.name = "n64_face_buttons", .type = CONFIG_TYPE_BOOL, .boolValue = &configN64FaceButtons},
#else
    {.name = "fullscreen",     .type = CONFIG_TYPE_BOOL, .boolValue = &configFullscreen},
    {.name = "key_a",          .type = CONFIG_TYPE_UINT, .uintValue = &configKeyA[0]},
    {.name = "key_b",          .type = CONFIG_TYPE_UINT, .uintValue = &configKeyB[0]},
    {.name = "key_start",      .type = CONFIG_TYPE_UINT, .uintValue = &configKeyStart[0]},
    {.name = "key_r",          .type = CONFIG_TYPE_UINT, .uintValue = &configKeyR[0]},
    {.name = "key_z",          .type = CONFIG_TYPE_UINT, .uintValue = &configKeyZ[0]},
    {.name = "key_cup",        .type = CONFIG_TYPE_UINT, .uintValue = &configKeyCUp[0]},
    {.name = "key_cdown",      .type = CONFIG_TYPE_UINT, .uintValue = &configKeyCDown[0]},
    {.name = "key_cleft",      .type = CONFIG_TYPE_UINT, .uintValue = &configKeyCLeft[0]},
    {.name = "key_cright",     .type = CONFIG_TYPE_UINT, .uintValue = &configKeyCRight[0]},
    {.name = "key_stickup",    .type = CONFIG_TYPE_UINT, .uintValue = &configKeyStickUp[0]},
    {.name = "key_stickdown",  .type = CONFIG_TYPE_UINT, .uintValue = &configKeyStickDown[0]},
    {.name = "key_stickleft",  .type = CONFIG_TYPE_UINT, .uintValue = &configKeyStickLeft[0]},
    {.name = "key_stickright", .type = CONFIG_TYPE_UINT, .uintValue = &configKeyStickRight[0]},
#endif
};

// Returns the position of the first non-whitespace character
static char *skip_whitespace(char *str) {
    while (isspace(*str))
        str++;
    return str;
}

// NULL-terminates the current whitespace-delimited word, and returns a pointer to the next word
static char *word_split(char *str) {
    // Precondition: str must not point to whitespace
    assert(!isspace(*str));

    // Find either the next whitespace char or end of string
    while (!isspace(*str) && *str != '\0')
        str++;
    if (*str == '\0') // End of string
        return str;

    // Terminate current word
    *(str++) = '\0';

    // Skip whitespace to next word
    return skip_whitespace(str);
}

// Splits a string into words, and stores the words into the 'tokens' array
// 'maxTokens' is the length of the 'tokens' array
// Returns the number of tokens parsed
static unsigned int tokenize_string(char *str, int maxTokens, char **tokens) {
    int count = 0;

    str = skip_whitespace(str);
    while (str[0] != '\0' && count < maxTokens) {
        tokens[count] = str;
        str = word_split(str);
        count++;
    }
    return count;
}

const char *configfile_name(void) {
    return CONFIGFILE_DEFAULT;
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
        char *p = line;
        char *tokens[2];
        int numTokens;

        while (isspace(*p))
            p++;
        numTokens = tokenize_string(p, 2, tokens);
        if (numTokens != 0) {
            if (numTokens == 2) {
                const struct ConfigOption *option = NULL;

                for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
                    if (strcmp(tokens[0], options[i].name) == 0) {
                        option = &options[i];
                        break;
                    }
                }
                if (option == NULL)
                    printf("unknown option '%s'\n", tokens[0]);
                else {
                    switch (option->type) {
                        case CONFIG_TYPE_BOOL:
                            if (strcmp(tokens[1], "true") == 0)
                                *option->boolValue = true;
                            else if (strcmp(tokens[1], "false") == 0)
                                *option->boolValue = false;
                            break;
                        case CONFIG_TYPE_UINT:
                            sscanf(tokens[1], "%u", option->uintValue);
                            break;
                        case CONFIG_TYPE_FLOAT:
                            sscanf(tokens[1], "%f", option->floatValue);
                            break;
                        default:
                            assert(0); // bad type
                    }
                    printf("option: '%s', value: '%s'\n", tokens[0], tokens[1]);
                }
            } else
                puts("error: expected value");
        }
    }

    fs_close(file);
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
            default:
                assert(0); // unknown type
        }
    }

    fclose(file);
}
