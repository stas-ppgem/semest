/* =====================================================================
   main.c  --  Entry point and game loop
   - Fixed-step logic at 60 Hz via QueryPerformanceCounter
   - No Sleep() — CPU is yielded with SwitchToThread() when idle
   - Edge-detection on select/restart keys (press once = one action)
   ===================================================================== */

#include <windows.h>
#include "core.h"
#include "ui.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd; (void)nShow;

    /* Create window */
    HWND hwnd = UI_CreateWindow(hInst, ARENA_W, ARENA_H,
                                "Brotato-like");
    if (!hwnd) return 1;

    /* Initialize game state */
    GameState gs;
    Core_Init(&gs);
    Scores_Load(&gs.highScores);

    /* High-resolution timer */
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    /* Fixed logic timestep: 60 ticks per second */
    const float FRAME_TIME = 1.0f / 60.0f;
    float       accumulator = 0.0f;

    MSG msg     = {0};
    int running = 1;

    /* ----------------------------------------------------------------- */
    /*  Main game loop                                                   */
    /* ----------------------------------------------------------------- */
    while (running) {

        /* -- Win32 message pump ---------------------------------------- */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = 0; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        /* -- Delta time via QPC ---------------------------------------- */
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
        prev = now;
        if (dt > 0.1f) dt = 0.1f;   /* clamp: resume after pause/debug  */
        accumulator += dt;

        /* -- Fixed-step logic ------------------------------------------ */
        int anyUpdate = 0;

        while (accumulator >= FRAME_TIME) {
            /* Snapshot previous input for edge-detection (new press only) */
            InputState prevInp = gs.input;
            UI_PollInput(&gs);

            /* Restart on fresh key-down only */
            if ((gs.phase == PHASE_DEAD || gs.phase == PHASE_WIN) &&
                    gs.input.restart && !prevInp.restart) {
                Core_Init(&gs);
            }

            /* Difficulty selection (menu) */
            if (gs.phase == PHASE_MENU) {
                if (gs.input.select1 && !prevInp.select1) Core_SetDifficulty(&gs, 0);
                if (gs.input.select2 && !prevInp.select2) Core_SetDifficulty(&gs, 1);
                if (gs.input.select3 && !prevInp.select3) Core_SetDifficulty(&gs, 2);
            }

            /* Upgrade selection */
            if (gs.phase == PHASE_UPGRADE) {
                if (gs.input.select1 && !prevInp.select1)
                    Core_ApplyUpgrade(&gs, gs.offerIds[0]);
                if (gs.input.select2 && !prevInp.select2)
                    Core_ApplyUpgrade(&gs, gs.offerIds[1]);
                if (gs.input.select3 && !prevInp.select3)
                    Core_ApplyUpgrade(&gs, gs.offerIds[2]);
            }

            /* Game logic */
            GamePhase prevPhase = gs.phase;
            Core_Update(&gs, FRAME_TIME);

            if ((gs.phase == PHASE_DEAD || gs.phase == PHASE_WIN)
                && prevPhase != PHASE_DEAD && prevPhase != PHASE_WIN) {
                Scores_Submit(&gs);
            }
            accumulator -= FRAME_TIME;
            anyUpdate    = 1;
        }

        /* -- Render only when logic has stepped; yield CPU otherwise --- */
        if (anyUpdate) {
            UI_Render(hwnd, &gs);
        } else {
            SwitchToThread();   /* yield without a fixed Sleep() */
        }
    }

    UI_Destroy();
    return (int)msg.wParam;
}
