#pragma once
#include "../game/game.h"
#include <SDL2/SDL.h>
#include "easywsclient.hpp"

//Events codes
#define M_MOVEMENT 1
#define M_ATTACK 2
#define M_CREATE_BARRACK 3
#define M_CREATE_PEASANT 4
#define M_CREATE_SOLDIER 5
#define M_HARVEST 6
#define M_END_TURN 7

#define PLAYER_LEAVE_GAME 8
#define PLAYER_JOIN_ROOM 9
#define PLAYER_LEAVE_ROOM 10
#define GAME_START 11
#define DESTROY_ROOM 14
#define CONNECTION 15



//isAIControlled codes
#define AI 1
#define PLAYER 2

typedef struct{
    char * pseudo;
    int isAIControlled;
    int slot;
}mPlayer;

typedef struct{
    char * name;
    int nPlayers;
    mPlayer * players;
    char * host;
    struct game game;
}room;

typedef struct{
    int type;
    int roomId;
    int clientId;
    mPlayer playerInfos;
    int unitId;
    coord target;
}event;


int wsConnect(SDL_Renderer * renderer, SDL_Texture * texture);

int lobby(easywsclient::WebSocket * ws, SDL_Renderer * renderer, SDL_Texture * texture);
int roomFunction(easywsclient::WebSocket * ws, SDL_Renderer * renderer, SDL_Texture * texture, room room, int roomId);
