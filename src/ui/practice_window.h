#pragma once

#include <QList>
#include <QPolygonF>
#include <QWidget>

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsScene;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;
class MyGraphicsView;

/**
 * @brief 单链连接演示主窗口。
 *
 * 类负责创建场景和控制栏、维护生成/连接状态，并协调算法与性能检测模块。
 * 场景中的图元由 QGraphicsScene 持有；连接期间为了清空场景，会额外保存
 * 一组脱离场景的原始线段副本，用于取消连接时恢复。
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
    /// 创建场景、控制栏、视图以及信号连接。
    void buildUi();

    /// 在当前可见场景范围内生成一组随机折线。
    void generatePolylines();

    /// 删除当前生成结果并清空几何数据，回到未生成状态。
    void clearPolylines();

    /// “生成/清除”按钮的统一入口。
    void toggleGeneration();

    /// “连接/取消连接”按钮的统一入口。
    void toggleConnection();

    /// 运行连接算法并把结果替换为场景中的单条折线。
    void connectPolylines();

    /// 删除连接结果并恢复连接前的原始线段和几何数据。
    void disconnectPolylines();

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

    // 当前显示在场景中的折线图元与对应几何数据。
    QList<QGraphicsItem *> m_generatedItems;
    QList<QPolygonF> m_polylines;

    // 连接会清空场景，因此用独立副本保存连接前状态。
    // m_savedItems 在连接期间脱离场景，由本类负责销毁；恢复后转交场景管理。
    QList<QGraphicsPathItem *> m_savedItems;
    QList<QPolygonF> m_savedPolylines;
    QPolygonF m_finalPolyline;

    // 两个状态互斥：连接期间禁止生成/清除，避免两套数据同时修改场景。
    bool m_generated = false;
    bool m_connected = false;
};
