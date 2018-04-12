#pragma once
#include "game_init.hpp"


int parseRooms(room ** rooms, char * jString);

int parsePlayers(mPlayer ** players, char * jString);
char * serializePlayerSelf(char * pseudo, int nPlayers, int roomId);

event parseEvent(char * jString);
char * serializeEvent(int type, int unitId, coord targetPos);
