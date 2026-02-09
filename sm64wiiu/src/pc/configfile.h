#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <stdbool.h>
#include <PR/ultratypes.h>

struct PlayerPalette;

#define CONFIGFILE_DEFAULT "sm64config.txt"
#define MAX_BINDS 3
#define MAX_CONFIG_STRING 64
#define MAX_SAVE_NAME_STRING 32
#define DEFAULT_PORT 7777

typedef struct {
    unsigned int x, y, w, h;
    bool vsync;
    bool reset;
    bool fullscreen;
    bool exiting_fullscreen;
    bool settings_changed;
    unsigned int msaa;
} ConfigWindow;

typedef struct {
    bool rotateLeft;
    bool invertLeftX;
    bool invertLeftY;
    bool rotateRight;
    bool invertRightX;
    bool invertRightY;
} ConfigStick;

enum RefreshRateMode {
    RRM_AUTO,
    RRM_MANUAL,
    RRM_UNLIMITED,
    RRM_MAX,
};

extern bool configFullscreen;
extern ConfigWindow configWindow;
extern ConfigStick configStick;

extern unsigned int configKeyA[MAX_BINDS];
extern unsigned int configKeyB[MAX_BINDS];
extern unsigned int configKeyStart[MAX_BINDS];
extern unsigned int configKeyL[MAX_BINDS];
extern unsigned int configKeyR[MAX_BINDS];
extern unsigned int configKeyZ[MAX_BINDS];
extern unsigned int configKeyCUp[MAX_BINDS];
extern unsigned int configKeyCDown[MAX_BINDS];
extern unsigned int configKeyCLeft[MAX_BINDS];
extern unsigned int configKeyCRight[MAX_BINDS];
extern unsigned int configKeyStickUp[MAX_BINDS];
extern unsigned int configKeyStickDown[MAX_BINDS];
extern unsigned int configKeyStickLeft[MAX_BINDS];
extern unsigned int configKeyStickRight[MAX_BINDS];
extern unsigned int configKeyX[MAX_BINDS];
extern unsigned int configKeyY[MAX_BINDS];
extern unsigned int configKeyChat[MAX_BINDS];
extern unsigned int configKeyPlayerList[MAX_BINDS];
extern unsigned int configKeyDUp[MAX_BINDS];
extern unsigned int configKeyDDown[MAX_BINDS];
extern unsigned int configKeyDLeft[MAX_BINDS];
extern unsigned int configKeyDRight[MAX_BINDS];
extern unsigned int configKeyConsole[MAX_BINDS];
extern unsigned int configKeyPrevPage[MAX_BINDS];
extern unsigned int configKeyNextPage[MAX_BINDS];
extern unsigned int configKeyDisconnect[MAX_BINDS];

extern unsigned int configFiltering;
extern bool configShowFPS;
extern bool configShowPing;
extern enum RefreshRateMode configFramerateMode;
extern unsigned int configFrameLimit;
extern unsigned int configInterpolationMode;
extern unsigned int configDrawDistance;

extern unsigned int configMasterVolume;
extern unsigned int configMusicVolume;
extern unsigned int configSfxVolume;
extern unsigned int configEnvVolume;
extern bool configFadeoutDistantSounds;
extern bool configMuteFocusLoss;

extern unsigned int configStickDeadzone;
extern unsigned int configRumbleStrength;
extern unsigned int configGamepadNumber;
extern bool configBackgroundGamepad;
extern bool configDisableGamepads;
extern bool configUseStandardKeyBindingsChat;
extern bool configSmoothScrolling;

extern bool configEnableFreeCamera;
extern bool configFreeCameraAnalog;
extern bool configFreeCameraLCentering;
extern bool configFreeCameraDPadBehavior;
extern bool configFreeCameraHasCollision;
extern bool configFreeCameraMouse;
extern unsigned int configFreeCameraXSens;
extern unsigned int configFreeCameraYSens;
extern unsigned int configFreeCameraAggr;
extern unsigned int configFreeCameraPan;
extern unsigned int configFreeCameraDegrade;
extern unsigned int configEnableRomhackCamera;
extern bool configRomhackCameraBowserFights;
extern bool configRomhackCameraHasCollision;
extern bool configRomhackCameraHasCentering;
extern bool configRomhackCameraDPadBehavior;
extern bool configRomhackCameraSlowFall;
extern bool configCameraInvertX;
extern bool configCameraInvertY;
extern bool configCameraToxicGas;

extern bool configLuaProfiler;
extern bool configDebugPrint;
extern bool configDebugInfo;
extern bool configDebugError;
extern bool configCtxProfiler;

extern char configSaveNames[4][MAX_SAVE_NAME_STRING];
extern char configPlayerName[MAX_CONFIG_STRING];
extern unsigned int configPlayerModel;
extern struct PlayerPalette configPlayerPalette;

extern unsigned int configAmountOfPlayers;
extern bool configBubbleDeath;
extern unsigned int configHostPort;
extern unsigned int configHostSaveSlot;
extern char configJoinIp[MAX_CONFIG_STRING];
extern unsigned int configJoinPort;
extern unsigned int configNetworkSystem;
extern unsigned int configPlayerInteraction;
extern unsigned int configPlayerKnockbackStrength;
extern unsigned int configStayInLevelAfterStar;
extern bool configNametags;
extern bool configModDevMode;
extern unsigned int configBouncyLevelBounds;
extern bool configSkipIntro;
extern bool configPauseAnywhere;
extern bool configMenuStaffRoll;
extern unsigned int configMenuLevel;
extern unsigned int configMenuSound;
extern bool configMenuRandom;
extern bool configMenuDemos;
extern bool configDisablePopups;
extern char configLanguage[MAX_CONFIG_STRING];
extern bool configForce4By3;
extern bool configDynosLocalPlayerModelOnly;
extern unsigned int configPvpType;
extern char configPassword[MAX_CONFIG_STRING];
extern unsigned int configRulesVersion;

extern unsigned int configDjuiTheme;
extern bool configDjuiThemeCenter;
extern bool configDjuiThemeGradients;
extern unsigned int configDjuiThemeFont;
extern unsigned int configDjuiScale;
extern bool configExCoopTheme;

#ifdef TARGET_WII_U
extern bool configN64FaceButtons;
#endif

void enable_queued_mods(void);
void enable_queued_dynos_packs(void);
void configfile_load(void);
void configfile_save(void);
const char *configfile_name(void);

#endif
