/* =====================================================================
   ui.c  --  Win32 окно, ввод, GDI-рендер с двойной буферизацией
   ===================================================================== */

#include "ui.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/*  Цвета (COLORREF = 0x00BBGGRR)                                       */
/* --------------------------------------------------------------------- */
#define COL_BG          RGB( 18,  18,  24)   /* фон арены                */
#define COL_PLAYER      RGB( 80, 200, 120)   /* игрок                    */
#define COL_PLAYER_INV  RGB(255, 255,  80)   /* мигание при неуязвимости */
#define COL_BULLET      RGB(255, 240,  60)   /* пуля                     */
#define COL_ENEMY_BASIC RGB(220,  60,  60)   /* базовый враг             */
#define COL_ENEMY_FAST  RGB(255, 140,  20)   /* быстрый враг             */
#define COL_ENEMY_TANK  RGB(140,  60, 200)   /* танк                     */
#define COL_GEM         RGB( 60, 220, 255)   /* гем опыта                */
#define COL_HP_BG       RGB( 60,  20,  20)   /* фон полоски HP           */
#define COL_HP_FG       RGB(220,  50,  50)   /* заполнение HP            */
#define COL_TEXT        RGB(220, 220, 220)   /* основной текст           */
#define COL_WAVE_TEXT   RGB(255, 200,  60)   /* текст номера волны       */
#define COL_UPGRADE_BG  RGB( 30,  30,  45)   /* фон карточки апгрейда   */
#define COL_UPGRADE_HL  RGB( 60, 100, 180)   /* выделение карточки       */
#define COL_DEAD_BG     RGB( 30,   0,   0)   /* оверлей смерти           */

static HBITMAP s_sprPlayer = NULL;

/* --------------------------------------------------------------------- */
/*  WndProc — обработчик Win32-сообщений                                */
/* --------------------------------------------------------------------- */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* --------------------------------------------------------------------- */
/*  UI_CreateWindow                                                      */
/* --------------------------------------------------------------------- */
HWND UI_CreateWindow(HINSTANCE hInst, int width, int height, const char *title) {
    WNDCLASSEXA wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "BrotolikeWnd";
    if (!RegisterClassExA(&wc)) return NULL;


    /* Подбираем размер окна с учётом заголовка и рамки */
    RECT rc = { 0, 0, width, height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(
        0, "BrotolikeWnd", title,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInst, NULL
    );
    if (!hwnd) return NULL;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    /* Load sprites after window is created */
    s_sprPlayer = (HBITMAP)LoadImageA(NULL, "assets/player.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    if (!s_sprPlayer) MessageBoxA(NULL, "player.bmp not found!", "Error", MB_OK);
    return hwnd;
}

/* --------------------------------------------------------------------- */
/*  UI_Destroy  —  освободить GDI-ресурсы (расширяй по мере роста кода) */
/* --------------------------------------------------------------------- */
void UI_Destroy(void) {
    if (s_sprPlayer) { DeleteObject(s_sprPlayer);     s_sprPlayer = NULL; }
    /* Здесь удалять шрифты, кисти и прочие объекты, созданные при     */
    /* инициализации. Сейчас они создаются и удаляются внутри Render.  */
}

/* --------------------------------------------------------------------- */
/*  UI_PollInput  —  состояние клавиш ? gs->input                      */
/* --------------------------------------------------------------------- */
void UI_PollInput(GameState *gs) {
    InputState *inp = &gs->input;
    inp->up      = (GetAsyncKeyState('W')      & 0x8000) || (GetAsyncKeyState(VK_UP)    & 0x8000);
    inp->down    = (GetAsyncKeyState('S')      & 0x8000) || (GetAsyncKeyState(VK_DOWN)  & 0x8000);
    inp->left    = (GetAsyncKeyState('A')      & 0x8000) || (GetAsyncKeyState(VK_LEFT)  & 0x8000);
    inp->right   = (GetAsyncKeyState('D')      & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000);
    inp->select1 = (GetAsyncKeyState('1')      & 0x8000);
    inp->select2 = (GetAsyncKeyState('2')      & 0x8000);
    inp->select3 = (GetAsyncKeyState('3')      & 0x8000);
    inp->restart = (GetAsyncKeyState('R')      & 0x8000);
}

/* --------------------------------------------------------------------- */
/*  Внутренние хелперы рендера                                          */
/* --------------------------------------------------------------------- */

/* Залить прямоугольник одним цветом */
static void FillRect2(HDC hdc, int x, int y, int w, int h, COLORREF col) {
    HBRUSH br = CreateSolidBrush(col);
    RECT   rc = { x, y, x+w, y+h };
    FillRect(hdc, &rc, br);
    DeleteObject(br);          /* обязательно — иначе утечка GDI       */
}

static void DrawSprite(HDC hdc, HBITMAP spr, int x, int y, int w, int h) {
    if (!spr) return;
    HDC hdcBmp = CreateCompatibleDC(hdc);
    HBITMAP old = SelectObject(hdcBmp, spr);
    BitBlt(hdc, x, y, w, h, hdcBmp, 0, 0, SRCCOPY);  /* без прозрачности */
    SelectObject(hdcBmp, old);
    DeleteDC(hdcBmp);
}

/* Нарисовать прямоугольник (рамку) */
static void DrawRect2(HDC hdc, int x, int y, int w, int h, COLORREF col, int thick) {
    HPEN pen = CreatePen(PS_SOLID, thick, col);
    HPEN old = SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x+w, y+h);
    SelectObject(hdc, old);
    DeleteObject(pen);
}

/* Вывести текст с заданным цветом */
static void DrawText2(HDC hdc, int x, int y, const char *str, COLORREF col) {
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, str, (int)strlen(str));
}

/* Вывести текст по центру прямоугольника */
static void DrawTextCenter(HDC hdc, int x, int y, int w, int h,
                            const char *str, COLORREF col) {
    SIZE sz;
    GetTextExtentPoint32A(hdc, str, (int)strlen(str), &sz);
    int tx = x + (w - sz.cx) / 2;
    int ty = y + (h - sz.cy) / 2;
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, tx, ty, str, (int)strlen(str));
}

/* Полоска прогресса (HP, опыт) */
static void DrawBar(HDC hdc, int x, int y, int w, int h,
                    int cur, int max, COLORREF colBg, COLORREF colFg) {
    FillRect2(hdc, x, y, w, h, colBg);
    if (max > 0) {
        int filled = (int)((float)cur / max * w);
        if (filled > 0) FillRect2(hdc, x, y, filled, h, colFg);
    }
    DrawRect2(hdc, x, y, w, h, RGB(100,100,100), 1);
}

/* --------------------------------------------------------------------- */
/*  Рендер экрана выбора сложности                                       */
/* --------------------------------------------------------------------- */
static void RenderMenu(HDC hdc) {
    FillRect2(hdc, 0, 0, ARENA_W, ARENA_H, RGB(10, 10, 20));

    HFONT bigFont = CreateFontA(42, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT old = SelectObject(hdc, bigFont);
    DrawTextCenter(hdc, 0, 60, ARENA_W, 50, "BROTATO-LIKE", RGB(255, 200, 60));
    SelectObject(hdc, old);
    DeleteObject(bigFont);

    DrawTextCenter(hdc, 0, 140, ARENA_W, 30, "Выбери сложность:", RGB(200, 200, 200));

    /* Три карточки сложности */
    struct { const char *label; const char *desc; COLORREF col; } levels[3] = {
        { "[1] Легко",    "+10% HP врагов за волну",  RGB( 60, 180,  80) },
        { "[2] Нормально","+20% HP врагов за волну",  RGB(220, 180,  40) },
        { "[3] Сложно",   "+35% HP врагов за волну",  RGB(220,  60,  60) },
    };

    int cardW = 200, cardH = 100;
    int startX = (ARENA_W - 3 * cardW - 2 * 20) / 2;

    for (int i = 0; i < 3; i++) {
        int cx = startX + i * (cardW + 20);
        int cy = 200;
        FillRect2(hdc, cx, cy, cardW, cardH, RGB(25, 25, 40));
        DrawRect2 (hdc, cx, cy, cardW, cardH, levels[i].col, 2);
        DrawTextCenter(hdc, cx, cy + 20, cardW, 28, levels[i].label, levels[i].col);
        DrawTextCenter(hdc, cx, cy + 58, cardW, 22, levels[i].desc,  RGB(160,160,160));
    }

    DrawTextCenter(hdc, 0, ARENA_H - 50, ARENA_W, 30,
                   "Нажми 1, 2 или 3 для начала", RGB(100, 100, 100));
}

/* --------------------------------------------------------------------- */
/*  Рендер игрового процесса                                             */
/* --------------------------------------------------------------------- */
static void RenderPlaying(HDC hdc, const GameState *gs) {
    char buf[128];

    /* Фон арены */
    FillRect2(hdc, 0, 0, ARENA_W, ARENA_H, COL_BG);

    /* Лёгкая сетка для ориентира (опционально) */
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(30, 30, 40));
    HPEN oldPen  = SelectObject(hdc, gridPen);
    for (int gx = 0; gx < ARENA_W; gx += 80) {
        MoveToEx(hdc, gx, 0, NULL); LineTo(hdc, gx, ARENA_H);
    }
    for (int gy = 0; gy < ARENA_H; gy += 80) {
        MoveToEx(hdc, 0, gy, NULL); LineTo(hdc, ARENA_W, gy);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(gridPen);

    /* Гемы опыта */
    for (int i = 0; i < MAX_GEMS; i++) {
        const XpGem *g = &gs->gems[i];
        if (!g->alive) continue;
        FillRect2(hdc, (int)g->x - 5, (int)g->y - 5, 10, 10, COL_GEM);
    }

    /* Враги */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &gs->enemies[i];
        if (!e->alive) continue;

        COLORREF col;
        int sz;
        switch (e->type) {
        case ENEMY_FAST:  col = COL_ENEMY_FAST; sz = 18; break;
        case ENEMY_TANK:  col = COL_ENEMY_TANK; sz = 38; break;
        default:          col = COL_ENEMY_BASIC; sz = 26; break;
        }

        int ex = (int)e->x - sz/2;
        int ey = (int)e->y - sz/2;
        FillRect2(hdc, ex, ey, sz, sz, col);

        /* Мини-полоска HP над врагом */
        if (e->hp < e->maxHp) {
            DrawBar(hdc, ex, ey - 6, sz, 4,
                    e->hp, e->maxHp, COL_HP_BG, COL_HP_FG);
        }
    }

    /* Пули */
    for (int i = 0; i < MAX_BULLETS; i++) {
        const Bullet *b = &gs->bullets[i];
        if (!b->alive) continue;
        FillRect2(hdc, (int)b->x - 4, (int)b->y - 4, 8, 8, COL_BULLET);
    }

    

    /* Игрок (мигает при неуязвимости) */
    int drawPlayer = 1;
    if (gs->player.invTimer > 0) {
        /* Мерцание: пропускаем каждый второй кадр-блок 0.1 с */
        drawPlayer = ((int)(gs->player.invTimer / 0.1f) % 2 == 0);
    }
    if (drawPlayer) {
        DrawSprite(hdc, s_sprPlayer, (int)gs->player.x - 12, (int)gs->player.y - 12, 24, 24);
    }

    /* HUD — верхняя полоска */
    FillRect2(hdc, 0, 0, ARENA_W, 36, RGB(10, 10, 18));

    /* HP */
    DrawText2(hdc, 8, 8, "HP:", COL_TEXT);
    DrawBar(hdc, 32, 10, 120, 16,
            gs->player.hp, gs->player.maxHp, COL_HP_BG, COL_HP_FG);
    snprintf(buf, sizeof(buf), "%d/%d", gs->player.hp, gs->player.maxHp);
    DrawText2(hdc, 158, 10, buf, COL_TEXT);

    /* Очки */
    snprintf(buf, sizeof(buf), "Score: %d", gs->score);
    DrawText2(hdc, 280, 10, buf, COL_TEXT);

    /* Номер волны */
    snprintf(buf, sizeof(buf), "Wave %d / %d", gs->wave, gs->totalWaves);
    DrawText2(hdc, ARENA_W - 130, 10, buf, COL_WAVE_TEXT);

    /* Подсказка управления снизу */
    DrawText2(hdc, 6, ARENA_H - 18, "WASD — движение  |  ESC — выход", RGB(80,80,80));
}

/* --------------------------------------------------------------------- */
/*  Рендер экрана выбора апгрейда                                        */
/* --------------------------------------------------------------------- */
static void RenderUpgrade(HDC hdc, const GameState *gs) {
    /* Затемнённый фон */
    FillRect2(hdc, 0, 0, ARENA_W, ARENA_H, RGB(10, 10, 20));

    /* Заголовок */
    HFONT bigFont = CreateFontA(32, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT oldFont = SelectObject(hdc, bigFont);
    DrawTextCenter(hdc, 0, 60, ARENA_W, 40, "Выбери апгрейд", RGB(255,220,60));
    SelectObject(hdc, oldFont);
    DeleteObject(bigFont);

    /* Три карточки апгрейда */
    int cardW = 200, cardH = 130;
    int startX = (ARENA_W - 3*cardW - 2*20) / 2;
    int cardY  = 150;

    for (int i = 0; i < 3; i++) {
        int id = gs->offerIds[i];
        if (id < 0) continue;

        const UpgradeInfo *info = Core_GetUpgradeInfo(id);
        if (!info) continue;

        int cx = startX + i * (cardW + 20);

        /* Фон карточки */
        FillRect2(hdc, cx, cardY, cardW, cardH, COL_UPGRADE_BG);
        DrawRect2(hdc, cx, cardY, cardW, cardH, COL_UPGRADE_HL, 2);

        /* Номер (1/2/3) */
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "[%d]", i+1);
        DrawTextCenter(hdc, cx, cardY + 10, cardW, 24, numBuf, RGB(255,220,60));

        /* Название */
        DrawTextCenter(hdc, cx, cardY + 42, cardW, 24, info->name, COL_TEXT);

        /* Описание */
        DrawTextCenter(hdc, cx, cardY + 75, cardW, 24, info->desc, RGB(160,160,160));

        /* Текущий уровень апгрейда */
        char lvlBuf[32];
        snprintf(lvlBuf, sizeof(lvlBuf), "Ур. %d", gs->player.upgrades[id]);
        DrawTextCenter(hdc, cx, cardY + 102, cardW, 24, lvlBuf, RGB(100,200,255));
    }

    DrawTextCenter(hdc, 0, ARENA_H - 50, ARENA_W, 30,
                   "Нажми 1, 2 или 3", RGB(120,120,120));
}

/* --------------------------------------------------------------------- */
/*  Рендер экрана смерти                                                 */
/* --------------------------------------------------------------------- */
static void RenderDead(HDC hdc, const GameState *gs) {
    FillRect2(hdc, 0, 0, ARENA_W, ARENA_H, RGB(15, 0, 0));

    HFONT bigFont = CreateFontA(48, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT old = SelectObject(hdc, bigFont);
    DrawTextCenter(hdc, 0, 160, ARENA_W, 60, "ВЫ ПОГИБЛИ", RGB(220,50,50));
    SelectObject(hdc, old);
    DeleteObject(bigFont);

    char buf[64];
    snprintf(buf, sizeof(buf), "Счёт: %d   |   Волна: %d", gs->score, gs->wave);
    DrawTextCenter(hdc, 0, 260, ARENA_W, 40, buf, COL_TEXT);
    DrawTextCenter(hdc, 0, 320, ARENA_W, 40, "Нажми R для перезапуска", RGB(140,140,140));
    DrawTextCenter(hdc, 0, 380, ARENA_W, 24, "Рекорды:", RGB(200, 200, 100));

    static const char* diffNames[] = { "Easy", "Normal", "Hard" };
    char buf2[64];
    for (int i = 0; i < gs->highScores.count; i++) {
        const ScoreEntry* e = &gs->highScores.entries[i];
        snprintf(buf2, sizeof(buf2), "#%d  %d очков  волна %d  [%s]",
            i + 1, e->score, e->wave, diffNames[e->difficulty]);
        DrawTextCenter(hdc, 0, 410 + i * 24, ARENA_W, 22, buf2, COL_TEXT);
    }
}

/* --------------------------------------------------------------------- */
/*  Рендер экрана победы                                                 */
/* --------------------------------------------------------------------- */
static void RenderWin(HDC hdc, const GameState *gs) {
    FillRect2(hdc, 0, 0, ARENA_W, ARENA_H, RGB(0, 15, 5));

    HFONT bigFont = CreateFontA(48, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                DEFAULT_CHARSET, 0, 0, 0, 0, "Arial");
    HFONT old = SelectObject(hdc, bigFont);
    DrawTextCenter(hdc, 0, 160, ARENA_W, 60, "ПОБЕДА!", RGB(60,220,100));
    SelectObject(hdc, old);
    DeleteObject(bigFont);

    char buf[64];
    snprintf(buf, sizeof(buf), "Итоговый счёт: %d", gs->score);
    DrawTextCenter(hdc, 0, 260, ARENA_W, 40, buf, COL_TEXT);
    DrawTextCenter(hdc, 0, 320, ARENA_W, 40, "Нажми R для новой игры", RGB(140,140,140));
    DrawTextCenter(hdc, 0, 380, ARENA_W, 24, "Рекорды:", RGB(200, 200, 100));

    static const char* diffNames[] = { "Easy", "Normal", "Hard" };
    char buf2[64];
    for (int i = 0; i < gs->highScores.count; i++) {
        const ScoreEntry* e = &gs->highScores.entries[i];
        snprintf(buf2, sizeof(buf2), "#%d  %d очков  волна %d  [%s]",
            i + 1, e->score, e->wave, diffNames[e->difficulty]);
        DrawTextCenter(hdc, 0, 410 + i * 24, ARENA_W, 22, buf2, COL_TEXT);
    }
}

/* --------------------------------------------------------------------- */
/*  UI_Render  —  точка входа рендера, двойная буферизация              */
/* --------------------------------------------------------------------- */
void UI_Render(HWND hwnd, const GameState *gs) {
    HDC hdc = GetDC(hwnd);

    /* --- Создаём back-buffer ---------------------------------------- */
    HDC     hdcMem = CreateCompatibleDC(hdc);
    HBITMAP bmp    = CreateCompatibleBitmap(hdc, ARENA_W, ARENA_H);
    HBITMAP oldBmp = SelectObject(hdcMem, bmp);

    /* --- Выбираем шрифт для HUD ------------------------------------ */
    HFONT hudFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                RUSSIAN_CHARSET, 0, 0, 0, 0, "Consolas");
    HFONT oldFont = SelectObject(hdcMem, hudFont);

    /* --- Рисуем нужный экран на back-buffer ------------------------- */
    switch (gs->phase) {
    case PHASE_MENU:    RenderMenu   (hdcMem);     break;
    case PHASE_PLAYING: RenderPlaying(hdcMem, gs); break;
    case PHASE_UPGRADE: RenderUpgrade(hdcMem, gs); break;
    case PHASE_DEAD:    RenderDead   (hdcMem, gs); break;
    case PHASE_WIN:     RenderWin    (hdcMem, gs); break;
    }

    /* --- Копируем на экран одним вызовом (нет мерцания) ------------ */
    BitBlt(hdc, 0, 0, ARENA_W, ARENA_H, hdcMem, 0, 0, SRCCOPY);

    /* --- Освобождаем все GDI-ресурсы ------------------------------- */
    SelectObject(hdcMem, oldFont);
    DeleteObject(hudFont);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(bmp);       /* <-- обязательно: без этого утечка      */
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdc);
}
