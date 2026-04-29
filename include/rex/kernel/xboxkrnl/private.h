/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#pragma once

#include <cstdint>

namespace rex::kernel::xboxkrnl {

// Mark a guest thread to never be resumed. Called by HandleSetThreadName
// when it detects a thread we know is going to crash if it runs (e.g.
// Skate 3's MoviePlayer2 Decode Thread). NtResumeThread checks this set
// and refuses to resume the host thread for any flagged guest TID.
void SetSkipThread(uint32_t thread_id);
bool ShouldSkipThread(uint32_t thread_id);

}  // namespace rex::kernel::xboxkrnl
