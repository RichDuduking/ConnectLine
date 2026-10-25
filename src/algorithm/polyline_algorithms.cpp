#include "algorithm/polyline_algorithms.h"

#include <QLineF>
#include <QPointF>
#include <QRandomGenerator>

#include <algorithm>

namespace {

/**
 * @brief 在矩形区域内随机选择一个点。
 */
QPointF randomPointIn(const QRectF &area)
{
    const double x =
        area.left() + QRandomGenerator::global()->generateDouble() * area.width();
    const double y =
        area.top() + QRandomGenerator::global()->generateDouble() * area.height();
    return QPointF(x, y);
}

/**
 * @brief 将 source 按正序或逆序追加到 target 末尾。
 *
 * 两条折线的连接线段由 target 的最后一个点和 source 的第一个追加点形成。
 */
void appendPolyline(QPolygonF &target, const QPolygonF &source, bool reverse)
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

/**
 * @brief 计算向量 OA 与 OB 的二维叉积。
 */
double cross(const QPointF &origin, const QPointF &a, const QPointF &b)
{
    return (a.x() - origin.x()) * (b.y() - origin.y())
        - (a.y() - origin.y()) * (b.x() - origin.x());
}

/**
 * @brief 使用严格跨立实验判断两条线段是否相交。
 *
 * 当前实现不把仅在端点接触的线段视为相交，与原有的避交逻辑保持一致。
 */
bool segmentsIntersect(const QPointF &p1, const QPointF &p2,
                       const QPointF &q1, const QPointF &q2)
{
    const double d1 = cross(q1, q2, p1);
    const double d2 = cross(q1, q2, p2);
    const double d3 = cross(p1, p2, q1);
    const double d4 = cross(p1, p2, q2);
    return ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0))
        && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0));
}

/**
 * @brief 检查两条折线是否存在任意一对相交的边。
 */
bool polylinesIntersect(const QPolygonF &a, const QPolygonF &b)
{
    for (int i = 0; i + 1 < a.size(); ++i) {
        for (int j = 0; j + 1 < b.size(); ++j) {
            if (segmentsIntersect(a.at(i), a.at(i + 1), b.at(j), b.at(j + 1)))
                return true;
        }
    }
    return false;
}

} // namespace

namespace polyline {

// 公共接口的完整说明位于 polyline_algorithms.h。
QPolygonF randomPolyline(const QRectF &area, int pointCount)
{
    QPolygonF result;
    for (int i = 0; i < pointCount; ++i)
        result.append(randomPointIn(area));
    return result;
}

bool intersectsAny(const QPolygonF &candidate, const QList<QPolygonF> &existing)
{
    const QRectF candidateBounds = candidate.boundingRect();
    for (const QPolygonF &line : existing) {
        if (!candidateBounds.intersects(line.boundingRect()))
            continue;
        if (polylinesIntersect(candidate, line))
            return true;
    }
    return false;
}

QPolygonF connectGreedily(const QList<QPolygonF> &polylines)
{
    if (polylines.isEmpty())
        return {};
    if (polylines.size() == 1)
        return polylines.first();

    int firstIndex = -1;
    int secondIndex = -1;
    int connectionType = 1;
    double shortestDistance = -1.0;

    // 先扫描所有折线组合，选择端点距离最近的一对作为初始链。
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

    QList<bool> used;
    used.fill(false, polylines.size());
    used[firstIndex] = true;
    used[secondIndex] = true;
    int remaining = polylines.size() - 2;

    // 后续只从当前链的两个端点寻找最近连接，已使用折线不再参与。
    while (remaining > 0) {
        int nextIndex = -1;
        bool attachAtTail = true;
        bool reverseNext = false;
        double bestDistance = -1.0;

        for (int i = 0; i < polylines.size(); ++i) {
            if (used.at(i) || polylines.at(i).isEmpty())
                continue;

            const QPolygonF &candidate = polylines.at(i);

            // 四次比较分别对应“链头/链尾”与“候选线头/尾”的组合。
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

        // 连接到链头时先反转整条链，再统一追加到尾部。
        if (!attachAtTail)
            std::reverse(chain.begin(), chain.end());
        appendPolyline(chain, polylines.at(nextIndex), reverseNext);
        used[nextIndex] = true;
        --remaining;
    }

    return chain;
}

} // namespace polyline
