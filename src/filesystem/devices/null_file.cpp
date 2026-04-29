/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstring>

#include <rex/filesystem/devices/null_entry.h>
#include <rex/filesystem/devices/null_file.h>

namespace rex::filesystem {

NullFile::NullFile(uint32_t file_access, NullEntry* entry) : File(file_access, entry) {}

NullFile::~NullFile() = default;

void NullFile::Destroy() {
  delete this;
}

X_STATUS NullFile::ReadSync(std::span<uint8_t> buffer, size_t byte_offset, size_t* out_bytes_read) {
  if (!(file_access_ & FileAccess::kFileReadData)) {
    return X_STATUS_ACCESS_DENIED;
  }

  // Skate 3 (and other EA RenderWare titles) read raw partition headers from
  // \Device\Harddisk0\Partition0 to validate disc / cache integrity. The
  // previous implementation silently returned SUCCESS but left
  // out_bytes_read at 0 and the buffer untouched -- the game saw a bogus
  // read of 0 bytes and looped forever retrying. Zero-fill the buffer and
  // claim the full requested length was satisfied; the game treats that as
  // "valid empty partition" and continues past the cache-validation phase.
  if (!buffer.empty()) {
    std::memset(buffer.data(), 0, buffer.size());
  }
  if (out_bytes_read) {
    *out_bytes_read = buffer.size();
  }
  return X_STATUS_SUCCESS;
}

X_STATUS NullFile::WriteSync(std::span<const uint8_t> buffer, size_t byte_offset,
                             size_t* out_bytes_written) {
  if (!(file_access_ & (FileAccess::kFileWriteData | FileAccess::kFileAppendData))) {
    return X_STATUS_ACCESS_DENIED;
  }
  // Pretend we wrote everything successfully -- it's /dev/null after all.
  if (out_bytes_written) {
    *out_bytes_written = buffer.size();
  }
  return X_STATUS_SUCCESS;
}

X_STATUS NullFile::SetLength(size_t length) {
  if (!(file_access_ & FileAccess::kFileWriteData)) {
    return X_STATUS_ACCESS_DENIED;
  }

  return X_STATUS_SUCCESS;
}

}  // namespace rex::filesystem
