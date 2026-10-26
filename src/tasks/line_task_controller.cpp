#include "tasks/line_task_controller.h"

#include "algorithm/polyline_algorithms.h"

#include <QMetaObject>
#include <QPointer>
#include <QRunnable>

#include <exception>
#include <utility>

namespace {

QString exceptionMessage(const std::exception &exception)
{
    return QString::fromUtf8(exception.what());
}

} // namespace

/**
 * @brief 生成折线的后台任务。
 *
 * run() 内不访问任何 QWidget 或 QGraphicsScene，只调用纯算法函数。
 */
class GenerateLinesTask : public QRunnable {
public:
    GenerateLinesTask(LineTaskController *controller,
                      quint64 requestId,
                      const QRectF &area,
                      int lineCount,
                      int avoidStrength)
        : m_controller(controller)
        , m_requestId(requestId)
        , m_area(area)
        , m_lineCount(lineCount)
        , m_avoidStrength(avoidStrength)
    {
    }

    void run() override
    {
        QList<QPolygonF> polylines;
        QString error;

        try {
            polylines = polyline::generatePolylines(
                m_area, m_lineCount, m_avoidStrength);
        } catch (const std::exception &exception) {
            error = exceptionMessage(exception);
        } catch (...) {
            error = QStringLiteral("生成折线时发生未知异常");
        }

        if (m_controller.isNull())
            return;

        if (!error.isEmpty()) {
            LineTaskController *controller = m_controller.data();
            QMetaObject::invokeMethod(
                controller,
                [controller, requestId = m_requestId, error] {
                    controller->handleTaskFailed(requestId, error);
                },
                Qt::QueuedConnection);
            return;
        }

        LineTaskController *controller = m_controller.data();
        QMetaObject::invokeMethod(
            controller,
            [controller, requestId = m_requestId, polylines = std::move(polylines)] {
                controller->handleGenerationFinished(requestId, polylines);
            },
            Qt::QueuedConnection);
    }

private:
    QPointer<LineTaskController> m_controller;
    quint64 m_requestId = 0;
    QRectF m_area;
    int m_lineCount = 0;
    int m_avoidStrength = 0;
};

/**
 * @brief 连接折线并采集性能数据的后台任务。
 */
class ConnectLinesTask : public QRunnable {
public:
    ConnectLinesTask(LineTaskController *controller,
                     quint64 requestId,
                     const QList<QPolygonF> &polylines)
        : m_controller(controller)
        , m_requestId(requestId)
        , m_polylines(polylines)
    {
    }

    void run() override
    {
        diagnostics::ConnectionBenchmarkResult result;
        QString error;

        try {
            result = diagnostics::benchmarkConnection(m_polylines);
        } catch (const std::exception &exception) {
            error = exceptionMessage(exception);
        } catch (...) {
            error = QStringLiteral("连接折线时发生未知异常");
        }

        if (m_controller.isNull())
            return;

        if (!error.isEmpty()) {
            LineTaskController *controller = m_controller.data();
            QMetaObject::invokeMethod(
                controller,
                [controller, requestId = m_requestId, error] {
                    controller->handleTaskFailed(requestId, error);
                },
                Qt::QueuedConnection);
            return;
        }

        LineTaskController *controller = m_controller.data();
        QMetaObject::invokeMethod(
            controller,
            [controller, requestId = m_requestId, result = std::move(result)]() mutable {
                controller->handleConnectionFinished(
                    requestId, std::move(result));
            },
            Qt::QueuedConnection);
    }

private:
    QPointer<LineTaskController> m_controller;
    quint64 m_requestId = 0;
    QList<QPolygonF> m_polylines;
};

LineTaskController::LineTaskController(QObject *parent)
    : QObject(parent)
{
    // 当前界面一次只允许一种后台操作，固定单线程也便于控制状态和测量数据。
    m_pool.setMaxThreadCount(1);
}

LineTaskController::~LineTaskController()
{
    // 任务捕获的 controller 指针必须在下一次事件循环前保持有效。
    // 析构时等待当前任务结束，避免工作线程回调已经销毁的对象。
    m_pool.clear();
    m_pool.waitForDone();
}

bool LineTaskController::isBusy() const
{
    return m_busy;
}

quint64 LineTaskController::startGeneration(const QRectF &area,
                                            int lineCount,
                                            int avoidStrength)
{
    const quint64 requestId = beginTask();
    if (requestId == 0)
        return 0;

    m_pool.start(new GenerateLinesTask(
        this, requestId, area, lineCount, avoidStrength));
    return requestId;
}

quint64 LineTaskController::startConnection(
    const QList<QPolygonF> &polylines)
{
    const quint64 requestId = beginTask();
    if (requestId == 0)
        return 0;

    // 复制输入快照，确保工作线程执行期间不会与 GUI 线程共享可变容器。
    QList<QPolygonF> snapshot;
    snapshot.reserve(polylines.size());
    for (const QPolygonF &line : polylines)
        snapshot.append(QPolygonF(line.constBegin(), line.constEnd()));

    m_pool.start(new ConnectLinesTask(this, requestId, snapshot));
    return requestId;
}

quint64 LineTaskController::beginTask()
{
    if (m_busy)
        return 0;

    m_activeRequestId = ++m_nextRequestId;
    m_busy = true;
    return m_activeRequestId;
}

void LineTaskController::finishTask(quint64 requestId)
{
    if (!m_busy || requestId != m_activeRequestId)
        return;

    m_busy = false;
    m_activeRequestId = 0;
}

void LineTaskController::failTask(quint64 requestId, const QString &message)
{
    if (!m_busy || requestId != m_activeRequestId)
        return;

    finishTask(requestId);
    emit taskFailed(requestId, message);
}

void LineTaskController::handleGenerationFinished(
    quint64 requestId,
    QList<QPolygonF> polylines)
{
    if (!m_busy || requestId != m_activeRequestId)
        return;

    finishTask(requestId);
    emit generationFinished(requestId, std::move(polylines));
}

void LineTaskController::handleConnectionFinished(
    quint64 requestId,
    diagnostics::ConnectionBenchmarkResult result)
{
    if (!m_busy || requestId != m_activeRequestId)
        return;

    finishTask(requestId);
    emit connectionFinished(requestId, std::move(result));
}

void LineTaskController::handleTaskFailed(quint64 requestId,
                                          const QString &message)
{
    failTask(requestId, message);
}
