#pragma once

#include <QList>
#include <QPolygonF>
#include <QString>
#include <QtGlobal>

namespace diagnostics {

/**
 * @brief 一次折线连接执行的结果与性能快照。
 *
 * 内存数据来自当前进程的 Windows 内存计数器，因此包含 Qt、分配器以及
 * 程序其他部分的少量背景噪声。负值表示对应指标不可用；差值可以为负数，
 * 表示调用结束时内存低于调用开始时。
 */
struct ConnectionBenchmarkResult {
    /// 连接算法生成的完整折线，可直接用于后续界面绘制。
    QPolygonF connectedPolyline;

    /// 仅包裹连接算法调用的单调时钟耗时，单位为纳秒。
    qint64 elapsedNanoseconds = -1;

    /// 算法调用前后的进程私有内存。
    qint64 privateBytesBefore = -1;
    qint64 privateBytesAfter = -1;
    qint64 privateBytesDelta = 0;

    /// 算法调用前后的进程工作集。
    qint64 workingSetBytesBefore = -1;
    qint64 workingSetBytesAfter = -1;
    qint64 workingSetDelta = 0;

    /// 操作系统记录的进程峰值工作集，不是单次算法调用的峰值。
    qint64 peakWorkingSetBytes = -1;

    /// 当前平台和系统调用是否成功提供了内存指标。
    bool memoryMetricsAvailable = false;

    /**
     * @brief 生成适合日志或工具提示显示的单行摘要。
     * @return 包含耗时和可用内存指标的本地化文本。
     */
    QString summary() const;
};

/**
 * @brief 执行连接算法并同时采集耗时和进程内存快照。
 *
 * 计时范围只包含 polyline::connectGreedily()；内存快照在算法前后各采集一次。
 * 在 Windows 上使用 GetProcessMemoryInfo，其他平台仍返回连接结果和耗时，
 * 但将 memoryMetricsAvailable 设为 false。
 *
 * @param polylines 待连接的原始折线集合。
 * @return 连接结果以及本次检测得到的性能数据。
 */
ConnectionBenchmarkResult benchmarkConnection(const QList<QPolygonF> &polylines);

} // namespace diagnostics
