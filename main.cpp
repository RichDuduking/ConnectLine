#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QList>
#include <QPolygonF>
#include <QGraphicsScene>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPen>
#include <QColor>
#include <QRandomGenerator>
#include <QLineF>

#include <algorithm>

#include "MyGraphicsView.h"

// 在给定矩形范围内随机取一个点
static QPointF randomPointIn(const QRectF &area)
{
    const double x = area.left() + QRandomGenerator::global()->generateDouble() * area.width();
    const double y = area.top() + QRandomGenerator::global()->generateDouble() * area.height();
    return QPointF(x, y);
}

// 生成一条由 pointCount 个随机点连成的折线（2 个点即一条普通线段）
static QPolygonF randomPolyline(const QRectF &area, int pointCount)
{
    QPolygonF poly;
    for (int i = 0; i < pointCount; ++i)
        poly << randomPointIn(area);
    return poly;
}

// 将一条折线按指定方向追加到目标折线末尾。
// 首尾点都会被保留，因此两条线连接时会自然形成一条端点到端点的连接线段。
static void appendPolyline(QPolygonF &target, const QPolygonF &source, bool reverse)
{
    target.reserve(target.size() + source.size());
    if (reverse) {
        for (int i = source.size() - 1; i >= 0; --i)
            target.append(source.at(i));
    } else {
        for (const QPointF &point : source)
            target.append(point);
    }
}

// 把所有原始折线串成一条链：
// 1. 从全局最近的一对线段作为种子；
// 2. 每次只从当前链的头、尾连接最近的一条未使用线段；
// 3. 每条原始线段只追加一次，直到全部连接完成。
static QPolygonF connectPolylinesGreedily(const QList<QPolygonF> &polylines)
{
    if (polylines.isEmpty())
        return {};
    if (polylines.size() == 1)
        return polylines.first();

    int firstIndex = -1;
    int secondIndex = -1;
    int connectionType = 1;
    double shortestDistance = -1.0;

    // 选最近的一对作为初始链。连接方式沿用原来的四种端点组合。
    for (int i = 0; i < polylines.size(); ++i) {
        for (int j = i + 1; j < polylines.size(); ++j) {
            const QPolygonF &a = polylines.at(i);
            const QPolygonF &b = polylines.at(j);
            if (a.isEmpty() || b.isEmpty())
                continue;

            const double distances[] = {
                QLineF(a.first(), b.first()).length(),
                QLineF(a.first(), b.last()).length(),
                QLineF(a.last(), b.first()).length(),
                QLineF(a.last(), b.last()).length()
            };

            for (int type = 1; type <= 4; ++type) {
                if (shortestDistance < 0.0 || distances[type - 1] < shortestDistance) {
                    shortestDistance = distances[type - 1];
                    firstIndex = i;
                    secondIndex = j;
                    connectionType = type;
                }
            }
        }
    }

    if (firstIndex < 0)
        return {};

    QPolygonF chain;
    const QPolygonF &first = polylines.at(firstIndex);
    const QPolygonF &second = polylines.at(secondIndex);

    switch (connectionType) {
    case 1: // a 首连 b 首
        appendPolyline(chain, first, true);
        appendPolyline(chain, second, false);
        break;
    case 2: // a 首连 b 尾
        appendPolyline(chain, first, true);
        appendPolyline(chain, second, true);
        break;
    case 3: // a 尾连 b 首
        appendPolyline(chain, first, false);
        appendPolyline(chain, second, false);
        break;
    case 4: // a 尾连 b 尾
        appendPolyline(chain, first, false);
        appendPolyline(chain, second, true);
        break;
    default:
        return {};
    }

    //给所有图元标记是否已连接
    QList<bool> used;
    used.fill(false, polylines.size());
    used[firstIndex] = true;
    used[secondIndex] = true;
    int remaining = polylines.size() - 2;

    while (remaining > 0)
    {
      int nextIndex = -1;
        //将连线是否要连接在尾部
        bool attachAtTail = true;
        //将连线是否要反转
        bool reverseNext = false;
        double bestDistance = -1.0;

        for (int i = 0; i < polylines.size(); ++i) {
            if (used.at(i) || polylines.at(i).isEmpty())
                continue;

            const QPolygonF &candidate = polylines.at(i);
            const auto consider = [&](double distance, bool atTail, bool reverseCandidate) {
                if (bestDistance < 0.0 || distance < bestDistance) {
                    bestDistance = distance;
                    nextIndex = i;
                    attachAtTail = atTail;
                    reverseNext = reverseCandidate;
                }
            };

            consider(QLineF(chain.first(), candidate.first()).length(), false, false);
            consider(QLineF(chain.first(), candidate.last()).length(), false, true);
            consider(QLineF(chain.last(), candidate.first()).length(), true, false);
            consider(QLineF(chain.last(), candidate.last()).length(), true, true);
        }

        if (nextIndex < 0)
            break;

        // 在头部接入时，先反转整条链，再按尾部追加，避免反复从头部插入。
        if (!attachAtTail)
            std::reverse(chain.begin(), chain.end());
        appendPolyline(chain, polylines.at(nextIndex), reverseNext);
        used[nextIndex] = true;
        --remaining;
    }

    return chain;
}

// 二维叉积：判断点 a、b 相对基点 o 的方向
static double cross(const QPointF &o, const QPointF &a, const QPointF &b)
{
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
}

// 标准「跨立实验」判断两条线段是否相交
static bool segmentsIntersect(const QPointF &p1, const QPointF &p2,
                              const QPointF &q1, const QPointF &q2)
{
    const double d1 = cross(q1, q2, p1);
    const double d2 = cross(q1, q2, p2);
    const double d3 = cross(p1, p2, q1);
    const double d4 = cross(p1, p2, q2);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
           ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

// 两条折线是否有任意两条边相交
static bool polylinesIntersect(const QPolygonF &a, const QPolygonF &b)
{
    for (int i = 0; i + 1 < a.size(); ++i)
        for (int j = 0; j + 1 < b.size(); ++j)
            if (segmentsIntersect(a[i], a[i + 1], b[j], b[j + 1]))
                return true;
    return false;
}

// 候选折线是否与已有的任意一条折线相交（先用包围盒快速排除）
static bool intersectsAny(const QPolygonF &candidate, const QList<QPolygonF> &existing)
{
    const QRectF cb = candidate.boundingRect();
    for (const QPolygonF &poly : existing) {
        if (!cb.intersects(poly.boundingRect()))
            continue;
        if (polylinesIntersect(candidate, poly))
            return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 场景给一个远大于视口的矩形：保证有滚动范围，才能拖动平移 + 以鼠标为中心缩放
    QGraphicsScene *mainScene = new QGraphicsScene(-4000, -3000, 8000, 6000);

    MyGraphicsView *mainView = new MyGraphicsView(mainScene);
    mainView->setRenderHint(QPainter::Antialiasing);
    mainView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // ---------- 顶部控制栏：线条数量 + 生成/清除 按钮 + 避交强度滑块 ----------
    QSpinBox *countBox = new QSpinBox;
    countBox->setRange(1, 1000);
    countBox->setValue(5);

    QPushButton *actionBtn = new QPushButton(QStringLiteral("生成"));

    QSlider *avoidSlider = new QSlider(Qt::Horizontal);
    avoidSlider->setRange(0, 100);
    avoidSlider->setValue(50);
    avoidSlider->setFixedWidth(160);
    avoidSlider->setToolTip(QStringLiteral("越大越多线段会尽量不与其它线段相交"));

    QLabel *avoidValue = new QLabel(QStringLiteral("50%"));
    avoidValue->setMinimumWidth(40);

    QPushButton* conn = new QPushButton(QStringLiteral("连接"));

    QHBoxLayout *topBar = new QHBoxLayout;
    topBar->addWidget(new QLabel(QStringLiteral("线条数量:")));
    topBar->addWidget(countBox);
    topBar->addWidget(actionBtn);
    topBar->addSpacing(24);
    topBar->addWidget(new QLabel(QStringLiteral("避交强度:")));
    topBar->addWidget(avoidSlider);
    topBar->addWidget(avoidValue);
    topBar->addStretch();
    topBar->addWidget(new QLabel(QStringLiteral("多线段连接")));
    topBar->addWidget(conn);

    QVBoxLayout *rootLayout = new QVBoxLayout;
    rootLayout->addLayout(topBar);
    rootLayout->addWidget(mainView, 1);

    QWidget window;
    window.setWindowTitle(QStringLiteral("线段模拟器"));
    window.setLayout(rootLayout);
    window.resize(1600, 900);
    window.show();

    mainView->centerOn(0, 0);

    // 滑块数值实时显示在右侧
    QObject::connect(avoidSlider, &QSlider::valueChanged, [avoidValue](int v) {
        avoidValue->setText(QStringLiteral("%1%").arg(v));
    });

    // ---------- 生成 / 清除 切换逻辑 ----------
    // generated：已生成的图元，便于一键清除
    // polylines：每条线的几何数据（用于判断相交）
    // isGenerated：当前按钮状态
    QList<QGraphicsItem *> *generated = new QList<QGraphicsItem *>;
    QList<QPolygonF> *polylines = new QList<QPolygonF>;
    bool *isGenerated = new bool(false);

    QObject::connect(actionBtn, &QPushButton::clicked, [=]() {
        if (!*isGenerated) {
            // 在当前可见的（场景）范围内随机生成
            const QRectF area = mainView->mapToScene(mainView->viewport()->rect()).boundingRect();
            const int lineCount = countBox->value();
            const int avoidStrength = avoidSlider->value(); // 0..100

            for (int i = 0; i < lineCount; ++i) {
                const int pointCount = QRandomGenerator::global()->bounded(2, 7);

                // 以 avoidStrength% 的概率，这一条线会「尽量避开」已有线段
                const bool tryAvoid =
                    QRandomGenerator::global()->bounded(100) < avoidStrength;

                QPolygonF poly = randomPolyline(area, pointCount);
                if (tryAvoid) {
                    const int maxAttempts = 100; // 重试上限，避免死循环
                    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
                        if (!intersectsAny(poly, *polylines))
                            break; // 已是一条不与任何已有线段相交的线，采用
                        poly = randomPolyline(area, pointCount); // 否则重新随机再试
                    }
                }

                // 用 moveTo/lineTo 画成「开放折线」，避免 addPolygon 可能产生的闭合边
                QPainterPath path;
                path.moveTo(poly.first());
                for (int k = 1; k < poly.size(); ++k)
                    path.lineTo(poly.at(k));

                const QColor color =
                    QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 230);
                generated->append(mainScene->addPath(path, QPen(color, 2)));
                polylines->append(poly);
            }

            *isGenerated = true;
            actionBtn->setText(QStringLiteral("清除"));
        } else {
            // 清除场景内生成的所有线段
            for (QGraphicsItem *item : *generated) {
                mainScene->removeItem(item);
                delete item;
            }
            generated->clear();
            polylines->clear();
            *isGenerated = false;
            actionBtn->setText(QStringLiteral("生成"));
        }
    });

    bool* isConnect = new bool(false);
    QPolygonF *finalPolyline = new QPolygonF;
    // 连接会清空场景，因此要保存独立的图元副本和原始几何。
    // 取消连接时两者都恢复，显示内容和后续操作状态才能一致。
    QList<QGraphicsPathItem *> *tmpGenerated = new QList<QGraphicsPathItem *>;
    QList<QPolygonF> *tmpPolylines = new QList<QPolygonF>;
    QObject::connect(conn, &QPushButton::clicked, [=]() {
        if (!*isConnect)
        {
            // 没有可连接线段时保持当前状态，不能只切换按钮文字。
            if (polylines->empty() || generated->empty())
                return;

            // 先取最近的一对作为种子，之后每次只扩展当前链的最近端点。
            // 这里只遍历候选线段，不再遍历所有线段组合，因此每条线只使用一次。
            *finalPolyline = connectPolylinesGreedily(*polylines);

            if (finalPolyline->isEmpty())
                return;

            // 场景清空会销毁原图元，所以快照必须是独立对象。
            // 画笔也要复制，否则取消连接后颜色和线宽会变成默认值。
            tmpGenerated->clear();
            tmpPolylines->clear();
            *tmpPolylines = *polylines;
            for (QGraphicsItem *item : *generated)
            {
                QGraphicsPathItem *_poly = dynamic_cast<QGraphicsPathItem *>(item);
                if (_poly) {
                    QGraphicsPathItem *poly =
                        new QGraphicsPathItem(_poly->path());
                    poly->setPen(_poly->pen());
                    poly->setBrush(_poly->brush());
                    tmpGenerated->append(poly);
                }
            }

            mainScene->clear();
            polylines->clear();
            generated->clear();
            // 用 moveTo/lineTo 画成「开放折线」，避免 addPolygon 可能产生的闭合边
            if (!finalPolyline->isEmpty())
            {
                QPainterPath path;
                path.moveTo((*finalPolyline).first());
                for (int k = 1; k < (*finalPolyline).size(); ++k)
                    path.lineTo((*finalPolyline).at(k));

                const QColor color =
                    QColor::fromHsv(QRandomGenerator::global()->bounded(360), 200, 230);
                generated->append(mainScene->addPath(path, QPen(color, 2)));
                polylines->append((*finalPolyline));
            }
            *isConnect = true;
            // 连接模式禁止生成/清除，避免两种模式同时改写场景内容。
            actionBtn->setEnabled(false);
            conn->setText(QStringLiteral("取消连接"));

        }
        else
        {
            // 取消连接时先删除连接结果，只保留快照中的原始线段。
            finalPolyline->clear();
            polylines->clear();
            for (QGraphicsItem *item : *generated)
            {
                mainScene->removeItem(item);
                delete item;
            }
            generated->clear();

            // 恢复的图元和几何要重新纳入当前状态，否则下一次清除或连接会漏掉它们。
            for (QGraphicsPathItem *item : *tmpGenerated)
            {
                mainScene->addItem(item);
                generated->append(item);
            }
            *polylines = *tmpPolylines;

            // 图元已交给场景管理；清空快照，避免下一轮连接复用旧地址。
            tmpGenerated->clear();
            tmpPolylines->clear();

            *isConnect = false;
            *isGenerated = !generated->isEmpty();
            actionBtn->setEnabled(true);
            actionBtn->setText(*isGenerated
                ? QStringLiteral("清除")
                : QStringLiteral("生成"));
            conn->setText(QStringLiteral("连接"));
        }
    });

    return a.exec();
}
