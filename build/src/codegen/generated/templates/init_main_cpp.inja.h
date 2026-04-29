// Auto-generated from resources/templates/init/main_cpp.inja -- DO NOT EDIT
#pragma once
#include <string_view>

namespace rex::codegen::embedded {
inline constexpr std::string_view init_main_cpp = R"__TMPL__(
// {{ names.snake_case }} - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.

#include "generated/{{ names.snake_case }}_init.h"

#include "{{ names.snake_case }}_app.h"

REX_DEFINE_APP({{ names.snake_case }}, {{ names.pascal_case }}App::Create)
)__TMPL__";
}  // namespace rex::codegen::embedded
