/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstring>

#include <rex/input/flags.h>
#include <rex/input/nop/nop_input_driver.h>
#include <rex/logging.h>

namespace rex::input::nop {

NopInputDriver::NopInputDriver(rex::ui::Window* window, size_t window_z_order)
    : InputDriver(window, window_z_order) {}

NopInputDriver::~NopInputDriver() = default;

X_STATUS NopInputDriver::Setup() {
  return X_STATUS_SUCCESS;
}

// Spoof a connected controller for ALL 4 user slots so games don't pause
// waiting for input. Returns idle state (no buttons pressed).
//
// Returning DEVICE_NOT_CONNECTED for users 1-3 (which the original code
// did) breaks EA RenderWare titles (Skate 3, SSX, etc). Those titles poll
// XamInputGetCapabilities for users 0-3, and if ANY of them ever returned
// DEVICE_NOT_CONNECTED the input pipeline stalled before it ever called
// XamInputGetState. Mirrors UnleashedRecomp's hid::GetCapabilities, which
// returns SUCCESS for any active controller without per-slot gating.

X_RESULT NopInputDriver::GetCapabilities(uint32_t user_index, uint32_t flags,
                                         X_INPUT_CAPABILITIES* out_caps) {
  if (user_index >= 4) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  if (out_caps) {
    std::memset(out_caps, 0, sizeof(*out_caps));
    out_caps->type = 0x01;  // XINPUT_DEVTYPE_GAMEPAD
    // Same fix as the XamInputGetCapabilities synth path: Skate 3 / EA
    // RenderWare expects sub_type=2 and flags bit 0 set; with the rexglue
    // defaults of (1, 0) the games' capability gate fails and they refuse
    // to read controller state.
    out_caps->sub_type = 0x02;
    out_caps->flags = 0x0003;  // bit 0 + bit 1 (Skate 3 / RenderWare gate)
    // Report standard gamepad capabilities
    out_caps->gamepad.buttons = 0xFFFF;  // All buttons supported
    out_caps->gamepad.left_trigger = 0xFF;
    out_caps->gamepad.right_trigger = 0xFF;
    out_caps->gamepad.thumb_lx = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_ly = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_rx = static_cast<int16_t>(0x7FFF);
    out_caps->gamepad.thumb_ry = static_cast<int16_t>(0x7FFF);
    out_caps->vibration.left_motor_speed = 0xFFFF;
    out_caps->vibration.right_motor_speed = 0xFFFF;
  }
  return X_ERROR_SUCCESS;
}

X_RESULT NopInputDriver::GetState(uint32_t user_index, X_INPUT_STATE* out_state) {
  if (user_index >= 4) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  if (out_state) {
    std::memset(out_state, 0, sizeof(*out_state));
    // Return idle controller state (no buttons pressed)
  }
  return X_ERROR_SUCCESS;
}

X_RESULT NopInputDriver::SetState(uint32_t user_index, X_INPUT_VIBRATION* vibration) {
  if (user_index >= 4) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  // Accept vibration but do nothing
  return X_ERROR_SUCCESS;
}

X_RESULT NopInputDriver::GetKeystroke(uint32_t user_index, uint32_t flags,
                                      X_INPUT_KEYSTROKE* out_keystroke) {
  if (user_index >= 4) {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }
  // No keystrokes available
  return X_ERROR_EMPTY;
}

}  // namespace rex::input::nop
