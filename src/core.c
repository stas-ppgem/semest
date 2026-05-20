/* =====================================================================
   core.c  --  Вся игровая логика
   БЕЗ #include <windows.h> — только стандартная библиотека C
   ===================================================================== */

#include "core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------- */
/*  Описания апгрейдов                                                   */
/* --------------------------------------------------------------------- */
static const UpgradeInfo s_upgrades[UPGRADE_COUNT] = {
    { 0, "+1 HP Regen",      "Восстанавливать 1 HP каждые 5 сек"  },
    { 1, "Скорость +20%",    "Игрок двигается быстрее"            },
    { 2, "Урон пуль x1.5",   "Пули наносят больше урона"          },
    { 3, "Скорострельность", "Кулдаун стрельбы уменьшен на 25%"   },
    { 4, "Пробитие",         "Пули пробивают 2 врагов"            },
    { 5, "+30 Max HP",       "Увеличивает максимальный запас HP"   },
};

const UpgradeInfo *Core_GetUpgradeInfo(int id) {
    if (id < 0 || id >= UPGRADE_COUNT) return NULL;
    return &s_upgrades[id];
}

/* --------------------------------------------------------------------- */
/*  Вспомогательные функции                                              */
/* --------------------------------------------------------------------- */

/* Генерация случайного float в диапазоне [lo, hi) */
static float RandF(float lo, float hi) {
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

/* Нормализация вектора */
static void Normalize(float *vx, float *vy) {
    float len = sqrtf((*vx) * (*vx) + (*vy) * (*vy));
    if (len > 0.0001f) { *vx /= len; *vy /= len; }
}

/* AABB-коллизия двух прямоугольников (x,y — центр) */
static int CheckAABB(float x1, float y1, float w1, float h1,
                     float x2, float y2, float w2, float h2) {
    return (fabsf(x1 - x2) < (w1 + w2) * 0.5f) &&
           (fabsf(y1 - y2) < (h1 + h2) * 0.5f);
}

/* Поиск ближайшего живого врага (возвращает индекс или -1) */
static int FindNearestEnemy(const GameState *gs) {
    int   best  = -1;
    float bestD = 1e30f;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!gs->enemies[i].alive) continue;
        float dx = gs->enemies[i].x - gs->player.x;
        float dy = gs->enemies[i].y - gs->player.y;
        float d  = dx*dx + dy*dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

/* Добавить пулю в массив */
static void SpawnBullet(GameState *gs, float vx, float vy) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (gs->bullets[i].alive) continue;
        gs->bullets[i].x      = gs->player.x;
        gs->bullets[i].y      = gs->player.y;
        gs->bullets[i].vx     = vx;
        gs->bullets[i].vy     = vy;
        gs->bullets[i].damage = gs->player.bulletDamage;
        gs->bullets[i].alive  = 1;
        break;
    }
}

/* Добавить гем опыта */
static void SpawnGem(GameState *gs, float x, float y, int value) {
    for (int i = 0; i < MAX_GEMS; i++) {
        if (gs->gems[i].alive) continue;
        gs->gems[i].x     = x;
        gs->gems[i].y     = y;
        gs->gems[i].value = value;
        gs->gems[i].alive = 1;
        break;
    }
}

/* Параметры врага по типу + масштаб HP по номеру волны.
   Формула: базовый HP * (1 + 0.2 * (wave-1))
   Волна 1 = x1.0, волна 5 = x1.8, волна 10 = x2.8               */
static void EnemyDefaults(Enemy *e, EnemyType type, int wave, float hpScalePerWave) {
    float hpScale = 1.0f + hpScalePerWave * (wave - 1);
    e->type = type;
    switch (type) {
    case ENEMY_BASIC:
        e->speed = 60.0f;  e->hp = e->maxHp = (int)(20 * hpScale);  e->damage = 10; break;
    case ENEMY_FAST:
        e->speed = 130.0f; e->hp = e->maxHp = (int)(10 * hpScale);  e->damage = 8;  break;
    case ENEMY_TANK:
        e->speed = 35.0f;  e->hp = e->maxHp = (int)(80 * hpScale);  e->damage = 20; break;
    }
}

/* Спавн врага на рандомном краю арены */
static void SpawnEnemy(GameState *gs) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (gs->enemies[i].alive) continue;

        /* 4 края: 0=top 1=bottom 2=left 3=right */
        int side = rand() % 4;
        switch (side) {
        case 0: gs->enemies[i].x = RandF(0, ARENA_W); gs->enemies[i].y = -20; break;
        case 1: gs->enemies[i].x = RandF(0, ARENA_W); gs->enemies[i].y = ARENA_H + 20; break;
        case 2: gs->enemies[i].x = -20;                gs->enemies[i].y = RandF(0, ARENA_H); break;
        case 3: gs->enemies[i].x = ARENA_W + 20;       gs->enemies[i].y = RandF(0, ARENA_H); break;
        }

        /* В поздних волнах чаще танки и быстрые */
        EnemyType type = ENEMY_BASIC;
        if (gs->wave >= 3 && rand() % 4 == 0) type = ENEMY_FAST;
        if (gs->wave >= 5 && rand() % 6 == 0) type = ENEMY_TANK;

        EnemyDefaults(&gs->enemies[i], type, gs->wave, gs->hpScalePerWave);
        gs->enemies[i].alive = 1;
        break;
    }
}

/* --------------------------------------------------------------------- */
/*  Предложить 3 случайных уникальных апгрейда                          */
/* --------------------------------------------------------------------- */
static void MakeUpgradeOffer(GameState *gs) {
    int picked[3] = { -1, -1, -1 };
    int count = 0;
    int attempts = 0;
    while (count < 3 && attempts < 100) {
        int id = rand() % UPGRADE_COUNT;
        int dup = 0;
        for (int i = 0; i < count; i++) if (picked[i] == id) { dup = 1; break; }
        if (!dup) picked[count++] = id;
        attempts++;
    }
    gs->offerIds[0] = picked[0];
    gs->offerIds[1] = picked[1];
    gs->offerIds[2] = picked[2];
}

/* --------------------------------------------------------------------- */
/*  Core_Init  —  сбросить состояние в начало игры                      */
/* --------------------------------------------------------------------- */
void Core_Init(GameState *gs) {
    srand((unsigned)time(NULL));
    memset(gs, 0, sizeof(*gs));

    gs->player.x            = ARENA_W * 0.5f;
    gs->player.y            = ARENA_H * 0.5f;
    gs->player.speed        = 180.0f;
    gs->player.hp           = 100;
    gs->player.maxHp        = 100;
    gs->player.fireCooldown = 0.35f;  /* секунд между выстрелами       */
    gs->player.bulletSpeed  = 400.0f;
    gs->player.bulletDamage = 10;

    gs->totalWaves       = 10;
    gs->spawnInterval    = 1.5f;
    gs->hpScalePerWave   = 0.2f;      /* default: нормальная            */
    gs->phase            = PHASE_MENU;

    //Core_StartWave(gs);
}

/* --------------------------------------------------------------------- */
/*  Core_SetDifficulty  —  запомнить сложность и перейти к игре         */
/* --------------------------------------------------------------------- */
void Core_SetDifficulty(GameState *gs, int difficulty) {
    gs->difficulty = difficulty;
    switch (difficulty) {
    case 0: gs->hpScalePerWave = 0.1f; break; /* лёгкая:   +10% HP за волну */
    case 1: gs->hpScalePerWave = 0.2f; break; /* нормальная: +20% HP за волну */
    case 2: gs->hpScalePerWave = 0.35f; break;/* сложная:  +35% HP за волну */
    }
    Core_StartWave(gs);
}

/* --------------------------------------------------------------------- */
/*  Core_StartWave  —  подготовить следующую волну                      */
/* --------------------------------------------------------------------- */
void Core_StartWave(GameState *gs) {
    gs->wave++;
    /* Врагов на волне: 5 + 3 за каждую волну */
    gs->enemiesLeft   = 5 + gs->wave * 3;
    gs->spawnTimer    = 0.0f;
    gs->spawnInterval = 1.5f - gs->wave * 0.08f;
    if (gs->spawnInterval < 0.3f) gs->spawnInterval = 0.3f;

    /* Очищаем пули, но оставляем гемы (игрок мог не подобрать) */
    memset(gs->bullets, 0, sizeof(gs->bullets));
    memset(gs->enemies, 0, sizeof(gs->enemies));

    gs->phase = PHASE_PLAYING;
}

/* --------------------------------------------------------------------- */
/*  Core_ApplyUpgrade  —  применить выбранный апгрейд                   */
/* --------------------------------------------------------------------- */
void Core_ApplyUpgrade(GameState *gs, int upgradeId) {
    if (upgradeId < 0 || upgradeId >= UPGRADE_COUNT) return;
    gs->player.upgrades[upgradeId]++;

    switch (upgradeId) {
    case 0: /* HP regen — обрабатывается в Core_Update */         break;
    case 1: gs->player.speed        *= 1.20f;                     break;
    case 2: gs->player.bulletDamage  = (int)(gs->player.bulletDamage * 1.5f); break;
    case 3: gs->player.fireCooldown *= 0.75f;                     break;
    case 4: /* пробитие — обрабатывается в логике пуль */         break;
    case 5:
        gs->player.maxHp += 30;
        gs->player.hp    += 30;
        break;
    }

    /* После выбора: есть ещё волны? */
    if (gs->wave >= gs->totalWaves) {
        gs->phase = PHASE_WIN;
    } else {
        Core_StartWave(gs);
    }
}

/* --------------------------------------------------------------------- */
/*  Core_Update  —  главный тик логики (вызывается каждый кадр)         */
/* --------------------------------------------------------------------- */
void Core_Update(GameState *gs, float dt) {
    if (gs->phase == PHASE_MENU)    return;
    if (gs->phase != PHASE_PLAYING) return;

    Player     *p   = &gs->player;
    InputState *inp = &gs->input;

    /* -- Движение игрока -------------------------------------------- */
    float dx = 0, dy = 0;
    if (inp->left)  dx -= 1.0f;
    if (inp->right) dx += 1.0f;
    if (inp->up)    dy -= 1.0f;
    if (inp->down)  dy += 1.0f;
    Normalize(&dx, &dy);

    p->x += dx * p->speed * dt;
    p->y += dy * p->speed * dt;

    /* Ограничение арены (игрок — квадрат 24x24) */
    if (p->x <  12)         p->x =  12;
    if (p->x >  ARENA_W-12) p->x =  ARENA_W-12;
    if (p->y <  12)         p->y =  12;
    if (p->y >  ARENA_H-12) p->y =  ARENA_H-12;

    /* -- Таймер неуязвимости ---------------------------------------- */
    if (p->invTimer > 0) p->invTimer -= dt;

    /* -- HP Regen (апгрейд 0) --------------------------------------- */
    /* Простейший вариант: накапливаем время, каждые 5 сек +1 HP        */
    /* Реализуется через счётчик тиков (добавь поле если нужно)         */

    /* -- Стрельба: авто по ближайшему врагу ------------------------- */
    p->fireTimer -= dt;
    if (p->fireTimer <= 0.0f) {
        int target = FindNearestEnemy(gs);
        if (target >= 0) {
            float vx = gs->enemies[target].x - p->x;
            float vy = gs->enemies[target].y - p->y;
            Normalize(&vx, &vy);
            vx *= p->bulletSpeed;
            vy *= p->bulletSpeed;
            SpawnBullet(gs, vx, vy);
            p->fireTimer = p->fireCooldown;
        }
    }

    /* -- Спавн врагов волны ----------------------------------------- */
    if (gs->enemiesLeft > 0) {
        gs->spawnTimer -= dt;
        if (gs->spawnTimer <= 0.0f) {
            SpawnEnemy(gs);
            gs->enemiesLeft--;
            gs->spawnTimer = gs->spawnInterval;
        }
    }

    /* -- Движение врагов и урон игроку ------------------------------ */
    int piercing = gs->player.upgrades[4]; /* апгрейд пробития         */

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;

        /* Движение к игроку */
        float ex = p->x - e->x;
        float ey = p->y - e->y;
        Normalize(&ex, &ey);
        e->x += ex * e->speed * dt;
        e->y += ey * e->speed * dt;

        /* Коллизия с игроком */
        if (p->invTimer <= 0.0f &&
            CheckAABB(p->x, p->y, 24, 24, e->x, e->y, 28, 28)) {
            p->hp      -= e->damage;
            p->invTimer = 0.8f;   /* 0.8 сек неуязвимости */
            if (p->hp <= 0) { p->hp = 0; gs->phase = PHASE_DEAD; return; }
        }
    }

    /* -- Движение пуль и попадания ---------------------------------- */
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet *b = &gs->bullets[i];
        if (!b->alive) continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        /* Вылетела за арену */
        if (b->x < -40 || b->x > ARENA_W+40 ||
            b->y < -40 || b->y > ARENA_H+40) {
            b->alive = 0; continue;
        }

        /* Проверяем попадания в каждого врага */
        int hits = 0;
        for (int j = 0; j < MAX_ENEMIES; j++) {
            Enemy *e = &gs->enemies[j];
            if (!e->alive) continue;
            if (!CheckAABB(b->x, b->y, 8, 8, e->x, e->y, 28, 28)) continue;

            e->hp -= b->damage;
            hits++;

            if (e->hp <= 0) {
                /* Враг убит — дроп гема */
                int gemValue = (e->type == ENEMY_TANK) ? 3 : 1;
                SpawnGem(gs, e->x, e->y, gemValue);
                gs->score += (e->type == ENEMY_TANK) ? 30 : 10;
                e->alive = 0;
            }

            /* Без пробития — пуля исчезает после 1 попадания */
            if (!piercing || hits > 1) { b->alive = 0; break; }
        }
    }

    /* -- Подбор гемов ----------------------------------------------- */
    for (int i = 0; i < MAX_GEMS; i++) {
        XpGem *g = &gs->gems[i];
        if (!g->alive) continue;
        if (CheckAABB(p->x, p->y, 32, 32, g->x, g->y, 14, 14)) {
            p->xp   += g->value;
            g->alive = 0;
        }
    }

    /* -- Конец волны: все враги убиты и больше не спавнятся ---------- */
    if (gs->enemiesLeft == 0) {
        int anyAlive = 0;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (gs->enemies[i].alive) { anyAlive = 1; break; }
        if (!anyAlive) {
            MakeUpgradeOffer(gs);
            gs->phase = PHASE_UPGRADE;
        }
    }
}
