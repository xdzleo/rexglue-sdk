#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <mutex>
#include <unordered_map>

#include <rex/system/kernel_state.h>
#include <rex/system/xam/app_manager.h>

namespace rex {
namespace kernel {
namespace xam {
namespace apps {

class XgiApp : public system::xam::App {
 public:
  explicit XgiApp(system::KernelState* kernel_state);

  X_HRESULT DispatchMessageSync(uint32_t message, uint32_t buffer_ptr,
                                uint32_t buffer_length) override;

 private:
  // Per-user context store. Skate 3 (and other EA RenderWare titles) write
  // context values via XGIUserSetContext (0x000B0006) and read them back
  // via XGIUserGetContext (0x000B0041). The menu UI is gated on these
  // values matching what the title set -- if Get returns X_E_FAIL or stale
  // 0, the title interprets it as "user state not ready" and never builds
  // the menu. Map key is (user_index << 32) | context_id, value is the
  // 32-bit context value. Mutex-guarded for thread safety.
  std::mutex user_data_lock_;
  std::unordered_map<uint64_t, uint32_t> user_contexts_;
};

}  // namespace apps
}  // namespace xam
}  // namespace kernel
}  // namespace rex
