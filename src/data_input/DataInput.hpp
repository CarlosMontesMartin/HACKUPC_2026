#pragma once
//===-- data_input/DataInput.hpp --------------------------------*- C++ -*-===//
// Module 1 - Data Input.
// Reads the four CSV files (warehouse, obstacles, ceiling, bay types) and
// produces an aggregated `WarehouseData` for the rest of the pipeline.
//===----------------------------------------------------------------------===//

#include "../common/Types.hpp"

#include <string>

namespace warehouse {

/// Stateless ingestion facade. All methods are static; constructing an
/// instance is unnecessary.
class DataInput {
public:
    /// Convenience: load all four files from a directory.
    /// Expected filenames are exactly: warehouse.csv, obstacles.csv,
    /// ceiling.csv, types_of_bays.csv (per the PRD).
    static WarehouseData loadAll(const std::string& dataDir);

    static std::vector<WarehouseVertex> readWarehouse(const std::string& path);
    static std::vector<Obstacle>        readObstacles(const std::string& path);
    static std::vector<CeilingPoint>    readCeiling  (const std::string& path);
    static std::vector<BayType>         readBayTypes (const std::string& path);
};

} // namespace warehouse
