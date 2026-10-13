// Test helper: keeps the install's INI files byte-identical across a game
// run. The engine rewrites icewind2.ini on exit (with any
// IWD2R_INI_OVERRIDE values it read), as the original does.
#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

class InstallIniBackup {
public:
    explicit InstallIniBackup(const std::filesystem::path& dir)
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            std::string ext = entry.path().extension().string();
            for (char& c : ext)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (entry.is_regular_file() && ext == ".ini") {
                std::ifstream in(entry.path(), std::ios::binary);
                m_files[entry.path()] = std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }
        }
    }
    ~InstallIniBackup()
    {
        for (const auto& file : m_files) {
            std::ofstream out(file.first, std::ios::binary | std::ios::trunc);
            out << file.second;
        }
    }
    InstallIniBackup(const InstallIniBackup&) = delete;
    InstallIniBackup& operator=(const InstallIniBackup&) = delete;

private:
    std::map<std::filesystem::path, std::string> m_files;
};
