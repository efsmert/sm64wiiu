#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <PR/ultratypes.h>
#include "network_player.h"
#include "pc/network/version.h"

enum NetworkType {
    NT_NONE,
    NT_CLIENT,
    NT_SERVER,
};

enum NetworkSystemType {
    NS_SOCKET,
    NS_COOPNET,
    NS_MAX,
};

struct ServerSettings {
    u8 playerInteractions;
    u8 bouncyLevelBounds;
    u8 pvpType;
    u8 playerKnockbackStrength;
    u8 stayInLevelAfterStar;
    u8 skipIntro;
    u8 bubbleDeath;
    u8 enablePlayersInLevelDisplay;
    u8 enablePlayerList;
    u8 headlessServer;
    u8 nametags;
    u8 maxPlayers;
    u8 pauseAnywhere;
};

struct NametagsSettings {
    bool showHealth;
    bool showSelfTag;
};

extern enum NetworkType gNetworkType;
extern struct ServerSettings gServerSettings;
extern struct NametagsSettings gNametagsSettings;

void network_set_system(enum NetworkSystemType nsType);
bool network_init(enum NetworkType inNetworkType, bool reconnecting);
bool network_client_available(void);
void network_reset_reconnect_and_rehost(void);
void network_reconnect_begin(void);
bool network_is_reconnecting(void);
void network_rehost_begin(void);
void network_shutdown(bool sendLeaving, bool exiting, bool popup, bool reconnecting);

void network_send_chat(const char *message, u8 fromGlobalIndex);
void network_send_player_settings(void);
void network_send_global_popup(const char* message, int lines);

u8* network_get_player_text_color(u8 localIndex);
const char* network_get_player_text_color_string(u8 localIndex);

#endif
