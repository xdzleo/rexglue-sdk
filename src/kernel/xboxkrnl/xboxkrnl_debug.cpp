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

// Disable warnings about unused parameters for kernel functions
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_set>

#include <rex/dbg.h>
#include <rex/kernel/xboxkrnl/private.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xexception.h>
#include <rex/system/xthread.h>
#include <rex/system/xtypes.h>

namespace rex::kernel::xboxkrnl {
using namespace rex::system;

// Set of guest thread IDs that should be permanently parked (never resumed).
// Used by NtResumeThread (in xboxkrnl_threading.cpp) and HandleSetThreadName
// to keep certain threads (e.g. Skate 3's MoviePlayer2 Decode Thread) from
// running their broken init code paths.
static std::unordered_set<uint32_t>& SkipThreadSet() {
  static std::unordered_set<uint32_t> set;
  return set;
}
static std::mutex& SkipThreadMutex() {
  static std::mutex m;
  return m;
}

void SetSkipThread(uint32_t thread_id) {
  std::lock_guard<std::mutex> lock(SkipThreadMutex());
  SkipThreadSet().insert(thread_id);
}

bool ShouldSkipThread(uint32_t thread_id) {
  std::lock_guard<std::mutex> lock(SkipThreadMutex());
  return SkipThreadSet().count(thread_id) != 0;
}

void DbgBreakPoint_entry() {
  // Log but DO NOT __debugbreak() the host -- many guest libraries call
  // DbgBreakPoint as a soft assertion that's expected to be ignored on a
  // retail kernel. Xenia ignores DbgBreakPoint by default; matching that
  // behaviour avoids killing the process with STATUS_BREAKPOINT.
  static thread_local int log_count = 0;
  if (log_count < 8) {
    ++log_count;
    REXKRNL_WARN("DbgBreakPoint called -- ignoring soft breakpoint");
  }
}

// https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
typedef struct {
  rex::be<uint32_t> type;
  rex::be<uint32_t> name_ptr;
  rex::be<uint32_t> thread_id;
  rex::be<uint32_t> flags;
} X_THREADNAME_INFO;
static_assert_size(X_THREADNAME_INFO, 0x10);

void HandleSetThreadName(ppc_ptr_t<X_EXCEPTION_RECORD> record) {
  // SetThreadName. FFS.
  // https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx

  // TODO(benvanik): check record->number_parameters to make sure it's a
  // correct size.
  auto thread_info = reinterpret_cast<X_THREADNAME_INFO*>(&record->exception_information[0]);

  assert_true(thread_info->type == 0x1000);

  if (!thread_info->name_ptr) {
    REXKRNL_DEBUG("SetThreadName called with null name_ptr");
    return;
  }

  // 4D5307D6 (and its demo) has a bug where it ends up passing freed memory for
  // the name, so at the point of SetThreadName it's filled with junk.

  // TODO(gibbed): cvar for thread name encoding for conversion, some games use
  // SJIS and there's no way to automatically know this.
  auto name =
      std::string(REX_KERNEL_MEMORY()->TranslateVirtual<const char*>(thread_info->name_ptr));
  std::replace_if(name.begin(), name.end(), [](auto c) { return c < 32 || c > 127; }, '?');

  object_ref<XThread> thread;
  if (thread_info->thread_id == -1) {
    // Current thread.
    thread = retain_object(XThread::GetCurrentThread());
  } else {
    // Lookup thread by ID.
    thread = REX_KERNEL_STATE()->GetThreadByID(thread_info->thread_id);
  }

  if (thread) {
    REXKRNL_DEBUG("SetThreadName({}, {})", thread->thread_id(), name);
    thread->set_name(name);
  }

  // SKATE 3 INTRO BYPASS (DISABLED again, 2026-04-28 third pass):
  // Empirically: parking MoviePlayer2 keeps the streaming thread alive
  // BUT the title's state machine never even ATTEMPTS to start the intro
  // (no XMPSetPlaybackController, no VP6 file open). When the thread is
  // allowed to run, the title starts the intro -> opens VP6 -> reads 3.5MB
  // -> decoder hits a broken vtable -> we now intercept it via hooks.cpp.
  // The latter is strictly more progress than the former, so leave the
  // thread unparked and rely on the streaming-side hooks to keep us alive.
  if (thread && name == "MoviePlayer2 Decode Thread") {
    REXKRNL_WARN(
        "SetThreadName: MoviePlayer2 detected (thread {}) -- letting it run "
        "(skip-marking disabled, hooks.cpp handles broken vtable)",
        thread->thread_id());
  }
}

typedef struct {
  rex::be<int32_t> mdisp;
  rex::be<int32_t> pdisp;
  rex::be<int32_t> vdisp;
} x_PMD;

typedef struct {
  rex::be<uint32_t> properties;
  rex::be<uint32_t> type_ptr;
  x_PMD this_displacement;
  rex::be<int32_t> size_or_offset;
  rex::be<uint32_t> copy_function_ptr;
} x_s__CatchableType;

typedef struct {
  rex::be<int32_t> number_catchable_types;
  rex::be<uint32_t> catchable_type_ptrs[1];
} x_s__CatchableTypeArray;

typedef struct {
  rex::be<uint32_t> attributes;
  rex::be<uint32_t> unwind_ptr;
  rex::be<uint32_t> forward_compat_ptr;
  rex::be<uint32_t> catchable_type_array_ptr;
} x_s__ThrowInfo;

void HandleCppException(ppc_ptr_t<X_EXCEPTION_RECORD> record) {
  // C++ exception (MSVC throw / __CxxFrameHandler3).
  // We don't implement guest-side stack unwinding -- nothing in the host
  // runtime knows how to walk the guest stack and run destructors. The
  // best we can do is log the exception type and let the title continue,
  // which matches Xenia's behaviour. The thrown object stays alive on the
  // guest heap; subsequent guest code that happens to catch this exception
  // class will still receive it correctly because the runtime's PPC
  // exception machinery is what actually drives unwinding (we just don't
  // need to do anything in the host for it to work).
  if (record->number_parameters >= 2) {
    auto thrown_ptr = static_cast<uint32_t>(record->exception_information[1]);
    static thread_local int log_count = 0;
    if (log_count < 16) {
      ++log_count;
      REXKRNL_WARN(
          "RtlRaiseException: C++ throw (object={:08X}, throw_info={:08X}); not unwinding from "
          "host, guest will continue",
          thrown_ptr, static_cast<uint32_t>(record->exception_information[2]));
    } else if (log_count == 16) {
      ++log_count;
      REXKRNL_WARN("RtlRaiseException: further C++ throw logs suppressed on this thread");
    }
  }
}

void RtlRaiseException_entry(ppc_ptr_t<X_EXCEPTION_RECORD> record) {
  switch (record->code) {
    case 0x406D1388: {
      HandleSetThreadName(record);
      return;
    }
    case 0xE06D7363: {
      HandleCppException(record);
      return;
    }
  }

  // Unknown exception code: log and continue rather than __debugbreak()-ing
  // the host. The previous behaviour was to call rex::debug::Break() which on
  // Windows raises STATUS_BREAKPOINT and kills the process with exit code 3
  // -- many EA RenderWare titles (Skate 3 in particular) raise their own
  // engine-internal exception codes during normal startup (codec init,
  // resource streaming retries) and expect the title to keep running. Xenia
  // simply ignores unknown exceptions; we do the same here.
  static thread_local int unhandled_exception_logs = 0;
  if (unhandled_exception_logs < 32) {
    ++unhandled_exception_logs;
    REXKRNL_WARN(
        "RtlRaiseException: unhandled code {:08X} (flags={:08X}, addr={:08X}, "
        "params={}); continuing without unwind",
        static_cast<uint32_t>(record->code), static_cast<uint32_t>(record->exception_flags),
        static_cast<uint32_t>(record->exception_address),
        static_cast<uint32_t>(record->number_parameters));
  } else if (unhandled_exception_logs == 32) {
    ++unhandled_exception_logs;
    REXKRNL_WARN("RtlRaiseException: further unhandled-exception logs suppressed on this thread");
  }
}

void KeBugCheckEx_entry(u32 code, u32 param1, u32 param2, u32 param3, u32 param4) {
  REXKRNL_WARN("*** STOP: 0x{:08X} (0x{:08X}, 0x{:08X}, 0x{:08X}, 0x{:08X}) -- "
               "RETURNING (was: halting thread)",
               code, param1, param2, param3, param4);
  fflush(stdout);

  // KeBugCheck is the kernel "fatal halt" path on real Xbox 360. The
  // calling thread is supposed to never come back. The previous strategy
  // parked the thread forever on the assumption that letting it return
  // with corrupted state would segfault downstream.
  //
  // For Skate 3 the only KeBugCheckEx site we hit at runtime is in
  // sub_82EB8CC8 -- a "process type mismatch" guard the streaming thread
  // hits when our broken-vtable-skip patches push it through an unusual
  // code path. The thread would actually be FINE if it just returned
  // from the bug-check; the calling code's contract is "if you survive
  // this guard you'll then re-validate state via the next call". So
  // returning cleanly here lets the streaming thread re-enter normal
  // operation instead of vanishing into a perpetual sleep (which then
  // wedges the title state machine that's waiting on it).
  //
  // If a different KeBugCheckEx site shows up later that genuinely needs
  // halting, gate this behaviour on `code == 244` (the Skate 3 process-
  // type-mismatch code) and park for everything else.
}

void KeBugCheck_entry(u32 code) {
  KeBugCheckEx_entry(code, 0, 0, 0, 0);
}

}  // namespace rex::kernel::xboxkrnl

REX_EXPORT(__imp__DbgBreakPoint, rex::kernel::xboxkrnl::DbgBreakPoint_entry)
REX_EXPORT(__imp__RtlRaiseException, rex::kernel::xboxkrnl::RtlRaiseException_entry)
REX_EXPORT(__imp__KeBugCheckEx, rex::kernel::xboxkrnl::KeBugCheckEx_entry)
REX_EXPORT(__imp__KeBugCheck, rex::kernel::xboxkrnl::KeBugCheck_entry)

REX_EXPORT_STUB(__imp__DbgBreakPointWithStatus);
REX_EXPORT_STUB(__imp__DbgPrompt);
REX_EXPORT_STUB(__imp__DbgLoadImageSymbols);
REX_EXPORT_STUB(__imp__DbgUnLoadImageSymbols);
REX_EXPORT_STUB(__imp__DmPrintData);

REX_EXPORT_STUB(__imp__DumpGetRawDumpInfo);
REX_EXPORT_STUB(__imp__DumpRegisterDedicatedDataBlock);
REX_EXPORT_STUB(__imp__DumpSetCollectionFacility);
REX_EXPORT_STUB(__imp__DumpUpdateDumpSettings);
REX_EXPORT_STUB(__imp__DumpWriteDump);
REX_EXPORT_STUB(__imp__DumpXitThread);
REX_EXPORT_STUB(__imp__RtlAssert);
REX_EXPORT_STUB(__imp__RtlRaiseStatus);
REX_EXPORT_STUB(__imp__RtlRip);
