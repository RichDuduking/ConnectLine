#include "ui/practice_window.h"

#include "algorithm/polyline_algorithms.h"
#include "diagnostics/connection_benchmark.h"
#include "ui/my_graphics_view.h"

#include <QColor>
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

    setWindowTitle(QStringLiteral("线段模拟器"));
    resize(1600, 900);
    m_view->centerOn(0, 0);
}

void PracticeWindow::toggleGeneration()
{
    // 连接期间按钮通常已禁用，此检查用于防御外部代码直接触发该方法。
    if (m_connected)
        return;

    if (m_generated)
        clearPolylines();
    else
        generatePolylines();
}

void PracticeWindow::generatePolylines()
{
    const QRectF area =
        m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
    const int lineCount = m_countBox->value();
    const int avoidStrength = m_avoidSlider->value();

    for (int i = 0; i < lineCount; ++i) {
        const int pointCount = QRandomGenerator::global()->bounded(2, 7);
        const bool tryAvoid =
            QRandomGenerator::global()->bounded(100) < avoidStrength;

        QPolygonF candidate = polyline::randomPolyline(area, pointCount);
        if (tryAvoid) {
            // 重试次数必须有上限，避免高密度场景中无限寻找不相交位置。
            const int maxAttempts = 100;
            for (int attempt = 0; attempt < maxAttempts; ++attempt) {
                if (!polyline::intersectsAny(candidate, m_polylines))
                    break;
                candidate = polyline::randomPolyline(area, pointCount);
            }
        }

        m_generatedItems.append(addPolylineItem(candidate));
        m_polylines.append(candidate);
    }

    m_generated = true;
    m_actionButton->setText(QStringLiteral("清除"));
}

void PracticeWindow::clearPolylines()
{
    deleteGeneratedItems();
    m_polylines.clear();
    m_generated = false;
    m_actionButton->setText(QStringLiteral("生成"));
}

void PracticeWindow::connectPolylines()
{
    if (m_polylines.isEmpty() || m_generatedItems.isEmpty())
        return;

    // 检测函数只包裹算法调用，场景替换和绘制不计入 elapsedNanoseconds。
    const diagnostics::ConnectionBenchmarkResult benchmark =
        diagnostics::benchmarkConnection(m_polylines);
    qInfo().noquote() << QStringLiteral("连接性能: %1").arg(benchmark.summary());
    m_connectButton->setToolTip(benchmark.summary());

    m_finalPolyline = benchmark.connectedPolyline;
    if (m_finalPolyline.isEmpty())
        return;

    m_savedItems.clear();
    m_savedPolylines = m_polylines;

    // 仅复制路径和可见画笔属性；副本脱离场景后可在取消连接时重新加入。
    for (QGraphicsItem *item : m_generatedItems) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (!pathItem)
            continue;

        auto *copy = new QGraphicsPathItem(pathItem->path());
        copy->setPen(pathItem->pen());
        copy->setBrush(pathItem->brush());
        m_savedItems.append(copy);
    }

    // clear() 会销毁原图元，已经脱离场景的副本不受影响。
    m_scene->clear();
    m_generatedItems.clear();
    m_polylines.clear();

    m_generatedItems.append(addPolylineItem(m_finalPolyline));
    m_polylines.append(m_finalPolyline);

    m_connected = true;
    m_actionButton->setEnabled(false);
    m_connectButton->setText(QStringLiteral("取消连接"));
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
    m_actionButton->setEnabled(true);
    m_actionButton->setText(
        m_generated ? QStringLiteral("清除") : QStringLiteral("生成"));
    m_connectButton->setText(QStringLiteral("连接"));
}

void PracticeWindow::toggleConnection()
{
    if (m_connected)
        disconnectPolylines();
    else
        connectPolylines();
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
