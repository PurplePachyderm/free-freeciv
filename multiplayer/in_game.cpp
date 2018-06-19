#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <json-c/json.h>

#include "../include/multiplayer/easywsclient.hpp"
#include "../include/multiplayer/game_init.hpp"
#include "../include/multiplayer/in_game.hpp"
#include "../include/multiplayer/multi_display.hpp"
#include "../include/multiplayer/json.h"


#include "../include/display/hud.h"
#include "../include/display/hud_display.h"
#include "../include/display/display.h"
#include "../include/display/menu.h"

#include "../include/coord.h"
#include "../include/game/game.h"
#include "../include/game/units_actions.h"
#include "../include/game/ai.h"



//XXX Basically solo functions sending events to server
// Multiplayer equivalent to "display/hud.c"
//XXX WIP



//Main HUD (Main game function, sets everything to default state)
int mMainHud(easywsclient::WebSocket * ws, room room, SDL_Renderer * renderer, SDL_Texture * texture, struct game game){

	int quit = 0;
	view camera;
    camera.offset.x = (SCREEN_WIDTH - (MAP_SIZE+2)*TILE_SIZE) / 2;	//Centers map
    camera.offset.y = (SCREEN_HEIGHT - (MAP_SIZE+2)*TILE_SIZE) / 2;
    camera.zoom = 1;
    camera.leftClick = 0;

	int quitGame = 0;
	int ownSlot = 5;

	game.currentPlayer--;	//To start at player 1, or current player if game already started
	while(ws->getReadyState() != easywsclient::WebSocket::CLOSED && !quit){
		game.currentPlayer = (game.currentPlayer+1) % game.nPlayers;
		busyReset(&game); //Resets units

		if(game.players[game.currentPlayer].nBuildings > 0){	//Player has not lost
			if(strcmp(readPseudo(), room.players[game.currentPlayer].pseudo) == 0){	//If it's our client
				//Trigger the "playing" function
				quitGame = mPlayerHud(ws, room, renderer, texture, &game, &camera);

				ownSlot = game.currentPlayer;

				if(quitGame)
					quit = 1;
			}

			else if(!room.players[game.currentPlayer].isAIControlled){	//Other client
				//Trigger the "spectate function"

				//Avoids receiving own next ture event
				quitGame = mEnemyPlayerHud(ws, room, renderer, texture, &(game), &camera, ownSlot);

			}

			else{	//AI
				//Trigger the AI function
				AIHud(renderer, texture, &game, &camera);
				camera.leftClick = 0;	//Stays at 1 after clicking on next turn button
			}
		}
	}

	return 1;
}



//Player Hud
int mPlayerHud(easywsclient::WebSocket * ws, room room, SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera){

	SDL_Event event;
	int quit = 0;
	int newEvent = 0;

	coord selectedTile;
	int tokenId;

	int countdown = TURN_TIME * 1000;	//30 sec in ms
	int countdownSec = TURN_TIME; //Approx in sec for display

	int quitGame = 0;
	mEvent sendedEvent;
	char * jString;

	//First display before any event
	mMainDisplay(renderer, texture, *game, *camera, countdownSec, readPseudo());


    while(ws->getReadyState() != easywsclient::WebSocket::CLOSED && !quit){
        SDL_Delay(REFRESH_PERIOD);

        while(SDL_PollEvent(&event)){
            newEvent = events(event, camera, *game, &selectedTile);

			//End turn (potentially overrides TILE_SELECTION)
			if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x >= SCREEN_WIDTH-TILE_SIZE*1.5 && event.button.y >= SCREEN_HEIGHT-TILE_SIZE*1.5){
				quit = QUIT_HUD;

				sendedEvent.roomId = room.roomId;
				sendedEvent.type = M_END_TURN;
				sendedEvent.unitId = 0;
				sendedEvent.target.x = 0;
				sendedEvent.target.y = 0;
				sendedEvent.clientId = game->currentPlayer;

				char * pseudo = (char*) malloc(75*sizeof(char));
				strcpy(pseudo, readPseudo());
				sendedEvent.playerInfos.pseudo = pseudo;
				jString = serializeEvent(sendedEvent);

				ws->send(jString);
				ws->poll();
				free(jString);
			}

            switch(newEvent){
                case MENU:
                    quitGame = inGameMenu(renderer, *game);
					if(quitGame)
						quit = QUIT_HUD;

					camera->leftClick = 0;	//Avoids moving camera if menu entered w click and left with Escp
                    break;

				case TILE_SELECTION:
					//Own unit
					tokenId = checkOwnUnit(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nUnits){	//Cycles units
						switch(game->players[game->currentPlayer].units[tokenId].type){

							case PEASANT:
							quitGame = mPeasantHud(ws, room, renderer, texture, game, camera, &countdown, &countdownSec, tokenId);
							if(quitGame)
								quit = QUIT_HUD;
							break;

							case SOLDIER:
							quitGame = mSoldierHud(ws, room, renderer, texture, game, camera, &countdown, &countdownSec, tokenId);
							if(quitGame)
								quit = QUIT_HUD;
							break;
						}
					}
					else if(checkOwnBuilding(*game, selectedTile) < game->players[game->currentPlayer].nBuildings){	//Cycles buildingss
						tokenId = checkOwnBuilding(*game, selectedTile);
						if(tokenId < game->players[game->currentPlayer].nBuildings){
								quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, &countdown, &countdownSec, tokenId);
								if(quitGame)
									quit = QUIT_HUD;

						}
					}
					else if(checkOwnUnit(*game, selectedTile) < game->players[game->currentPlayer].nUnits){	//Cycles buildingss
						tokenId = checkOwnBuilding(*game, selectedTile);
						if(tokenId < game->players[game->currentPlayer].nBuildings){
								quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, &countdown, &countdownSec, tokenId);
								if(quitGame)
									quit = QUIT_HUD;

						}
					}
					//Foreign unit selection
					else{

						int ownerId;
						int isUnit = 1;
						tokenId = checkForeignUnit(*game, selectedTile, &ownerId);
						if(ownerId > game->nPlayers){
							tokenId = checkForeignBuilding(*game, selectedTile, &ownerId);
							isUnit = 0;
						}


						if(ownerId < game->nPlayers){
							quitGame = foreignHud(renderer, texture, game, camera, &countdown, &countdownSec, ownerId, tokenId, isUnit);
							if(quitGame)
								quit = QUIT_HUD;
						}
					}
            }
        }

		countdownUpdate(&countdown, &countdownSec, quit, &newEvent, game);

        if(newEvent){  //Refresh display if a new event has occured
			mMainDisplay(renderer, texture, *game, *camera, countdownSec, readPseudo());
			newEvent = 0;
		}
    }

	return quitGame;
}



//Peasant Hud
int mPeasantHud(easywsclient::WebSocket * ws, room room, SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int peasantId){
	srand(time(NULL));

	int soundId = rand()%3;
	soundId++;
	char soundPath [100];
	sprintf(soundPath, "resources/sounds/peasant_select%d.mp3", soundId);

	Mix_Music * music = NULL;
	music = Mix_LoadMUS(soundPath);
	Mix_PlayMusic( music, 1);


	SDL_Event event;
	int quit = 0;
	int quitGame = 0;	//Return value (quits the entire game)
	int newEvent = 0;

	coord target;
	coord * path = NULL;
	coord selectedTile;
	mEvent sendedEvent;
	char * jString;

	mPeasantDisplay(renderer, texture, *game, *camera, *countdownSec, peasantId, readPseudo());

	while(!quit){
		SDL_Delay(REFRESH_PERIOD);

		//Events
		while(SDL_PollEvent(&event)){
			newEvent = events(event, camera, *game, &selectedTile);

			//Menu
			if(newEvent == MENU){
				quitGame = inGameMenu(renderer, *game);
				camera->leftClick = 0;
				if(quitGame)
					quit = 1;
			}

			//Cancel button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH-TILE_SIZE*1.5 && event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.5){
				quit = QUIT_HUD;
			}

			//Move button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2-TILE_SIZE*2*1.75 && event.button.x<SCREEN_WIDTH/2-TILE_SIZE*1.75
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 1, selectedTile, &target);
				if(quitGame)
					quit = QUIT_HUD;


				int length = moveUnit(game, peasantId, target, &path);
				if(length){
					movementAnim(renderer, texture, camera, game, path, length, peasantId);
					free(path);

					sendedEvent.roomId = room.roomId;
					sendedEvent.type = M_MOVEMENT;
					sendedEvent.unitId = peasantId;
					sendedEvent.target.x = target.x;
					sendedEvent.target.y = target.y;
					sendedEvent.clientId = game->currentPlayer;

					char * pseudo = (char*) malloc(75*sizeof(char));
					strcpy(pseudo, readPseudo());
					sendedEvent.playerInfos.pseudo = pseudo;
					jString = serializeEvent(sendedEvent);

					ws->send(jString);
					ws->poll();
					free(jString);
				}
			}

			//Attack Button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2-TILE_SIZE*1.75 && event.button.x<SCREEN_WIDTH/2
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){

				coord tokenPos = game->players[game->currentPlayer].units[peasantId].pos;
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 0, tokenPos, &target);

				if(quitGame)
					quit = QUIT_HUD;

				int success = attack(game, peasantId, target);

				if(success){
					sendedEvent.roomId = room.roomId;
					sendedEvent.type = M_ATTACK;
					sendedEvent.unitId = peasantId;
					sendedEvent.target.x = target.x;
					sendedEvent.target.y = target.y;
					sendedEvent.clientId = game->currentPlayer;

					char * pseudo = (char*) malloc(75*sizeof(char));
					strcpy(pseudo, readPseudo());
					sendedEvent.playerInfos.pseudo = pseudo;
					jString = serializeEvent(sendedEvent);

					ws->send(jString);
					ws->poll();
					free(jString);
				}
			}

			//Barrack Creation Button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2 && event.button.x<SCREEN_WIDTH/2+TILE_SIZE*1.75
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){

				coord tokenPos = game->players[game->currentPlayer].units[peasantId].pos;
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 0, tokenPos, &target);

				if(quitGame)
					quit = QUIT_HUD;

				createBarrack(game, target, peasantId);

				sendedEvent.roomId = room.roomId;
				sendedEvent.type = M_CREATE_BARRACK;
				sendedEvent.unitId = peasantId;
				sendedEvent.target.x = target.x;
				sendedEvent.target.y = target.y;
				sendedEvent.clientId = game->currentPlayer;

				char * pseudo = (char*) malloc(75*sizeof(char));
				strcpy(pseudo, readPseudo());
				sendedEvent.playerInfos.pseudo = pseudo;
				jString = serializeEvent(sendedEvent);

				ws->send(jString);
				ws->poll();
				free(jString);
			}

			//Collect Button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2+TILE_SIZE*1.75 && event.button.x<SCREEN_WIDTH/2+TILE_SIZE*2*1.75
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){

				coord tokenPos = game->players[game->currentPlayer].units[peasantId].pos;
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 0, tokenPos, &target);

				if(quitGame)
					quit = QUIT_HUD;

				int success = collect(game, peasantId, target);

				if(success){
					sendedEvent.roomId = room.roomId;
					sendedEvent.type = M_HARVEST;
					sendedEvent.unitId = peasantId;
					sendedEvent.target.x = target.x;
					sendedEvent.target.y = target.y;
					sendedEvent.clientId = game->currentPlayer;

					char * pseudo = (char*) malloc(75*sizeof(char));
					strcpy(pseudo, readPseudo());
					sendedEvent.playerInfos.pseudo = pseudo;
					jString = serializeEvent(sendedEvent);

					ws->send(jString);
					ws->poll();
					free(jString);
				}
			}

			//Tile selection for other HUD
			else if(newEvent == TILE_SELECTION){
				//Own unit
				int tokenId = checkOwnUnit(*game, selectedTile);
				if(tokenId < game->players[game->currentPlayer].nUnits){	//Cycles units
					switch(game->players[game->currentPlayer].units[tokenId].type){

						case PEASANT:
						quitGame = mPeasantHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;	//This Hud will also be quitted with the "child"
						newEvent = 0;	//Avoids screen refreshing and all previous menus appearing for a frame
						break;

						case SOLDIER:
						quitGame = mSoldierHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;
						newEvent = 0;
						break;
					}
				}
				else if(checkOwnBuilding(*game, selectedTile) < game->players[game->currentPlayer].nBuildings){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;
					}
				}
				else if(checkOwnUnit(*game, selectedTile) < game->players[game->currentPlayer].nUnits){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;
					}
				}
				//Foreign unit selection
				else{

					int ownerId;
					int isUnit = 1;
					tokenId = checkForeignUnit(*game, selectedTile, &ownerId);
					if(ownerId > game->nPlayers){
						tokenId = checkForeignBuilding(*game, selectedTile, &ownerId);
						isUnit = 0;
					}


					if(ownerId < game->nPlayers){
						quitGame = foreignHud(renderer, texture, game, camera, countdown, countdownSec, ownerId, tokenId, isUnit);
						quit = 1;
						newEvent = 0;
					}
				}
			}
		}

		//Countdown
		quit = countdownUpdate(countdown, countdownSec, quit, &newEvent, game);

		//Refresh
		if(newEvent){
			mPeasantDisplay(renderer, texture, *game, *camera, *countdownSec, peasantId, readPseudo());
		}
	}

	Mix_FreeMusic(music);

	return quitGame;
}



//Soldier Hud
int mSoldierHud(easywsclient::WebSocket * ws, room room, SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int soldierId){
	srand(time(NULL));

	int soundId = rand()%2;
	soundId++;
	char soundPath [100];
	sprintf(soundPath, "resources/sounds/soldier_select%d.mp3", soundId);

	Mix_Music * music = NULL;
	music = Mix_LoadMUS(soundPath);
	Mix_PlayMusic( music, 1);


	SDL_Event event;
	int quit = 0;
	int quitGame = 0;	//Return value (quits the entire game)
	int newEvent = 0;

	coord selectedTile;
	coord target;
	coord * path = NULL;
	mEvent sendedEvent;
	char * jString;

	mSoldierDisplay(renderer, texture, *game, *camera, *countdownSec, soldierId, readPseudo());

	while(!quit){
		SDL_Delay(REFRESH_PERIOD);

		//Events
		while(SDL_PollEvent(&event)){
			newEvent = events(event, camera, *game, &selectedTile);

			//Menu
			if(newEvent == MENU){
				quitGame = inGameMenu(renderer, *game);
				quit = quitGame;
				camera->leftClick = 0;
				if(quitGame)
					quit = QUIT_HUD;
			}

			//Cancel button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH-TILE_SIZE*1.5 && event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.5){
				quit = 1;
			}

			//Move button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2-TILE_SIZE*1.75 && event.button.x<SCREEN_WIDTH/2
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 1, selectedTile, &target);

				if(quitGame)
					quit = QUIT_HUD;

				int length = moveUnit(game, soldierId, target, &path);
				if(length){
					movementAnim(renderer, texture, camera, game, path, length, soldierId);
					free(path);

					sendedEvent.roomId = room.roomId;
					sendedEvent.type = M_MOVEMENT;
					sendedEvent.unitId = soldierId;
					sendedEvent.target.x = target.x;
					sendedEvent.target.y = target.y;
					sendedEvent.clientId = game->currentPlayer;

					char * pseudo = (char*) malloc(75*sizeof(char));
					strcpy(pseudo, readPseudo());
					sendedEvent.playerInfos.pseudo = pseudo;
					jString = serializeEvent(sendedEvent);

					ws->send(jString);
					ws->poll();
					free(jString);

					target.x = 0;
					target.y = 0;
				}
			}
			//Attack button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2 && event.button.x<SCREEN_WIDTH/2+TILE_SIZE*1.75
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){

				coord tokenPos = game->players[game->currentPlayer].units[soldierId].pos;
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 0, tokenPos, &target);

				if(quitGame)
					quit = QUIT_HUD;

				int success = attack(game, soldierId, target);

				if(success){
					sendedEvent.roomId = room.roomId;
					sendedEvent.type = M_ATTACK;
					sendedEvent.unitId = soldierId;
					sendedEvent.target.x = target.x;
					sendedEvent.target.y = target.y;
					sendedEvent.clientId = game->currentPlayer;

					char * pseudo = (char*) malloc(75*sizeof(char));
					strcpy(pseudo, readPseudo());
					sendedEvent.playerInfos.pseudo = pseudo;
					jString = serializeEvent(sendedEvent);

					ws->send(jString);
					ws->poll();
					free(jString);
				}
			}

			//Tile selection for other HUD
			else if(newEvent == TILE_SELECTION){
				//Own unit
				int tokenId = checkOwnUnit(*game, selectedTile);
				if(tokenId < game->players[game->currentPlayer].nUnits){	//Cycles units
					switch(game->players[game->currentPlayer].units[tokenId].type){

						case PEASANT:
						quitGame = mPeasantHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;	//This Hud will also be quitted with the "child"
						newEvent = 0;	//Avoids screen refreshing and all previous menus appearing for a frame
						break;

						case SOLDIER:
						quitGame = mSoldierHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;
						newEvent = 0;
						break;
					}
				}
				else if(checkOwnBuilding(*game, selectedTile) < game->players[game->currentPlayer].nBuildings){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;
					}
				}
				else if(checkOwnUnit(*game, selectedTile) < game->players[game->currentPlayer].nUnits){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;
					}
				}
				//Foreign unit selection
				else{

					int ownerId;
					int isUnit = 1;
					tokenId = checkForeignUnit(*game, selectedTile, &ownerId);
					if(ownerId > game->nPlayers){
						tokenId = checkForeignBuilding(*game, selectedTile, &ownerId);
						isUnit = 0;
					}


					if(ownerId < game->nPlayers){
						quitGame = foreignHud(renderer, texture, game, camera, countdown, countdownSec, ownerId, tokenId, isUnit);
						quit = 1;
						newEvent = 0;
					}
				}
			}
		}

		//Countdown
		quit = countdownUpdate(countdown, countdownSec, quit, &newEvent, game);

		//Refresh
		if(newEvent){
			mSoldierDisplay(renderer, texture, *game, *camera, *countdownSec, soldierId, readPseudo());
		}
	}

	Mix_FreeMusic(music);

	return quitGame;
}



//Building Hud
int mBuildingHud(easywsclient::WebSocket * ws, room room, SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int * countdown, int * countdownSec, int buildingId){

	Mix_Music * music = NULL;
	if(game->players[game->currentPlayer].buildings[buildingId].type == BARRACK)
			music = Mix_LoadMUS("resources/sounds/barrack_select.mp3");
	else
		music = Mix_LoadMUS("resources/sounds/city_select1.mp3");

	Mix_PlayMusic( music, 1);


	SDL_Event event;
	int quit = 0;
	int quitGame = 0;	//Return value (quits the entire game)
	int newEvent = 0;
	int success = 0;

	coord selectedTile;
	coord target;
	//int tokenId;
	//int ownerId;

	mEvent sendedEvent;
	char * jString;

	mBuildingDisplay(renderer, texture, *game, *camera, *countdownSec, buildingId, readPseudo());

	while(!quit){
		SDL_Delay(REFRESH_PERIOD);

		//Events
		while(SDL_PollEvent(&event)){
			newEvent = events(event, camera, *game, &selectedTile);

			//Menu
			if(newEvent == MENU){
				quitGame = inGameMenu(renderer, *game);
				quit = quitGame;
				camera->leftClick = 0;
				if(quitGame)
					quit = 1;
			}

			//Cancel button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH-TILE_SIZE*1.5 && event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.5){
				quit = QUIT_HUD;
			}

			//Create unit button
			else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& event.button.x>SCREEN_WIDTH/2-TILE_SIZE*1.75/2 && event.button.x<SCREEN_WIDTH/2+TILE_SIZE*1.75/2
			&& event.button.y>SCREEN_HEIGHT-TILE_SIZE*1.75 && event.button.y<SCREEN_HEIGHT){

				coord tokenPos = game->players[game->currentPlayer].buildings[buildingId].pos;
				quitGame = targetHud(renderer, texture, game, camera, countdown, countdownSec, 0, tokenPos, &target);

				if(quitGame)
					quit = QUIT_HUD;

				switch(game->players[game->currentPlayer].buildings[buildingId].type){
					case CITY:
						success = createPeasant(game, target, buildingId);

						if(success){
							sendedEvent.roomId = room.roomId;
							sendedEvent.type = M_CREATE_PEASANT;
							sendedEvent.unitId = buildingId;
							sendedEvent.target.x = target.x;
							sendedEvent.target.y = target.y;
							sendedEvent.clientId = game->currentPlayer;

							char * pseudo = (char*) malloc(75*sizeof(char));
							strcpy(pseudo, readPseudo());
							sendedEvent.playerInfos.pseudo = pseudo;
							jString = serializeEvent(sendedEvent);

							ws->send(jString);
							ws->poll();
							free(jString);
						}

						break;

					case BARRACK:
						success = createSoldier(game, target, buildingId);

						if(success){
							sendedEvent.roomId = room.roomId;
							sendedEvent.type = M_CREATE_SOLDIER;
							sendedEvent.unitId = buildingId;
							sendedEvent.target.x = target.x;
							sendedEvent.target.y = target.y;
							sendedEvent.clientId = game->currentPlayer;

							char * pseudo = (char*) malloc(75*sizeof(char));
							strcpy(pseudo, readPseudo());
							sendedEvent.playerInfos.pseudo = pseudo;
							jString = serializeEvent(sendedEvent);

							ws->send(jString);
							ws->poll();
							free(jString);
						}

						break;
				}
			}

			//Tile selection for other HUD
			else if(newEvent == TILE_SELECTION){
				//Own unit
				int tokenId = checkOwnUnit(*game, selectedTile);
				if(tokenId < game->players[game->currentPlayer].nUnits){	//Cycles units
					switch(game->players[game->currentPlayer].units[tokenId].type){

						case PEASANT:
						quitGame = mPeasantHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;	//This Hud will also be quitted with the "child"
						newEvent = 0;	//Avoids screen refreshing and all previous menus appearing for a frame
						break;

						case SOLDIER:
						quitGame = mSoldierHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
						quit = 1;
						newEvent = 0;
						break;
					}
				}
				else if(checkOwnBuilding(*game, selectedTile) < game->players[game->currentPlayer].nBuildings){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;


					}
				}
				else if(checkOwnUnit(*game, selectedTile) < game->players[game->currentPlayer].nUnits){	//Cycles buildingss
					tokenId = checkOwnBuilding(*game, selectedTile);
					if(tokenId < game->players[game->currentPlayer].nBuildings){
							quitGame = mBuildingHud(ws, room, renderer, texture, game, camera, countdown, countdownSec, tokenId);
							quit = 1;
							newEvent = 0;


					}
				}
				//Foreign unit selection
				else{

					int ownerId;
					int isUnit = 1;
					tokenId = checkForeignUnit(*game, selectedTile, &ownerId);
					if(ownerId > game->nPlayers){
						tokenId = checkForeignBuilding(*game, selectedTile, &ownerId);
						isUnit = 0;
					}


					if(ownerId < game->nPlayers){
						quitGame = foreignHud(renderer, texture, game, camera, countdown, countdownSec, ownerId, tokenId, isUnit);
						quit = 1;
						newEvent = 0;
					}
				}
			}
		}

		//Countdown
		quit = countdownUpdate(countdown, countdownSec, quit, &newEvent, game);

		//Refresh
		if(newEvent){
			mBuildingDisplay(renderer, texture, *game, *camera, *countdownSec, buildingId, readPseudo());
		}
	}

	Mix_FreeMusic(music);

	return quitGame;
}


//Foreign Hud
//Ingame events listening

class inGameIntermediary {
public:
	struct game * game;
	SDL_Renderer * renderer;
	SDL_Texture * texture;
	view * camera;
	int * quit;
	mEvent event;
	int ownSlot;

    void callbackInGame(const std::string & message, mEvent event, SDL_Renderer * renderer, SDL_Texture * texture, view * camera, int * quit, int ownSlot){

        //Checking if game is starting
        json_object * json = json_tokener_parse(&message[0]);
        json_object * jType = json_object_object_get(json, "type");
		int length;

        //If receiving a typed event
        if(jType != NULL){
            int type = json_object_get_int(jType);
			mEvent event = parseEvent(&message[0]);
            free(jType);

			switch(type){
				case M_MOVEMENT:
					coord * path;
					length = moveUnit(game, event.unitId, event.target, &path);
					if(length){
						movementAnim(renderer, texture, camera, game, path, length, event.unitId);
						free(path);
					}
					break;

				case M_ATTACK:
					attack(game, event.unitId, event.target);
					break;

				case M_CREATE_PEASANT:
					createPeasant(game, event.target, event.unitId);
					break;

				case M_CREATE_SOLDIER:
					createSoldier(game, event.target, event.unitId);
					break;

				case M_CREATE_BARRACK:
					createBarrack(game, event.target, event.unitId);
					break;

				case M_HARVEST:
					collect(game, event.unitId, event.target);
					break;


				case PLAYER_LEAVE_GAME:
				case PLAYER_LEAVE_ROOM:
					//This will quit the foreignHud function
						*quit = 1;
					break;

				case M_END_TURN:
					if(ownSlot != event.clientId){
						*quit = 1;
					}
					break;
			}

        }

    }
};


class inGameFunctor {
  public:
      inGameIntermediary * instance;

    inGameFunctor(inGameIntermediary * instance) : instance(instance) {
    }
    void operator()(const std::string& message) {
		//TODO Set parameters properly
        instance->callbackInGame(message, instance->event, instance->renderer, instance->texture, instance->camera, instance->quit, instance->ownSlot);
    }
};



//In this funcion, we can move the camera & access menu only. Game will check for updates coming from server
int mEnemyPlayerHud(easywsclient::WebSocket * ws, room room,SDL_Renderer * renderer, SDL_Texture * texture, struct game * game, view * camera, int ownSlot){

	SDL_Event event;
	int quit = 0;
	int newEvent = 0;

	coord selectedTile;

	int countdown = TURN_TIME * 1000;	//30 sec in ms
	int countdownSec = TURN_TIME; //Approx in sec for display

	int quitGame = 0;

	inGameIntermediary instance;
	instance.game = game;
	instance.renderer = renderer;
	instance.texture = texture;
	instance.camera = camera;
	instance.quit = &quit;
	instance.ownSlot = ownSlot;

	inGameFunctor functor(&instance);

	//First display before any event
	//XXXFucking bus error
	mBasicDisplay(renderer, texture, *game, *camera, countdownSec, 0, room.players[game->currentPlayer].pseudo);
	SDL_RenderPresent(renderer);

    while(ws->getReadyState() != easywsclient::WebSocket::CLOSED && !quit){
		ws->poll();
		SDL_Delay(REFRESH_PERIOD);
		ws->dispatch(functor);

        while(SDL_PollEvent(&event)){
            newEvent = events(event, camera, *game, &selectedTile);

            switch(newEvent){
                case MENU:
                    quitGame = inGameMenu(renderer, *game);
					if(quitGame)
						quit = QUIT_HUD;

					camera->leftClick = 0;	//Avoids moving camera if menu entered w click and left with Escp
                    break;
			}
		}

		countdownUpdate(&countdown, &countdownSec, quit, &newEvent, game);

        if(newEvent){  //Refresh display if a new event has occured
			mBasicDisplay(renderer, texture, *game, *camera, countdownSec, 0, room.players[game->currentPlayer].pseudo);
			SDL_RenderPresent(renderer);
			newEvent = 0;
		}
    }

	return quitGame;
}
