#ifndef NETWORK_PLAYER_H
#define NETWORK_PLAYER_H

#include <stdbool.h>
#include <PR/ultratypes.h>
#include "game/player_palette.h"
#include "pc/configfile.h"

#define UNKNOWN_LOCAL_INDEX ((u8)-1)
#define UNKNOWN_GLOBAL_INDEX ((u8)-1)
#define UNKNOWN_NETWORK_INDEX ((u64)-1)
#define MAX_DESCRIPTION_STRING 64

enum NetworkPlayerType {
    NPT_UNKNOWN,
    NPT_LOCAL,
    NPT_SERVER,
    NPT_CLIENT,
};

struct NetworkPlayer {
    bool connected;
    u8 type;
    u8 localIndex;
    u8 globalIndex;
    bool moderator;
    s16 currCourseNum;
    s16 currActNum;
    s16 currLevelNum;
    s16 currAreaIndex;
    bool currLevelSyncValid;
    bool currAreaSyncValid;
    bool currPositionValid;
    u8 modelIndex;
    u8 overrideModelIndex;
    u32 ping;
    struct PlayerPalette palette;
    struct PlayerPalette overridePalette;
    char name[MAX_CONFIG_STRING];
    char description[MAX_DESCRIPTION_STRING];
    u8 descriptionR;
    u8 descriptionG;
    u8 descriptionB;
    u8 descriptionA;
    char overrideLocation[256];
};

extern struct NetworkPlayer gNetworkPlayers[];
extern struct NetworkPlayer* gNetworkPlayerLocal;
extern struct NetworkPlayer* gNetworkPlayerServer;

bool network_player_name_valid(char* buffer);
void network_player_update_model(u8 localIndex);
u8 network_player_connected_count(void);
struct NetworkPlayer* network_player_from_global_index(u8 globalIndex);

#endif
