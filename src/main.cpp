// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch.h"
#include "Engine.h"
#include "util.h"


LRESULT CALLBACK window_procedure(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);


struct Could_not_open_file
{
};

struct Read_error
{
    Read_error(const std::string& input_) : input(input_) {}
    std::string input;
};

struct Monitor_number_too_small
{
    Monitor_number_too_small(int requested_monitor_, int monitor_count_) :
        requested_monitor(requested_monitor_), monitor_count(monitor_count_) {}
    int requested_monitor;
    int monitor_count;
};

struct Monitor_number_too_big
{
    Monitor_number_too_big(int requested_monitor_, int monitor_count_) :
        requested_monitor(requested_monitor_), monitor_count(monitor_count_) {}
    int requested_monitor;
    int monitor_count;
};

Config read_config(const std::string& config_file)
{
    std::ifstream file(config_file);
    if (!file.is_open())
        throw Could_not_open_file();

    Config config;

    while (file)
    {
        std::string input;
        file >> input;

        if (input == "width")
        {
            file >> config.width;
        }
        else if (input == "height")
        {
            file >> config.height;
        }
        else if (input == "scene")
        {
#ifndef NO_SCENE_FILE
            file >> config.scene_file;
#else
            file >> input;
#endif
        }
        else if (input == "edit_mode")
        {
            file >> config.edit_mode;
        }
        else if (input == "invert_mouse")
        {
            file >> config.invert_mouse;
        }
        else if (input == "mouse_sensitivity")
        {
            file >> config.mouse_sensitivity;
        }
        else if (input == "max_speed")
        {
            file >> config.max_speed;
        }
        else if (input == "fov")
        {
            file >> config.fov;
        }
        else if (input == "borderless_windowed_fullscreen")
        {
            file >> config.borderless_windowed_fullscreen;
        }
        else if (input == "vsync")
        {
            file >> config.vsync;
        }
        else if (input == "use_vertex_colors")
        {
            file >> config.use_vertex_colors;
        }
        else if (input == "backface_culling")
        {
            file >> config.backface_culling;
        }
        else if (input == "early_z_pass")
        {
            file >> config.early_z_pass;
        }
        else if (input == "monitor")
        {
            file >> config.monitor;
        }
        else if (input == "swap_chain_buffer_count")
        {
            file >> config.swap_chain_buffer_count;
        }
        else if (!input.empty() && input[0] == '#')
        {
            std::getline(file, input);
        }
        else if (input == "")
        {
        }
        else
            throw Read_error(input);
    }

    int monitor_count = GetSystemMetrics(SM_CMONITORS); // Only count real visible monitors.
    if (config.monitor > monitor_count)
        throw Monitor_number_too_big(config.monitor, monitor_count);
    else if (config.monitor < 1)
        throw Monitor_number_too_small(config.monitor, monitor_count);

    return config;
}

struct Monitor
{
    HMONITOR monitor;
    RECT rect;
};

BOOL CALLBACK monitor_enum_proc(HMONITOR h_monitor, HDC, LPRECT monitor_rect, LPARAM data)
{
    auto monitors = bit_cast<std::vector<Monitor>*>(data);
    Monitor monitor = { h_monitor, *monitor_rect };
    monitors->push_back(monitor);
    return TRUE;
}

constexpr DWORD windowed_window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmd_show)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
         
#ifndef NO_CFG_FILE
    std::string config_file(data_path + std::string("init.cfg"));
#endif

#ifdef __cpp_exceptions
    try
#endif
    {
#ifndef NO_CFG_FILE
        Config config = read_config(config_file);
#else
        Config config;
#endif
        std::vector<Monitor> monitors;
        EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, bit_cast<LPARAM>(&monitors));

        BOOL use_menu = FALSE;
        RECT window_rect;
        DWORD window_style = WS_POPUP; // To not have any titlebar in fullscreen
        int position_x = 0;
        int position_y = 0;
        RECT& monitor_rect = monitors.at(static_cast<size_t>(config.monitor) - 1).rect;
        if (config.borderless_windowed_fullscreen)
        {
            window_rect = monitor_rect;
            config.width = monitor_rect.right - monitor_rect.left;
            config.height = monitor_rect.bottom - monitor_rect.top;
            position_x = monitor_rect.left;
            position_y = monitor_rect.top;
        }
        else
        {
            window_rect = { 0, 0, config.width, config.height };
            window_style = windowed_window_style;
            AdjustWindowRect(&window_rect, window_style, use_menu);
            position_x = 100 + monitor_rect.left;
            position_y = 30 + monitor_rect.top;
        }

        WNDCLASS c {};
        c.lpfnWndProc = window_procedure;
        c.hInstance = instance;
        c.lpszClassName = L"Jadette_Class";
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        auto& window_class = c;
        RegisterClass(&window_class);

        LPCTSTR title = L"Jadette 3D Engine";
        HWND parent_window = nullptr;
        HMENU menu = nullptr;
        int window_width = window_rect.right - window_rect.left;
        int window_height = window_rect.bottom - window_rect.top;

        HWND window = CreateWindow(window_class.lpszClassName, title, window_style,
            position_x, position_y, window_width, window_height,
            parent_window, menu, instance, &config);

        ShowWindow(window, cmd_show);
        SetFocus(window);

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
#ifdef __cpp_exceptions
    catch (std::bad_alloc&)
    {
        print("Tried to allocate more memory than is available.", "Fatal error.");
    }
    catch (Could_not_open_file&)
    {
        print("Could not open config file: " + config_file, "Error");
    }
    catch (Read_error& e)
    {
        print("Error reading file: " + config_file + "\nunrecognized token: " +
            e.input, "Error");
    }
    catch (Monitor_number_too_big& e)
    {
        print("Error in config file: " + config_file + "\nRequested monitor number " +
            std::to_string(e.requested_monitor) + (e.monitor_count == 1 ? 
                ", but it has to be 1, as there is only " : ", but there are only ") + 
            std::to_string(e.monitor_count) + " monitor" + (e.monitor_count > 1 ? "s": "") +
            " connected to this computer.", "Error");
    }
    catch (Monitor_number_too_small& e)
    {
        print("Error in config file: " + config_file + "\nRequested monitor number " +
            std::to_string(e.requested_monitor) + ", but it has to be " +
            (e.monitor_count == 1 ? "1.": "\nat least 1 and maximum " + 
                std::to_string(e.monitor_count) + "."), "Error");
    }
#endif
    return 1;
}


void scaling_changed(HWND window, uint16_t dpi, Engine* engine, Config* config)
{
    engine->graphics.scaling_changed(dpi);
    RECT window_rect = { 0, 0, config->width, config->height };
    if (!config->borderless_windowed_fullscreen)
    {
        const DWORD window_style = windowed_window_style;
        const BOOL use_menu = FALSE;
        const DWORD ex_style = 0;
        AdjustWindowRectExForDpi(&window_rect, window_style, use_menu, ex_style, dpi);
        UINT width = window_rect.right - window_rect.left;
        UINT height = window_rect.bottom - window_rect.top;
        const HWND not_used = nullptr;
        const UINT dont_move = 0;
        SetWindowPos(window, not_used, dont_move, dont_move, width, height,
            SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER);
    }
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    static Engine* engine = nullptr;
    static Config* config = nullptr;

    switch (message)
    {
        case WM_CREATE:
        {
            LPCREATESTRUCT create_struct = bit_cast<LPCREATESTRUCT>(l_param);
            config = bit_cast<Config*>(create_struct->lpCreateParams);
            static Engine the_engine(window, *config);
            engine = &the_engine;
            return 0;
        }

        // "The system sends this message when there are no other messages in the
        // application's message queue."
        // https://docs.microsoft.com/en-us/windows/win32/gdi/wm-paint
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
#ifndef NO_UI
            else
                if (engine)
                {
                    engine->input.key_down(w_param);
                }
#endif
            return 0;
#ifndef NO_UI
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

        case WM_LBUTTONDOWN:
            if (engine)
            {
                if (w_param & MK_SHIFT)
                    engine->input.shift_mouse_left_button_down();
                else if (w_param & MK_CONTROL)
                    engine->input.control_mouse_left_button_down();
                else
                    engine->input.mouse_left_button_down();
            }
            break;

        case WM_LBUTTONUP:
            if (engine)
            {
                engine->input.mouse_left_button_up();
            }
            break;

        case WM_RBUTTONDOWN:
            if (engine)
            {
                engine->input.mouse_right_button_just_down(l_param);
                if (w_param & MK_SHIFT)
                    engine->input.shift_mouse_right_button_down();
                else if (w_param & MK_CONTROL)
                    engine->input.control_mouse_right_button_down();
                else
                    engine->input.mouse_right_button_down();
            }
            break;

        case WM_RBUTTONUP:
            if (engine)
            {
                engine->input.mouse_right_button_up();
            }
            break;

        case WM_MOUSEWHEEL:
            if (engine)
            {
                engine->input.mouse_wheel_roll(GET_WHEEL_DELTA_WPARAM(w_param));
            }
            break;

        case WM_DPICHANGED:
        {
            uint16_t dpi = HIWORD(w_param);
            if (engine)
            {
                scaling_changed(window, dpi, engine, config);
            }
            break;
        }
#endif
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    // Call default window procedure
    return DefWindowProc(window, message, w_param, l_param);
}
