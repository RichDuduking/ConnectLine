# Project Structure And Performance Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the single-file Qt demo into focused algorithm, diagnostics, UI, and entry-point modules, and measure the connection algorithm's elapsed time and process memory usage.

**Architecture:** Keep the greedy connection algorithm independent from widgets. Put timing and Windows process-memory sampling behind a diagnostics module that returns the connected polyline plus a measurement snapshot. Move all QWidget state into `PracticeWindow`, leaving `main.cpp` as a thin entry point.

**Tech Stack:** C++17, Qt 6 Widgets, CMake, CTest, Windows `GetProcessMemoryInfo`.

## Global Constraints

- Do not change the greedy connection behavior.
- Preserve connection cancellation and original-line snapshot behavior.
- Keep the project buildable on non-Windows platforms; report memory metrics as unavailable there.
- Do not take ownership of global `operator new` or `operator delete`.
- Do not commit changes unless explicitly requested.

---

### Task 1: Extract Polyline Algorithms

**Files:**
- Create: `src/algorithm/polyline_algorithms.h`
- Create: `src/algorithm/polyline_algorithms.cpp`
- Create: `tests/polyline_algorithms_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `QPolygonF polyline::connectGreedily(const QList<QPolygonF> &polylines);`
- Produces: `QPolygonF polyline::randomPolyline(const QRectF &area, int pointCount);`
- Produces: `bool polyline::intersectsAny(const QPolygonF &candidate, const QList<QPolygonF> &existing);`

- [ ] Write a deterministic test proving three polylines become one chain containing every input point exactly once.
- [ ] Run the test and verify it fails because the algorithm module does not exist.
- [ ] Move the random generation, intersection checks, and greedy connection implementation into the module.
- [ ] Run the test and verify it passes.

### Task 2: Add Independent Connection Benchmark

**Files:**
- Create: `src/diagnostics/connection_benchmark.h`
- Create: `src/diagnostics/connection_benchmark.cpp`
- Modify: `tests/polyline_algorithms_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `ConnectionBenchmarkResult diagnostics::benchmarkConnection(const QList<QPolygonF> &polylines);`
- Produces fields: `connectedPolyline`, `elapsedNanoseconds`, `privateBytesBefore`, `privateBytesAfter`, `privateBytesDelta`, `workingSetBytesBefore`, `workingSetBytesAfter`, `workingSetDelta`, `peakWorkingSetBytes`, `memoryMetricsAvailable`.

- [ ] Write a test that benchmarks three deterministic polylines and verifies the returned chain and non-negative elapsed time.
- [ ] Run the test and verify it fails because the diagnostics module does not exist.
- [ ] Implement `QElapsedTimer` timing and Windows process-memory snapshots with `GetProcessMemoryInfo`.
- [ ] Return unavailable memory metrics on non-Windows platforms without changing the algorithm result.
- [ ] Run the test and verify it passes.

### Task 3: Split UI From Entry Point

**Files:**
- Create: `src/main.cpp`
- Create: `src/ui/practice_window.h`
- Create: `src/ui/practice_window.cpp`
- Move: `MyGraphicsView.h` to `src/ui/my_graphics_view.h`
- Delete: root `main.cpp`
- Delete: root `MyGraphicsView.h`

**Interfaces:**
- Produces: `class PracticeWindow : public QWidget`
- Produces: `PracticeWindow::PracticeWindow(QWidget *parent = nullptr)`
- Consumes: `polyline::randomPolyline`, `polyline::intersectsAny`, `diagnostics::benchmarkConnection`

- [ ] Move all widget creation and button state into `PracticeWindow`.
- [ ] Keep generated items, saved geometry, and connection state as members instead of heap-allocated local captures.
- [ ] Log the benchmark summary with `qInfo()` when connecting.
- [ ] Reduce `main.cpp` to `QApplication` plus `PracticeWindow`.
- [ ] Update the lifecycle test to instantiate `PracticeWindow` directly.

### Task 4: CMake And Final Verification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `tests/connect_cancel_lifecycle_test.cpp`

**Interfaces:**
- Produces: application and test targets that share algorithm, diagnostics, and UI source lists.

- [ ] Link `psapi` on Windows.
- [ ] Add all module sources to the application and test targets.
- [ ] Build both targets.
- [ ] Run algorithm, benchmark, and UI lifecycle tests repeatedly.
- [ ] Run the application with the offscreen Qt platform to verify startup.
