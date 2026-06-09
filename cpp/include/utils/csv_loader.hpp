#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <concepts>
#include <filesystem>


template <typename T>
concept FromCsvRow = requires(const std::vector<std::string>& row) {
    { T::from_csv_row(row) } -> std::same_as<T>;
};

// row count as file_size / line_size
inline std::size_t estimate_row_count(const std::string& path, std::ifstream& file, bool has_header) {
    // read file size metadata
    std::error_code ec;
    auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size == 0) return 1024;

    // get one line
    std::string line;
    if (has_header) std::getline(file, line);
    if (!std::getline(file, line) || line.empty()) { file.seekg(0); return 1024; }

    // get bytes in 1 line
    std::size_t row_bytes = line.size() + 1;
    file.seekg(0);
    return (file_size / row_bytes) + 1;
}


template <FromCsvRow T>
std::vector<T> load_csv(const std::string& path, bool has_header = true) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("could not open: " + path);
    }

    std::vector<T> events;
    events.reserve(estimate_row_count(path, file, has_header));

    std::string line;
    if (has_header) std::getline(file, line);

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::vector<std::string> cells;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) cells.push_back(cell);

        events.push_back(T::from_csv_row(cells));
    }

    return events;
}