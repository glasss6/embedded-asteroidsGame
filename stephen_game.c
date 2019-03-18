/*
 * stephen_game.c
 *
 *  Created on: Mar 16, 2019
 *      Author: sglas
 */

#include "project_settings.h"
#include "random_int.h"
#include "stddef.h"
#include "strings.h"
#include "game.h"
#include "timing.h"
#include "task.h"
#include "terminal.h"
#include "random_int.h"

#define MAP_WIDTH                   60
#define MAP_HEIGHT                  18

#define MAX_ASTEROIDS               250
#define MAX_ASTEROIDS_PER_COLUMN    MAP_HEIGHT-2
#define MAX_COLUMNS                 MAP_WIDTH-2

#define FIRE_SPEED                  100
#define RECHARGE_RATE               750
#define MAX_SHOTS                   5

/* (1/asteroidSpawnChance) [e.g increasing spawn chance decreases spawn rate] */
#define STARTING_DIFFICULTY         24 

/// game structure
struct stephen_game_t {
    uint8_t x; ///< x coordinate of ship
    uint8_t y; ///< y coordinate of ship
    char c; ///< character of ship
    uint8_t health;
    uint8_t shotCooldown;
    int score; ///< score for the round
    int shotsFired; ///< shots fired for the round
    uint8_t id; ///< ID of game=
};
static struct stephen_game_t game;

/* Create 2D array size of the playable area to map asteroids */
static uint8_t asteroids[MAP_WIDTH-1][MAP_HEIGHT-1];

/* Shots fired */
static char_object_t shots[MAX_SHOTS];

/* note the user doesn't need to access these functions directly so they are
   defined here instead of in the .h file
   further they are made static so that no other files can access them
   ALSO OTHER MODULES CAN USE THESE SAME NAMES SINCE THEY ARE STATIC */
static void Callback(int argc, char * argv[]);
static void Receiver(uint8_t c);

static void Play(void);
static void Help(void);
static void MoveRight(void);
static void MoveLeft(void);
static void MoveUp(void);
static void MoveDown(void);
static void UpdateScore(void);
static void UpdateHealth(void);
static void UpdateShotCooldown(void);
static void GenerateAsteroidColumn(void);
static void ShiftAsteroidColumns(void);
static void GenerateAndShift(void);
static void ResetScreenColor(void);
static void IncreaseScore(void);
static void Shoot(void);
static void MoveRightShot(char_object_t * o);
static void DecreaseCooldown(void);
static void UpdateDifficulty(void);
static void GameOver(void);

static uint8_t gRechargingWeapon = 0;
static uint8_t asteroidSpawnProbability = STARTING_DIFFICULTY; // (1/asteroidSpawnChance) [e.g increasing spawn chance decreases spawn rate]

void StephenGame_Init(void) {
    // Register the module with the game system and give it the name "MUH3"
    game.id = Game_Register("FLY1", "space pilot", Play, Help);
    // Register a callback with the game system.
    // this is only needed if your game supports more than "play", "help" and "highscores"
    Game_RegisterCallback(game.id, Callback);
}

void Play(void) {
    volatile uint8_t i, j;

    // clear the screen
    Game_ClearScreen();
    // draw a box around our map
    Game_DrawRect(0, 0, MAP_WIDTH, MAP_HEIGHT);

    /* Initialize game variables */
    for(i = 0; i < MAP_WIDTH-1; i++) {
        for(j = 0; j < MAP_HEIGHT-1; j++) {
            asteroids[i][j] = 0;
        }
    }

    /* Set default position of space ship */
    game.x = 1;
    game.y = MAP_HEIGHT / 2;
    game.c = '>';
    game.score = 0;
    game.shotsFired = 0;
    game.health = 3;
    game.shotCooldown = 6;

    /* Draw the space ship */
    Game_SetColor(ForegroundCyan);
    Game_CharXY(game.c, game.x, game.y);
    Game_SetColor(ForegroundWhite);
    Game_RegisterPlayer1Receiver(Receiver);

    /* Hide the cursor */
    Game_HideCursor();

    /* Set cursor below the game view and show score */
    UpdateScore();

    /* Show starting health */
    UpdateHealth();

    /* Show starting shoot cooldown */
    UpdateShotCooldown();

    /* Show starting difficulty */
    UpdateDifficulty();

    Task_Schedule(GenerateAndShift, 0, 500, 1000);
    Task_Schedule(IncreaseScore, 0, 2500, 2500);
}

void GameOver(void) {
    Task_Remove(GenerateAndShift, 0);
    Task_Remove(IncreaseScore, 0);

    volatile uint8_t i;
    char_object_t * shot = 0;
    for(i = 0; i < MAX_SHOTS; i++) {
        if(shots[i].status == 1) { // if active shot
            shot = &shots[i];
            shot->status = 0;
            Task_Remove((task_t)MoveRightShot, shot);
        }
    }

    Game_SetColor(ForegroundRed);
    Game_CharXY('\r', 0, MAP_HEIGHT + 1);
    Game_Printf("Game Over! Final score: %d, Total shots fired: %d", game.score, game.shotsFired);
    // unregister the receiver used to run the game
    Game_UnregisterPlayer1Receiver(Receiver);
    // show cursor (it was hidden at the beginning
    Game_CharXY('\r', 0, MAP_HEIGHT + 5);
    Game_ShowCursor();
    Game_GameOver();
}

void IncreaseScore(void) {
    game.score += 1;
    Task_Queue(UpdateScore, 0);
}

void GenerateAndShift(void) {
    GenerateAsteroidColumn();
    ShiftAsteroidColumns();
}

void GenerateAsteroidColumn(void) {
    uint8_t randomCheck, randomType;
    volatile uint8_t i;
    for(i = 1; i < MAP_HEIGHT; i++) {
        randomCheck = random_int(1, asteroidSpawnProbability);
        if(randomCheck == 1) { // create asteroid if probability check
            randomType = random_int(1, 2);
            if(randomType == 1) {
                asteroids[MAX_COLUMNS][i] = 1; // o type
                Game_CharXY('o', MAX_COLUMNS, i);
            }
            else {
                asteroids[MAX_COLUMNS][i] = 2; // O type
                Game_CharXY('O', MAX_COLUMNS, i);
            }
        }
        else { // did not pass check, overwrite with blank
            asteroids[MAX_COLUMNS][i] = 0;
            Game_CharXY(' ', MAX_COLUMNS, i);
        }
    }
}

void ShiftAsteroidColumns(void) {
    volatile uint8_t column, i;
    for(column = 1; column < MAP_WIDTH-1; column++) {
        for(i = 1; i < MAP_HEIGHT; i++) {
            asteroids[column][i] = asteroids[column+1][i];
            if(asteroids[column][i] == 0 && !(game.x == column && game.y == i)) {
                // check if it overlaps a shot
                uint8_t checkShots, foundShot = 0;
                for(checkShots = 0; checkShots < MAX_SHOTS; checkShots++) {
                    if(shots[checkShots].status && shots[checkShots].x == column && shots[checkShots].y == i) {
                        foundShot = 1;
                        break;
                    }
                }
                if(!foundShot) Game_CharXY(' ', column, i);
            }
            else if(asteroids[column][i] == 1) {
                if(game.x == column && game.y == i) { // if collided
                    asteroids[column][i] = 0; // destroy asteroid
                    Game_SetColor(BackgroundRed);
                    Game_CharXY('*', column, i);
                    Game_SetColor(BackgroundBlack);
                    Game_Bell();
                    if(game.health > 0) game.health--;
                    Task_Queue(UpdateHealth, 0);
                    Task_Schedule(ResetScreenColor, 0, 250, 0);
                }
                else Game_CharXY('o', column, i);
            }
            else if(asteroids[column][i] == 2) {
                if(game.x == column && game.y == i) { // if collided
                    asteroids[column][i] = 0; // destroy asteroid
                    Game_SetColor(BackgroundRed);
                    Game_CharXY('*', column, i);
                    Game_SetColor(BackgroundBlack);
                    Game_Bell();
                    if(game.health > 0) game.health--;
                    Task_Queue(UpdateHealth, 0);
                    Task_Schedule(ResetScreenColor, 0, 250, 0);
                }
                else Game_CharXY('O', column, i);
            }
        }
    }
}

void Shoot(void) {
    if(game.shotCooldown >= 3) { // at least 3 to shoot one
        char_object_t * shot = 0;

        volatile uint8_t i;
        // find and unused bullet
        for(i = 0; i < MAX_SHOTS; i++) if(shots[i].status == 0) shot = &shots[i];
        if(shot) { // if found empty slot
            game.shotCooldown -= 3;
            shot->status = 1;
            shot->y = game.y;
            shot->x = game.x+1;
            Game_SetColor(ForegroundYellow);
            Game_CharXY('-', game.x+1, game.y);
            Game_SetColor(ForegroundWhite);
            game.shotsFired++;
            Task_Queue((task_t)UpdateShotCooldown, 0);
            Task_Schedule((task_t)MoveRightShot, shot, FIRE_SPEED, FIRE_SPEED);

            if(!gRechargingWeapon) { // if task is not already assigned to recharge
                gRechargingWeapon = 1;
                Task_Schedule((task_t)DecreaseCooldown, 0, RECHARGE_RATE, RECHARGE_RATE);
            }
        }
    }
}

void MoveRightShot(char_object_t * o) {
    if (o->x < MAP_WIDTH-2) { // if not at edge
        // clear location
        Game_CharXY(' ', o->x, o->y);
        o->x++;
        if(asteroids[o->x][o->y]) { // if collided
            asteroids[o->x][o->y] = 0;
            Game_SetColor(BackgroundYellow);
            Game_CharXY('*', o->x, o->y);
            Game_SetColor(BackgroundBlack);
            Game_Bell();
            o->status = 0;
            game.score += 1;
            Task_Remove((task_t)MoveRightShot, o);
            Task_Queue((task_t)UpdateScore, 0);
        }
        else { // if no collision, move shot
            Game_SetColor(ForegroundYellow);
            Game_CharXY('-', o->x, o->y);
            Game_SetColor(ForegroundWhite);
        }
    }
    else { // at edge
        // clear the shot
        Game_CharXY(' ', o->x, o->y);
        o->status = 0;
        Task_Remove((task_t)MoveRightShot, o);
    }
}

void DecreaseCooldown(void) {
    if(game.shotCooldown < 6) {
        game.shotCooldown++;
        Task_Queue(UpdateShotCooldown, 0);
    }
    else { // greater than or equal to 6
        gRechargingWeapon = 0;
        Task_Remove((task_t)DecreaseCooldown, 0);
    }
}

void Help(void) {
    Game_Printf("WASD to move the spaceship\r\nSPACEBAR to FIRE\r\n");
    Game_Printf("Weapon recharges over time. Difficulty increases with score.\r\n");
}

void UpdateScore(void) {
    /* Set cursor below the game view and show score */
    Game_CharXY('\r', 0, MAP_HEIGHT + 1);
    Game_Printf("Score: %d", game.score);

    /* Conditional block below is simply just setting the difficulty
    based on whatever score the player achieves */
    if(game.score == 35) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 1;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 55) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 2;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 65) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 3;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 80) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 4;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 100) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 5;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 120) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 6;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 140) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 7;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 160) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 8;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 180) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 9;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
    else if(game.score == 200) {
        asteroidSpawnProbability = STARTING_DIFFICULTY - 10;
        Task_Queue((task_t)UpdateDifficulty, 0);
    }
}

void UpdateDifficulty(void) {
    /* Set cursor below the game view and show difficulty */
    uint8_t score = (STARTING_DIFFICULTY-asteroidSpawnProbability) + 1;
    Game_CharXY('\r', 0, MAP_HEIGHT + 4);
    Game_Printf("Difficulty: %d", score);
}

void UpdateHealth(void) {
    Game_CharXY('\r', 0, MAP_HEIGHT + 2);
    Game_Printf("Health: ");
    Game_SetColor(ForegroundRed);
    if(game.health >= 3) Game_Printf("<3 <3 <3");
    else if(game.health == 2) Game_Printf("<3 <3   ");
    else if(game.health == 1) Game_Printf("<3      ");
    else if(game.health == 0) {
        Game_Printf(":(      ");
        GameOver();
    }
    Game_SetColor(ForegroundWhite);
}

void ResetScreenColor(void) {
    Game_SetColor(ForegroundCyan);
    Game_CharXY(game.c, game.x, game.y); // reset back to spaceship too
    Game_SetColor(ForegroundWhite);
}

void UpdateShotCooldown(void) {
    Game_CharXY('\r', 0, MAP_HEIGHT + 3);
    Game_Printf("Weapon Charge: [");
    if(game.shotCooldown == 6) {
        Game_SetColor(ForegroundGreen);
        Game_Printf("++++++");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 5) {
        Game_SetColor(ForegroundYellow);
        Game_Printf("+++++ ");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 4) {
        Game_SetColor(ForegroundYellow);
        Game_Printf("++++  ");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 3) {
        Game_SetColor(ForegroundYellow);
        Game_Printf("+++   ");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 2) {
        Game_SetColor(ForegroundRed);
        Game_Printf("++    ");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 1) {
        Game_SetColor(ForegroundRed);
        Game_Printf("+     ");
        Game_SetColor(ForegroundWhite);
        Game_Printf("]");
    }
    else if(game.shotCooldown == 0) {
        Game_Printf("      ]");
    }
}

void MoveRight(void) {
    // make sure we can move right
    if (game.x < MAP_WIDTH - 3) {
        /* Clear location and update */
        Game_CharXY(' ', game.x, game.y);
        game.x++;
        if(asteroids[game.x][game.y]) { // moved into asteroid
            asteroids[game.x][game.y] = 0; // destroy asteroid
            Game_SetColor(BackgroundRed);
            Game_CharXY('*', game.x, game.y);
            Game_SetColor(BackgroundBlack);
            Game_Bell();
            if(game.health > 0) game.health--;
            Task_Queue(UpdateHealth, 0);
            Task_Schedule(ResetScreenColor, 0, 250, 0);
        }
        else {
            Game_SetColor(ForegroundCyan);
            Game_CharXY(game.c, game.x, game.y);
            Game_SetColor(ForegroundWhite);
        }
    }
}

void MoveLeft(void) {
    // make sure we can move right
    if (game.x > 1) {
        /* Clear location and update */
        Game_CharXY(' ', game.x, game.y);
        game.x--;
        if(asteroids[game.x][game.y]) { // moved into asteroid
            asteroids[game.x][game.y] = 0; // destroy asteroid
            Game_SetColor(BackgroundRed);
            Game_CharXY('*', game.x, game.y);
            Game_SetColor(BackgroundBlack);
            Game_Bell();
            if(game.health > 0) game.health--;
            Task_Queue(UpdateHealth, 0);
            Task_Schedule(ResetScreenColor, 0, 250, 0);
        }
        else {
            Game_SetColor(ForegroundCyan);
            Game_CharXY(game.c, game.x, game.y);
            Game_SetColor(ForegroundWhite);
        }
    }
}

void MoveDown(void) {
    // make sure we can move up
    if (game.y < MAP_HEIGHT - 1) {
        /* Clear location and update */
        Game_CharXY(' ', game.x, game.y);
        game.y++;
        if(asteroids[game.x][game.y]) { // moved into asteroid
            asteroids[game.x][game.y] = 0; // destroy asteroid
            Game_SetColor(BackgroundRed);
            Game_CharXY('*', game.x, game.y);
            Game_SetColor(BackgroundBlack);
            Game_Bell();
            if(game.health > 0) game.health--;
            Task_Queue(UpdateHealth, 0);
            Task_Schedule(ResetScreenColor, 0, 250, 0);
        }
        else {
            Game_SetColor(ForegroundCyan);
            Game_CharXY(game.c, game.x, game.y);
            Game_SetColor(ForegroundWhite);
        }
    }
}

void MoveUp(void) {
    // make sure we can move right
    if (game.y > 1) {
        /* Clear location and update */
        Game_CharXY(' ', game.x, game.y);
        game.y--;
        if(asteroids[game.x][game.y]) { // moved into asteroid
            asteroids[game.x][game.y] = 0; // destroy asteroid
            Game_SetColor(BackgroundRed);
            Game_CharXY('*', game.x, game.y);
            Game_SetColor(BackgroundBlack);
            Game_Bell();
            if(game.health > 0) game.health--;
            Task_Queue(UpdateHealth, 0);
            Task_Schedule(ResetScreenColor, 0, 250, 0);
        }
        else {
            Game_SetColor(ForegroundCyan);
            Game_CharXY(game.c, game.x, game.y);
            Game_SetColor(ForegroundWhite);
        }
    }
}

void Receiver(uint8_t c) {
    switch (c) {
        case 'a':
        case 'A':
            MoveLeft();
            break;
        case 'd':
        case 'D':
            MoveRight();
            break;
        case 'w':
        case 'W':
            MoveUp();
            break;
        case 's':
        case 'S':
            MoveDown();
            break;
        case ' ':
            Shoot();
            break;
        /*case '\r':
            GameOver();
            break;*/
        default:
            break;
    }
}

void Callback(int argc, char * argv[]) {
    // "play" and "help" are called automatically so just process "reset" here
    if(argc == 0) Game_Log(game.id, "too few args");
    if(strcasecmp(argv[0],"reset") == 0) {
        // reset scores
        game.score = 0;
        Game_Log(game.id, "Scores reset");
    }
    else Game_Log(game.id, "command not supported");
}
