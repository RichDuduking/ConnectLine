#pragma once

#include <QList>
#include <QPolygonF>
#include <QRectF>

namespace polyline {

/**
 * @brief 在指定矩形区域内生成一条随机折线。
 * @param area 随机点的取值范围，通常为当前视图对应的场景区域。
 * @param pointCount 折线包含的顶点数；小于等于 0 时返回空折线。
 * @return 按生成顺序排列的顶点集合。
 */
QPolygonF randomPolyline(const QRectF &area, int pointCount);

/**
 * @brief 判断候选折线是否与任意已有折线相交。
 *
 * 先使用包围盒快速排除不可能相交的线段，再执行逐边相交检测。
 *
 * @param candidate 待检测的候选折线。
 * @param existing 已经生成的折线集合。
 * @return 存在任意边相交时返回 true，否则返回 false。
 */
bool intersectsAny(const QPolygonF &candidate, const QList<QPolygonF> &existing);

/**
 * @brief 使用贪心策略将所有折线连接成一条连续链。
 *
 * 算法先选取端点距离最近的一对折线作为初始链，随后每一轮只比较
 * 当前链的头、尾与所有未使用折线的两个端点，选择最短连接继续扩展。
 * 每条非空折线最多使用一次。
 *
 * @param polylines 原始折线集合，函数不会修改输入。
 * @return 连接后的单条折线；输入为空或无可连接数据时返回空折线。
 *
 * @note 这是局部贪心算法，不保证得到全局总长度最短的连接结果。
 * @par 复杂度
 * 时间为 O(n²)，额外空间为 O(n)，其中 n 为折线数量。
 */
QPolygonF connectGreedily(const QList<QPolygonF> &polylines);

} // namespace polyline
