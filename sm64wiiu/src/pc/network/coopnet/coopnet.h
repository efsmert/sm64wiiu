#ifndef NETWORK_COOPNET_H
#define NETWORK_COOPNET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef COOPNET
typedef void (*QueryCallbackPtr)(uint64_t aLobbyId, uint64_t aOwnerId, uint16_t aConnections,
                                 uint16_t aMaxConnections, const char* aGame, const char* aVersion,
                                 const char* aHostName, const char* aMode, const char* aDescription);
typedef void (*QueryFinishCallbackPtr)(void);

extern uint64_t gCoopNetDesiredLobby;
extern char gCoopNetPassword[];

bool ns_coopnet_query(QueryCallbackPtr callback, QueryFinishCallbackPtr finishCallback, const char* password);
bool ns_coopnet_is_connected(void);
void ns_coopnet_update(void);
#endif

#endif
