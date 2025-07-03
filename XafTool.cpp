#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <print>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <filesystem>

#include "CLI11.hpp"
#include "LZWDecoder.hpp"

constexpr auto ANSI_COLOR_RED = "\x1b[31m";
constexpr auto ANSI_COLOR_GREEN = "\x1b[32m";
constexpr auto ANSI_COLOR_YELLOW = "\x1b[33m";
constexpr auto ANSI_COLOR_BLUE = "\x1b[34m";
constexpr auto ANSI_COLOR_MAGENTA = "\x1b[35m";
constexpr auto ANSI_COLOR_CYAN = "\x1b[36m";
constexpr auto ANSI_COLOR_RESET = "\x1b[0m";

// Notes:
// A XAF is split into "sectors", which are usually 2048 bytes in size.
// Check the sectorSize in the header.
// So, to get a file offset, you can multiply the sectorSize by the sectorIndex of a file
// and then you have the actual position/offset of the file inside the archive file.
// I assume this was done so that compressed files are aligned.
struct XafHeader {
	char signature[4]; // "xaf0"
	uint16_t majorVersion;
	uint16_t minorVersion;
	uint32_t sectorSize;
	uint32_t totalRecords;
	uint32_t totalDirectories;
	uint32_t totalFiles;
	uint64_t dataSectorCount;
	uint64_t headerSectorCount;
	uint64_t totalSectorCount;
	char title[64];
	char comment[64];
	uint32_t totalVolumes;
	uint32_t padding; // unsure
};

// Note: It looks like Sega actually did not compress all files. Based on the compression threshold,
// if the rate was too low, they just stored the file uncompressed.
// I've also not seen the encryption type being used in any game.
// The yabukita dll checks if any encryption is set, and throws an error if that's the case.
struct XafFileFlags {
	uint8_t isFile; // 0x01 if it's a file, 0x00 if it's a directory
	uint8_t compressionType; // 0x01 for LZW compression, 0x00 for uncompressed
	uint8_t encryptionType;
	uint8_t flag4;
};

struct XafFileEntry {
	char name[64];
	XafFileFlags flags;
	uint32_t parentId;
	uint32_t nextSibling;
	uint32_t firstChild;
	uint32_t size;
	uint32_t compressedSize;
	uint64_t sectorStartIndex;
};

// used in newer versions of the game
struct XafFileEntry2 {
	char name[128];
	XafFileFlags flags;
	uint32_t parentId;
	uint32_t nextSibling;
	uint32_t firstChild;
	uint32_t padding1;
	uint32_t size;
	uint32_t compressedSize;
	uint32_t padding2;
	uint64_t sectorStartIndex;
	uint64_t padding3;
};

enum XafCompressionType {
	UNCOMPRESSED = 0,
	LZW_COMPRESSED = 1
};

static constexpr int kHEADER_SIZE = 0x100;

static int readFromFile(HANDLE fileHandle, void* buffer, size_t size) {
	DWORD bytesRead;
	if (!ReadFile(fileHandle, buffer, static_cast<DWORD>(size), &bytesRead, nullptr)) {
		std::println("Error reading from file: {}", GetLastError());
		return -1;
	}
	return static_cast<int>(bytesRead);
}

static int moveFileCursor(HANDLE fileHandle, uint64_t offset, DWORD moveMethod) {
	LARGE_INTEGER li{};
	li.QuadPart = offset;
	if (!SetFilePointerEx(fileHandle, li, nullptr, moveMethod)) {
		std::println("Error moving file cursor: {}", GetLastError());
		return -1;
	}
	return 0;
}

int main(int argc, char** argv)
{
	CLI::App app{ "XafTool" };
	XafHeader header{};
	std::string filePath;

#ifdef _DEBUG
	bool isVerbose = true;
#else
	bool isVerbose = false;
#endif

	app.add_option("file", filePath, "Input file path")
		->required()
		->check(CLI::ExistingFile.description("The specified .xaf file cannot be found."));
	app.add_flag("-v,--verbose", isVerbose, "Enable verbose output");
	CLI11_PARSE(app, argc, argv);

	// For now, extract to the same folder the .xaf is in.
	std::string outputFolder = std::filesystem::canonical(filePath).remove_filename().string();

	HANDLE fileHandle = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		std::println("Failed to open file: {}", filePath);
		return 1;
	}

	std::println("Parsing archive: {}", filePath);

	readFromFile(fileHandle, header.signature, sizeof(header.signature));
	// check if the signature is correct, if not we can bail here already...
	std::string signature = std::string(header.signature, 4);
	if (signature != "xaf0")
	{
		std::println("{}Invalid XAF file signature: {}{}", ANSI_COLOR_RED, signature, ANSI_COLOR_RESET);
		CloseHandle(fileHandle);
		return 1;
	}

	readFromFile(fileHandle, &header.majorVersion, sizeof(header.majorVersion));
	readFromFile(fileHandle, &header.minorVersion, sizeof(header.minorVersion));
	readFromFile(fileHandle, &header.sectorSize, sizeof(header.sectorSize));
	readFromFile(fileHandle, &header.totalRecords, sizeof(header.totalRecords));
	readFromFile(fileHandle, &header.totalDirectories, sizeof(header.totalDirectories));
	readFromFile(fileHandle, &header.totalFiles, sizeof(header.totalFiles));
	readFromFile(fileHandle, &header.dataSectorCount, sizeof(header.dataSectorCount));
	readFromFile(fileHandle, &header.headerSectorCount, sizeof(header.headerSectorCount));
	readFromFile(fileHandle, &header.totalSectorCount, sizeof(header.totalSectorCount));
	readFromFile(fileHandle, header.title, sizeof(header.title));
	readFromFile(fileHandle, header.comment, sizeof(header.comment));
	readFromFile(fileHandle, &header.totalVolumes, sizeof(header.totalVolumes));
	readFromFile(fileHandle, &header.padding, sizeof(header.padding));
	std::println("Format Type: {}", signature);
	std::println("Version: {}.{}", header.majorVersion, header.minorVersion);
	std::println("Sector Size: {}", header.sectorSize);
	std::println("Total Records: {}", header.totalRecords);
	std::println("Total Directories: {}", header.totalDirectories);
	std::println("Total Files: {}", header.totalFiles);
	std::println("Header Sector Count: {}", header.headerSectorCount);
	std::println("Data Sector Count: {}", header.dataSectorCount);
	std::println("Total Sector Count: {}", header.totalSectorCount);
	std::println("Title: {}", std::string(header.title, 64));
	std::println("Comment: {}", std::string(header.comment, 64));

	// Header's all parsed, let's move on to the file list
	moveFileCursor(fileHandle, kHEADER_SIZE, FILE_BEGIN);
	std::vector<XafFileEntry> fileEntries;
	// Newer game versions have a different file entry structure, so we need to check the version
	// This kinda sucks I think but it works for now.
	std::vector<XafFileEntry2> fileEntries2; 
	bool isVersion2 = header.majorVersion == 2;
	if (isVersion2)
	{
		fileEntries2.reserve(header.totalRecords);
	}
	else {
		fileEntries.reserve(header.totalRecords);
	}

	bool firstCompressed = true;
	std::println("{}------------File Entries-----------{}", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
	for (uint32_t i = 0; i < header.totalRecords; i++)
	{
		if (isVersion2)
		{
			XafFileEntry2 fileEntry{};
			readFromFile(fileHandle, fileEntry.name, sizeof(fileEntry.name));
			readFromFile(fileHandle, &fileEntry.flags, sizeof(fileEntry.flags));
			readFromFile(fileHandle, &fileEntry.parentId, sizeof(fileEntry.parentId));
			readFromFile(fileHandle, &fileEntry.nextSibling, sizeof(fileEntry.nextSibling));
			readFromFile(fileHandle, &fileEntry.firstChild, sizeof(fileEntry.firstChild));
			readFromFile(fileHandle, &fileEntry.padding1, sizeof(fileEntry.padding1));
			readFromFile(fileHandle, &fileEntry.size, sizeof(fileEntry.size));
			readFromFile(fileHandle, &fileEntry.compressedSize, sizeof(fileEntry.compressedSize));
			readFromFile(fileHandle, &fileEntry.padding2, sizeof(fileEntry.padding2));
			readFromFile(fileHandle, &fileEntry.sectorStartIndex, sizeof(fileEntry.sectorStartIndex));
			readFromFile(fileHandle, &fileEntry.padding3, sizeof(fileEntry.padding3));
			fileEntries2.push_back(fileEntry);
		}
		else {
			XafFileEntry fileEntry{};
			readFromFile(fileHandle, fileEntry.name, sizeof(fileEntry.name));
			readFromFile(fileHandle, &fileEntry.flags, sizeof(fileEntry.flags));
			readFromFile(fileHandle, &fileEntry.parentId, sizeof(fileEntry.parentId));
			readFromFile(fileHandle, &fileEntry.nextSibling, sizeof(fileEntry.nextSibling));
			readFromFile(fileHandle, &fileEntry.firstChild, sizeof(fileEntry.firstChild));
			readFromFile(fileHandle, &fileEntry.size, sizeof(fileEntry.size));
			readFromFile(fileHandle, &fileEntry.compressedSize, sizeof(fileEntry.compressedSize));
			readFromFile(fileHandle, &fileEntry.sectorStartIndex, sizeof(fileEntry.sectorStartIndex));

			fileEntries.push_back(fileEntry);
		}
	}

	// The file list is parsed, so now we can just iterate over the parsed entries. This means we only need to jump to each file,
	// instead of from and back to the file entry list.
	// Lambda so we can use it for both versions, as we don't need to access any of the newer fields and the structure
	// (apart from the 2x file name size) is identical otherwise
	auto processEntries = [&](auto& entries) {
		std::println("Total File Entries: {}", entries.size());
		for (size_t i = 0; i < entries.size(); ++i)
		{
			std::println("Parsing file entry {} of {}", i + 1, header.totalRecords);
			const auto& fileEntry = entries[i];

			std::string fullPath = "";
			// Let's try and get the full path
			if (fileEntry.parentId != 0xFFFFFFFF) {

				std::vector<std::string> folderList;
				uint32_t parentId = fileEntry.parentId;
				while (true)
				{
					if (parentId == 0xFFFFFFFF) {
						// Root folder hit, so let's bounce
						break;
					}
					auto& parent = entries[parentId];
					std::string parentPath = parent.name;
					if (parentPath.back() != '\\') {
						parentPath += '\\';
					}

					folderList.push_back(parentPath);
					parentId = parent.parentId;
				}
				// Reverse the folder list to get the correct order because we dig em inside out
				for (auto it = folderList.rbegin(); it != folderList.rend(); ++it) {
					fullPath += *it;
				}
				fullPath += std::string(fileEntry.name, 64);
			}

			std::println("File: {}", fullPath);
			if (isVerbose)
			{

				std::string flagsStr = fileEntry.flags.isFile ? "File" : "Directory";
				std::println("Type: {} | Compression: {} | Encryption: {} | Flag4: {}",
					flagsStr,
					fileEntry.flags.compressionType,
					fileEntry.flags.encryptionType,
					fileEntry.flags.flag4);
				std::println("Parent Id: {} ({:X})", fileEntry.parentId, fileEntry.parentId);
				std::println("Next Sibling: {} ({:X})", fileEntry.nextSibling, fileEntry.nextSibling);
				std::println("First Child: {} ({:X})", fileEntry.firstChild, fileEntry.firstChild);
				std::println("File Size: {} ({:X})", fileEntry.size, fileEntry.size);
				std::println("Compressed Size: {} ({:X})", fileEntry.compressedSize, fileEntry.compressedSize);
				std::println("Sector Start Index: {} ({:X})", fileEntry.sectorStartIndex, fileEntry.sectorStartIndex);
				std::println("----------------------------------------");
			}

			if (!fileEntry.flags.isFile) {
				std::string finalPath = outputFolder + "\\" + fullPath;
				std::filesystem::path outputPath = std::filesystem::path(finalPath);
				try {
					std::filesystem::create_directories(outputPath);
				}
				catch (...) {
					std::println("{}Failed to create directories for file: {}{}", ANSI_COLOR_RED, outputPath.string(), ANSI_COLOR_RESET);
				}
				continue;
			}

			// For now, we only support uncompressed and LZW compressed files, unless I stumble onto an example that has a diff comp type
			// or uses the magical encryption flag.
			switch (fileEntry.flags.compressionType)
			{
			case UNCOMPRESSED:
			{
				moveFileCursor(fileHandle, fileEntry.sectorStartIndex * header.sectorSize, FILE_BEGIN);

				// Uncompressed files do not have the YS header, so we can just dump them to disk immediately. Yay.
				std::vector<uint8_t> fileBuffer(fileEntry.size);
				readFromFile(fileHandle, fileBuffer.data(), fileEntry.size);

				std::string finalPath = outputFolder + "\\" + fullPath;

				HANDLE hOutFile = CreateFileA(finalPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hOutFile != INVALID_HANDLE_VALUE) {
					DWORD bytesWritten;
					WriteFile(hOutFile, fileBuffer.data(), fileBuffer.size(), &bytesWritten, NULL);
					CloseHandle(hOutFile);
				}
			}
			break;
			case LZW_COMPRESSED:
			{
				moveFileCursor(fileHandle, fileEntry.sectorStartIndex * header.sectorSize, FILE_BEGIN);
				// Compressed files all have a 4 byte header starting with "YS", I assume because Yabukita::Stream
				// We need to skip past it, and make sure to subtract that tiny header from the final size too...
				size_t actualSize = fileEntry.compressedSize - 4;
				std::vector<uint8_t> inputBuffer(actualSize);

				DWORD ysMagic = 0;
				readFromFile(fileHandle, &ysMagic, 4);
				readFromFile(fileHandle, inputBuffer.data(), actualSize);

				LZWDecoder decoder(inputBuffer.data(), actualSize, fileEntry.size);
				std::vector<uint8_t> decompressed = decoder.decode();

				std::string finalPath = outputFolder + "\\" + fullPath;
				HANDLE hOutFile = CreateFileA(finalPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (hOutFile != INVALID_HANDLE_VALUE) {
					DWORD bytesWritten;
					WriteFile(hOutFile, decompressed.data(), decompressed.size(), &bytesWritten, NULL);
					CloseHandle(hOutFile);
				}
				else {
					std::println("{}Failed to create output file: {}{}", ANSI_COLOR_RED, finalPath, ANSI_COLOR_RESET);
					continue;
				}
			}
			break;
			default:
				std::println("{}Unknown compression type for file {}: {}{}", ANSI_COLOR_RED, i, fileEntry.flags.compressionType, ANSI_COLOR_RESET);
				std::println("Skipping file...");
				continue;
			}
		}
		};

	if (isVersion2) {
		processEntries(fileEntries2);
	}
	else {
		processEntries(fileEntries);
	}

	CloseHandle(fileHandle);
	std::println("{}My work here is done.{}", ANSI_COLOR_GREEN, ANSI_COLOR_RESET);
}