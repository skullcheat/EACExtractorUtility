/**
 * Document Name: EAC_Decrypt_Extract.cpp
 * Author: Lucas Guilherme
 * Description: This C++ program decrypts and extracts files from an EAC (Easy Anti-Cheat) binary.
 *              It performs a sequence of operations such as loading an encrypted file, decrypting the file,
 *              searching for specific patterns within the loaded library, copying encrypted data into separate vectors,
 *              and decrypting the data, followed by saving them as separate files.
 * Date: 22/05/2023
 *
 * This program performs the following steps:
 * 1. Loads an encrypted file specified as a command-line argument.
 * 2. Decrypts the loaded file using a custom decryption algorithm.
 * 3. Retrieves the current directory path.
 * 4. Saves the decrypted file with a specific name in the current directory.
 * 5. Loads the decrypted file as a library.
 * 6. Searches for specific patterns within the loaded library to determine the start and size of data.
 * 7. Copies encrypted data based on the identified patterns into separate vectors.
 * 8. Decrypts the data in the vectors.
 * 9. Saves the decrypted data as separate files in the current directory.
 *
 * Command-line usage: EAC_Decrypt_Extract.exe <EAC.Bin>
 *
 * Dependencies: Windows.h, fstream, format
 *
 */

#include <iostream>
#include <vector>
#include <Windows.h>
#include <fstream>
#include <format>

uintptr_t ResolveRelative(const uintptr_t adressPointer, const ULONG offsetCount,
                          const ULONG     sizeOfInstruction)
{
    const ULONG_PTR adressToResolve               = adressPointer;
    const LONG      totalBytesFromSpecifiedAdress = *(PLONG)(adressToResolve + offsetCount);
    const uintptr_t resultFinal                   = (adressToResolve + sizeOfInstruction + totalBytesFromSpecifiedAdress
    );

    return resultFinal;
}

std::vector<uintptr_t> PatternScan(const uintptr_t moduleAdress, const char* signature)
{
    std::vector<uintptr_t> tmp;

    if (!moduleAdress)
        return tmp;

    static auto patternToByte = [](const char* pattern)
    {
        auto       bytes = std::vector<int>{};
        const auto start = const_cast<char*>(pattern);
        const auto end   = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current)
        {
            if (*current == '?')
            {
                ++current;
                if (*current == '?')
                    ++current;
                bytes.push_back(-1);
            }
            else { bytes.push_back(strtoul(current, &current, 16)); }
        }
        return bytes;
    };

    const auto dosHeader = (PIMAGE_DOS_HEADER)moduleAdress;
    const auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)moduleAdress + dosHeader->e_lfanew);

    const auto sizeOfImage  = ntHeaders->OptionalHeader.SizeOfImage;
    auto       patternBytes = patternToByte(signature);
    const auto scanBytes    = reinterpret_cast<std::uint8_t*>(moduleAdress);

    const auto s = patternBytes.size();
    const auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i)
    {
        bool found = true;
        for (auto j = 0ul; j < s; ++j)
        {
            if (scanBytes[i + j] != d[j] && d[j] != -1)
            {
                found = false;
                break;
            }
        }
        if (found)
        {
            tmp.push_back(reinterpret_cast<uintptr_t>(&scanBytes[i]));
        }
    }
    return tmp;
}

std::vector<uint8_t> LoadBinaryFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open the file: " << filePath << std::endl;
        return {};
    }

    // Determine the size of the file
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Create a vector with the appropriate size
    std::vector<uint8_t> buffer(fileSize);

    // Read the file into the vector
    if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
    {
        std::cerr << "Failed to read the file: " << filePath << std::endl;
        return {};
    }

    return buffer;
}

bool SaveBinaryFile(const std::string& filePath, const std::vector<uint8_t>& data)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to create the file: " << filePath << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!file)
    {
        std::cerr << "Failed to write the data to the file: " << filePath << std::endl;
        return false;
    }

    return true;
}


std::vector<uint8_t> DecryptBuffer(const std::vector<uint8_t>* encryptedVector)
{
    // Copy the buffer to a temporary vector
    std::vector<uint8_t> tmp = *encryptedVector;

    // Begin
    auto beginBuffer = tmp.data();

    // End
    auto endBuffer = beginBuffer + tmp.size();

    // Pointer to the current byte in the buffer
    uint8_t* pCurrentByte = tmp.data();

    // Calculate the size of the module
    size_t moduleSize = endBuffer - beginBuffer;

    // Ensure the module size is at least 2
    if (moduleSize >= 2)
    {
        // Adjust the last byte in the buffer
        endBuffer[-1] += 3 - 3 * (LOBYTE(endBuffer) - LOBYTE(beginBuffer));

        // Iterate over the buffer in reverse order, starting from the second last byte
        for (size_t i = moduleSize - 2; i; --i)
            pCurrentByte[i] += -3 * i - pCurrentByte[i + 1];

        // Adjust the first byte in the buffer
        *pCurrentByte -= pCurrentByte[1];
    }

    return tmp;
}


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf("Usage: %s <EAC.Bin>\n", argv[0]);
        return 1;
    }

    std::printf("[-] Decrypting files...\n");

    std::string filePath = argv[1];

    // 1. Load the file
    auto encryptedfile = LoadBinaryFile(filePath);

    // 2. Decrypt the file
    auto decryptedBuffer = DecryptBuffer(&encryptedfile);


    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);

    // 3. Save the decrypted file
    SaveBinaryFile(std::format("{}\\EAC_Launcher.dll", currentDir), decryptedBuffer);

    // 4. Load the EAC_Launcher
    auto launcherData = LoadLibraryA(std::format("{}\\EAC_Launcher.dll", currentDir).c_str());

    // 5. Find the Pattern of start data and the size of the data
    auto result = PatternScan((uintptr_t)launcherData, "A7 ED 96 0C 0F");

    auto size1 = *(uint32_t*)ResolveRelative(
        PatternScan((uintptr_t)launcherData, "BE ? ? ? ? E9 ? ? ? ? 8B 3D").at(0) + 0xA, 2, 6);
    auto size2 = *(uint32_t*)ResolveRelative(PatternScan((uintptr_t)launcherData, "8B 15 ? ? ? ? 48 89 7C 24").at(0), 2,
                                             6);

    // 6. Copy to an Vector
    auto driverDataEncrypted     = std::vector<uint8_t>((uint8_t*)result.at(0), (uint8_t*)result.at(0) + size1);
    auto userModeDriverEncrypted = std::vector<uint8_t>((uint8_t*)result.at(1), (uint8_t*)result.at(1) + size2);

    // 7. Decrypt the data
    auto driverDataDecrypted   = DecryptBuffer(&driverDataEncrypted);
    auto usermodeDataDecrypted = DecryptBuffer(&userModeDriverEncrypted);

    // 8. Save the data
    SaveBinaryFile(std::format("{}\\EAC_Driver.sys", currentDir), driverDataDecrypted);
    SaveBinaryFile(std::format("{}\\EAC_UserMode.dll", currentDir), usermodeDataDecrypted);

    // 9. Print the result
    std::printf("[-] All files successfully generated! \n");
    Sleep(1000);


    return 0;
}
