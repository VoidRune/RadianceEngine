#pragma once
// miniaudio with the features Timbre doesn't use compiled out. Include this instead of
// miniaudio.h so the declarations match the implementation in MiniaudioImpl.c.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE

#include <miniaudio/miniaudio.h>
