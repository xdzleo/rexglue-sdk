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

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>

#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/kernel/xam/private.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xtypes.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace rex {
namespace kernel {
namespace xam {
using namespace rex::system;

using rex::input::X_INPUT_CAPABILITIES;
using rex::input::X_INPUT_KEYSTROKE;
using rex::input::X_INPUT_STATE;
using rex::input::X_INPUT_VIBRATION;

constexpr uint32_t XINPUT_FLAG_GAMEPAD = 0x01;
constexpr uint32_t XINPUT_FLAG_ANY_USER = 1 << 30;

rex::input::InputSystem* input_system() {
  return static_cast<rex::input::InputSystem*>(REX_KERNEL_STATE()->emulator()->input_system());
}

bool HasActiveInput(const X_INPUT_STATE& state) {
  return static_cast<uint16_t>(state.gamepad.buttons) != 0 ||
         state.gamepad.left_trigger != 0 || state.gamepad.right_trigger != 0 ||
         static_cast<int16_t>(state.gamepad.thumb_lx) != 0 ||
         static_cast<int16_t>(state.gamepad.thumb_ly) != 0 ||
         static_cast<int16_t>(state.gamepad.thumb_rx) != 0 ||
         static_cast<int16_t>(state.gamepad.thumb_ry) != 0;
}

void NormalizeSkateGamepadCapabilities(X_INPUT_CAPABILITIES* caps) {
  caps->type = 0x01;      // XINPUT_DEVTYPE_GAMEPAD
  // Skate 3 (and several other EA RenderWare titles) gate input processing
  // behind this wired-controller-looking descriptor. Normalize real and
  // fallback driver responses so the active slot can't get a different shape.
  caps->sub_type = 0x02;
  caps->flags = static_cast<uint16_t>(caps->flags) | 0x0003;

  if (static_cast<uint16_t>(caps->gamepad.buttons) == 0) {
    caps->gamepad.buttons = 0xFFFF;
  }
  caps->gamepad.left_trigger = std::max<uint8_t>(caps->gamepad.left_trigger, 0xFF);
  caps->gamepad.right_trigger = std::max<uint8_t>(caps->gamepad.right_trigger, 0xFF);
  caps->gamepad.thumb_lx =
      static_cast<int16_t>(std::max<int16_t>(static_cast<int16_t>(caps->gamepad.thumb_lx), 0x7FFF));
  caps->gamepad.thumb_ly =
      static_cast<int16_t>(std::max<int16_t>(static_cast<int16_t>(caps->gamepad.thumb_ly), 0x7FFF));
  caps->gamepad.thumb_rx =
      static_cast<int16_t>(std::max<int16_t>(static_cast<int16_t>(caps->gamepad.thumb_rx), 0x7FFF));
  caps->gamepad.thumb_ry =
      static_cast<int16_t>(std::max<int16_t>(static_cast<int16_t>(caps->gamepad.thumb_ry), 0x7FFF));
  caps->vibration.left_motor_speed =
      static_cast<uint16_t>(std::max<uint16_t>(caps->vibration.left_motor_speed, 0xFFFF));
  caps->vibration.right_motor_speed =
      static_cast<uint16_t>(std::max<uint16_t>(caps->vibration.right_motor_speed, 0xFFFF));
}

void FillSkateGamepadCapabilities(X_INPUT_CAPABILITIES* caps) {
  std::memset(caps, 0, sizeof(X_INPUT_CAPABILITIES));
  NormalizeSkateGamepadCapabilities(caps);
}

void XamResetInactivity_entry() {
  // Do we need to do anything?
}

u32 XamEnableInactivityProcessing_entry(u32 unk, u32 enable) {
  return X_ERROR_SUCCESS;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetcapabilities(v=vs.85).aspx
u32 XamInputGetCapabilities_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  REXKRNL_TRACE("[XAM] XamInputGetCapabilities called: user={}, flags=0x{:X}", (uint32_t)user_index,
                (uint32_t)flags);
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  // Periodically rebroadcast XN_SYS_INPUTDEVICESCHANGED with a non-zero
  // mask (0x0F = all 4 users have a new device) so titles like Skate 3
  // that interpret param=0 as "noise / no real change" actually treat the
  // event as actionable and re-poll XamInputGetState. Some EA RenderWare
  // titles also expect XN_SYS_SIGNINCHANGED (0x0A) with data=1 to signal
  // "user 0 just signed in"; if the game gates input behind sign-in
  // detection it'll still ignore device-changed events otherwise. We
  // throttle to once per second to avoid flooding the listener queues.
  {
    static std::atomic<int64_t> s_last_broadcast_ms{0};
    int64_t now_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                              std::chrono::steady_clock::now().time_since_epoch())
                                              .count());
    int64_t last = s_last_broadcast_ms.load(std::memory_order_relaxed);
    if (now_ms - last >= 1000) {
      if (s_last_broadcast_ms.compare_exchange_strong(last, now_ms,
                                                       std::memory_order_relaxed)) {
        auto* ks = REX_KERNEL_STATE();
        if (ks) {
          // data = bitmask of users with new devices. 0x0F = all four users.
          ks->BroadcastNotification(0x00000012, 0xF);
          ks->BroadcastNotification(0x00000013, 0xF);
          ks->BroadcastNotification(0x0000000A, 1);  // SIGNINCHANGED user 0
        }
      }
    }
  }

  auto* is = input_system();
  X_RESULT result = is->GetCapabilities(actual_user_index, flags, caps);

  // Fallback: pretend a virtual gamepad is connected for ALL 4 user slots
  // when no driver claims them. EA RenderWare titles (Skate 3, etc) treat
  // "all users disconnected" as "no controller world available" and stall
  // their entire input pipeline. Mirroring UnleashedRecomp's hid::
  // GetCapabilities pattern, we synthesize a valid gamepad descriptor so
  // every Capabilities query for any user returns SUCCESS.
  if (result != X_ERROR_SUCCESS) {
    FillSkateGamepadCapabilities(static_cast<X_INPUT_CAPABILITIES*>(caps));
    return X_ERROR_SUCCESS;
  }
  NormalizeSkateGamepadCapabilities(static_cast<X_INPUT_CAPABILITIES*>(caps));
  return result;
}

u32 XamInputGetCapabilitiesEx_entry(u32 unk, u32 user_index, u32 flags,
                                    ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  X_RESULT result = is->GetCapabilities(actual_user_index, flags, caps);
  if (result != X_ERROR_SUCCESS) {
    FillSkateGamepadCapabilities(static_cast<X_INPUT_CAPABILITIES*>(caps));
    return X_ERROR_SUCCESS;
  }
  NormalizeSkateGamepadCapabilities(static_cast<X_INPUT_CAPABILITIES*>(caps));
  return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetstate(v=vs.85).aspx
u32 XamInputGetState_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_STATE> input_state) {
  // Games call this with a NULL state ptr, probably as a query.
  // Removed the call-count cap so we can verify the game is actually polling
  // when the SDL keyboard fallback fires.
  static std::atomic<int> call_count{0};
  int n = call_count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 100 || (n % 60 == 0)) {
    REXLOG_INFO("[XAM] XamInputGetState called #{} user={}, flags=0x{:X}", n,
                (uint32_t)user_index, (uint32_t)flags);
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  X_RESULT result = is->GetState(actual_user_index, input_state);
  if (input_state && result != X_ERROR_SUCCESS) {
    std::memset(static_cast<X_INPUT_STATE*>(input_state), 0, sizeof(X_INPUT_STATE));
  }

  // Skate 3 may bind its menu input owner to user 3 even when the only real
  // host controller is exposed by XInput as user 0. The NOP driver keeps all
  // four guest slots connected, but its idle state can otherwise hide the real
  // slot-0 controller from the menu poller.
  if (input_state && actual_user_index != 0) {
    X_INPUT_STATE user0_state{};
    X_RESULT user0_result = is->GetState(0, &user0_state);
    if (user0_result == X_ERROR_SUCCESS && HasActiveInput(user0_state) &&
        (result != X_ERROR_SUCCESS ||
         !HasActiveInput(*static_cast<X_INPUT_STATE*>(input_state)))) {
      *static_cast<X_INPUT_STATE*>(input_state) = user0_state;
      result = X_ERROR_SUCCESS;

      static thread_local uint16_t last_mirrored_buttons = 0;
      uint16_t buttons = static_cast<uint16_t>(user0_state.gamepad.buttons);
      if (buttons != last_mirrored_buttons) {
        last_mirrored_buttons = buttons;
        REXLOG_INFO("[XAM] Mirrored user 0 input to user {}: buttons=0x{:04X}",
                    (uint32_t)actual_user_index, buttons);
      }
    }
  }

  // SDL keyboard fallback (LibertyRecomp pattern): if the InputSystem driver
  // chain didn't populate any buttons, query SDL_GetKeyboardState directly
  // so titles that don't see a real controller can still drive the menus
  // with WASD/Space/Enter. Skate 3's input layer ignores the rexglue MnK
  // driver entirely (its polling loop only fires once at startup), so we
  // need this redundant path right here at the XamInputGetState boundary.
  //
  // Apply to ALL 4 user slots: Skate 3's main input loop walks through
  // user 0..3 sequentially polling whichever is "active". If we only
  // mapped user 0 the menu would never see the keyboard input on titles
  // that picked another slot at boot.
  if (input_state) {
    int num_keys = 0;
    const bool* keys = SDL_GetKeyboardState(&num_keys);
    if (keys && num_keys > 0) {
      uint16_t buttons = static_cast<uint16_t>(input_state->gamepad.buttons);
      auto check = [&](int sc, uint16_t bit) {
        if (sc < num_keys && keys[sc])
          buttons |= bit;
      };
      // Map common keys (SDL3 scancodes) to Xbox 360 buttons.
      check(SDL_SCANCODE_SPACE, 0x1000);  // A
      check(SDL_SCANCODE_RETURN, 0x1000); // A (alt)
      check(SDL_SCANCODE_LSHIFT, 0x2000); // B
      check(SDL_SCANCODE_RSHIFT, 0x2000); // B
      check(SDL_SCANCODE_R, 0x4000);      // X
      check(SDL_SCANCODE_E, 0x8000);      // Y
      check(SDL_SCANCODE_TAB, 0x0020);    // Back
      check(SDL_SCANCODE_ESCAPE, 0x0010); // Start
      check(SDL_SCANCODE_UP, 0x0001);     // DPad Up
      check(SDL_SCANCODE_DOWN, 0x0002);   // DPad Down
      check(SDL_SCANCODE_LEFT, 0x0004);   // DPad Left
      check(SDL_SCANCODE_RIGHT, 0x0008);  // DPad Right
      check(SDL_SCANCODE_W, 0x0001);      // WASD also drives DPad
      check(SDL_SCANCODE_S, 0x0002);
      check(SDL_SCANCODE_A, 0x0004);
      check(SDL_SCANCODE_D, 0x0008);
      check(SDL_SCANCODE_Q, 0x0100);      // LB
      check(SDL_SCANCODE_F, 0x0200);      // RB

      // Auto-press A at ~8s to advance the language menu. Held 300ms
      // gives a clean rising/falling edge for any edge-detected check.
      if (actual_user_index == 0) {
        static auto s_t0 = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_t0).count();
        if (ms >= 8000 && ms < 8300) {
          buttons |= 0x1000;
        }
      }

      // Always bump the packet so "is state different from last?" checks
      // in the game's input layer see a moving counter -- some titles cache
      // the state pointer/packet and only re-evaluate buttons when they
      // see the packet advance. RenderWare's input update loop is one of
      // those, so we keep the packet monotonically increasing rather than
      // gating the bump on button changes.
      static thread_local uint16_t last_buttons = 0;
      static std::atomic<uint32_t> monotonic_packet{0};
      uint32_t packet = monotonic_packet.fetch_add(1, std::memory_order_relaxed) + 1;
      if (buttons != last_buttons) {
        last_buttons = buttons;
        REXLOG_INFO("[XAM] SDL keyboard fallback: buttons=0x{:04X} packet={} (CHANGED)",
                    buttons, packet);
      }
      input_state->gamepad.buttons = buttons;
      input_state->packet_number = packet;
      if (result != X_ERROR_SUCCESS) {
        // Make failed-driver fallback state explicit rather than preserving
        // whatever the caller had in the buffer.
        input_state->gamepad.left_trigger = 0;
        input_state->gamepad.right_trigger = 0;
      }
    }
  }

  if (result != X_ERROR_SUCCESS && input_state) {
    return X_ERROR_SUCCESS;
  }
  return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputsetstate(v=vs.85).aspx
u32 XamInputSetState_entry(u32 user_index, u32 unk, ppc_ptr_t<X_INPUT_VIBRATION> vibration) {
  if (!vibration) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  uint32_t actual_user_index = user_index;
  if ((user_index & 0xFF) == 0xFF) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  return is->SetState(actual_user_index, vibration);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetkeystroke(v=vs.85).aspx
u32 XamInputGetKeystroke_entry(u32 user_index, u32 flags, ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  // https://github.com/CodeAsm/ffplay360/blob/master/Common/AtgXime.cpp
  // user index = index or XUSER_INDEX_ANY
  // flags = XINPUT_FLAG_GAMEPAD (| _ANYUSER | _ANYDEVICE)

  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  return is->GetKeystroke(actual_user_index, flags, keystroke);
}

// Same as non-ex, just takes a pointer to user index.
u32 XamInputGetKeystrokeEx_entry(mapped_u32 user_index_ptr, u32 flags,
                                 ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t user_index = *user_index_ptr;
  if ((user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    user_index = 0;
  }

  auto* is = input_system();
  auto result = is->GetKeystroke(user_index, flags, keystroke);
  if (XSUCCEEDED(result)) {
    *user_index_ptr = keystroke->user_index;
  }
  return result;
}

i32 XamUserGetDeviceContext_entry(u32 user_index, u32 unk, mapped_u32 out_ptr) {
  // Games check the result - usually with some masking.
  // If this function fails they assume zero, so let's fail AND
  // set zero just to be safe.
  *out_ptr = 0;
  if (!user_index || (user_index & 0xFF) == 0xFF) {
    return X_E_SUCCESS;
  } else {
    return X_E_DEVICE_NOT_CONNECTED;
  }
}

}  // namespace xam
}  // namespace kernel
}  // namespace rex

REX_EXPORT(__imp__XamResetInactivity, rex::kernel::xam::XamResetInactivity_entry)
REX_EXPORT(__imp__XamEnableInactivityProcessing,
           rex::kernel::xam::XamEnableInactivityProcessing_entry)
REX_EXPORT(__imp__XamInputGetCapabilities, rex::kernel::xam::XamInputGetCapabilities_entry)
REX_EXPORT(__imp__XamInputGetCapabilitiesEx, rex::kernel::xam::XamInputGetCapabilitiesEx_entry)
REX_EXPORT(__imp__XamInputGetState, rex::kernel::xam::XamInputGetState_entry)
REX_EXPORT(__imp__XamInputSetState, rex::kernel::xam::XamInputSetState_entry)
REX_EXPORT(__imp__XamInputGetKeystroke, rex::kernel::xam::XamInputGetKeystroke_entry)
REX_EXPORT(__imp__XamInputGetKeystrokeEx, rex::kernel::xam::XamInputGetKeystrokeEx_entry)
REX_EXPORT(__imp__XamUserGetDeviceContext, rex::kernel::xam::XamUserGetDeviceContext_entry)

REX_EXPORT_STUB(__imp__XamInputControl);
REX_EXPORT_STUB(__imp__XamInputEnableAutobind);
REX_EXPORT_STUB(__imp__XamInputGetDeviceStats);
REX_EXPORT_STUB(__imp__XamInputGetFailedConnectionOrBind);
REX_EXPORT_STUB(__imp__XamInputGetKeyLocks);
REX_EXPORT_STUB(__imp__XamInputGetKeystrokeHud);
REX_EXPORT_STUB(__imp__XamInputGetKeystrokeHudEx);
REX_EXPORT_STUB(__imp__XamInputGetUserVibrationLevel);
REX_EXPORT_STUB(__imp__XamInputNonControllerGetRaw);
REX_EXPORT_STUB(__imp__XamInputNonControllerGetRawEx);
REX_EXPORT_STUB(__imp__XamInputNonControllerSetRaw);
REX_EXPORT_STUB(__imp__XamInputNonControllerSetRawEx);
REX_EXPORT_STUB(__imp__XamInputRawState);
REX_EXPORT_STUB(__imp__XamInputResetLayoutKeyboard);
REX_EXPORT_STUB(__imp__XamInputSendStayAliveRequest);
REX_EXPORT_STUB(__imp__XamInputSendXenonButtonPress);
REX_EXPORT_STUB(__imp__XamInputSetKeyLocks);
REX_EXPORT_STUB(__imp__XamInputSetKeyboardTranslationHud);
REX_EXPORT_STUB(__imp__XamInputSetLayoutKeyboard);
REX_EXPORT_STUB(__imp__XamInputSetMinMaxAuthDelay);
REX_EXPORT_STUB(__imp__XamInputSetTextMessengerIndicator);
REX_EXPORT_STUB(__imp__XamInputToggleKeyLocks);
