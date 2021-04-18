// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


class Graphics_impl;
class Input;

struct Config
{
    Config() : width(800), height(600), monitor(1), swap_chain_buffer_count(2),
        borderless_windowed_fullscreen(false), vsync(false), use_vertex_colors(false),
        backface_culling(true), early_z_pass(false), edit_mode(true),
        invert_mouse(false), mouse_sensitivity(0.3f), fov(70.0f) {}
    int width;
    int height;
#ifndef NO_SCENE_FILE
    std::string scene_file;
#endif
    int monitor;
    int swap_chain_buffer_count;
    bool borderless_windowed_fullscreen;
    bool vsync;
    bool use_vertex_colors;
    bool backface_culling;
    bool early_z_pass;
    bool edit_mode;
    bool invert_mouse;
    float mouse_sensitivity;
    float fov;
};

class Graphics
{

public:
    Graphics(HWND window, const Config& config, Input& input);

    void update();
    void render();
    void scaling_changed(float dpi);
private:
    Graphics_impl* impl;
};

