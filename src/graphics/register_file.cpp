/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstring>

#include <rex/graphics/register_file.h>
#include <rex/math.h>

namespace rex::graphics {

RegisterFile::RegisterFile() {
  std::memset(values, 0, sizeof(values));

  // Several context registers power up with non-zero values on real Xenos
  // hardware, so a title that reads one of them before writing it observes 0
  // here instead. The one that bites hardest in this tree is
  // PA_SC_SCREEN_SCISSOR_BR: GetScissor() in util/draw.cpp clamps the window
  // scissor with std::min(br, screen_scissor_br), so a zero bottom-right
  // collapses every scissor rectangle to zero extent and nothing rasterizes
  // until the guest writes that register. 0x20002000 (8192 x 8192) is the
  // hardware reset value, and a no-op clamp.
  //
  // Values corroborated against the AMD R6xx/R7xx 3D register reference and
  // the Mesa r600g driver, which programs the same registers to the same
  // values at context init. Ported from Xenia Canary e20f26963.
  values[XE_GPU_REG_VGT_MAX_VTX_INDX] = 0x0000FFFF;
  values[XE_GPU_REG_VGT_MULTI_PRIM_IB_RESET_INDX] = 0x0000FFFF;
  values[XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR] = 0x20002000;  // 8192 x 8192
  values[XE_GPU_REG_RB_STENCILREFMASK_BF] = 0x00FFFF00;  // ref 0, mask/write 0xFF
  values[XE_GPU_REG_PA_SU_POINT_SIZE] = 0x00080008;  // 0.5 x 0.5 half-size, 12.4
  values[XE_GPU_REG_PA_SU_POINT_MINMAX] = 0x04000010;  // 1.0 .. 64.0 radius, 12.4
  values[XE_GPU_REG_PA_SU_LINE_CNTL] = 0x00000008;
  values[XE_GPU_REG_PA_SC_LINE_CNTL] = 0x00000400;
  values[XE_GPU_REG_VGT_HOS_REUSE_DEPTH] = 0x0000000E;
  values[XE_GPU_REG_VGT_VERTEX_REUSE_BLOCK_CNTL] = 0x0000000E;
  values[XE_GPU_REG_VGT_OUT_DEALLOC_CNTL] = 0x00000010;
  // Guard-band clip / discard adjust (2.0 clip, 1.0 discard) - hardwired on
  // hardware.
  values[XE_GPU_REG_PA_CL_GB_VERT_CLIP_ADJ] = 0x40000000;  // 2.0f
  values[XE_GPU_REG_PA_CL_GB_VERT_DISC_ADJ] = 0x3F800000;  // 1.0f
  values[XE_GPU_REG_PA_CL_GB_HORZ_CLIP_ADJ] = 0x40000000;  // 2.0f
  values[XE_GPU_REG_PA_CL_GB_HORZ_DISC_ADJ] = 0x3F800000;  // 1.0f
  values[XE_GPU_REG_PA_SC_AA_MASK] = 0x0000FFFF;
  // VGT_HOS_MAX/MIN_TESS_LEVEL also reset to 1.0f on hardware, but both
  // command processors compute the effective factor as (register + 1.0f), so
  // seeding them here would move the reset factor from 1.0 to 2.0. Left out
  // deliberately - that needs its own change.
  // values[XE_GPU_REG_VGT_HOS_MAX_TESS_LEVEL] = 0x3F800000;  // 1.0f
  // values[XE_GPU_REG_VGT_HOS_MIN_TESS_LEVEL] = 0x3F800000;  // 1.0f
}

const RegisterInfo* RegisterFile::GetRegisterInfo(uint32_t index) {
  switch (index) {
#define XE_GPU_REGISTER(index, type, name) \
  case index: {                            \
    static const RegisterInfo reg_info = { \
        RegisterInfo::Type::type,          \
        #name,                             \
    };                                     \
    return &reg_info;                      \
  }
#include <rex/graphics/register_table.inc>
#undef XE_GPU_REGISTER
    default:
      return nullptr;
  }
}

}  // namespace rex::graphics
