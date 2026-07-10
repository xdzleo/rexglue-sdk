/**
 * @file        runtime/runtime.cpp
 * @brief       Runtime subsystem implementation
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/devices/null_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/perf/counter.h>
#include <rex/platform.h>
#include <rex/ppc/context.h>          // PPCFuncMapping
#include <rex/platform/exceptions.h>  // SEH exception support
#include <rex/kernel/crt/heap.h>
#include <rex/runtime.h>
#include <rex/system/export_resolver.h>
#include <rex/system/kernel_state.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/user_module.h>
#include <rex/system/xmemory.h>
#include <rex/system/xthread.h>
#include <rex/thread.h>

REXCVAR_DEFINE_STRING(game_data_root, "", "Runtime", "Override game data path");
REXCVAR_DEFINE_STRING(user_data_root, "", "Runtime", "Override user data path");
REXCVAR_DEFINE_STRING(update_data_root, "", "Runtime", "Override update data path");
REXCVAR_DEFINE_STRING(cache_path, "", "Runtime", "Override shader cache path");
REXCVAR_DEFINE_STRING(cache_root, "", "Runtime", "Override shader cache path");
REXCVAR_DEFINE_STRING(metadata_root, "", "Runtime", "Override metadata path");

namespace rex {

// Static instance for global access
Runtime* Runtime::instance_ = nullptr;

Runtime* Runtime::instance() {
  return instance_;
}

Runtime::Runtime(const std::filesystem::path& game_data_root,
                 const std::filesystem::path& user_data_root,
                 const std::filesystem::path& update_data_root,
                 const std::filesystem::path& cache_root,
                 const std::filesystem::path& metadata_root)
    : game_data_root_(game_data_root),
      user_data_root_(user_data_root.empty() ? game_data_root : user_data_root),
      update_data_root_(update_data_root),
      cache_root_(cache_root),
      metadata_root_(metadata_root) {}

Runtime::~Runtime() {
  Shutdown();
}

std::optional<std::filesystem::path> Runtime::FindMetadataPath(
    const std::filesystem::path& relative_path) const {
  if (!metadata_root_.empty()) {
    std::filesystem::path candidate = metadata_root_ / relative_path;
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec)) {
      return candidate;
    }
    return std::nullopt;
  }

  const std::filesystem::path candidates[] = {
      game_data_root_ / "metadata" / relative_path,
      game_data_root_.parent_path() / "metadata" / relative_path,
      game_data_root_ / relative_path,
  };
  for (const auto& candidate : candidates) {
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec)) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::optional<EmbeddedMetadataAsset> Runtime::FindEmbeddedMetadata(
    const std::filesystem::path& relative_path) const {
  return FindEmbeddedMetadataAsset(relative_path);
}

X_STATUS Runtime::Setup(RuntimeConfig config) {
  if (instance_ != nullptr) {
    REXSYS_ERROR("Runtime::Setup() called but global instance already exists");
    return X_STATUS_UNSUCCESSFUL;
  }
  instance_ = this;

  if (!config.config_path.empty() && std::filesystem::exists(config.config_path)) {
    REXSYS_INFO("Runtime applying config: {}", config.config_path.string());
    rex::cvar::LoadConfig(config.config_path);
  }

  auto fail = [this](X_STATUS status, std::string_view reason) {
    REXSYS_ERROR("Runtime::Setup failed: {}", reason);
    Shutdown();
    return status;
  };

  // Start profiler (Tracy network threads, counter init)
  rex::perf::Profiler::Startup();

  // Initialize SEH exception support for hardware exception handling
  rex::initialize_seh();

  // Initialize clock
  chrono::Clock::set_guest_tick_frequency(50000000);
  chrono::Clock::set_guest_system_time_base(chrono::Clock::QueryHostSystemTime());
  chrono::Clock::set_guest_time_scalar(1.0);

  // Enable threading affinity configuration
  thread::EnableAffinityConfiguration();

  tool_mode_ = config.tool_mode;

  // Create memory system first
  memory_ = std::make_unique<memory::Memory>();
  if (!memory_->Initialize()) {
    return fail(X_STATUS_UNSUCCESSFUL, "memory init failed");
  }

  export_resolver_ = std::make_unique<runtime::ExportResolver>();

  function_dispatcher_ =
      std::make_unique<runtime::FunctionDispatcher>(memory_.get(), export_resolver_.get());
  REXSYS_INFO("FunctionDispatcher initialized");

  // Create virtual file system
  file_system_ = std::make_unique<rex::filesystem::VirtualFileSystem>();

  // Create kernel state - this sets the global singleton
  kernel_state_ = std::make_unique<system::KernelState>(this);

  // Initialize input from injected config
  if (config.input_factory) {
    input_system_ = config.input_factory(tool_mode_);
    if (input_system_) {
      X_STATUS input_status = input_system_->Setup();
      if (XFAILED(input_status)) {
        REXSYS_WARN("Failed to initialize input system (status {:08X}) - input disabled",
                    input_status);
        input_system_.reset();
      } else {
        REXSYS_INFO("Input system initialized");
      }
    }
  }

  // HLE kernel modules and apps.
  if (config.kernel_init) {
    config.kernel_init(this, kernel_state_.get());
  }

  // Initialize the APU (Audio Processing Unit) from injected config
  if (config.audio_factory) {
    audio_system_ = config.audio_factory(function_dispatcher_.get());
    if (audio_system_) {
      X_STATUS audio_status = audio_system_->Setup(kernel_state_.get());
      if (XFAILED(audio_status)) {
        REXSYS_WARN("Failed to initialize audio system (status {:08X}) - audio disabled",
                    audio_status);
        audio_system_.reset();
      } else {
        REXSYS_INFO("Audio system initialized");
      }
    }
  }

  // Set up VFS: game_data_root as game:/d:, update_data_root as update:
  if (!SetupVfs()) {
    return fail(X_STATUS_UNSUCCESSFUL, "VFS setup failed");
  }

  // Skip GPU initialization in tool mode (for analysis tools like codegen)
  if (tool_mode_) {
    REXSYS_INFO("Runtime initialized in tool mode (no GPU)");
    setup_complete_ = true;
    return X_STATUS_SUCCESS;
  }

  // Initialize GPU from injected config
  if (config.graphics) {
    graphics_system_ = std::move(config.graphics);
    bool with_presentation = (app_context_ != nullptr);
    X_STATUS gpu_status = graphics_system_->Setup(function_dispatcher_.get(), kernel_state_.get(),
                                                  app_context_, with_presentation);
    if (XFAILED(gpu_status)) {
      return fail(gpu_status, "GPU setup failed");
    }
    REXSYS_INFO("GPU system initialized (presentation={})", with_presentation);
  } else {
    REXSYS_INFO("Runtime initialized without graphics system (native rendering mode)");
  }

  REXSYS_INFO("Runtime initialized successfully");
  setup_complete_ = true;
  return X_STATUS_SUCCESS;
}

X_STATUS Runtime::Setup(const rex::PPCImageInfo& image_info, RuntimeConfig config) {
  X_STATUS status = Setup(std::move(config));
  if (status != X_STATUS_SUCCESS) {
    return status;
  }

  if (!function_dispatcher_->InitializeFunctionTable(image_info.code_base, image_info.code_size,
                                                     image_info.image_base, image_info.image_size,
                                                     /*is_entrypoint=*/true,
                                                     image_info.function_table_base)) {
    REXSYS_ERROR("Failed to initialize function table");
    Shutdown();
    return X_STATUS_UNSUCCESSFUL;
  }

  if (image_info.func_mappings) {
    int count = 0;
    int duplicates = 0;
    int rejected = 0;
    for (int i = 0; image_info.func_mappings[i].guest != 0; ++i) {
      uint32_t guest = static_cast<uint32_t>(image_info.func_mappings[i].guest);
      auto* host = image_info.func_mappings[i].host;
      if (!host) {
        continue;
      }
      if (function_dispatcher_->GetFunction(guest)) {
        REXSYS_WARN("func_mappings: duplicate guest address {:08X}", guest);
        ++duplicates;
      }
      if (!function_dispatcher_->SetFunction(guest, host)) {
        ++rejected;
      } else {
        ++count;
      }
    }
    REXSYS_DEBUG("Registered {} recompiled functions ({} duplicates, {} rejected)", count,
                 duplicates, rejected);
    if (rejected > 0) {
      REXSYS_ERROR("PPCImageInfo registration: {} func_mappings entries rejected", rejected);
      Shutdown();
      return X_STATUS_UNSUCCESSFUL;
    }
  }

  REXSYS_DEBUG("Runtime setup complete (code: {:08X}-{:08X}, image: {:08X}-{:08X})",
               image_info.code_base, image_info.code_base + image_info.code_size,
               image_info.image_base, image_info.image_base + image_info.image_size);
  return X_STATUS_SUCCESS;
}

void Runtime::Shutdown() {
  if (!instance_ && !setup_complete_ && !memory_) {
    return;
  }

  if (instance_ == this) {
    instance_ = nullptr;
  }

  if (graphics_system_) {
    graphics_system_->Shutdown();
    graphics_system_.reset();
  }
  if (audio_system_) {
    audio_system_->Shutdown();
    audio_system_.reset();
  }
  if (input_system_) {
    input_system_->Shutdown();
    input_system_.reset();
  }
  kernel_state_.reset();
#if REX_PLATFORM_MAC
  // macOS tears down guest threads asynchronously. During process exit, a late
  // guest callback can still hold references into the dispatcher or mapped
  // guest memory after the owning systems have stopped. Keep those mappings
  // alive until the process exits; the OS reclaims them immediately afterward.
  function_dispatcher_.release();
#else
  function_dispatcher_.reset();
#endif
  export_resolver_.reset();
  file_system_.reset();
#if REX_PLATFORM_MAC
  memory_.release();
#else
  memory_.reset();
#endif

  rex::perf::Profiler::Shutdown();
  setup_complete_ = false;
}

uint8_t* Runtime::virtual_membase() const {
  return memory_ ? memory_->virtual_membase() : nullptr;
}

bool Runtime::SetupVfs() {
  if (game_data_root_.empty()) {
    REXSYS_WARN("Runtime::SetupVfs: No game_data_root specified, skipping VFS setup");
    return true;
  }

  auto abs_game_root = std::filesystem::absolute(game_data_root_);
  if (!std::filesystem::exists(abs_game_root)) {
    REXSYS_ERROR("Runtime::SetupVfs: game_data_root does not exist: {}", abs_game_root.string());
    return false;
  }

  // Mount game_data_root as \Device\Harddisk0\Partition1
  auto mount_path = "\\Device\\Harddisk0\\Partition1";
  auto device = std::make_unique<rex::filesystem::HostPathDevice>(
      mount_path, abs_game_root, !REXCVAR_GET(allow_game_relative_writes));
  if (!device->Initialize()) {
    REXSYS_ERROR("Runtime::SetupVfs: Failed to initialize host path device");
    return false;
  }
  if (!file_system_->RegisterDevice(std::move(device))) {
    REXSYS_ERROR("Runtime::SetupVfs: Failed to register host path device");
    return false;
  }
  REXSYS_INFO("  Mounted {} at {}", abs_game_root.string(), mount_path);

  // Register symbolic links for game: and D:
  file_system_->RegisterSymbolicLink("game:", mount_path);
  file_system_->RegisterSymbolicLink("d:", mount_path);
  REXSYS_DEBUG("  Registered symbolic links: game:, d:");

  // Mount update:\ -- the Title Update partition. If update_data_root points
  // at a staged TU, mount it; OTHERWISE mount an EMPTY dir so the device still
  // EXISTS. The distinction matters: a title probing "update:\update.img" with
  // no device mounted gets X_STATUS_NO_SUCH_DEVICE ("device not found"), an
  // error some engines don't handle -- GTA V (RAGE) faults its init sequence
  // on it and hangs the loading screen (a game thread blocks; the render
  // thread idles re-submitting viz queries forever). With the device present
  // the same probe returns X_STATUS_NO_SUCH_FILE ("file not found"), the
  // clean "no update installed -> use base version" path. Read-only; empty dir
  // created under cache_root. Harmless fleet-wide (an empty update: is what a
  // console with no TU also presents).
  {
    std::filesystem::path update_host;
    bool have_staged = false;
    if (!update_data_root_.empty()) {
      auto abs_update_root = std::filesystem::absolute(update_data_root_);
      if (std::filesystem::exists(abs_update_root)) {
        update_host = abs_update_root;
        have_staged = true;
      }
    }
    if (!have_staged) {
      update_host = !cache_root_.empty() ? (cache_root_ / "guest_update")
                                         : (abs_game_root.parent_path() / "update");
      std::error_code ec;
      std::filesystem::create_directories(update_host, ec);  // read-only mount needs it to exist
    }
    auto update_mount = "\\Device\\Harddisk0\\PartitionUpdate";
    auto update_device =
        std::make_unique<rex::filesystem::HostPathDevice>(update_mount, update_host, true);
    if (update_device->Initialize() && file_system_->RegisterDevice(std::move(update_device))) {
      file_system_->RegisterSymbolicLink("update:", update_mount);
      REXSYS_INFO("  Mounted {} at update: ({})", update_host.string(),
                  have_staged ? "staged TU" : "empty -- no TU installed");
    }
  }

  // Setup NullDevice for raw HDD partition accesses
  // Cache/STFC code baked into games tries reading/writing to these
  // Using a NullDevice returns success to all IO requests, allowing games
  // to believe cache/raw disk was accessed successfully.
  // NOTE: Must be registered AFTER Partition1 so Partition1 requests don't
  // go to NullDevice (VFS resolves devices in registration order)
  auto null_paths = {std::string("\\Partition0"), std::string("\\Cache0"), std::string("\\Cache1")};
  auto null_device =
      std::make_unique<rex::filesystem::NullDevice>("\\Device\\Harddisk0", null_paths);
  if (null_device->Initialize()) {
    file_system_->RegisterDevice(std::move(null_device));
    REXSYS_DEBUG("  Registered NullDevice for \\Device\\Harddisk0\\{{Partition0,Cache0,Cache1}}");
  }

  // Mount a WRITABLE cache:\ device. Yukes-engine titles (e.g. WWE SmackDown vs
  // Raw, title 545107E0) decompress their PAC asset packs from the disc into the
  // Xbox 360 CACHE: scratch partition and then read them back; with no cache
  // device every CACHE:\PAC\... open returns 0xC000000F and the title stalls
  // loading content (menus/models). A writable HostPathDevice creates the dir
  // and lets the stage+read round-trip succeed, matching the newer community SDK.
  {
    std::filesystem::path cache_host = !cache_root_.empty()
                                           ? (cache_root_ / "guest_cache")
                                           : (abs_game_root.parent_path() / "cache");
    auto cache_mount = "\\Device\\Harddisk0\\PartitionCache";
    auto cache_device = std::make_unique<rex::filesystem::HostPathDevice>(
        cache_mount, cache_host, /*read_only=*/false);
    if (cache_device->Initialize() && file_system_->RegisterDevice(std::move(cache_device))) {
      file_system_->RegisterSymbolicLink("cache:", cache_mount);
      REXSYS_INFO("  Mounted writable {} at cache:", cache_host.string());
    } else {
      REXSYS_WARN("  cache: mount failed ({}); title cache reads will 0xC000000F", cache_host.string());
    }
    // cache0: / cache1: -- the 360's two utility/scratch partitions, distinct
    // from cache: above. Halo 3 streams its maps through cache0:/cache1: and
    // shows XamShowDirtyDiscErrorUI after ~200 failed opens ("device not
    // found") when only cache: exists. Same writable HostPathDevice recipe,
    // separate host subdirs so titles that use both see two devices.
    for (int cache_i = 0; cache_i < 2; ++cache_i) {
      std::filesystem::path ci_host = !cache_root_.empty()
                                          ? (cache_root_ / (cache_i ? "guest_cache1" : "guest_cache0"))
                                          : (abs_game_root.parent_path() / (cache_i ? "cache1" : "cache0"));
      std::string ci_mount =
          cache_i ? "\Device\Harddisk0\PartitionCache1" : "\Device\Harddisk0\PartitionCache0";
      std::string ci_link = cache_i ? "cache1:" : "cache0:";
      auto ci_device = std::make_unique<rex::filesystem::HostPathDevice>(
          ci_mount, ci_host, /*read_only=*/false);
      if (ci_device->Initialize() && file_system_->RegisterDevice(std::move(ci_device))) {
        file_system_->RegisterSymbolicLink(ci_link, ci_mount);
        REXSYS_INFO("  Mounted writable {} at {}", ci_host.string(), ci_link);
      } else {
        REXSYS_WARN("  {} mount failed ({})", ci_link, ci_host.string());
      }
    }

    // RAGE-engine scratch mounts (GTA V, Max Payne 3, Red Dead, Midnight Club):
    // the title creates a writable "gamecache:" partition for its processed
    // asset cache (gta5_cache_x.dat) and "commoncrc:" for CRC state, and mounts
    // them by drive-style symlink -- NOT under \Device\Harddisk0, so the
    // longest-match resolver never reaches them. Without a device every open
    // returns [no device] and GTA V loops re-generating a cache it can never
    // write, never finishing the loading screen. Same writable-HostPathDevice
    // recipe; harmless for non-RAGE titles (they never touch these links).
    for (const char* tag : {"gamecache", "commoncrc"}) {
      std::filesystem::path rg_host = !cache_root_.empty()
                                          ? (cache_root_ / (std::string("guest_") + tag))
                                          : (abs_game_root.parent_path() / tag);
      std::string rg_mount = std::string("\\Device\\") + tag;
      std::string rg_link = std::string(tag) + ":";
      auto rg_device = std::make_unique<rex::filesystem::HostPathDevice>(
          rg_mount, rg_host, /*read_only=*/false);
      if (rg_device->Initialize() && file_system_->RegisterDevice(std::move(rg_device))) {
        file_system_->RegisterSymbolicLink(rg_link, rg_mount);
        REXSYS_INFO("  Mounted writable {} at {}", rg_host.string(), rg_link);
      } else {
        REXSYS_WARN("  {} mount failed ({})", rg_link, rg_host.string());
      }
    }
  }

  return true;
}

X_STATUS Runtime::LoadXexImage(const std::string_view module_path) {
  REXSYS_INFO("Loading XEX image: {}", std::string(module_path));

  auto module = system::object_ref<system::UserModule>(new system::UserModule(kernel_state_.get()));
  X_STATUS status = module->LoadFromFile(module_path);
  if (XFAILED(status)) {
    REXSYS_ERROR("Runtime::LoadXexImage: Failed to load module, status {:08X}", status);
    return status;
  }

  kernel_state_->SetExecutableModule(module);
  REXSYS_DEBUG("  XEX image loaded successfully");
  return X_STATUS_SUCCESS;
}

system::object_ref<system::XThread> Runtime::PrepareModuleLaunch() {
  auto executable = kernel_state_->GetExecutableModule();
  if (!executable) {
    REXSYS_ERROR("Runtime::PrepareModuleLaunch: No executable module loaded");
    return nullptr;
  }

  auto thread = kernel_state_->PrepareModuleLaunch(executable);
  if (!thread) {
    REXSYS_ERROR("Runtime::PrepareModuleLaunch: Failed to prepare module");
    return nullptr;
  }

  REXSYS_DEBUG("  Module prepared on thread '{}'", thread->name());
  return thread;
}

system::object_ref<system::XThread> Runtime::LaunchModule() {
  auto thread = PrepareModuleLaunch();
  if (thread) {
    thread->Resume();
    REXSYS_DEBUG("  Module launched on thread '{}'", thread->name());
  }
  return thread;
}

}  // namespace rex
