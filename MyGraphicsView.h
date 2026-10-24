#pragma once

#include <QGraphicsView>
#include <QWheelEvent>

class MyGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MyGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr)
        : QGraphicsView(scene, parent) 
    {
        // 设置缩放锚点为鼠标当前所在位置（以鼠标为中心进行放大缩小）
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        // 2. 开启内置的手型抓取平移模式（解决无法拖拽平移问题）
        setDragMode(QGraphicsView::ScrollHandDrag);
    }

protected:
    void wheelEvent(QWheelEvent *event) override {
        // 定义每次缩放的倍数因子
        const double scaleFactor = 1.15;

        if (event->angleDelta().y() > 0) {
            // 滚轮向上：放大
            scale(scaleFactor, scaleFactor);
        } else {
            // 滚轮向下：缩小
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }
        event->accept();
        // 注意：这里千万不要调用父类的 wheelEvent(event)，
        // 否则缩放的同时还会触发默认的滚动条滚动！
    }
};