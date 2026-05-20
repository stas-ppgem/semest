#pragma once

/* =====================================================================
   ui.h  --  Прототипы рендера и оконного интерфейса
   Здесь разрешён Windows.h — это граница Win32-слоя
   ===================================================================== */

#include <windows.h>
#include "core.h"

/* Создать окно игры. Возвращает HWND или NULL при ошибке. */
HWND UI_CreateWindow(HINSTANCE hInst, int width, int height, const char *title);

/* Заполнить gs->input текущим состоянием клавиш (GetAsyncKeyState) */
void UI_PollInput(GameState *gs);

/* Отрисовать кадр с двойной буферизацией */
void UI_Render(HWND hwnd, const GameState *gs);

/* Освободить GDI-ресурсы (вызвать при завершении) */
void UI_Destroy(void);
