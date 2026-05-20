/* =====================================================================
   main.c  --  Точка входа, игровой цикл
   ===================================================================== */

#include <windows.h>
#include "core.h"
#include "ui.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev; (void)lpCmd; (void)nShow;

    /* Создать окно */
    HWND hwnd = UI_CreateWindow(hInst, ARENA_W, ARENA_H, "Brotato-like | WASD to move");
    if (!hwnd) return 1;

    /* Инициализировать игровое состояние */
    GameState gs;
    Core_Init(&gs);

    /* Таймер высокого разрешения */
    LARGE_INTEGER freq, prev, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    /* Ограничение FPS (~60) */
    const float FRAME_TIME = 1.0f / 60.0f;
    float accumulator = 0.0f;

    /* ----------------------------------------------------------------- */
    /*  Главный игровой цикл                                             */
    /* ----------------------------------------------------------------- */
    MSG msg = {0};
    while (1) {
        /* Обработать все накопившиеся Win32-сообщения */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto exit_loop;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        /* Вычислить delta time */
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
        prev = now;

        /* Защита от слишком большого dt (пауза/отладка) */
        if (dt > 0.1f) dt = 0.1f;
        accumulator += dt;

        /* Обновляем логику фиксированным шагом */
        while (accumulator >= FRAME_TIME) {
            /* Ввод */
            UI_PollInput(&gs);

            /* Перезапуск → возврат в меню */
            if ((gs.phase == PHASE_DEAD || gs.phase == PHASE_WIN)
                && gs.input.restart) {
                Core_Init(&gs);
            }

            /* Выбор сложности в меню */
            if (gs.phase == PHASE_MENU) {
                if (gs.input.select1) Core_SetDifficulty(&gs, 0);
                if (gs.input.select2) Core_SetDifficulty(&gs, 1);
                if (gs.input.select3) Core_SetDifficulty(&gs, 2);
            }

            /* Обработка выбора апгрейда */
            if (gs.phase == PHASE_UPGRADE) {
                if (gs.input.select1) Core_ApplyUpgrade(&gs, gs.offerIds[0]);
                if (gs.input.select2) Core_ApplyUpgrade(&gs, gs.offerIds[1]);
                if (gs.input.select3) Core_ApplyUpgrade(&gs, gs.offerIds[2]);
            }

            /* Игровая логика */
            Core_Update(&gs, FRAME_TIME);

            accumulator -= FRAME_TIME;
        }

        /* Рендер */
        UI_Render(hwnd, &gs);

        /* Небольшой sleep чтобы не грузить CPU на 100% */
        Sleep(1);
    }

exit_loop:
    UI_Destroy();
    return (int)msg.wParam;
}
