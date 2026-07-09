/**
 * @file        image_info.h
 * @brief       Image layout descriptor for recompiled binaries
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#pragma once

#include <rex/types.h>

struct PPCFuncMapping;

namespace rex::system {
class KernelState;
}

namespace rex {

/**
 * Callback for registering recompiled modules with KernelState (multi-binary projects).
 */
using RegisterModulesFunc = void (*)(system::KernelState*);

/// PPC image layout passed from the generated config header into ReXApp.
struct PPCImageInfo {
  u32 code_base;
  u32 code_size;
  u32 image_base;
  u32 image_size;
  const PPCFuncMapping* func_mappings;
  bool rexcrt_heap = false;  ///< Set by codegen when [rexcrt] has heap functions
  RegisterModulesFunc register_modules = nullptr;  ///< Set by codegen for multi-binary projects
  /// Guest VA of the module's function-pointer dispatch table; 0 = legacy default
  /// (image_base + image_size). Set by codegen when the manifest overrides
  /// function_table_base -- needed when another recompiled module's image loads
  /// right after this one (e.g. FIFA Street's companion at 0x82300000) and would
  /// collide with the default table placement. Appended last: older generated
  /// init.cpp default-initializes it to 0 (legacy) under the new header.
  u32 function_table_base = 0;
};

}  // namespace rex
