#pragma once

#include "diagnostics/connection_benchmark.h"

#include <QList>
#include <QObject>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QThreadPool>
#include <QtGlobal>

class GenerateLinesTask;
class ConnectLinesTask;

/**
 * @brief 管理折线生成和连接后台任务。
 *
 * 控制器拥有一个单线程 QThreadPool，并保证同一时间最多执行一个任务。
 * QRunnable 只处理纯数据；任务完成后通过排队调用回到本对象所在线程，
 * 再从 GUI 线程发出公共完成信号。
 */
class LineTaskController : public QObject {
    Q_OBJECT
public:
    explicit LineTaskController(QObject *parent = nullptr);
    ~LineTaskController() override;

    /**
     * @brief 当前是否有后台任务正在执行。
     */
    bool isBusy() const;

    /**
     * @brief 启动随机折线生成任务。
     * @return 非零请求 ID；已有任务运行时返回 0。
     */
    quint64 startGeneration(const QRectF &area, int lineCount, int avoidStrength);

    /**
     * @brief 启动折线连接和性能检测任务。
     * @return 非零请求 ID；已有任务运行时返回 0。
     */
    quint64 startConnection(const QList<QPolygonF> &polylines);

signals:
    /// 折线生成完成；信号在控制器所在线程发出。
    void generationFinished(quint64 requestId, QList<QPolygonF> polylines);

    /// 连接和性能检测完成；信号在控制器所在线程发出。
    void connectionFinished(quint64 requestId,
                            diagnostics::ConnectionBenchmarkResult result);

    /// 后台任务发生异常或无法完成。
    void taskFailed(quint64 requestId, QString message);

private:
    quint64 beginTask();
    void finishTask(quint64 requestId);
    void failTask(quint64 requestId, const QString &message);
    void handleGenerationFinished(quint64 requestId, QList<QPolygonF> polylines);
    void handleConnectionFinished(
        quint64 requestId,
        diagnostics::ConnectionBenchmarkResult result);
    void handleTaskFailed(quint64 requestId, const QString &message);

    QThreadPool m_pool;
    quint64 m_nextRequestId = 0;
    quint64 m_activeRequestId = 0;
    bool m_busy = false;

    friend class GenerateLinesTask;
    friend class ConnectLinesTask;
};
