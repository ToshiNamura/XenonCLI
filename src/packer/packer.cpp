#include "xenon/packer/packer.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "xenon/module/module_file.hpp"
#include "xenon/packer/manifest.hpp"

namespace xenon
{
namespace
{

constexpr std::uint32_t LocalHeaderSignature = 0x04034b50;
constexpr std::uint32_t CentralSignature = 0x02014b50;
constexpr std::uint32_t EndSignature = 0x06054b50;
constexpr std::uint16_t StoreMethod = 0;

std::uint32_t crc32(const std::uint8_t* data, std::size_t size)
{
    static const std::array<std::uint32_t, 256> table = []
    {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1u) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            }
            result[i] = c;
        }
        return result;
    }();

    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i)
    {
        crc = table[(crc ^ data[i]) & 0xffu] ^ (crc >> 8);
    }
    return crc ^ 0xffffffffu;
}

void writeU16(std::ostream& os, std::uint16_t value)
{
    os.put(static_cast<char>(value & 0xff));
    os.put(static_cast<char>((value >> 8) & 0xff));
}

void writeU32(std::ostream& os, std::uint32_t value)
{
    os.put(static_cast<char>(value & 0xff));
    os.put(static_cast<char>((value >> 8) & 0xff));
    os.put(static_cast<char>((value >> 16) & 0xff));
    os.put(static_cast<char>((value >> 24) & 0xff));
}

std::uint16_t readU16(std::istream& is)
{
    const int a = is.get();
    const int b = is.get();
    if (a < 0 || b < 0)
    {
        throw PackError("truncated archive");
    }
    return static_cast<std::uint16_t>(a | (b << 8));
}

std::uint32_t readU32(std::istream& is)
{
    const int a = is.get();
    const int b = is.get();
    const int c = is.get();
    const int d = is.get();
    if (a < 0 || b < 0 || c < 0 || d < 0)
    {
        throw PackError("truncated archive");
    }
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8) |
           (static_cast<std::uint32_t>(c) << 16) | (static_cast<std::uint32_t>(d) << 24);
}

std::string readBytes(std::istream& is, std::size_t size)
{
    std::string value(size, '\0');
    if (size != 0)
    {
        is.read(value.data(), static_cast<std::streamsize>(size));
        if (is.gcount() != static_cast<std::streamsize>(size))
        {
            throw PackError("truncated archive");
        }
    }
    return value;
}

struct EntryData
{
    std::string Name;
    std::vector<std::uint8_t> Data;
};

struct CentralEntry
{
    std::string Name;
    std::uint32_t Crc = 0;
    std::uint32_t Offset = 0;
    std::uint32_t Size = 0;
};

std::string baseName(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string readFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw PackError("cannot open file for packing: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void writeFile(const std::string& path, const std::string& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        throw PackError("cannot write file: " + path);
    }
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string buildDefaultManifest(const std::vector<std::string>& entries)
{
    for (const auto& path : entries)
    {
        if (path.size() < std::string(".module").size() ||
            path.substr(path.size() - std::string(".module").size()) != ".module")
        {
            continue;
        }

        try
        {
            Module module;
            readModuleFile(path, module);
            if (module.find("main") != nullptr)
            {
                return Manifest::fromModule(module).toString();
            }
        }
        catch (const ModuleFileError&)
        {
            // Skip non-module files packed alongside the modules.
        }
    }

    return "Manifest-Version: 1.0\n";
}

} // namespace

void Packer::pack(const std::vector<std::string>& entries, const std::string& archivePath,
                  const std::string& manifestPath)
{
    std::vector<EntryData> packed;

    for (const auto& entry : entries)
    {
        EntryData item;
        item.Name = baseName(entry);
        const std::string text = readFile(entry);
        item.Data.assign(text.begin(), text.end());
        packed.push_back(std::move(item));
    }

    const std::string manifestText =
        manifestPath.empty() ? buildDefaultManifest(entries) : readFile(manifestPath);

    EntryData manifestEntry;
    manifestEntry.Name = "META-INF/MANIFEST.MF";
    manifestEntry.Data.assign(manifestText.begin(), manifestText.end());
    packed.push_back(std::move(manifestEntry));

    std::ofstream output(archivePath, std::ios::binary);
    if (!output)
    {
        throw PackError("cannot create archive: " + archivePath);
    }

    std::vector<CentralEntry> central;
    std::uint32_t offset = 0;

    for (const auto& item : packed)
    {
        const std::uint32_t crc = crc32(item.Data.data(), item.Data.size());
        const std::uint32_t size = static_cast<std::uint32_t>(item.Data.size());

        writeU32(output, LocalHeaderSignature);
        writeU16(output, 20); // version needed
        writeU16(output, 0);  // flags
        writeU16(output, StoreMethod);
        writeU16(output, 0); // mod time
        writeU16(output, 0); // mod date
        writeU32(output, crc);
        writeU32(output, size); // compressed size
        writeU32(output, size); // uncompressed size
        writeU16(output, static_cast<std::uint16_t>(item.Name.size()));
        writeU16(output, 0); // extra length
        output.write(item.Name.data(), static_cast<std::streamsize>(item.Name.size()));
        if (!item.Data.empty())
        {
            output.write(reinterpret_cast<const char*>(item.Data.data()),
                         static_cast<std::streamsize>(item.Data.size()));
        }

        central.push_back(CentralEntry{item.Name, crc, offset, size});
        offset += 30 + static_cast<std::uint32_t>(item.Name.size()) + size;
    }

    const std::uint32_t centralOffset = offset;

    for (const auto& item : central)
    {
        writeU32(output, CentralSignature);
        writeU16(output, 20); // version made by
        writeU16(output, 20); // version needed
        writeU16(output, 0);  // flags
        writeU16(output, StoreMethod);
        writeU16(output, 0); // time
        writeU16(output, 0); // date
        writeU32(output, item.Crc);
        writeU32(output, item.Size);
        writeU32(output, item.Size);
        writeU16(output, static_cast<std::uint16_t>(item.Name.size()));
        writeU16(output, 0); // extra length
        writeU16(output, 0); // comment length
        writeU16(output, 0); // disk number start
        writeU16(output, 0); // internal attributes
        writeU32(output, 0); // external attributes
        writeU32(output, item.Offset);
        output.write(item.Name.data(), static_cast<std::streamsize>(item.Name.size()));
    }

    const std::uint32_t centralSize = static_cast<std::uint32_t>(output.tellp()) - centralOffset;

    writeU32(output, EndSignature);
    writeU16(output, 0); // disk number
    writeU16(output, 0); // central directory disk
    writeU16(output, static_cast<std::uint16_t>(central.size()));
    writeU16(output, static_cast<std::uint16_t>(central.size()));
    writeU32(output, centralSize);
    writeU32(output, centralOffset);
    writeU16(output, 0); // comment length
}

void Packer::unpack(const std::string& archivePath, const std::string& outputDir)
{
    const auto entries = readArchive(archivePath);

    std::filesystem::create_directories(outputDir);
    for (const auto& entry : entries)
    {
        const std::filesystem::path target = std::filesystem::path(outputDir) / entry.Name;
        std::filesystem::create_directories(target.parent_path());
        writeFile(target.string(), std::string(entry.Data.begin(), entry.Data.end()));
    }
}

std::vector<PackedEntry> Packer::readArchive(const std::string& archivePath)
{
    std::ifstream input(archivePath, std::ios::binary);
    if (!input)
    {
        throw PackError("cannot open archive: " + archivePath);
    }

    input.seekg(0, std::ios::end);
    const std::streamoff fileSize = input.tellg();
    if (fileSize < 22)
    {
        throw PackError("invalid archive (too small)");
    }

    std::streamoff endOffset = -1;
    for (std::streamoff back = 0; back < 65536 && back <= fileSize - 22; ++back)
    {
        input.seekg(fileSize - 22 - back, std::ios::beg);
        if (readU32(input) == EndSignature)
        {
            endOffset = fileSize - 22 - back;
            break;
        }
    }
    if (endOffset < 0)
    {
        throw PackError("invalid archive (no end record)");
    }

    input.seekg(endOffset + 4, std::ios::beg);
    (void)readU16(input); // disk number
    (void)readU16(input); // central directory disk
    (void)readU16(input); // entries on this disk
    const std::uint16_t totalEntries = readU16(input);
    (void)readU32(input); // central directory size
    const std::uint32_t centralOffset = readU32(input);

    std::vector<PackedEntry> result;
    result.reserve(totalEntries);

    input.seekg(centralOffset, std::ios::beg);
    for (std::uint16_t i = 0; i < totalEntries; ++i)
    {
        if (readU32(input) != CentralSignature)
        {
            throw PackError("invalid central directory entry");
        }
        (void)readU16(input); // version made by
        (void)readU16(input); // version needed
        (void)readU16(input); // flags
        (void)readU16(input); // method
        (void)readU16(input); // time
        (void)readU16(input); // date
        (void)readU32(input); // crc
        (void)readU32(input); // compressed size
        (void)readU32(input); // uncompressed size
        const std::uint16_t nameLength = readU16(input);
        const std::uint16_t extraLength = readU16(input);
        const std::uint16_t commentLength = readU16(input);
        (void)readU16(input); // disk number start
        (void)readU16(input); // internal attributes
        (void)readU32(input); // external attributes
        const std::uint32_t localOffset = readU32(input);
        const std::string name = readBytes(input, nameLength);
        input.seekg(extraLength, std::ios::cur);
        input.seekg(commentLength, std::ios::cur);

        const std::streampos saved = input.tellg();

        input.seekg(localOffset, std::ios::beg);
        if (readU32(input) != LocalHeaderSignature)
        {
            throw PackError("invalid local header");
        }
        (void)readU16(input); // version
        (void)readU16(input); // flags
        const std::uint16_t method = readU16(input);
        (void)readU16(input); // time
        (void)readU16(input); // date
        (void)readU32(input); // crc
        const std::uint32_t compressedSize = readU32(input);
        (void)readU32(input); // uncompressed size
        const std::uint16_t localNameLength = readU16(input);
        const std::uint16_t localExtraLength = readU16(input);

        if (method != StoreMethod)
        {
            throw PackError("unsupported compression method in archive");
        }

        const std::streamoff dataOffset = localOffset + 30 + localNameLength + localExtraLength;

        PackedEntry entry;
        entry.Name = name;
        input.seekg(dataOffset, std::ios::beg);
        const std::string raw = readBytes(input, compressedSize);
        entry.Data.assign(raw.begin(), raw.end());
        result.push_back(std::move(entry));

        input.seekg(saved, std::ios::beg);
    }

    return result;
}

std::string Packer::readManifest(const std::string& archivePath)
{
    const auto entries = readArchive(archivePath);
    for (const auto& entry : entries)
    {
        if (entry.Name == "META-INF/MANIFEST.MF" || entry.Name == "MANIFEST.MF")
        {
            return std::string(entry.Data.begin(), entry.Data.end());
        }
    }
    throw PackError("no manifest in archive");
}

} // namespace xenon
