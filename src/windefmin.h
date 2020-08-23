// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2020 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#pragma once


#if defined(_M_IX86)
#ifndef _X86_
#define _X86_
#include <windef.h>
#undef _X86_
#else
#include <windef.h>
#endif
#elif defined(_M_AMD64)
#ifndef _AMD64_
#define _AMD64_
#include <windef.h>
#undef _AMD64_
#else
#include <windef.h>
#endif
#endif
