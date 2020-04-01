// NextEngine.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "core/memory/allocator.h"
#include "core/memory/linear_allocator.h"

MallocAllocator ENGINE_API default_allocator;
LinearAllocator ENGINE_API temporary_allocator(mb(10));
LinearAllocator ENGINE_API permanent_allocator(mb(50));