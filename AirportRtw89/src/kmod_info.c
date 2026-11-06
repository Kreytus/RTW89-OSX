/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Kext module descriptor. Xcode normally generates this file; we build with
 * a Makefile, so it lives in the tree. KEXT_BUNDLE_ID and KEXT_VERSION are
 * passed in by the Makefile so they stay in sync with Info.plist.
 */
#include <mach/mach_types.h>

extern kern_return_t _start(kmod_info_t *ki, void *data);
extern kern_return_t _stop(kmod_info_t *ki, void *data);

__attribute__((visibility("default")))
KMOD_EXPLICIT_DECL(KEXT_BUNDLE_ID, KEXT_VERSION, _start, _stop)

__private_extern__ kmod_start_func_t *_realmain = 0;
__private_extern__ kmod_stop_func_t *_antimain = 0;
__private_extern__ int _kext_apple_cc = __APPLE_CC__;
