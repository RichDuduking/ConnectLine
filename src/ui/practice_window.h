#pragma once

#include <QList>
#include <QPolygonF>
#include <QString>
#include <QWidget>
#include <QtGlobal>

namespace diagnostics {
struct ConnectionBenchmarkResult;
}

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsScene;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class LineTaskController;
class MyGraphicsView;

/**
 * @brief 单链连接演示主窗口。
 *
 * 类负责创建场景和控制栏、维护生成/连接状态，并协调算法与性能检测模块。
 * 场景中的图元由 QGraphicsScene 持有；连接期间会把原始线段图元暂时从
 * 场景摘下并保存在本类中，用于取消连接时原样恢复。
 */
class PracticeWindow : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief 创建窗口并构建全部界面控件。
     * @param parent 可选的 Qt 父窗口。
     */
    explicit PracticeWindow(QWidget *parent = nullptr);

    /**
     * @brief 销毁窗口并释放仍处于脱离场景状态的原始线段副本。
     */
    ~PracticeWindow() override;

private:
    /**
     * @brief GUI 线程当前等待的后台操作类型。
     */
    enum class TaskOperation {
        Idle,
        Generating,
        Connecting
    };

    /// 创建场景、控制栏、视图以及信号连接。
    void buildUi();

    /// 提交后台生成任务；函数本身立即返回。
    void generatePolylines();

    /// 删除当前生成结果并清空几何数据，回到未生成状态。
    void clearPolylines();

    /// “生成/清除”按钮的统一入口。
    void toggleGeneration();

    /// “连接/取消连接”按钮的统一入口。
    void toggleConnection();

    /// 提交后台连接任务；函数本身立即返回。
    void connectPolylines();

    /// 删除连接结果并恢复连接前的原始线段和几何数据。
    void disconnectPolylines();

    /// 在 GUI 线程把后台生成的折线转换成场景图元。
    void applyGeneratedPolylines(quint64 requestId,
                                 const QList<QPolygonF> &polylines);

    /// 在 GUI 线程应用后台连接和性能检测结果。
    void applyConnectionResult(
        quint64 requestId,
        const diagnostics::ConnectionBenchmarkResult &result);

    /// 后台任务失败时恢复控件状态并记录错误。
    void handleTaskFailed(quint64 requestId, const QString &message);

    /// 根据当前任务和连接状态同步所有控件的可用性与文字。
    void updateControlState();

    /// 根据原始线段规模切换抗锯齿，避免高密度场景重绘占用主线程。
    void updateRenderQuality();

    /// 从场景移除并删除 m_generatedItems 中的全部图元。
    void deleteGeneratedItems();

    /**
     * @brief 把几何折线转换成带随机颜色的 QGraphicsPathItem 并加入场景。
     * @param polyline 要绘制的折线。
     * @return 新加入场景的路径图元，所有权归场景。
     */
    QGraphicsPathItem *addPolylineItem(const QPolygonF &polyline);

    // 由本窗口或布局管理的界面对象。
    QGraphicsScene *m_scene = nullptr;
    MyGraphicsView *m_view = nullptr;
    QSpinBox *m_countBox = nullptr;
    QPushButton *m_actionButton = nullptr;
    QSlider *m_avoidSlider = nullptr;
    QLabel *m_avoidValue = nullptr;
    QPushButton *m_connectButton = nullptr;
    LineTaskController *m_taskController = nullptr;

    // 当前显示在场景中的折线图元与对应几何数据。
    QList<QGraphicsItem *> m_generatedItems;
    QList<QPolygonF> m_polylines;

    // 连接会清空场景，因此暂时摘下原始图元并保存连接前几何。
    // m_savedItems 在连接期间脱离场景，由本类负责销毁；恢复后转交场景管理。
    QList<QGraphicsPathItem *> m_savedItems;
    QList<QPolygonF> m_savedPolylines;
    QPolygonF m_finalPolyline;

    // 两个状态互斥：连接期间禁止生成/清除，避免两套数据同时修改场景。
    bool m_generated = false;
    bool m_connected = false;
    TaskOperation m_taskOperation = TaskOperation::Idle;
    quint64 m_activeRequestId = 0;
};
