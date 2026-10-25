#pragma once

#include <QGraphicsView>
#include <QWheelEvent>

/**
 * @brief 支持鼠标锚点缩放和手型拖拽的图形视图。
 *
 * 缩放始终围绕鼠标所在位置进行；场景远大于视口，因此用户可以拖动平移。
 * 场景对象由外部创建，本视图不接管其所有权。
 */
class MyGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    /**
     * @brief 创建视图并启用缩放锚点及拖动模式。
     * @param scene 要显示的图形场景。
     * @param parent 可选的 Qt 父窗口。
     */
    explicit MyGraphicsView(QGraphicsScene *scene, QWidget *parent = nullptr)
        : QGraphicsView(scene, parent)
    {
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setResizeAnchor(QGraphicsView::AnchorUnderMouse);
        setDragMode(QGraphicsView::ScrollHandDrag);
    }

protected:
    /**
     * @brief 使用滚轮缩放场景，并阻止父类同时滚动滚动条。
     * @param event 当前滚轮事件。
     */
    void wheelEvent(QWheelEvent *event) override
    {
        const double scaleFactor = 1.15;
        if (event->angleDelta().y() > 0)
            scale(scaleFactor, scaleFactor);
        else
            scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        event->accept();
    }
};
