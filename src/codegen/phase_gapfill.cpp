/**
 * @file        codegen/phase_gapfill.cpp
 * @brief       GapFill phase: find uncovered code regions and register them as functions
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include "ppc/instruction.h"

#include <unordered_set>

#include <rex/codegen/phases.h>
#include "phase_helpers.h"

#include <rex/logging.h>

#include "codegen_logging.h"
#include <rex/memory/utils.h>

#include <ppc.h>

using rex::codegen::ppc::decode_instruction;
using rex::codegen::ppc::Opcode;
using rex::memory::load_and_swap;

namespace rex::codegen {

namespace {

//=============================================================================
// Data Section Function-Pointer Scan
//=============================================================================
// rexglue's MSVC-RTTI-based vtable scanner finds nothing in many EA titles
// (Skate 3 included) because they ship without RTTI. Without that, function
// pointers stored in vtables / global tables / static arrays in non-executable
// sections never get registered as function entries -- so when the recompiled
// code does `bctrl` to one, the function-table lookup is NULL and the host
// segfaults.
//
// This scanner walks every non-executable section, byte-swaps every 4-byte
// aligned dword (PPC is big-endian), and if the value lands inside the code
// region and is 4-byte aligned, registers it as a candidate function. The
// follow-up `discoverPendingFunctions` call decodes blocks for each, so the
// codegen treats them as proper function entries with their own table slot.
//
// Heuristics to limit false positives:
//   - Only consider values that are 4-byte aligned (PPC instruction alignment).
//   - Only consider values inside [code_base, code_base + code_size).
//   - Skip if already a known entry point or inside another function's blocks.
//   - Skip if pointing into the function table reservation past code_size.
size_t dataSectionFunctionPointerScan(CodegenContext& ctx) {
  auto& binary = ctx.binary();
  auto& graph = ctx.graph;

  // Determine the executable code address range from the executable sections.
  // We accept addresses in any executable section; this works for binaries
  // with multiple .text-like sections.
  uint32_t code_lo = UINT32_MAX;
  uint32_t code_hi = 0;
  for (const auto& section : binary.sections()) {
    if (!section.executable)
      continue;
    code_lo = std::min(code_lo, section.baseAddress);
    code_hi = std::max(code_hi, section.baseAddress + section.size);
  }
  if (code_lo >= code_hi) {
    REXCODEGEN_DEBUG("Analyze: dataSectionFunctionPointerScan -- no executable sections");
    return 0;
  }

  std::unordered_set<uint32_t> existing;
  existing.reserve(graph.functions().size() * 2);
  for (const auto& [addr, node] : graph.functions()) {
    existing.insert(addr);
  }

  size_t found = 0;
  size_t scanned_bytes = 0;
  for (const auto& section : binary.sections()) {
    if (section.executable)
      continue;  // skip code sections; this scanner is for data only

    const uint8_t* data = section.data;
    if (!data || section.size < 4)
      continue;
    // 4-byte aligned walk; align section base if needed.
    uint32_t walk_base = (section.baseAddress + 3) & ~uint32_t{3};
    uint32_t walk_end = section.baseAddress + (section.size & ~uint32_t{3});
    if (walk_end <= walk_base)
      continue;

    for (uint32_t addr = walk_base; addr + 4 <= walk_end; addr += 4) {
      uint32_t off = addr - section.baseAddress;
      uint32_t value = load_and_swap<uint32_t>(data + off);
      scanned_bytes += 4;

      // Filters: must look like a code address.
      if (value < code_lo || value >= code_hi)
        continue;
      if (value & 0x3)
        continue;
      if (existing.count(value))
        continue;
      // Skip if it's already inside an existing function's block range.
      // (We don't try to split mid-function entries here -- that's the
      // codegen's job once they're registered as separate functions.)
      if (graph.getFunctionContaining(value))
        continue;

      // Register as DISCOVERED so the discover/gapfill passes treat it as a
      // first-class entry. Size 4 is a placeholder; the discover pass will
      // expand the function out by walking its blocks.
      graph.addFunction(value, 4, FunctionAuthority::DISCOVERED, true);
      existing.insert(value);
      found++;
    }
  }

  if (found > 0) {
    REXCODEGEN_INFO(
        "Analyze: dataSectionFunctionPointerScan -- registered {} candidate function entries from "
        "{} bytes of data sections",
        found, scanned_bytes);
  } else {
    REXCODEGEN_DEBUG(
        "Analyze: dataSectionFunctionPointerScan -- no new candidates from {} bytes",
        scanned_bytes);
  }
  return found;
}

//=============================================================================
// GapFill to register uncovered code regions
//=============================================================================

// Split a code region into function segments based on terminators (blr, tail calls).
std::vector<CodeRegion> splitRegionOnTerminators(
    const CodeRegion& region, const BinaryView& binary,
    const std::unordered_set<uint32_t>& knownCallables) {
  std::vector<CodeRegion> segments;
  uint32_t segmentStart = region.start;

  for (uint32_t addr = region.start; addr < region.end; addr += 4) {
    const uint8_t* data = binary.translate(addr);
    if (!data)
      break;

    uint32_t raw = load_and_swap<uint32_t>(data);
    auto decoded = decode_instruction(addr, raw);
    bool shouldSplit = false;
    const char* reason = nullptr;

    // Check for terminators
    if (decoded.is_return()) {
      shouldSplit = true;
      reason = "blr";
    } else if (decoded.opcode == Opcode::b && decoded.branch_target.has_value()) {
      uint32_t target = decoded.branch_target.value();
      // Don't split on tail recursion (branch to own segment start)
      if (target != segmentStart && knownCallables.contains(target)) {
        shouldSplit = true;
        reason = "tail call";
      }
    }
    // Note: we deliberately do NOT split on unconditional `bctr` here.
    // It can be either a switch-table dispatch (next address is a switch arm
    // inside the same function) or a computed tail call (next address is a
    // different function entirely). Without switch-table metadata we cannot
    // tell, and splitting in the switch case breaks intra-function branch
    // resolution. Mid-function entries reachable via `bctr`-tail-call-after-bctr
    // are picked up instead by the data-section function-pointer scan and the
    // runtime fallback handler.

    if (shouldSplit) {
      uint32_t segmentEnd = addr + 4;
      if (segmentEnd > segmentStart) {
        segments.push_back({segmentStart, segmentEnd});
        REXCODEGEN_TRACE("GapFill: split segment 0x{:08X}-0x{:08X} ({} at 0x{:08X})", segmentStart,
                         segmentEnd, reason, addr);
      }
      segmentStart = segmentEnd;
    }
  }

  // Handle remaining code after last terminator
  if (segmentStart < region.end) {
    segments.push_back({segmentStart, region.end});
  }

  return segments;
}

// Check if address looks like exception handler data (handler ptr + rdata ptr)
bool looksLikeExceptionData(const BinaryView& binary, const FunctionGraph& graph, uint32_t addr) {
  const uint8_t* data = binary.translate(addr);
  if (!data)
    return false;

  // Exception handler data pattern:
  // [addr+0]: pointer to __C_specific_handler (entry point)
  // [addr+4]: pointer to scope table in .rdata
  uint32_t firstDword = load_and_swap<uint32_t>(data);
  uint32_t secondDword = load_and_swap<uint32_t>(data + 4);

  // Check if first dword is a known entry point (like __C_specific_handler)
  if (!graph.isEntryPoint(firstDword)) {
    return false;
  }

  // Check if second dword points to .rdata section
  auto* rdataSection = binary.findSectionByName(".rdata");
  if (!rdataSection)
    return false;

  uint32_t rdataStart = rdataSection->baseAddress;
  uint32_t rdataEnd = rdataStart + rdataSection->size;

  if (secondDword >= rdataStart && secondDword < rdataEnd) {
    REXCODEGEN_TRACE(
        "GapFill: 0x{:08X} looks like exception data (handler=0x{:08X}, scope=0x{:08X}), skipping",
        addr, firstDword, secondDword);
    return true;
  }

  return false;
}

void gapFillCodeRegions(CodegenContext& ctx) {
  REXCODEGEN_INFO("Analyze: checking for uncovered code regions...");

  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& scan = ctx.scan;

  // Build set of known callables for tail call detection
  std::unordered_set<uint32_t> knownCallables;
  for (const auto& [addr, node] : graph.functions()) {
    knownCallables.insert(addr);
  }

  size_t gapsFound = 0;
  size_t segmentsCreated = 0;

  for (const auto& region : scan.codeRegions) {
    // Split region on terminators (blr, tail calls), then check each segment
    auto segments = splitRegionOnTerminators(region, binary, knownCallables);

    for (const auto& segment : segments) {
      // Skip if this segment's start is already a registered function entry
      if (graph.isEntryPoint(segment.start))
        continue;

      // Skip if this segment's start is inside another function
      if (auto* containingFunc = graph.getFunctionContaining(segment.start)) {
        continue;
      }

      // Skip if this looks like exception handler data (handler ptr + rdata ptr)
      if (looksLikeExceptionData(binary, graph, segment.start))
        continue;

      uint32_t segmentSize = segment.size();
      graph.addFunction(segment.start, segmentSize, FunctionAuthority::GAP_FILL, false);

      REXCODEGEN_TRACE("GapFill: registered sub_{:08X} (0x{:08X}-0x{:08X}, {} bytes)",
                       segment.start, segment.start, segment.end, segmentSize);
      segmentsCreated++;
    }

    gapsFound++;
  }

  if (segmentsCreated > 0) {
    REXCODEGEN_INFO("Analyze: registered {} gap functions from {} regions", segmentsCreated,
                    gapsFound);
  } else {
    REXCODEGEN_INFO("Analyze: no uncovered regions found");
  }
}

//=============================================================================
// Cleanup absorbed GAP_FILL functions
//=============================================================================

void cleanupAbsorbedGapFills(CodegenContext& ctx) {
  auto& graph = ctx.graph;
  std::vector<uint32_t> toRemove;

  for (const auto& [addr, node] : graph.functions()) {
    if (node->authority() != FunctionAuthority::GAP_FILL)
      continue;

    for (const auto& [otherAddr, otherNode] : graph.functions()) {
      if (otherAddr == addr)
        continue;
      if (!otherNode->containsAddress(addr))
        continue;

      // This GAP_FILL is inside another function's blocks
      if (otherNode->authority() != FunctionAuthority::GAP_FILL) {
        // Absorbed by higher authority - remove
        toRemove.push_back(addr);
        break;
      } else if (otherAddr < addr) {
        // Both GAP_FILL, other has lower address - it survives
        toRemove.push_back(addr);
        break;
      }
    }
  }

  for (uint32_t addr : toRemove) {
    graph.removeFunction(addr);
  }

  if (!toRemove.empty()) {
    REXCODEGEN_INFO("Analyze: removed {} absorbed GAP_FILL functions", toRemove.size());
  }
}

}  // anonymous namespace

namespace phases {

VoidResult GapFill(CodegenContext& ctx) {
  // First: scan non-executable sections for guest function pointers stored as
  // data (vtables, function-pointer tables, callback registries). These often
  // live in .rdata for EA titles that ship without MSVC RTTI -- where the
  // vtable_scanner finds nothing. Catching them here means the subsequent
  // gap-fill / discover passes treat each as a real entry point with its own
  // function-table slot, instead of getting silently merged into the next
  // PDATA function and crashing at runtime when called via `bctrl`.
  size_t dsfpFound = dataSectionFunctionPointerScan(ctx);
  if (dsfpFound > 0) {
    auto knownAfterScan = buildKnownFunctions(ctx.graph, /*excludeGapFill=*/false);
    size_t discoveredFromDsfp = discoverPendingFunctions(ctx, knownAfterScan);
    REXCODEGEN_DEBUG(
        "Analyze: discovered blocks for {} data-section-pointer candidates",
        discoveredFromDsfp);
  }

  gapFillCodeRegions(ctx);

  // Discover blocks for gap-filled functions
  auto known = buildKnownFunctions(ctx.graph, /*excludeGapFill=*/true);
  size_t discovered = discoverPendingFunctions(ctx, known);
  REXCODEGEN_INFO("Analyze: discovered blocks for {} gap-filled functions", discovered);

  cleanupAbsorbedGapFills(ctx);

  return Ok();
}

}  // namespace phases

}  // namespace rex::codegen
