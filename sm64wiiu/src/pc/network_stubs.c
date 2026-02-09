#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "sm64.h"
#include "pc/configfile.h"
#include "pc/network/network.h"
#include "pc/network/socket/socket.h"

enum NetworkType gNetworkType = NT_NONE;
struct ServerSettings gServerSettings = {
    .enablePlayerList = true,
    .headlessServer = false,
    .maxPlayers = 1,
};
struct NametagsSettings gNametagsSettings = {
    .showHealth = true,
    .showSelfTag = true,
};

struct NetworkPlayer gNetworkPlayers[MAX_PLAYERS] = { 0 };
struct NetworkPlayer* gNetworkPlayerLocal = &gNetworkPlayers[0];
struct NetworkPlayer* gNetworkPlayerServer = &gNetworkPlayers[0];

char gGetHostName[MAX_CONFIG_STRING] = "";

#ifdef COOPNET
uint64_t gCoopNetDesiredLobby = 0;
char gCoopNetPassword[64] = "";
#endif

static bool sNetworkReconnecting = false;
static enum NetworkSystemType sNetworkSystem = NS_SOCKET;

static void network_stub_init_local_player(void) {
    gNetworkPlayers[0].connected = true;
    gNetworkPlayers[0].type = (gNetworkType == NT_SERVER) ? NPT_SERVER : NPT_LOCAL;
    gNetworkPlayers[0].localIndex = 0;
    gNetworkPlayers[0].globalIndex = 0;
    gNetworkPlayers[0].currCourseNum = 0;
    gNetworkPlayers[0].currActNum = 0;
    gNetworkPlayers[0].currLevelNum = 0;
    gNetworkPlayers[0].currAreaIndex = 0;
    gNetworkPlayers[0].currLevelSyncValid = true;
    gNetworkPlayers[0].currAreaSyncValid = true;
    gNetworkPlayers[0].currPositionValid = true;
    gNetworkPlayers[0].modelIndex = (u8)configPlayerModel;
    gNetworkPlayers[0].overrideModelIndex = (u8)configPlayerModel;
    gNetworkPlayers[0].palette = configPlayerPalette;
    gNetworkPlayers[0].overridePalette = configPlayerPalette;
    gNetworkPlayers[0].ping = 0;
    snprintf(gNetworkPlayers[0].name, MAX_CONFIG_STRING, "%s", configPlayerName);
    gNetworkPlayers[0].name[MAX_CONFIG_STRING - 1] = '\0';
    snprintf(gNetworkPlayers[0].description, MAX_DESCRIPTION_STRING, "%s", "OFFLINE");
    gNetworkPlayers[0].descriptionR = 0xDC;
    gNetworkPlayers[0].descriptionG = 0xDC;
    gNetworkPlayers[0].descriptionB = 0xDC;
    gNetworkPlayers[0].descriptionA = 0xFF;

    gNetworkPlayerLocal = &gNetworkPlayers[0];
    gNetworkPlayerServer = &gNetworkPlayers[0];
}

void network_set_system(enum NetworkSystemType nsType) {
    if (nsType >= NS_MAX) {
        return;
    }
    sNetworkSystem = nsType;
    (void)sNetworkSystem;
}

bool network_init(enum NetworkType inNetworkType, bool reconnecting) {
    if (inNetworkType == NT_CLIENT) {
        gNetworkType = NT_NONE;
        sNetworkReconnecting = false;
        network_stub_init_local_player();
        return false;
    }

    gNetworkType = inNetworkType;
    sNetworkReconnecting = reconnecting;
    network_stub_init_local_player();
    return true;
}

bool network_client_available(void) {
    return false;
}

void network_reset_reconnect_and_rehost(void) {
    sNetworkReconnecting = false;
}

void network_reconnect_begin(void) {
    sNetworkReconnecting = true;
}

bool network_is_reconnecting(void) {
    return sNetworkReconnecting;
}

void network_rehost_begin(void) {
    gNetworkType = NT_SERVER;
    network_stub_init_local_player();
}

void network_shutdown(bool sendLeaving, bool exiting, bool popup, bool reconnecting) {
    (void)sendLeaving;
    (void)exiting;
    (void)popup;
    (void)reconnecting;
    gNetworkType = NT_NONE;
    sNetworkReconnecting = false;
    network_stub_init_local_player();
}

void network_send_chat(const char *message, u8 fromGlobalIndex) {
    (void)message;
    (void)fromGlobalIndex;
}

void network_send_player_settings(void) {
    network_stub_init_local_player();
}

void network_send_global_popup(const char* message, int lines) {
    (void)message;
    (void)lines;
}

u8 network_player_connected_count(void) {
    u8 count = 0;
    for (u8 i = 0; i < MAX_PLAYERS; i++) {
        if (gNetworkPlayers[i].connected) {
            count++;
        }
    }
    return count;
}

struct NetworkPlayer* network_player_from_global_index(u8 globalIndex) {
    for (u8 i = 0; i < MAX_PLAYERS; i++) {
        if (gNetworkPlayers[i].connected && gNetworkPlayers[i].globalIndex == globalIndex) {
            return &gNetworkPlayers[i];
        }
    }
    return NULL;
}

bool network_player_name_valid(char* buffer) {
    size_t len = 0;
    if (buffer == NULL) {
        return false;
    }

    while (*buffer != '\0') {
        if ((unsigned char)*buffer < 0x20) {
            return false;
        }
        buffer++;
        len++;
    }

    return len > 0 && len < MAX_CONFIG_STRING;
}

void network_player_update_model(u8 localIndex) {
    if (localIndex >= MAX_PLAYERS) {
        return;
    }
    gNetworkPlayers[localIndex].modelIndex = (u8)configPlayerModel;
    gNetworkPlayers[localIndex].overrideModelIndex = (u8)configPlayerModel;
    gNetworkPlayers[localIndex].palette = configPlayerPalette;
    gNetworkPlayers[localIndex].overridePalette = configPlayerPalette;
}

u8* network_get_player_text_color(u8 localIndex) {
    static u8 sColor[3] = { 0xFF, 0xFF, 0xFF };
    (void)localIndex;
    return sColor;
}

const char* network_get_player_text_color_string(u8 localIndex) {
    (void)localIndex;
    return "\\#ffffff\\";
}

SOCKET socket_initialize(void) {
    return 0;
}

void socket_shutdown(SOCKET socketHandle) {
    (void)socketHandle;
}

const char* get_version(void) {
    return SM64COOPDX_VERSION;
}

#ifdef COMPILE_TIME
const char* get_version_with_build_date(void) {
    return SM64COOPDX_VERSION;
}
#endif

#ifdef COOPNET
bool ns_coopnet_query(QueryCallbackPtr callback, QueryFinishCallbackPtr finishCallback, const char* password) {
    (void)callback;
    (void)finishCallback;
    (void)password;
    return false;
}

bool ns_coopnet_is_connected(void) {
    return false;
}

void ns_coopnet_update(void) {
}
#endif
