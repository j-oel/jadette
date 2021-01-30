// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "windefmin.h"
#include <string>

class Graphics_impl;
class Input;

struct Config
{
    Config() : width(800), height(600), monitor(1), borderless_windowed_fullscreen(false),
        vsync(false), early_z_pass(false), mirror_texture_addressing(false),
        edit_mode(false), invert_mouse(false), mouse_sensitivity(0.3f), fov(70.0f) {}
    int width;
    int height;
    std::string scene_file;
    int monitor;
    bool borderless_windowed_fullscreen;
    bool vsync;
    bool early_z_pass;
    bool mirror_texture_addressing; // It would be better to have this per texture. However,
                                    // I don't want to handle that extra complexity right now as
                                    // AFAIK the most common thing is to have it as wrap (tile),
                                    // which is default, but I need mirror for my test scene.
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

