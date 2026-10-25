#include "diagnostics/connection_benchmark.h"

#include "algorithm/polyline_algorithms.h"

#include <QElapsedTimer>

#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace {

/**
 * @brief 一次进程级内存快照。
 */
struct ProcessMemorySnapshot {
    qint64 privateBytes = -1;
    qint64 workingSetBytes = -1;
    qint64 peakWorkingSetBytes = -1;
    bool available = false;
};

/**
 * @brief 读取当前进程的私有内存、工作集和峰值工作集。
 *
 * 非 Windows 平台返回 available == false，不伪造任何内存值。
 */
ProcessMemorySnapshot captureProcessMemory()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
            sizeof(counters))) {
        return {};
    }

    ProcessMemorySnapshot snapshot;
    snapshot.privateBytes = static_cast<qint64>(counters.PrivateUsage);
    snapshot.workingSetBytes = static_cast<qint64>(counters.WorkingSetSize);
    snapshot.peakWorkingSetBytes = static_cast<qint64>(counters.PeakWorkingSetSize);
    snapshot.available = true;
    return snapshot;
#else
    return {};
#endif
}

/**
 * @brief 将字节数格式化为两位小数的 MiB 文本。
 */
QString formatBytes(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("不可用");

    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MiB").arg(mib, 0, 'f', 2);
}

/**
 * @brief 将内存差值格式化为带正负号的 MiB 文本。
 */
QString formatDelta(qint64 bytes)
{
    const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    const QString sign = bytes >= 0 ? QStringLiteral("+") : QString();
    return QStringLiteral("%1%2 MiB").arg(sign).arg(mib, 0, 'f', 2);
}

} // namespace

namespace diagnostics {

// 公共接口的完整约定位于 connection_benchmark.h。
ConnectionBenchmarkResult benchmarkConnection(const QList<QPolygonF> &polylines)
{
    // 快照必须位于算法调用两侧，返回的 connectedPolyline 已计入 after。
    const ProcessMemorySnapshot before = captureProcessMemory();

    QElapsedTimer timer;
    timer.start();
    QPolygonF connectedPolyline = polyline::connectGreedily(polylines);
    const qint64 elapsedNanoseconds = timer.nsecsElapsed();

    const ProcessMemorySnapshot after = captureProcessMemory();

    ConnectionBenchmarkResult result;
    result.connectedPolyline = std::move(connectedPolyline);
    result.elapsedNanoseconds = elapsedNanoseconds;
    result.privateBytesBefore = before.privateBytes;
    result.privateBytesAfter = after.privateBytes;
    result.workingSetBytesBefore = before.workingSetBytes;
    result.workingSetBytesAfter = after.workingSetBytes;
    result.peakWorkingSetBytes = after.peakWorkingSetBytes;
    result.memoryMetricsAvailable = before.available && after.available;

    if (result.memoryMetricsAvailable) {
        // 快照是同一进程内的绝对字节数，差值可能因为清理或系统波动而为负。
        result.privateBytesDelta = after.privateBytes - before.privateBytes;
        result.workingSetDelta = after.workingSetBytes - before.workingSetBytes;
    }

    return result;
}

QString ConnectionBenchmarkResult::summary() const
{
    const QString elapsed =
        QStringLiteral("%1 ms").arg(elapsedNanoseconds / 1'000'000.0, 0, 'f', 3);

    // 非 Windows 平台或系统读取失败时，只展示始终可靠的耗时。
    if (!memoryMetricsAvailable) {
        return QStringLiteral("连接耗时: %1 | 内存: 当前平台不可用").arg(elapsed);
    }

    return QStringLiteral(
               "连接耗时: %1 | 私有内存: %2 -> %3 (%4) | "
               "工作集: %5 -> %6 (%7) | 进程峰值工作集: %8")
        .arg(elapsed)
        .arg(formatBytes(privateBytesBefore))
        .arg(formatBytes(privateBytesAfter))
        .arg(formatDelta(privateBytesDelta))
        .arg(formatBytes(workingSetBytesBefore))
        .arg(formatBytes(workingSetBytesAfter))
        .arg(formatDelta(workingSetDelta))
        .arg(formatBytes(peakWorkingSetBytes));
}

} // namespace diagnostics
