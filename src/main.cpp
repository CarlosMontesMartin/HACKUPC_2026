//===-- main.cpp ------------------------------------------------*- C++ -*-===//
// Warehouse Optimization & Visualizer - executable entry point.
//
// Pipeline (per the PRD's Section 4):
//   1. Read Data
//   2. Run Optimization
//   3. Print Terminal Output
//   4. Map to GL Objects
//   5. Open OpenGL Window
//===----------------------------------------------------------------------===//

#include "common/Types.hpp"
#include "data_input/DataInput.hpp"
#include "data_output/SceneBuilder.hpp"
#include "optimization/Optimizer.hpp"
#include "visualization/Renderer.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

namespace {

void printUsage(const char* prog) {
    std::cerr <<
        "Usage: " << prog << " [data_dir]\n"
        "  data_dir defaults to './data'.\n"
        "  Expected files inside: warehouse.csv, obstacles.csv,\n"
        "                         ceiling.csv, types_of_bays.csv\n";
}

} // anonymous namespace

int main(int argc, char** argv) {
    const std::string dataDir = (argc >= 2) ? argv[1] : "data";

    try {
        /*
         * 1. Read Data
         * Load warehouse configuration from CSV files in the specified directory.
         */
        std::cout << "[1/5] Reading data from '" << dataDir << "'...\n";
        warehouse::WarehouseData data =
            warehouse::DataInput::loadAll(dataDir);
        std::cout << "      perimeter vertices: " << data.perimeter.size()
                  << ", obstacles: "              << data.obstacles.size()
                  << ", ceiling segments: "       << data.ceiling.size()
                  << ", bay types: "              << data.bayTypes.size()
                  << '\n';
        std::cout << "      AABB: ["
                  << data.minX << ", " << data.minY << "] x ["
                  << data.maxX << ", " << data.maxY << "]\n";

        /*
         * 1.5. Initialize OpenGL Context
         * The Renderer creates a hidden window which provides the OpenGL
         * context needed by the Optimizer's GPU compute shaders.
         */
        std::cout << "[2/5] Initializing GPU context...\n";
        warehouse::Renderer renderer(1280, 800, "Warehouse Optimizer - Visualizer");

        /*
         * 2. Run Optimization
         * Execute the GPU-accelerated greedy placement heuristic.
         */
        std::cout << "[3/5] Running GPU optimizer...\n";
        const auto t0 = std::chrono::steady_clock::now();

        // 100 mm grid is a good default for mm-scale warehouse coords.
        // The optimiser is bounded above by ~30 s for typical PRD inputs.
        const float gridCell = 100.0f;
        warehouse::Optimizer optimiser(data, gridCell);
        const auto result = optimiser.run();

        const auto t1 = std::chrono::steady_clock::now();
        const double elapsedMs =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "      placements: " << result.placements.size()
                  << ", elapsed: "        << elapsedMs << " ms\n";

        /*
         * 3. Print Terminal Output
         * Display the optimized bay placements and summary statistics.
         * Format specified in PRD: "ID, X, Y, Rotation".
         */
        std::cout << "[3/5] Optimised placements:\n";
        std::cout << "ID, X, Y, Rotation\n";
        for (const auto& p : result.placements) {
            // Print rotation as a number with no decimals when integral.
            if (p.rotation == static_cast<int>(p.rotation)) {
                std::printf("%d, %.3f, %.3f, %d\n",
                            p.id, p.x, p.y,
                            static_cast<int>(p.rotation));
            } else {
                std::printf("%d, %.3f, %.3f, %.3f\n",
                            p.id, p.x, p.y, p.rotation);
            }
        }
        std::cout << "----------------------------------------\n";
        std::cout << "Total price : " << result.totalPrice         << '\n';
        std::cout << "Total loads : " << result.totalLoads         << '\n';
        std::cout << "Used area   : " << result.usedArea           << '\n';
        std::cout << "Total area  : " << result.warehouseArea      << '\n';
        std::cout << "Area used % : " << (result.percentageAreaUsed * 100.0)
                  << " %\n";
        std::cout << "Q score     : " << result.qScore             << '\n';
        std::cout << "----------------------------------------\n";

        /*
         * 4. Map to GL Objects
         * Convert the optimized data into a 3D scene for visualization.
         */
        std::cout << "[4/5] Building scene for the renderer...\n";
        warehouse::Scene scene =
            warehouse::SceneBuilder::build(data, result.placements);

        /*
         * 5. Open OpenGL Window
         * Display the 3D visualization to the user.
         */
        std::cout << "[5/5] Opening visualisation window. "
                     "Drag the mouse to rotate; ESC to exit.\n";
        renderer.run(scene);

        std::cout << "Goodbye.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << '\n';
        printUsage(argv[0]);
        return 1;
    }
}
