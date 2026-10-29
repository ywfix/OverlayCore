#pragma once

#ifndef OVERLAY_TARGET_MODE
#define OVERLAY_TARGET_MODE 1
#endif

#ifndef OVERLAY_VIRTUAL_CURSOR
#define OVERLAY_VIRTUAL_CURSOR 1
#endif

#ifndef OVERLAY_ZORDER_MODE
#define OVERLAY_ZORDER_MODE 0
#endif

#ifndef OVERLAY_USE_ZBID
#define OVERLAY_USE_ZBID 0
#endif

#ifndef OVERLAY_ENABLE_CONSOLE
#define OVERLAY_ENABLE_CONSOLE 1
#endif

#if (OVERLAY_TARGET_MODE != 0 && OVERLAY_TARGET_MODE != 1) || \
    (OVERLAY_VIRTUAL_CURSOR != 0 && OVERLAY_VIRTUAL_CURSOR != 1) || \
    (OVERLAY_ZORDER_MODE != 0 && OVERLAY_ZORDER_MODE != 1) || \
    (OVERLAY_USE_ZBID != 0 && OVERLAY_USE_ZBID != 1) || \
    (OVERLAY_ENABLE_CONSOLE != 0 && OVERLAY_ENABLE_CONSOLE != 1)
#error "Overlay configuration flags must be 0 or 1."
#endif

#if OVERLAY_ZORDER_MODE && (!OVERLAY_TARGET_MODE || OVERLAY_USE_ZBID)
#error "Z-order mode requires target mode and cannot be combined with ZBID."
#endif
