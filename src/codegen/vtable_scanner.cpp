/**
 * @file        rexcodegen/vtable_scanner.cpp
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/codegen/vtable_scanner.h>
#include <rex/logging.h>

#include "codegen_logging.h"
#include <rex/memory/utils.h>
#include <rex/types.h>

using rex::memory::load_and_swap;

namespace rex::codegen {

VTableScanner::VTableScanner(const BinaryView& binary) : binary_(binary) {}

std::vector<VTableInfo> VTableScanner::scan() {
  std::vector<VTableInfo> vtables;

  // Step 1: Find all Complete Object Locators
  auto cols = findCompleteObjectLocators();
  REXCODEGEN_DEBUG("VTableScanner: found {} Complete Object Locators", cols.size());

  // Step 2: For each COL, find its vtable and read slots
  for (uint32_t colAddr : cols) {
    auto vtableAddr = findVTableForCOL(colAddr);
    if (!vtableAddr) {
      REXCODEGEN_TRACE("VTableScanner: COL at 0x{:08X} has no referencing vtable", colAddr);
      continue;
    }

    VTableInfo info;
    info.vtableAddress = *vtableAddr;
    info.colAddress = colAddr;
    info.className = extractClassName(colAddr);
    info.slots = readVTableSlots(*vtableAddr);

    if (info.slots.empty()) {
      REXCODEGEN_TRACE("VTableScanner: vtable at 0x{:08X} has no valid slots", *vtableAddr);
      continue;
    }

    REXCODEGEN_DEBUG("VTableScanner: vtable at 0x{:08X} ({}) has {} slots", info.vtableAddress,
                     info.className, info.slots.size());

    vtables.push_back(std::move(info));
  }

  return vtables;
}

std::vector<VTableInfo> VTableScanner::scanHeuristic() {
  // Heuristic vtable discovery for binaries with no RTTI / no Complete
  // Object Locators. We scan .rdata at 4-byte intervals looking for runs
  // of dwords that all decode as valid in-section function pointers. A
  // "valid" entry is either:
  //   * an executable address aligned to 4 (treated as a real function)
  //   * 0 (treated as an abstract method / padding within a vtable)
  // A run becomes a candidate vtable when we see at least kMinValidEntries
  // real (non-zero) functions among the first kMaxScanSlots slots, and is
  // separated from neighbors by either a non-aligned dword, an obviously
  // non-pointer dword, or a long run of zeros.
  //
  // This is necessarily approximate: we may pick up jump tables, function
  // pointer arrays that aren't C++ vtables, or even short floating-point
  // arrays that happen to look like code addresses. False positives just
  // mean a few extra functions get queued for recompilation, which is
  // fine; false negatives are what we're trying to fix.
  std::vector<VTableInfo> vtables;

  const auto* rdata = binary_.findSectionByName(".rdata");
  if (!rdata || !rdata->data) {
    return vtables;
  }

  const uint8_t* data = rdata->data;
  uint32_t base = rdata->baseAddress;
  size_t size = rdata->size;

  if (size < 16) {
    return vtables;
  }

  constexpr size_t kMinValidEntries = 4;     // need >=4 real functions
  constexpr size_t kMaxScanSlots = 256;      // cap per candidate
  constexpr int kMaxConsecutiveNulls = 3;    // tolerated mid-vtable

  size_t found = 0;

  // A "looks like a function prologue" sniff. Returns true if the dword
  // at `funcAddr` starts with one of the common PPC function prologue
  // patterns (mflr / mfspr lr / stwu r1,-X(r1)). Filters out false-
  // positive vtable candidates where slot[0] points into the middle of
  // a function or into raw data that happens to land in .text.
  auto looksLikePrologue = [&](uint32_t funcAddr) -> bool {
    const auto* sec = binary_.findSection(funcAddr);
    if (!sec || !sec->data) return false;
    uint32_t off = funcAddr - sec->baseAddress;
    if (off + 4 > sec->size) return false;
    uint32_t insn = load_and_swap<uint32_t>(sec->data + off);

    // mflr rD: opcode=31, ext=339, the canonical "mflr r12" is 7D8802A6
    if ((insn & 0xFC0007FE) == 0x7C0002A6) {
      // mfspr rD, SPR -- check SPR == 8 (LR)
      uint32_t spr = ((insn >> 16) & 0x1F) | (((insn >> 11) & 0x1F) << 5);
      if (spr == 8) return true;
    }
    // stwu r1, -X(r1): primary opcode 37, with RA == RS == 1
    // 0x9421FF80 is the canonical "stwu r1, -128(r1)" pattern.
    if ((insn & 0xFFFF0000) == 0x94210000) {
      int16_t simm = static_cast<int16_t>(insn & 0xFFFF);
      if (simm < 0) return true;
    }
    // bl __savegprlr_NN  (the recompiled XBox 360 prologue helper).
    // primary opcode 18, AA=0, LK=1.
    if ((insn & 0xFC000003) == 0x48000001) return true;
    return false;
  };

  for (size_t offset = 0; offset + 4 <= size; offset += 4) {
    // Quick reject: first dword must be a valid function pointer that
    // starts with a real prologue. Without this we pick up jump tables
    // and switch-statement data, whose entries point into the middle of
    // functions and produce garbage when treated as vtable methods.
    uint32_t firstSlot = load_and_swap<uint32_t>(data + offset);
    if (firstSlot == 0 || (firstSlot & 0x3) != 0 || !isExecutableAddress(firstSlot)) {
      continue;
    }
    // (No prologue check -- requiring a prologue in slot[0..3] filtered
    // out real EA RenderWare vtables whose first method is a
    // single-instruction trivial destructor or a tail-call helper.
    // Without the check we get more false positives but also catch
    // more real vtables; the recompilation pipeline tolerates extra
    // function entries gracefully -- any false-positive that gets
    // called at runtime hits the standard NULL-bctrl fallback path
    // rather than causing a host crash.)

    // Walk forward collecting slots.
    std::vector<uint32_t> slots;
    int consecutiveNulls = 0;
    size_t realCount = 0;
    size_t walkOffset = offset;

    for (size_t s = 0; s < kMaxScanSlots && walkOffset + 4 <= size; ++s) {
      uint32_t slot = load_and_swap<uint32_t>(data + walkOffset);

      if (slot == 0) {
        ++consecutiveNulls;
        if (consecutiveNulls > kMaxConsecutiveNulls) {
          break;
        }
        slots.push_back(0);
        walkOffset += 4;
        continue;
      }

      if ((slot & 0x3) != 0) {
        // misaligned -- end of vtable
        break;
      }
      if (!isExecutableAddress(slot)) {
        // pointer that doesn't go into .text -- end of vtable
        break;
      }

      consecutiveNulls = 0;
      slots.push_back(slot);
      ++realCount;
      walkOffset += 4;
    }

    // Trim trailing NULLs.
    while (!slots.empty() && slots.back() == 0) {
      slots.pop_back();
    }

    if (realCount < kMinValidEntries) {
      continue;  // too short to be a real vtable
    }

    VTableInfo info;
    info.vtableAddress = base + static_cast<uint32_t>(offset);
    info.colAddress = 0;  // no RTTI for these
    info.className.clear();
    info.slots = std::move(slots);
    vtables.push_back(std::move(info));

    // Skip past the vtable we just consumed so we don't re-emit overlap.
    offset = walkOffset - 4;  // -4 because the for loop adds 4
    ++found;
  }

  REXCODEGEN_DEBUG("VTableScanner: heuristic scan found {} candidate vtables", found);
  return vtables;
}

std::vector<uint32_t> VTableScanner::findCompleteObjectLocators() {
  std::vector<uint32_t> cols;

  // Scan .rdata section for COL patterns
  const auto* rdata = binary_.findSectionByName(".rdata");
  if (!rdata || !rdata->data) {
    REXCODEGEN_WARN("VTableScanner: .rdata section not found");
    return cols;
  }

  const uint8_t* data = rdata->data;
  uint32_t base = rdata->baseAddress;
  size_t size = rdata->size;

  // COL is 20 bytes, need room for it
  if (size < sizeof(RTTICompleteObjectLocator)) {
    return cols;
  }

  // Scan for COL pattern: signature=0, valid type descriptor pointer
  for (size_t offset = 0; offset + sizeof(RTTICompleteObjectLocator) <= size; offset += 4) {
    auto* col = reinterpret_cast<const RTTICompleteObjectLocator*>(data + offset);

    uint32_t signature = load_and_swap<uint32_t>(&col->signature);
    uint32_t typeDescPtr = load_and_swap<uint32_t>(&col->pTypeDescriptor);

    // COL signature must be 0 for 32-bit MSVC RTTI
    if (signature != 0) {
      continue;
    }

    // Type descriptor must point to valid memory
    const auto* typeDescSection = binary_.findSection(typeDescPtr);
    if (!typeDescSection || !typeDescSection->data) {
      continue;
    }

    // Check if type descriptor has ".?AV" mangling prefix
    std::string typeName = readString(typeDescPtr + 8, 64);
    if (typeName.find(".?AV") != 0 && typeName.find(".?AU") != 0) {
      continue;
    }

    uint32_t colAddr = base + static_cast<uint32_t>(offset);
    cols.push_back(colAddr);

    REXCODEGEN_TRACE("VTableScanner: found COL at 0x{:08X} -> {}", colAddr, typeName);
  }

  return cols;
}

std::optional<uint32_t> VTableScanner::findVTableForCOL(uint32_t colAddr) {
  // The vtable pointer to COL is stored at vtable[-1]
  // So we need to find a dword in .rdata that contains colAddr,
  // and the vtable starts at that address + 4

  const auto* rdata = binary_.findSectionByName(".rdata");
  if (!rdata || !rdata->data) {
    return std::nullopt;
  }

  const uint8_t* data = rdata->data;
  uint32_t base = rdata->baseAddress;
  size_t size = rdata->size;

  for (size_t offset = 0; offset + 4 <= size; offset += 4) {
    uint32_t value = load_and_swap<uint32_t>(data + offset);

    if (value == colAddr) {
      // Found reference to COL - vtable starts at next dword
      uint32_t vtableAddr = base + static_cast<uint32_t>(offset) + 4;
      return vtableAddr;
    }
  }

  return std::nullopt;
}

std::vector<uint32_t> VTableScanner::readVTableSlots(uint32_t vtableStart) {
  // The original implementation stopped reading slots the moment it saw a
  // NULL or non-executable dword. That's WRONG for many real-world C++
  // vtables: abstract / unimplemented methods, padding between method
  // groups, or alignment fillers can all show up as NULL slots in the
  // middle of an otherwise valid vtable. Stopping early made the
  // recompiler miss every method beyond the first NULL entry, which then
  // didn't get its body emitted -- those slots came back as
  // mem[vtable + N] == 0x00000000 at runtime, causing NULL bctrl crashes
  // (the entire reason Skate 3's intro / video pipelines were broken).
  //
  // New strategy: walk slot-by-slot, tolerate isolated NULL entries (we
  // record them as 0 so callers know the slot exists but is abstract),
  // and only terminate when we see strong evidence we've left the vtable
  // -- e.g. several non-executable slots in a row, an aligned
  // not-a-function-pointer pattern that looks like a new RTTI structure,
  // or running off the section. We also cap at a sane upper bound to
  // avoid runaway scans into adjacent data.
  std::vector<uint32_t> slots;

  constexpr size_t kMaxSlots = 512;          // generous upper bound
  constexpr int kMaxConsecutiveNonExec = 3;  // tolerate isolated NULLs

  uint32_t slotAddr = vtableStart;
  int consecutiveNonExec = 0;

  for (size_t slotIndex = 0; slotIndex < kMaxSlots; ++slotIndex) {
    auto funcAddr = readDword(slotAddr);
    if (!funcAddr) {
      break;  // Off the section
    }

    uint32_t addr = *funcAddr;

    // Tolerate NULL entries (abstract methods / padding); record as 0 so
    // downstream code knows the slot exists, then continue.
    if (addr == 0) {
      ++consecutiveNonExec;
      if (consecutiveNonExec > kMaxConsecutiveNonExec) {
        // Too many in a row -- we've almost certainly left the vtable.
        // Trim the trailing zeros we just recorded.
        while (!slots.empty() && slots.back() == 0) {
          slots.pop_back();
        }
        break;
      }
      slots.push_back(0);
      slotAddr += 4;
      continue;
    }

    // 4-byte aligned check (PPC instructions are 4-aligned). A misaligned
    // entry is almost always start of an unrelated structure.
    if (addr & 0x3) {
      break;
    }

    // If the address isn't executable, treat it as a possibly-NULL slot
    // (e.g. it might point into .rdata for a sub-table). We only fully
    // bail when several non-executable entries appear in a row.
    if (!isExecutableAddress(addr)) {
      ++consecutiveNonExec;
      if (consecutiveNonExec > kMaxConsecutiveNonExec) {
        // Trim trailing non-exec entries we recorded as 0.
        while (!slots.empty() && slots.back() == 0) {
          slots.pop_back();
        }
        break;
      }
      slots.push_back(0);
      slotAddr += 4;
      continue;
    }

    // Genuine function pointer.
    consecutiveNonExec = 0;
    slots.push_back(addr);
    slotAddr += 4;
  }

  // Trim any trailing NULLs from the result so callers don't see the
  // sentinel padding at the end.
  while (!slots.empty() && slots.back() == 0) {
    slots.pop_back();
  }
  return slots;
}

std::string VTableScanner::extractClassName(uint32_t colAddr) {
  auto typeDescPtr = readDword(colAddr + 12);  // pTypeDescriptor offset
  if (!typeDescPtr) {
    return "";
  }

  // Class name is at typeDescriptor + 8
  std::string mangled = readString(*typeDescPtr + 8, 256);

  // Simple demangling: ".?AVClassName@@" -> "ClassName"
  if (mangled.size() > 4 && (mangled.substr(0, 4) == ".?AV" || mangled.substr(0, 4) == ".?AU")) {
    size_t end = mangled.find("@@");
    if (end != std::string::npos) {
      return mangled.substr(4, end - 4);
    }
  }

  return mangled;
}

bool VTableScanner::isExecutableAddress(uint32_t addr) const {
  return binary_.isExecutable(addr);
}

std::optional<uint32_t> VTableScanner::readDword(uint32_t addr) const {
  const auto* section = binary_.findSection(addr);
  if (!section || !section->data) {
    return std::nullopt;
  }

  uint32_t offset = addr - section->baseAddress;
  if (offset + 4 > section->size) {
    return std::nullopt;
  }

  return load_and_swap<uint32_t>(section->data + offset);
}

std::string VTableScanner::readString(uint32_t addr, size_t maxLen) const {
  const auto* section = binary_.findSection(addr);
  if (!section || !section->data) {
    return "";
  }

  uint32_t offset = addr - section->baseAddress;
  size_t available = section->size - offset;
  size_t len = std::min(maxLen, available);

  const char* str = reinterpret_cast<const char*>(section->data + offset);
  size_t actualLen = strnlen(str, len);

  return std::string(str, actualLen);
}

}  // namespace rex::codegen
