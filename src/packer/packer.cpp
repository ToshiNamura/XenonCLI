#include <xenon/packer/packer.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <xenon/module/module_file.hpp>
#include <xenon/packer/manifest.hpp>

namespace xenon
{
	constexpr std::uint32_t LocalHeaderSignature = 0x04034b50;
	constexpr std::uint32_t CentralSignature     = 0x02014b50;
	constexpr std::uint32_t EndSignature         = 0x06054b50;
	constexpr std::uint16_t StoreMethod          = 0;

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

	static void writeU32(std::ostream& os, std::uint32_t value)
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

	static std::uint32_t readU32(std::istream& is)
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
		std::string               Name;
		std::vector<std::uint8_t> Data;
	};

	struct CentralEntry
	{
		std::string   Name;
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
			if (path.size() < std::string(".module").size() || path.substr(path.size() - std::string(".module").size()) != ".module")
			{
				continue;
			}

			try
			{
				xenon::Module module;
				readModuleFile(path, module);
				if (module.findFunction("main") != nullptr)
				{
					return xenon::Manifest::fromModule(module).toString();
				}
			}
			catch (const xenon::ModuleFileError&)
			{
				// Skip non-module files packed alongside the modules.
			}
		}

		return "Manifest-Version: 1.0\n";
	}
}

void xenon::Packer::pack(const std::vector<std::string>& entries, const std::string& archivePath, const std::string& manifestPath)
{
	std::vector<xenon::EntryData> packed;

	for (const auto& entry : entries)
	{
		xenon::EntryData item;
		item.Name = xenon::baseName(entry);
		const std::string text = xenon::readFile(entry);
		item.Data.assign(text.begin(), text.end());
		packed.push_back(std::move(item));
	}

	const std::string manifestText = manifestPath.empty() ? xenon::buildDefaultManifest(entries) : xenon::readFile(manifestPath);

	xenon::EntryData manifestEntry;
	manifestEntry.Name = "META-INF/MANIFEST.MF";
	manifestEntry.Data.assign(manifestText.begin(), manifestText.end());
	packed.push_back(std::move(manifestEntry));

	std::ofstream output(archivePath, std::ios::binary);
	if (!output)
	{
		throw xenon::PackError("cannot create archive: " + archivePath);
	}

	std::vector<xenon::CentralEntry> central;
	std::uint32_t offset = 0;

	for (const auto& item : packed)
	{
		const std::uint32_t crc  = xenon::crc32(item.Data.data(), item.Data.size());
		const std::uint32_t size = static_cast<std::uint32_t>(item.Data.size());

		xenon::writeU32(output, xenon::LocalHeaderSignature);
		xenon::writeU16(output, 20);                                                    // version needed
		xenon::writeU16(output, 0);                                                     // flags
		xenon::writeU16(output, xenon::StoreMethod);
		xenon::writeU16(output, 0);                                                     // mod time
		xenon::writeU16(output, 0);                                                     // mod date
		xenon::writeU32(output, crc);
		xenon::writeU32(output, size);                                                  // compressed size
		xenon::writeU32(output, size);                                                  // uncompressed size
		xenon::writeU16(output, static_cast<std::uint16_t>(item.Name.size()));
		xenon::writeU16(output, 0);                                                     // extra length
		output.write(item.Name.data(), static_cast<std::streamsize>(item.Name.size()));
		if (!item.Data.empty())
		{
			output.write(reinterpret_cast<const char*>(item.Data.data()), static_cast<std::streamsize>(item.Data.size()));
		}

		central.push_back(xenon::CentralEntry{item.Name, crc, offset, size});
		offset += 30 + static_cast<std::uint32_t>(item.Name.size()) + size;
	}

	const std::uint32_t centralOffset = offset;

	for (const auto& item : central)
	{
		xenon::writeU32(output, xenon::CentralSignature);
		xenon::writeU16(output, 20);                                                    // version made by
		xenon::writeU16(output, 20);                                                    // version needed
		xenon::writeU16(output, 0);                                                     // flags
		xenon::writeU16(output, xenon::StoreMethod);
		xenon::writeU16(output, 0);                                                     // time
		xenon::writeU16(output, 0);                                                     // date
		xenon::writeU32(output, item.Crc);
		xenon::writeU32(output, item.Size);
		xenon::writeU32(output, item.Size);
		xenon::writeU16(output, static_cast<std::uint16_t>(item.Name.size()));
		xenon::writeU16(output, 0);                                                     // extra length
		xenon::writeU16(output, 0);                                                     // comment length
		xenon::writeU16(output, 0);                                                     // disk number start
		xenon::writeU16(output, 0);                                                     // internal attributes
		xenon::writeU32(output, 0);                                                     // external attributes
		xenon::writeU32(output, item.Offset);
		output.write(item.Name.data(), static_cast<std::streamsize>(item.Name.size()));
	}

	const std::uint32_t centralSize = static_cast<std::uint32_t>(output.tellp()) - centralOffset;

	xenon::writeU32(output, xenon::EndSignature);
	xenon::writeU16(output, 0);                                          // disk number
	xenon::writeU16(output, 0);                                          // central directory disk
	xenon::writeU16(output, static_cast<std::uint16_t>(central.size()));
	xenon::writeU16(output, static_cast<std::uint16_t>(central.size()));
	xenon::writeU32(output, centralSize);
	xenon::writeU32(output, centralOffset);
	xenon::writeU16(output, 0);                                          // comment length
}

void xenon::Packer::unpack(const std::string& archivePath, const std::string& outputDir)
{
	const auto entries = readArchive(archivePath);

	std::filesystem::create_directories(outputDir);
	for (const auto& entry : entries)
	{
		const std::filesystem::path target = std::filesystem::path(outputDir) / entry.Name;
		std::filesystem::create_directories(target.parent_path());
		xenon::writeFile(target.string(), std::string(entry.Data.begin(), entry.Data.end()));
	}
}

std::vector<xenon::PackedEntry> xenon::Packer::readArchive(const std::string& archivePath)
{
	std::ifstream input(archivePath, std::ios::binary);
	if (!input)
	{
		throw xenon::PackError("cannot open archive: " + archivePath);
	}

	input.seekg(0, std::ios::end);
	const std::streamoff fileSize = input.tellg();
	if (fileSize < 22)
	{
		throw xenon::PackError("invalid archive (too small)");
	}

	std::streamoff endOffset = -1;
	for (std::streamoff back = 0; back < 65536 && back <= fileSize - 22; ++back)
	{
		input.seekg(fileSize - 22 - back, std::ios::beg);
		if (xenon::readU32(input) == xenon::EndSignature)
		{
			endOffset = fileSize - 22 - back;
			break;
		}
	}
	if (endOffset < 0)
	{
		throw xenon::PackError("invalid archive (no end record)");
	}

	input.seekg(endOffset + 4, std::ios::beg);
	(void)xenon::readU16(input);                               // disk number
	(void)xenon::readU16(input);                               // central directory disk
	(void)xenon::readU16(input);                               // entries on this disk
	const std::uint16_t totalEntries = xenon::readU16(input);
	(void)xenon::readU32(input);                               // central directory size
	const std::uint32_t centralOffset = xenon::readU32(input);

	std::vector<PackedEntry> result;
	result.reserve(totalEntries);

	input.seekg(centralOffset, std::ios::beg);
	for (std::uint16_t i = 0; i < totalEntries; ++i)
	{
		if (xenon::readU32(input) != xenon::CentralSignature)
		{
			throw xenon::PackError("invalid central directory entry");
		}
		(void)xenon::readU16(input);                                  // version made by
		(void)xenon::readU16(input);                                  // version needed
		(void)xenon::readU16(input);                                  // flags
		(void)xenon::readU16(input);                                  // method
		(void)xenon::readU16(input);                                  // time
		(void)xenon::readU16(input);                                  // date
		(void)xenon::readU32(input);                                  // crc
		(void)xenon::readU32(input);                                  // compressed size
		(void)xenon::readU32(input);                                  // uncompressed size
		const std::uint16_t nameLength    = xenon::readU16(input);
		const std::uint16_t extraLength   = xenon::readU16(input);
		const std::uint16_t commentLength = xenon::readU16(input);
		(void)xenon::readU16(input);                                  // disk number start
		(void)xenon::readU16(input);                                  // internal attributes
		(void)xenon::readU32(input);                                  // external attributes
		const std::uint32_t localOffset = xenon::readU32(input);
		const std::string   name        = xenon::readBytes(input, nameLength);
		input.seekg(extraLength, std::ios::cur);
		input.seekg(commentLength, std::ios::cur);

		const std::streampos saved = input.tellg();

		input.seekg(localOffset, std::ios::beg);
		if (xenon::readU32(input) != xenon::LocalHeaderSignature)
		{
			throw PackError("invalid local header");
		}
		(void)xenon::readU16(input);                                  // version
		(void)xenon::readU16(input);                                  // flags
		const std::uint16_t method = xenon::readU16(input);
		(void)xenon::readU16(input);                                  // time
		(void)xenon::readU16(input);                                  // date
		(void)xenon::readU32(input);                                  // crc
		const std::uint32_t compressedSize = xenon::readU32(input);
		(void)xenon::readU32(input);                                  // uncompressed size
		const std::uint16_t localNameLength  = xenon::readU16(input);
		const std::uint16_t localExtraLength = xenon::readU16(input);

		if (method != xenon::StoreMethod)
		{
			throw xenon::PackError("unsupported compression method in archive");
		}

		const std::streamoff dataOffset = localOffset + 30 + localNameLength + localExtraLength;

		xenon::PackedEntry entry;
		entry.Name = name;
		input.seekg(dataOffset, std::ios::beg);
		const std::string raw = xenon::readBytes(input, compressedSize);
		entry.Data.assign(raw.begin(), raw.end());
		result.push_back(std::move(entry));

		input.seekg(saved, std::ios::beg);
	}

	return result;
}

std::string xenon::Packer::readManifest(const std::string& archivePath)
{
	const auto entries = readArchive(archivePath);
	for (const auto& entry : entries)
	{
		if (entry.Name == "META-INF/MANIFEST.MF" || entry.Name == "MANIFEST.MF")
		{
			return std::string(entry.Data.begin(), entry.Data.end());
		}
	}
	throw xenon::PackError("no manifest in archive");
}
