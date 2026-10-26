# QRunnable Async Generation And Connection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move polyline generation/avoidance and connection/benchmark work to background `QRunnable` tasks so the Qt GUI thread remains responsive.

**Architecture:** Add a task controller that owns a single-thread `QThreadPool` and exposes request IDs plus queued completion signals. Keep all scene and widget changes in `PracticeWindow`, and perform only pure data work inside runnable tasks.

**Tech Stack:** C++17, Qt 6 Widgets/Core, `QThreadPool`, `QRunnable`, CMake, CTest.

## Global Constraints

- `QRunnable::run()` must not access QWidget, QGraphicsScene, QGraphicsItem, or PracticeWindow.
- Only one background generation/connection task may be active at a time.
- Scene updates occur only in the GUI thread after a queued completion signal.
- Cancellation may ignore stale results, but must not access a destroyed receiver.
- Preserve the greedy connection result and existing connection/cancel behavior.
- Do not commit unless explicitly requested.

---

### Task 1: Pure Generation Function

**Files:**
- Modify: `src/algorithm/polyline_algorithms.h`
- Modify: `src/algorithm/polyline_algorithms.cpp`
- Modify: `tests/polyline_algorithms_test.cpp`

**Interfaces:**
- Produces: `QList<QPolygonF> polyline::generatePolylines(const QRectF &area, int lineCount, int avoidStrength, int maxAttempts = 100);`

- [x] Write a test that requests four lines and verifies the count and valid point counts.
- [x] Run the test and verify it fails because the function is missing.
- [x] Implement generation with a task-local random generator and the existing intersection helper.
- [x] Run the test and verify it passes.

### Task 2: QRunnable Task Controller

**Files:**
- Create: `src/tasks/line_task_controller.h`
- Create: `src/tasks/line_task_controller.cpp`
- Create: `tests/line_task_controller_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `quint64 LineTaskController::startGeneration(const QRectF &, int, int);`
- Produces: `quint64 LineTaskController::startConnection(const QList<QPolygonF> &);`
- Produces signals: `generationFinished`, `connectionFinished`, `taskFailed`.

- [x] Write a test that starts a generation task and waits for a queued completion signal.
- [x] Run the test and verify it fails because the controller is missing.
- [x] Implement plain `QRunnable` subclasses and a single-thread `QThreadPool`.
- [x] Use a queued object signal to marshal results back to the GUI thread.
- [x] Run the test and verify it passes.

### Task 3: Asynchronous PracticeWindow

**Files:**
- Modify: `src/ui/practice_window.h`
- Modify: `src/ui/practice_window.cpp`
- Modify: `tests/connect_cancel_lifecycle_test.cpp`

**Interfaces:**
- Consumes: `LineTaskController::startGeneration`
- Consumes: `LineTaskController::startConnection`
- Consumes: `generationFinished`, `connectionFinished`, `taskFailed`

- [x] Update the lifecycle test to wait for asynchronous completion.
- [x] Verify the updated test fails against the current synchronous window.
- [x] Replace synchronous generation and connection calls with task requests.
- [x] Apply results in GUI-thread handlers and restore controls on failure.
- [x] Run the lifecycle test repeatedly and verify it passes.

### Task 4: CMake And Final Verification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `README.md`

**Interfaces:**
- Produces application and test targets containing the task controller module.

- [x] Add task controller sources to application and UI test targets.
- [x] Build all targets.
- [x] Run all three test suites repeatedly.
- [x] Start the application with the offscreen Qt platform and confirm it remains running.
