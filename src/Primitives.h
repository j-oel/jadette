// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once

#include "Mesh.h"

class Cube : public Mesh
{
public:
    Cube(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);

private:

};

class Plane : public Mesh
{
public:
    Plane(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList>& command_list);
};
