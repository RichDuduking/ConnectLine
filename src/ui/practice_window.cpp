#include "ui/practice_window.h"

#include "algorithm/polyline_algorithms.h"
#include "diagnostics/connection_benchmark.h"
#include "tasks/line_task_controller.h"
#include "ui/my_graphics_view.h"

#include <QColor>
#include <QDebug>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// 高密度场景中，大量跨屏线段在抗锯齿下会产生极高的光栅化成本。
// 超过该数量后优先保证交互和重绘响应，关闭抗锯齿。
constexpr int kAntialiasingLineLimit = 20;

} // namespace

PracticeWindow::PracticeWindow(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

PracticeWindow::~PracticeWindow()
{
    // 连接结果显示期间，保存的原始图元不在场景中，需要由窗口主动释放。
    qDeleteAll(m_savedItems);
}

// 所有界面对象都指定了父对象或布局归属，生命周期跟随窗口。
void PracticeWindow::buildUi()
{
    m_scene = new QGraphicsScene(-4000, -3000, 8000, 6000, this);

    m_view = new MyGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_countBox = new QSpinBox(this);
    m_countBox->setRange(1, 1000);
    m_countBox->setValue(5);

    m_actionButton = new QPushButton(QStringLiteral("生成"), this);

    m_avoidSlider = new QSlider(Qt::Horizontal, this);
    m_avoidSlider->setRange(0, 100);
    m_avoidSlider->setValue(50);
    m_avoidSlider->setFixedWidth(160);
    m_avoidSlider->setToolTip(QStringLiteral("越大越多线段会尽量不与其它线段相交"));

    m_avoidValue = new QLabel(QStringLiteral("50%"), this);
    m_avoidValue->setMinimumWidth(40);

    m_connectButton = new QPushButton(QStringLiteral("连接"), this);
    m_taskController = new LineTaskController(this);

    auto *topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel(QStringLiteral("线条数量:"), this));
    topBar->addWidget(m_countBox);
    topBar->addWidget(m_actionButton);
    topBar->addSpacing(24);
    topBar->addWidget(new QLabel(QStringLiteral("避交强度:"), this));
    topBar->addWidget(m_avoidSlider);
    topBar->addWidget(m_avoidValue);
    topBar->addStretch();
    topBar->addWidget(new QLabel(QStringLiteral("多线段连接"), this));
    topBar->addWidget(m_connectButton);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(topBar);
    rootLayout->addWidget(m_view, 1);

    // 控件事件只改变窗口状态，具体场景操作集中在私有方法中。
    connect(m_avoidSlider, &QSlider::valueChanged, this, [this](int value) {
        m_avoidValue->setText(QStringLiteral("%1%").arg(value));
    });
    connect(m_actionButton, &QPushButton::clicked, this, &PracticeWindow::toggleGeneration);
    connect(m_connectButton, &QPushButton::clicked, this, &PracticeWindow::toggleConnection);
    connect(m_taskController,
            &LineTaskController::generationFinished,
            this,
            [this](quint64 requestId, QList<QPolygonF> polylines) {
                applyGeneratedPolylines(requestId, polylines);
            });
    connect(m_taskController,
            &LineTaskController::connectionFinished,
            this,
            [this](quint64 requestId,
                   diagnostics::ConnectionBenchmarkResult result) {
                applyConnectionResult(requestId, result);
            });
    connect(m_taskController,
            &LineTaskController::taskFailed,
            this,
            &PracticeWindow::handleTaskFailed);

    setWindowTitle(QStringLiteral("线段模拟器"));
    resize(1600, 900);
    m_view->centerOn(0, 0);
    updateControlState();
}

void PracticeWindow::toggleGeneration()
{
    // 连接或后台任务期间按钮通常已禁用，此检查用于防御外部调用。
    if (m_connected || m_taskOperation != TaskOperation::Idle)
        return;

    if (m_generated)
        clearPolylines();
    else
        generatePolylines();
}

void PracticeWindow::generatePolylines()
{
    if (m_taskOperation != TaskOperation::Idle || m_connected)
        return;

    const QRectF area =
        m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
    const int lineCount = m_countBox->value();
    const int avoidStrength = m_avoidSlider->value();

    m_taskOperation = TaskOperation::Generating;
    updateControlState();

    m_activeRequestId = m_taskController->startGeneration(
        area, lineCount, avoidStrength);
    if (m_activeRequestId == 0) {
        m_taskOperation = TaskOperation::Idle;
        updateControlState();
    }
}

void PracticeWindow::clearPolylines()
{
    deleteGeneratedItems();
    m_polylines.clear();
    m_generated = false;
    updateControlState();
}

void PracticeWindow::connectPolylines()
{
    if (m_taskOperation != TaskOperation::Idle
        || m_polylines.isEmpty()
        || m_generatedItems.isEmpty()) {
        return;
    }

    m_taskOperation = TaskOperation::Connecting;
    updateControlState();

    m_activeRequestId = m_taskController->startConnection(m_polylines);
    if (m_activeRequestId == 0) {
        m_taskOperation = TaskOperation::Idle;
        updateControlState();
    }
}

void PracticeWindow::applyGeneratedPolylines(
    quint64 requestId,
    const QList<QPolygonF> &polylines)
{
    if (requestId != m_activeRequestId
        || m_taskOperation != TaskOperation::Generating) {
        return;
    }

    if (polylines.isEmpty()) {
        handleTaskFailed(requestId, QStringLiteral("生成任务未返回任何折线"));
        return;
    }

    for (const QPolygonF &polyline : polylines) {
        m_generatedItems.append(addPolylineItem(polyline));
        m_polylines.append(polyline);
    }

    m_generated = true;
    m_taskOperation = TaskOperation::Idle;
    m_activeRequestId = 0;
    updateControlState();
}

void PracticeWindow::applyConnectionResult(
    quint64 requestId,
    const diagnostics::ConnectionBenchmarkResult &benchmark)
{
    if (requestId != m_activeRequestId
        || m_taskOperation != TaskOperation::Connecting) {
        return;
    }

    // 后台任务只计算数据；工具提示和场景替换必须回到 GUI 线程执行。
    qInfo().noquote() << QStringLiteral("连接性能: %1").arg(benchmark.summary());
    m_connectButton->setToolTip(benchmark.summary());

    m_finalPolyline = benchmark.connectedPolyline;
    if (m_finalPolyline.isEmpty()) {
        handleTaskFailed(requestId, QStringLiteral("连接任务未返回有效折线"));
        return;
    }

    m_savedItems.clear();
    m_savedPolylines = m_polylines;

    // 暂时从场景摘下原图元，既保留全部属性，也避免在 GUI 线程复制路径数据。
    for (QGraphicsItem *item : m_generatedItems) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (!pathItem)
            continue;

        m_scene->removeItem(pathItem);
        m_savedItems.append(pathItem);
    }

    // 原始线段已经脱离场景；clear() 不会销毁它们。
    m_scene->clear();
    m_generatedItems.clear();
    m_polylines.clear();

    m_generatedItems.append(addPolylineItem(m_finalPolyline));
    m_polylines.append(m_finalPolyline);

    m_connected = true;
    m_taskOperation = TaskOperation::Idle;
    m_activeRequestId = 0;
    updateControlState();
}

void PracticeWindow::disconnectPolylines()
{
    // 先移除当前连接结果，再把连接前的独立副本恢复为正常生成状态。
    m_finalPolyline.clear();
    m_polylines.clear();
    deleteGeneratedItems();

    for (QGraphicsPathItem *item : m_savedItems) {
        m_scene->addItem(item);
        m_generatedItems.append(item);
    }
    // 图元已转交场景管理，清空列表即可，不能在这里删除。
    m_savedItems.clear();

    m_polylines = m_savedPolylines;
    m_savedPolylines.clear();

    m_connected = false;
    m_generated = !m_generatedItems.isEmpty();
    updateControlState();
}

void PracticeWindow::toggleConnection()
{
    if (m_taskOperation != TaskOperation::Idle)
        return;

    if (m_connected)
        disconnectPolylines();
    else
        connectPolylines();
}

void PracticeWindow::handleTaskFailed(quint64 requestId, const QString &message)
{
    if (requestId != m_activeRequestId)
        return;

    qWarning().noquote() << QStringLiteral("后台任务失败: %1").arg(message);
    m_connectButton->setToolTip(
        QStringLiteral("上次后台任务失败: %1").arg(message));
    m_taskOperation = TaskOperation::Idle;
    m_activeRequestId = 0;
    updateControlState();
}

void PracticeWindow::updateControlState()
{
    const bool idle = m_taskOperation == TaskOperation::Idle;

    m_countBox->setEnabled(idle);
    m_avoidSlider->setEnabled(idle);
    m_actionButton->setEnabled(idle && !m_connected);
    m_connectButton->setEnabled(idle);

    if (m_taskOperation == TaskOperation::Generating) {
        m_actionButton->setText(QStringLiteral("生成中..."));
        m_connectButton->setText(QStringLiteral("连接"));
    } else if (m_taskOperation == TaskOperation::Connecting) {
        m_actionButton->setText(
            m_generated ? QStringLiteral("清除") : QStringLiteral("生成"));
        m_connectButton->setText(QStringLiteral("连接中..."));
    } else {
        m_actionButton->setText(
            m_generated ? QStringLiteral("清除") : QStringLiteral("生成"));
        m_connectButton->setText(
            m_connected ? QStringLiteral("取消连接") : QStringLiteral("连接"));
    }

    updateRenderQuality();
}

void PracticeWindow::updateRenderQuality()
{
    // 连接后 m_polylines 只保留一条结果线，复杂度仍应按连接前的原始线数判断。
    const int sourceLineCount =
        m_connected ? m_savedPolylines.size() : m_polylines.size();
    const bool antialiasingEnabled = sourceLineCount <= kAntialiasingLineLimit;
    m_view->setRenderHint(QPainter::Antialiasing, antialiasingEnabled);
}

void PracticeWindow::deleteGeneratedItems()
{
    // 只有仍在场景中的图元才需要先 removeItem，随后统一 delete。
    for (QGraphicsItem *item : m_generatedItems) {
        if (item->scene() == m_scene)
            m_scene->removeItem(item);
        delete item;
    }
    m_generatedItems.clear();
}

QGraphicsPathItem *PracticeWindow::addPolylineItem(const QPolygonF &polyline)
{
    // 显式 moveTo/lineTo 生成开放折线，避免闭合图形产生额外边。
    QPainterPath path;
    if (!polyline.isEmpty()) {
        path.moveTo(polyline.first());
        for (int i = 1; i < polyline.size(); ++i)
            path.lineTo(polyline.at(i));
    }

    const QColor color =
        QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 230);
    return m_scene->addPath(path, QPen(color, 2));
}
