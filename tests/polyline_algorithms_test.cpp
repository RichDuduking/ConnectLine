#include "algorithm/polyline_algorithms.h"
#include "diagnostics/connection_benchmark.h"

#include <QDebug>
#include <QPolygonF>

/**
 * @brief 验证确定性连接结果，并检查性能检测返回值的基本约定。
 *
 * 使用三条手工构造的折线，确保最近连接顺序唯一且结果可精确比较。
 */
int main()
{
    const QList<QPolygonF> input = {
        QPolygonF{QPointF(0, 0), QPointF(10, 0)},
        QPolygonF{QPointF(10, 1), QPointF(20, 1)},
        QPolygonF{QPointF(20, 2), QPointF(30, 2)}
    };

    const QPolygonF expected = {
        QPointF(0, 0),
        QPointF(10, 0),
        QPointF(10, 1),
        QPointF(20, 1),
        QPointF(20, 2),
        QPointF(30, 2)
    };

    const QPolygonF actual = polyline::connectGreedily(input);
    if (actual != expected) {
        qCritical() << "unexpected connected chain" << actual;
        return 1;
    }

    const diagnostics::ConnectionBenchmarkResult benchmark =
        diagnostics::benchmarkConnection(input);
    if (benchmark.connectedPolyline != expected) {
        qCritical() << "benchmark returned a different connected chain";
        return 1;
    }
    if (benchmark.elapsedNanoseconds < 0) {
        qCritical() << "benchmark did not report elapsed time";
        return 1;
    }
    if (!benchmark.memoryMetricsAvailable) {
        qCritical() << "Windows process memory metrics should be available";
        return 1;
    }
    if (benchmark.privateBytesBefore <= 0 || benchmark.privateBytesAfter <= 0
        || benchmark.workingSetBytesBefore <= 0
        || benchmark.workingSetBytesAfter <= 0) {
        qCritical() << "benchmark returned invalid process memory counters";
        return 1;
    }
    if (benchmark.privateBytesDelta
            != benchmark.privateBytesAfter - benchmark.privateBytesBefore
        || benchmark.workingSetDelta
            != benchmark.workingSetBytesAfter - benchmark.workingSetBytesBefore) {
        qCritical() << "benchmark memory deltas do not match the snapshots";
        return 1;
    }

    const QList<QPolygonF> generated =
        polyline::generatePolylines(QRectF(0, 0, 100, 100), 4, 50);
    if (generated.size() != 4) {
        qCritical() << "generation should return the requested number of lines";
        return 1;
    }
    for (const QPolygonF &line : generated) {
        if (line.size() < 2 || line.size() > 6) {
            qCritical() << "generated line has an invalid point count";
            return 1;
        }
    }

    return 0;
}
