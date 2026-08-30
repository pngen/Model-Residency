#pragma once

// ---------------------------------------------------------------------------
// Feature detection and platform configuration for Model Residency.
// ---------------------------------------------------------------------------
// This header is intended to be included first by most translation units. It
// centralizes macro definitions that control optional subsystems (CUDA),
// symbol visibility, and platform quirks.
// ---------------------------------------------------------------------------

#ifndef MR_CONFIG_HPP
#define MR_CONFIG_HPP

#include <cstdint>

// Optional subsystem feature macros. These are set by the build system via
// target_compile_definitions when the corresponding subsystem is available.
#if defined(MR_ENABLE_CUDA)
#  define MR_HAVE_CUDA 1
#else
#  define MR_HAVE_CUDA 0
#endif

// Symbol visibility / export macro.
#if defined(_WIN32)
#  if defined(MR_BUILD_SHARED)
#    if defined(MR_EXPORTS)
#      define MR_API __declspec(dllexport)
#    else
#      define MR_API __declspec(dllimport)
#    endif
#  else
#    define MR_API
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define MR_API __attribute__((visibility("default")))
#else
#  define MR_API
#endif

// MSVC deprecation / secure CRT noise suppression. Model Residency never uses
// unsafe CRT entry points; this simply avoids vendor header noise that would
// otherwise raise /W4 /WX errors in downstream consumers.
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#  define _CRT_SECURE_NO_WARNINGS
#endif

#if !defined(MR_NAMESPACE)
#  define MR_NAMESPACE mr
#endif

#endif // MR_CONFIG_HPP
