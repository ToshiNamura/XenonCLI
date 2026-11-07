#pragma once

#ifndef XENON__MANIFEST_HPP
#define XENON__MANIFEST_HPP 1

#include <string>
#include <string_view>

#include "xenon/module/module.hpp"

namespace xenon
{

/// The manifest of a `.xenon` package (JAR-like `META-INF/MANIFEST.MF`).
///
/// It designates the entry module and the entry function/method used by
/// `xenon run <package>`.
struct Manifest
{
    std::string MainModule;   ///< Entry module.
    std::string MainFunction; ///< Entry function/method inside the module.

    [[nodiscard]] bool hasEntryPoint() const noexcept
    {
        return !MainModule.empty() && !MainFunction.empty();
    }

    /// Serializes the manifest to its textual representation.
    [[nodiscard]] std::string toString() const;

    /// Parses a manifest from `text`.
    static Manifest parse(std::string_view text);

    /// Builds a manifest for `module`, using `main` as the entry function when
    /// the module exports one.
    static Manifest fromModule(const Module& module);
};

} // namespace xenon

#endif // !XENON__MANIFEST_HPP
