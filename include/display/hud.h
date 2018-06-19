//Contains game "loops" functions (one for each hud to be displayed)
#include "display.h"
#include "../game/game.h"

#define QUIT_HUD 1
#define QUIT_GAME 2
#define QUIT_PROGRAM 3


int countdownUpdate(int * countdown, int * countdownSec, int quit, int * newEvent, struct game * game);

int mainHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game game);
int playerHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera);
void AIHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera);

int peasantHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int peasantId);   //This is way too long
int soldierHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int soldierId);
int buildingHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int soldierId);
int targetHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int isMovement, coord pos, coord * target);
int foreignHud(SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int ownerId, int tokenId, int isUnit);
