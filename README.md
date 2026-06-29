# Warehouse Optimization & Visualizer

A single C++17 executable that reads a warehouse description from CSV,
greedily packs storage bays to minimise the cost function

```
Q = ( Σ price / Σ loads ) ^ ( 2 - %areaUsed )
```

prints the chosen layout, and opens an interactive 3D / minimap OpenGL
visualisation of the result.

## Project Structure

The codebase is organised around the four modules defined in the PRD,
each with its own folder under `src/`. Every module exposes a small,
header-only interface and is independently testable.

```
warehouse_optimizer/
├── CMakeLists.txt
├── README.md
├── data/                            # Sample input CSVs
│   ├── warehouse.csv                # Perimeter vertices       (X,Y)
│   ├── obstacles.csv                # Internal obstacles       (X,Y,W,D)
│   ├── ceiling.csv                  # Ceiling profile          (X,H)
│   └── types_of_bays.csv            # Bay catalogue            (Id,W,D,H,Gap,Loads,Price)
└── src/
    ├── main.cpp                     # Pipeline driver: 5 sequential phases
    ├── common/                      # Shared POD-like types
    │   ├── Types.hpp / .cpp
    ├── data_input/                  # MODULE 1 - CSV ingestion
    │   ├── DataInput.hpp / .cpp
    ├── optimization/                # MODULE 2 - Greedy heuristic
    │   ├── Optimizer.hpp / .cpp
    ├── data_output/                 # MODULE 3 - Internal mapping for GL
    │   ├── SceneBuilder.hpp / .cpp
    └── visualization/               # MODULE 4 - OpenGL renderer
        ├── Camera.hpp / .cpp        # Orbit camera (mouse-drag only)
        ├── Shader.hpp / .cpp        # RAII shader-program wrapper
        └── Renderer.hpp / .cpp      # Window + 3D view + minimap
```

### Module separation

| Module                   | Owns                                       | Knows nothing about                |
|--------------------------|--------------------------------------------|------------------------------------|
| 1. `data_input`          | CSV parsing, error reporting               | Optimisation, OpenGL               |
| 2. `optimization`        | Grid, polygon test, greedy fill, Q score   | I/O, OpenGL                        |
| 3. `data_output`         | Mapping placements + geometry to `Scene`   | Files, GL calls                    |
| 4. `visualization`       | GLFW window, GLEW, shaders, camera, draws  | CSV, optimisation                  |

The data flow is strictly one-way:

```
CSV ──▶ DataInput ──▶ WarehouseData ──▶ Optimizer ──▶ Result
                                                       │
                                                       ▼
                                          SceneBuilder ──▶ Scene
                                                       │
                                                       ▼
                                                   Renderer
```

## Prerequisites

* A C++17 compiler (GCC ≥ 7, Clang ≥ 6, MSVC 2017+)
* CMake 3.10 +
* `GLFW` 3.3 +     (windowing / input)
* `GLEW` 2.1 +     (OpenGL function pointers)
* `GLM` 0.9.9 +    (math; header-only)
* OpenGL 3.3 core profile capable GPU + drivers

### Installing dependencies

* **Debian / Ubuntu**

  ```bash
  sudo apt-get install build-essential cmake libglfw3-dev libglew-dev libglm-dev
  ```

* **Fedora / RHEL**

  ```bash
  sudo dnf install gcc-c++ cmake glfw-devel glew-devel glm-devel
  ```

* **macOS (Homebrew)**

  ```bash
  brew install cmake glfw glew glm
  ```

* **Windows (vcpkg)**

  ```powershell
  vcpkg install glfw3 glew glm
  cmake -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ..
  ```

## Build Instructions

```bash
mkdir build && cd build
cmake ..
make            # or  cmake --build .
```

The `data/` directory is automatically copied next to the executable on
build, so you can run the binary directly from the build tree.

## Running

```bash
./WarehouseOptimizer            # uses ./data
./WarehouseOptimizer mydata     # custom data folder
```

You will see, in order:

1. CSV summary (vertex / obstacle / segment / bay-type counts + AABB).
2. Optimiser timing.
3. The exact placement list, one per line, in the PRD-mandated format
   `ID, X, Y, Rotation`, followed by a totals block.
4. A scene-build line.
5. An OpenGL window opens — left-mouse-drag to orbit the camera,
   `Esc` to quit. The minimap (top-down orthographic view of the same
   scene) is shown in the top-right corner.

## Input File Formats

All files accept an optional header row (auto-detected by trying to
parse the first cell as a number). Comments may be used by starting a
line with `#`. Numeric values are world units (millimetres in the
provided sample data, but any consistent unit works).

```csv
# warehouse.csv
CoordX,CoordY
0,0
30000,0
30000,20000
0,20000
```

```csv
# obstacles.csv
CoordX,CoordY,Width,Depth
5000,5000,2000,3000
```

```csv
# ceiling.csv  (piecewise-constant along X)
CoordX,CeilingHeight
0,8000
10000,6000
20000,8000
```

```csv
# types_of_bays.csv
Id,Width,Depth,Height,Gap,nLoads,Price
1,1200,1000,3000,100,4,500
```

## Algorithm Notes

* Greedy bottom-left fill on a uniform 100 mm occupancy grid with three
  cell states: `kFree`, `kSolid` (walls / obstacles / bay solids), and
  `kGap` (only other gaps may share the cell).
* Bay types are pre-ranked by `(nLoads × area) / price` (descending).
* **Continuous rotation search.** At every leftmost-bottommost free cell
  the algorithm tries a priority-ordered list of angles - the four
  cardinals (0, 90, 180, 270) first, then a 30-degree sweep through the
  rest of the circle (30, 60, 120, 150, 210, 240, 300, 330). Adding more
  candidate angles is a one-line change in `kCandidateAngles`. The grid
  validator handles any angle uniformly via OBB-on-grid rasterisation,
  so finer angular steps work without code changes.
* **Gap clearance rule (PRD ampliation §1).** Each bay-type carries a
  `gap` clearance that must be kept empty IN FRONT of the bay (local +Y
  direction). The algorithm enforces:
   * **Hard collision.** No solid (warehouse wall, obstacle, or another
     bay's solid structure) may intersect a gap.
   * **Gap overlap.** Two gaps MAY share cells - the effective spacing
     between two facing bays is `max(gap1, gap2)`, gaps do NOT sum.
* **OBB rasterisation.** A rotated rectangle is tested against the grid
  by enumerating cells in its world-AABB and applying a 4-axis SAT test
  (axis-aligned cell vs oriented bounding box). This conservatively
  marks every cell the OBB clips, even when the OBB's centre falls
  outside the cell - which is essential for rotated rectangles whose
  corners poke through cell boundaries.
* The piecewise-constant ceiling is honoured by sampling every cell
  along the bay's X-span via a binary-search lookup.
* Sample input fills ~79 % of the available area in ~33 ms on a
  modern laptop - well under the PRD's 30 s budget.

## Customising the Visualisation

Open `src/visualization/Renderer.cpp` to tweak:

* The minimap's location / size (`renderMinimap`).
* The default starting yaw / pitch (`Renderer::run`).
* Per-face shading factors (`kFaceShade`).
* Background colours (`renderMainView`, `renderMinimap`).

Visualisation rules from the PRD ampliation:

* **Bays** carry a thin black outline drawn as a 12-edge wireframe cube
  on top of the filled solid (polygon-offset disambiguates the two
  passes).
* **Gaps** are explicitly rendered as bright neon-purple thin slabs
  (30 mm tall, RGB `(0.85, 0.0, 0.95)`) sitting on the floor in front
  of each bay, also outlined in black. Purple is reserved exclusively
  for gaps so collisions during debugging are immediately obvious.
* **Floor and translucent ceiling** are clipped to the warehouse
  perimeter polygon via an axis-aligned rectilinear decomposition -
  cells whose centre lies outside the polygon are dropped from the
  mesh. The ceiling additionally honours the per-segment height profile
  by elevating each cell to `ceilingAt(cell.x)`.

Bay colours are derived from a deterministic golden-angle hash of the
bay id, so identical types are always rendered with the same hue
without needing a palette table (`SceneBuilder::colorForBayId`).

## Memory & Resource Management

The codebase respects the PRD's RAII rule:

* No raw `new` / `delete` anywhere.
* `Shader` is a non-copyable, movable RAII wrapper around the GL program.
* GL buffers / VAOs are created in `Renderer::initGL` and freed in
  `~Renderer()`; `Renderer` itself is non-copyable.
* `std::unique_ptr` is used for the two shader programs.
* GLFW lifecycle (`glfwInit` / `glfwTerminate`) is bound to the
  `Renderer` object lifetime.

## License

Provided as-is for evaluation purposes.
