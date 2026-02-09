#include "configfile.h"
#include "game/player_palette.h"

unsigned int configFiltering = 2;
bool configShowFPS = false;
bool configShowPing = false;
enum RefreshRateMode configFramerateMode = RRM_AUTO;
unsigned int configFrameLimit = 60;
unsigned int configInterpolationMode = 1;
unsigned int configDrawDistance = 1;

unsigned int configMasterVolume = 80;
unsigned int configMusicVolume = 127;
unsigned int configSfxVolume = 127;
unsigned int configEnvVolume = 127;
bool configFadeoutDistantSounds = false;
bool configMuteFocusLoss = false;

unsigned int configStickDeadzone = 16;
unsigned int configRumbleStrength = 50;
unsigned int configGamepadNumber = 0;
bool configBackgroundGamepad = true;
bool configDisableGamepads = false;
bool configUseStandardKeyBindingsChat = false;
bool configSmoothScrolling = false;

bool configEnableFreeCamera = false;
bool configFreeCameraAnalog = false;
bool configFreeCameraLCentering = false;
bool configFreeCameraDPadBehavior = false;
bool configFreeCameraHasCollision = true;
bool configFreeCameraMouse = false;
unsigned int configFreeCameraXSens = 50;
unsigned int configFreeCameraYSens = 50;
unsigned int configFreeCameraAggr = 0;
unsigned int configFreeCameraPan = 0;
unsigned int configFreeCameraDegrade = 50;
unsigned int configEnableRomhackCamera = 0;
bool configRomhackCameraBowserFights = false;
bool configRomhackCameraHasCollision = true;
bool configRomhackCameraHasCentering = false;
bool configRomhackCameraDPadBehavior = false;
bool configRomhackCameraSlowFall = true;
bool configCameraInvertX = false;
bool configCameraInvertY = true;
bool configCameraToxicGas = true;

bool configLuaProfiler = false;
bool configDebugPrint = false;
bool configDebugInfo = false;
bool configDebugError = false;
bool configCtxProfiler = false;

char configSaveNames[4][MAX_SAVE_NAME_STRING] = {
    "Mario A",
    "Mario B",
    "Mario C",
    "Mario D",
};

char configPlayerName[MAX_CONFIG_STRING] = "Player";
unsigned int configPlayerModel = 0;
struct PlayerPalette configPlayerPalette = {
    .parts = {
        { 0x00, 0x00, 0xff },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0xff, 0xff },
        { 0x72, 0x1c, 0x0e },
        { 0x73, 0x06, 0x00 },
        { 0xfe, 0xc1, 0x79 },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0x00, 0x00 },
    },
};

unsigned int configAmountOfPlayers = 1;
bool configBubbleDeath = true;
unsigned int configHostPort = DEFAULT_PORT;
unsigned int configHostSaveSlot = 1;
char configJoinIp[MAX_CONFIG_STRING] = "127.0.0.1";
unsigned int configJoinPort = DEFAULT_PORT;
unsigned int configNetworkSystem = 0;
unsigned int configPlayerInteraction = 1;
unsigned int configPlayerKnockbackStrength = 25;
unsigned int configStayInLevelAfterStar = 0;
bool configNametags = true;
bool configModDevMode = false;
unsigned int configBouncyLevelBounds = 0;
bool configSkipIntro = false;
bool configPauseAnywhere = false;
bool configMenuStaffRoll = false;
unsigned int configMenuLevel = 0;
unsigned int configMenuSound = 0;
bool configMenuRandom = false;
bool configMenuDemos = false;
bool configDisablePopups = false;
char configLanguage[MAX_CONFIG_STRING] = "English";
bool configForce4By3 = false;
bool configDynosLocalPlayerModelOnly = false;
unsigned int configPvpType = 0;
char configPassword[MAX_CONFIG_STRING] = "";
unsigned int configRulesVersion = 0;

unsigned int configDjuiTheme = 1;
bool configDjuiThemeCenter = true;
bool configDjuiThemeGradients = true;
unsigned int configDjuiThemeFont = 0;
unsigned int configDjuiScale = 0;
bool configExCoopTheme = false;
