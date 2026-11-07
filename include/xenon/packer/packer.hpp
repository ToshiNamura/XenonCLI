#pragma once

#ifndef XENON__PACKER_HPP
#define XENON__PACKER_HPP 1

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace xenon
{

/// Raised when a `.xenon` archive cannot be read or has an invalid format.
class PackError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

/// A single entry inside a `.xenon` archive.
struct PackedEntry
{
    std::string Name;                ///< Stored file name (e.g. `META-INF/MANIFEST.MF`).
    std::vector<std::uint8_t> Data;  ///< Raw file bytes.
};

/// Packs `.module`/`.hint` files into a ZIP-based `.xenon` package (JAR-like),
/// creating and configuring the manifest.
///
/// The archive uses the ZIP container format with uncompressed (STORE) entries,
/// so it can be inspected with any standard ZIP tool.
class Packer
{
  public:
    /// Packs `entries` (file paths) into `archivePath`. When `manifestPath` is
    /// empty a manifest is generated automatically from the packed modules.
    void pack(const std::vector<std::string>& entries,
              const std::string& archivePath,
              const std::string& manifestPath = {});

    /// Extracts the archive into `outputDir`.
    void unpack(const std::string& archivePath, const std::string& outputDir);

    /// Reads every entry of the archive into memory.
    static std::vector<PackedEntry> readArchive(const std::string& archivePath);

    /// Reads the manifest (`META-INF/MANIFEST.MF`) from the archive.
    static std::string readManifest(const std::string& archivePath);
};

} // namespace xenon

#endif // !XENON__PACKER_HPP
