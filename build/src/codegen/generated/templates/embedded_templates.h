// Auto-generated master template index -- DO NOT EDIT
#pragma once

#include <string_view>
#include <unordered_map>
#include <string>

#include "codegen_init_cpp.inja.h"
#include "codegen_init_h.inja.h"
#include "codegen_sources_cmake.inja.h"
#include "init_app_header.inja.h"
#include "init_cmake_presets.inja.h"
#include "init_cmakelists.inja.h"
#include "init_config_toml.inja.h"
#include "init_main_cpp.inja.h"
#include "init_rexglue_cmake.inja.h"
#include "test_ppc_config_h.inja.h"
#include "test_ppc_test_cases_cpp.inja.h"
#include "test_ppc_test_decls_h.inja.h"
#include "test_ppc_test_functions_cpp.inja.h"

namespace rex::codegen {

inline const std::unordered_map<std::string, std::string_view>& embeddedTemplates() {
  static const std::unordered_map<std::string, std::string_view> map = {
    {"codegen/init_cpp", embedded::codegen_init_cpp},
    {"codegen/init_h", embedded::codegen_init_h},
    {"codegen/sources_cmake", embedded::codegen_sources_cmake},
    {"init/app_header", embedded::init_app_header},
    {"init/cmake_presets", embedded::init_cmake_presets},
    {"init/cmakelists", embedded::init_cmakelists},
    {"init/config_toml", embedded::init_config_toml},
    {"init/main_cpp", embedded::init_main_cpp},
    {"init/rexglue_cmake", embedded::init_rexglue_cmake},
    {"test/ppc_config_h", embedded::test_ppc_config_h},
    {"test/ppc_test_cases_cpp", embedded::test_ppc_test_cases_cpp},
    {"test/ppc_test_decls_h", embedded::test_ppc_test_decls_h},
    {"test/ppc_test_functions_cpp", embedded::test_ppc_test_functions_cpp},
  };
  return map;
}

}  // namespace rex::codegen
