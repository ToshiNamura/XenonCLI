#pragma once

#ifndef XENON__DEFINES_HPP
#define XENON__DEFINES_HPP 1

// ---------------------------------------------------------------------------
// xenon.core — preprocessor definitions shared by the whole project.
// ---------------------------------------------------------------------------

/// Version of the Xenon toolchain and runtime.
#define XENON_VERSION_MAJOR 0
#define XENON_VERSION_MINOR 1
#define XENON_VERSION_PATCH 0

/// Human-readable version string.
#define XENON_VERSION_STRING "0.1.0"

/// Standard file extensions used by the Xenon toolchain.
#define XENON_SOURCE_EXTENSION ".ixx"
#define XENON_HINT_EXTENSION ".hint"
#define XENON_MODULE_EXTENSION ".module"
#define XENON_PACKAGE_EXTENSION ".xenon"

#endif // !XENON__DEFINES_HPP
