// Test helper: a private view of the install for one engine run, built by
// the rules in tools/dataview_rules.json (the same file tools/dataview.py
// reads). Engines running at the same time then do not share temp/, cache/
// or the INI files, and the install is never written.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct DataViewRules {
    std::vector<std::string> emptyDirs;
    std::vector<std::string> copiedDirs;
    std::vector<std::string> copiedFiles;
};

// Reads the rules file (names and patterns as written).
DataViewRules LoadDataViewRules(const std::filesystem::path& file);

// Matches name against a pattern with * and ?, ignoring case.
bool DataViewGlob(const std::string& pattern, const std::string& name);

// Builds view as a private view of data (created if missing; completes an
// existing one). Throws std::runtime_error when view is inside data or data
// is inside view, or when a link cannot be made.
void MakeDataView(const std::filesystem::path& data, const std::filesystem::path& view, const DataViewRules& rules);

// Deletes a view without following its links into the install.
void RemoveDataView(const std::filesystem::path& view);

// A view in a fresh temporary folder, removed by the destructor unless the
// environment variable IWD2R_KEEP_DATA_VIEW is 1. Path() is what IWD2_DATA
// should be for the engine.
class DataView {
public:
    explicit DataView(const std::string& data);
    ~DataView();
    DataView(const DataView&) = delete;
    DataView& operator=(const DataView&) = delete;
    const std::string& Path() const { return m_path; }

private:
    std::filesystem::path m_root;
    std::string m_path;
};
