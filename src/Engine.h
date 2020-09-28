// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Graphics.h"
#include "Input.h"

struct Engine
{
    Engine(HWND window, const Config& config) : graphics(window, config, input) {}
    Input input;
    Graphics graphics;
};

