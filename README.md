# ResumePractice

Qt 6 单链贪心连接演示程序。

## 目录结构

```text
src/
  main.cpp                    程序入口
  algorithm/
    polyline_algorithms.*     随机折线、相交判断、贪心连接
  diagnostics/
    connection_benchmark.*    耗时与进程内存检测
  tasks/
    line_task_controller.*    QRunnable 后台生成/连接任务
  ui/
    practice_window.*         窗口、按钮状态、场景图元生命周期
    my_graphics_view.h        缩放与平移视图
tests/
  polyline_algorithms_test.cpp      算法与检测模块测试
  line_task_controller_test.cpp     QRunnable 线程归属与完成信号测试
  connect_cancel_lifecycle_test.cpp UI 连接/取消生命周期测试
  ui_performance_probe.cpp          指定线数的生成/连接/重绘性能探针
```

## 性能检测

折线生成和连接都通过 `QRunnable` 提交到单线程 `QThreadPool`。工作线程只处理
纯几何数据和 `diagnostics::benchmarkConnection()`，QWidget、QGraphicsScene
及图元更新始终在 GUI 线程执行。

当原始线段超过 20 条时，视图会自动关闭抗锯齿，避免大量跨屏线段在软件光栅化
阶段占满 GUI 线程；低密度场景仍保留抗锯齿效果。

连接时记录算法耗时和 Windows 进程内存快照，并通过 `qInfo()` 输出：

- 算法耗时
- 私有内存调用前后变化
- 工作集调用前后变化
- 操作系统记录的进程峰值工作集

内存数据是进程级观测值，包含 Qt 分配器和运行环境的少量噪声。

## 构建与测试

```powershell
cmake -S . -B builds/Debug -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH=E:/Qt6.8.3/6.8.3/mingw_64
cmake --build builds/Debug
ctest --test-dir builds/Debug --output-on-failure
```

高密度性能探针可指定线数，例如 500 条：

```powershell
.\builds\Debug\ResumePracticeUiPerformanceProbe.exe 500
```
