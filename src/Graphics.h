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
        vsync(false) {}
    long width;
    long height;
    std::string scene_file;
    int monitor;
    bool borderless_windowed_fullscreen;
    bool vsync;
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

