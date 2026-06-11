#pragma once

/* =====================================================================
   core.h  --  Game types, structures, and logic prototypes
   Rule: NO #include <windows.h> here or in core.c
   ===================================================================== */

#define ARENA_W       800      /* arena width in pixels                  */
#define ARENA_H       600      /* arena height in pixels                 */
#define MAX_ENEMIES   128
#define MAX_BULLETS   256
#define MAX_GEMS      128
#define UPGRADE_COUNT 6        /* number of distinct upgrades            */
#define MAX_SCORES    5        /* top-N entries in the high score table  */

/* --------------------------------------------------------------------- */
/*  Game phases                                                          */
/* --------------------------------------------------------------------- */
typedef enum {
    PHASE_MENU,                /* difficulty selection screen            */
    PHASE_PLAYING,             /* wave in progress                       */
    PHASE_UPGRADE,             /* upgrade selection between waves        */
    PHASE_DEAD,                /* player died                            */
    PHASE_WIN                  /* all waves survived                     */
} GamePhase;

/* --------------------------------------------------------------------- */
/*  Input state  (filled in ui.c, read in core.c)                       */
/* --------------------------------------------------------------------- */
typedef struct {
    int up, down, left, right; /* WASD / arrow keys                      */
    int select1, select2, select3; /* 1/2/3 — upgrade/difficulty choice  */
    int restart;               /* R — restart                            */
} InputState;

/* --------------------------------------------------------------------- */
/*  Player                                                               */
/* --------------------------------------------------------------------- */
typedef struct {
    float x, y;                /* center position                        */
    float speed;               /* pixels per second                      */
    int   hp, maxHp;
    float fireTimer;           /* seconds until next shot                */
    float fireCooldown;        /* base cooldown                          */
    float bulletSpeed;
    int   bulletDamage;
    float invTimer;            /* invincibility timer after hit          */
    float regenTimer;          /* HP-regen timer (upgrade 0)             */
    int   upgrades[UPGRADE_COUNT]; /* stack count of each upgrade        */
} Player;

/* --------------------------------------------------------------------- */
/*  Enemy types                                                          */
/* --------------------------------------------------------------------- */
typedef enum {
    ENEMY_BASIC = 0,           /* slow, low HP                          */
    ENEMY_FAST  = 1,           /* fast, low HP                          */
    ENEMY_TANK  = 2            /* slow, high HP                         */
} EnemyType;

/* --------------------------------------------------------------------- */
/*  Enemy                                                                */
/* --------------------------------------------------------------------- */
typedef struct {
    float     x, y;
    float     speed;
    int       hp, maxHp;
    int       damage;          /* contact damage                         */
    EnemyType type;
    int       alive;           /* 0 = slot is free                       */
} Enemy;

/* --------------------------------------------------------------------- */
/*  Bullet                                                               */
/* --------------------------------------------------------------------- */
typedef struct {
    float x, y;
    float vx, vy;              /* velocity vector                        */
    int   damage;
    int   alive;
    int   hitCount;            /* how many enemies already struck        */
    int   hitIds[16];          /* indices of struck enemies (no re-hit)  */
} Bullet;

/* --------------------------------------------------------------------- */
/*  XP gem                                                               */
/* --------------------------------------------------------------------- */
typedef struct {
    float x, y;
    int   value;
    int   alive;
} XpGem;

/* --------------------------------------------------------------------- */
/*  Upgrade description (for the upgrade selection screen)              */
/* --------------------------------------------------------------------- */
typedef struct {
    int         id;
    const char *name;
    const char *desc;
} UpgradeInfo;

/* --------------------------------------------------------------------- */
/*  High score table  (persisted to scores.dat)                         */
/* --------------------------------------------------------------------- */
typedef struct {
    int score;
    int wave;
    int difficulty;            /* 0=Easy  1=Normal  2=Hard               */
} ScoreEntry;

typedef struct {
    ScoreEntry entries[MAX_SCORES];
    int        count;
} HighScores;

/* --------------------------------------------------------------------- */
/*  Full game state — the single object passed between modules          */
/* --------------------------------------------------------------------- */
typedef struct {
    Player player;
    Enemy   enemies[MAX_ENEMIES];
    Bullet  bullets[MAX_BULLETS];
    XpGem   gems[MAX_GEMS];

    int   wave;                /* current wave number (1-based)          */
    int   totalWaves;          /* waves until victory                    */
    int   enemiesLeft;         /* enemies remaining this wave            */
    float spawnTimer;          /* time until next spawn                  */
    float spawnInterval;       /* seconds between spawns                 */
    float baseSpawnInterval;

    int       score;
    GamePhase phase;

    int   difficulty;          /* 0=Easy  1=Normal  2=Hard               */
    float hpScalePerWave;      /* enemy HP growth per wave               */

    int offerIds[3];           /* upgrade indices offered between waves  */

    InputState input;          /* current input state                    */

    HighScores highScores;     /* persisted score table                  */
} GameState;

/* --------------------------------------------------------------------- */
/*  Public functions — core module                                       */
/* --------------------------------------------------------------------- */
void Core_Init         (GameState *gs);
void Core_SetDifficulty(GameState *gs, int difficulty);  /* 0/1/2 */
void Core_Update       (GameState *gs, float dt);
void Core_StartWave    (GameState *gs);
void Core_ApplyUpgrade (GameState *gs, int upgradeId);

const UpgradeInfo *Core_GetUpgradeInfo(int id);

/* --------------------------------------------------------------------- */
/*  High score functions                                                 */
/* --------------------------------------------------------------------- */
void Scores_Load  (HighScores *hs);
void Scores_Save  (const HighScores *hs);
void Scores_Submit(GameState *gs);   /* insert current result, save file */
