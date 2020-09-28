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
    long width;
    long height;
    std::string scene_file;
    bool vsync;
};

class Graphics
{

public:
    Graphics(HWND window, const Config& config, Input& input);

    void update();
    void render();

private:
    Graphics_impl* impl;
};

