// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020-2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#define D3DX12_NO_STATE_OBJECT_HELPERS
#include <wrl/client.h> // For ComPtr
#define COM_NO_WINDOWS_H
#include <ole2.h>
#include "3rdparty/MS/d3dx12.h"
