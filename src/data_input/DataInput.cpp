//===-- data_input/DataInput.cpp --------------------------------*- C++ -*-===//
// Module 1 - Data Input implementation.
// Robust, header-tolerant CSV parsing with whitespace trimming and clear
// error reporting via std::runtime_error.
//===----------------------------------------------------------------------===//

#include "DataInput.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace warehouse {
namespace {

/*
 * Local helper functions for string manipulation and CSV parsing.
 * These are kept file-static to prevent leakage outside this translation unit.
 */

/// Trim leading/trailing whitespace (in-place is unnecessary, return copy).
std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

/// Split a CSV line on commas. Does not understand quoted fields - sufficient
/// for the simple numeric inputs described in the PRD.
std::vector<std::string> split(const std::string& line, char delim = ',') {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, delim)) tokens.push_back(trim(item));
    return tokens;
}

/// Heuristic: detect a header row by trying to parse the first cell as
/// a number. If parsing fails, treat the row as a header.
bool isHeaderRow(const std::vector<std::string>& tokens) {
    if (tokens.empty() || tokens.front().empty()) return false;
    try {
        size_t consumed = 0;
        std::stof(tokens.front(), &consumed);
        return consumed == 0;
    } catch (...) {
        return true;
    }
}

/// Open the file and parse line-by-line. The lambda receives the parsed
/// tokens of every data row (header is auto-skipped).
template <typename Fn>
void parseCsv(const std::string& path, Fn&& onRow) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("DataInput: cannot open '" + path + "'");
    }

    std::string line;
    bool firstRow = true;
    std::size_t lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;
        // Skip blanks and obvious comments.
        std::string t = trim(line);
        if (t.empty() || t.front() == '#') continue;

        auto tokens = split(t);
        if (firstRow) {
            firstRow = false;
            if (isHeaderRow(tokens)) continue;
        }
        try {
            onRow(tokens);
        } catch (const std::exception& e) {
            throw std::runtime_error("DataInput: " + path + ":" +
                                     std::to_string(lineNo) +
                                     " -> " + e.what());
        }
    }
}

} // anonymous namespace

/*
 * Public API implementation for data input.
 * Provides functions to read warehouse configuration from CSV files.
 */

std::vector<WarehouseVertex>
DataInput::readWarehouse(const std::string& path) {
    /*
     * Reads the warehouse perimeter polygon from a CSV file.
     * Expects 2 columns: X and Y coordinates.
     * Throws an error if the polygon has fewer than 3 vertices,
     * as a valid polygon must have at least 3 points.
     */
    std::vector<WarehouseVertex> out;
    parseCsv(path, [&](const std::vector<std::string>& t) {
        if (t.size() < 2)
            throw std::runtime_error("warehouse.csv expects 2 columns");
        out.push_back({std::stof(t[0]), std::stof(t[1])});
    });
    if (out.size() < 3)
        throw std::runtime_error(
            "DataInput: warehouse polygon needs at least 3 vertices");
    return out;
}

std::vector<Obstacle>
DataInput::readObstacles(const std::string& path) {
    std::vector<Obstacle> out;
    // obstacles file is allowed to be empty.
    std::ifstream probe(path);
    if (!probe.is_open()) return out;
    probe.close();

    parseCsv(path, [&](const std::vector<std::string>& t) {
        if (t.size() < 4)
            throw std::runtime_error("obstacles.csv expects 4 columns");
        out.push_back({std::stof(t[0]), std::stof(t[1]),
                       std::stof(t[2]), std::stof(t[3])});
    });
    return out;
}

std::vector<CeilingPoint>
DataInput::readCeiling(const std::string& path) {
    std::vector<CeilingPoint> out;
    parseCsv(path, [&](const std::vector<std::string>& t) {
        if (t.size() < 2)
            throw std::runtime_error("ceiling.csv expects 2 columns");
        out.push_back({std::stof(t[0]), std::stof(t[1])});
    });
    // The ceiling profile must be sorted by X for the binary-search lookup.
    std::sort(out.begin(), out.end(),
              [](const CeilingPoint& a, const CeilingPoint& b) {
                  return a.x < b.x;
              });
    return out;
}

std::vector<BayType>
DataInput::readBayTypes(const std::string& path) {
    /*
     * Reads the types of bays available for placement from a CSV file.
     * Expects 7 columns containing ID, dimensions (width, depth, height),
     * gap constraint, capacity (nLoads), and price.
     * Validates that all physical dimensions and values are positive.
     */
    std::vector<BayType> out;
    parseCsv(path, [&](const std::vector<std::string>& t) {
        if (t.size() < 7)
            throw std::runtime_error("types_of_bays.csv expects 7 columns");
        BayType b;
        b.id     = std::stoi(t[0]);
        b.width  = std::stof(t[1]);
        b.depth  = std::stof(t[2]);
        b.height = std::stof(t[3]);
        b.gap    = std::stof(t[4]);
        b.nLoads = std::stoi(t[5]);
        b.price  = std::stof(t[6]);
        if (b.width <= 0 || b.depth <= 0 || b.height <= 0)
            throw std::runtime_error("bay type has non-positive dimensions");
        if (b.nLoads <= 0 || b.price <= 0)
            throw std::runtime_error("bay type has non-positive loads/price");
        out.push_back(b);
    });
    if (out.empty())
        throw std::runtime_error("DataInput: no bay types defined");
    return out;
}

WarehouseData DataInput::loadAll(const std::string& dataDir) {
    auto join = [&](const std::string& f) {
        if (dataDir.empty()) return f;
        if (dataDir.back() == '/' || dataDir.back() == '\\') return dataDir + f;
        return dataDir + "/" + f;
    };
    WarehouseData data;
    data.perimeter = readWarehouse (join("warehouse.csv"));
    data.obstacles = readObstacles (join("obstacles.csv"));
    data.ceiling   = readCeiling   (join("ceiling.csv"));
    data.bayTypes  = readBayTypes  (join("types_of_bays.csv"));
    data.computeBounds();
    return data;
}

} // namespace warehouse
