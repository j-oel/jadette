// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>



#include "Engine.h"

#include "util.h"

#include <winuser.h>


LRESULT CALLBACK window_procedure(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);


int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
    const long width = 800;
    const long height = 600;

    Engine engine(width, height);

    RECT window_rect = { 0, 0, width, height };
    const DWORD window_style = WS_TILEDWINDOW;
    BOOL use_menu = FALSE;
    AdjustWindowRect(&window_rect, window_style, use_menu);

    WNDCLASS c{};
    c.lpfnWndProc = window_procedure;
    c.hInstance = instance;
    c.lpszClassName = L"Jadette_Class";
    c.hCursor = LoadCursor(NULL, IDC_ARROW);
    auto& window_class = c;
    RegisterClass(&window_class);

    LPCTSTR title = L"Jadette 3D Engine";
    HWND parent_window = nullptr;
    HMENU menu = nullptr;
    int position_x = 100;
    int position_y = 30;
    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    HWND window = CreateWindow(window_class.lpszClassName, title, window_style, 
        position_x, position_y, window_width, window_height, 
        parent_window, menu, instance, &engine);


    engine.graphics.init(window);

    ShowWindow(window, cmd_show);

    const HWND value_indicating_all_messages = NULL;
    const UINT filter_min = 0; // If this and
    const UINT filter_max = 0; // this are both zero, no filtering is performed.
    MSG m {};

    while (m.message != WM_QUIT)
        if (PeekMessage(&m, value_indicating_all_messages, filter_min, filter_max, PM_REMOVE))
        {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }

    int exit_code = static_cast<int>(m.wParam);

    return exit_code;
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    static Engine* engine = nullptr;

    switch (message)
    {
        case WM_CREATE:
        {
            LPCREATESTRUCT create_struct = bit_cast<LPCREATESTRUCT>(l_param);
            engine = bit_cast<Engine*>(create_struct->lpCreateParams);
            return 0;
        }

        case WM_PAINT:
            if (engine)
            {
                engine->graphics.update();
                engine->graphics.render();
            }
            return 0;

        case WM_KEYDOWN:
            if (w_param == VK_ESCAPE)
                PostQuitMessage(0);
            else
                if (engine)
                {
                    engine->input.key_down(w_param);
                }
            return 0;

        case WM_KEYUP:
            if (engine)
            {
                engine->input.key_up(w_param);
            }
            return 0;


        case WM_MOUSEMOVE:
            if (engine)
            {
                engine->input.mouse_move(l_param);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    // Call default window procedure
    return DefWindowProc(window, message, w_param, l_param);
}
