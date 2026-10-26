#include "tasks/line_task_controller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QList>
#include <QPolygonF>
#include <QThread>
#include <QTimer>

namespace {

/**
 * @brief 等待控制器信号完成，超时返回 false。
 */
bool waitForSignal(QEventLoop &loop)
{
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(5000);
    loop.exec();
    return timer.isActive();
}

} // namespace

/**
 * @brief 验证生成和连接任务均通过排队信号返回结果。
 */
int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    LineTaskController controller;
    QThread *mainThread = application.thread();

    QList<QPolygonF> generatedLines;
    bool generationResultOnMainThread = false;
    QEventLoop generationLoop;
    QObject::connect(
        &controller,
        &LineTaskController::generationFinished,
        &generationLoop,
        [&](quint64, QList<QPolygonF> lines) {
            generationResultOnMainThread =
                QThread::currentThread() == mainThread;
            generatedLines = std::move(lines);
            generationLoop.quit();
        });

    if (controller.startGeneration(QRectF(0, 0, 200, 200), 3, 0) == 0) {
        qCritical() << "generation task did not start";
        return 1;
    }
    if (!controller.isBusy() || !waitForSignal(generationLoop)) {
        qCritical() << "generation task did not finish in time";
        return 1;
    }
    if (generatedLines.size() != 3 || controller.isBusy()
        || !generationResultOnMainThread) {
        qCritical() << "generation result or busy state is invalid";
        return 1;
    }

    diagnostics::ConnectionBenchmarkResult connectionResult;
    bool connectionResultOnMainThread = false;
    QEventLoop connectionLoop;
    QObject::connect(
        &controller,
        &LineTaskController::connectionFinished,
        &connectionLoop,
        [&](quint64, diagnostics::ConnectionBenchmarkResult result) {
            connectionResultOnMainThread =
                QThread::currentThread() == mainThread;
            connectionResult = std::move(result);
            connectionLoop.quit();
        });

    if (controller.startConnection(generatedLines) == 0) {
        qCritical() << "connection task did not start";
        return 1;
    }
    if (!controller.isBusy() || !waitForSignal(connectionLoop)) {
        qCritical() << "connection task did not finish in time";
        return 1;
    }
    if (connectionResult.connectedPolyline.isEmpty() || controller.isBusy()
        || !connectionResultOnMainThread) {
        qCritical() << "connection result or busy state is invalid";
        return 1;
    }

    return 0;
}
