#include "ui/practice_window.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>

#include <functional>

namespace {

/**
 * @brief 按按钮当前文本在窗口子树中查找按钮。
 */
QPushButton *findButtonByText(QWidget *window, const QString &text)
{
    const QList<QPushButton *> buttons = window->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

/**
 * @brief 统计场景中的 QGraphicsPathItem 数量。
 */
int pathItemCount(QGraphicsScene *scene)
{
    int count = 0;
    for (QGraphicsItem *item : scene->items()) {
        if (dynamic_cast<QGraphicsPathItem *>(item))
            ++count;
    }
    return count;
}

/**
 * @brief 输出失败信息并返回断言结果。
 */
bool check(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

/**
 * @brief 在不冻结 Qt 事件循环的情况下等待后台任务完成。
 */
bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();

    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

/**
 * @brief 覆盖生成、连接、取消、再次连接以及三线段最终链的行为。
 *
 * 测试直接构造真实的 PracticeWindow，并点击真实按钮，避免复制生产状态逻辑。
 */
bool exerciseConnectCancelLifecycle(PracticeWindow &window)
{
    QGraphicsView *view = window.findChild<QGraphicsView *>();
    QGraphicsScene *scene = view ? view->scene() : nullptr;
    QSpinBox *countBox = window.findChild<QSpinBox *>();
    QPushButton *actionButton = findButtonByText(&window, QStringLiteral("生成"));
    QPushButton *connectButton = findButtonByText(&window, QStringLiteral("连接"));

    if (!check(scene && countBox && actionButton && connectButton,
               "required controls or scene are missing")) {
        return false;
    }

    countBox->setValue(2);
    actionButton->click();

    if (!check(pathItemCount(scene) == 0 && !actionButton->isEnabled(),
               "generation should start asynchronously")
        || !waitUntil([&] {
               return pathItemCount(scene) == 2 && actionButton->isEnabled();
           })) {
        qCritical() << "asynchronous generation did not complete";
        return false;
    }
    if (!check(view->renderHints().testFlag(QPainter::Antialiasing),
               "low-density scenes should keep antialiasing")) {
        return false;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 2 && !connectButton->isEnabled(),
               "connection should start asynchronously")
        || !waitUntil([&] {
               return pathItemCount(scene) == 1
                   && connectButton->text() == QStringLiteral("取消连接");
           })
        || !check(connectButton->toolTip().contains(QStringLiteral("连接耗时")),
                  "connect button should expose the benchmark summary")) {
        qCritical() << "asynchronous connection did not complete";
        return false;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 2,
               "cancelling should leave exactly the two original segments")) {
        return false;
    }

    if (!check(actionButton->isEnabled() && actionButton->text() == QStringLiteral("清除"),
               "cancelling should restore the generated-segments UI state")
        || !check(connectButton->text() == QStringLiteral("连接"),
                  "cancelling should restore the connect button")) {
        return false;
    }

    for (QGraphicsItem *item : scene->items()) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (!check(pathItem != nullptr && pathItem->pen().width() == 2,
                   "restored segments should keep their original pen width")) {
            return false;
        }
    }

    connectButton->click();
    if (!waitUntil([&] { return pathItemCount(scene) == 1; })
        || !check(connectButton->text() == QStringLiteral("取消连接"),
                  "cancelled originals should be connectable again")) {
        return false;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 2,
               "second cancellation should restore exactly two segments")) {
        return false;
    }

    // 三条线可以捕获“重新遍历所有线段组合”的回归。
    actionButton->click();
    countBox->setValue(3);
    actionButton->click();

    if (!waitUntil([&] {
            return pathItemCount(scene) == 3 && actionButton->isEnabled();
        })) {
        qCritical() << "second asynchronous generation did not complete";
        return false;
    }

    int originalElementCount = 0;
    for (QGraphicsItem *item : scene->items()) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem)
            originalElementCount += pathItem->path().elementCount();
    }

    if (!check(pathItemCount(scene) == 3 && originalElementCount > 0,
               "three original segments should be generated")) {
        return false;
    }

    connectButton->click();
    if (!waitUntil([&] { return pathItemCount(scene) == 1; })) {
        qCritical() << "final asynchronous connection did not complete";
        return false;
    }

    QGraphicsPathItem *connectedItem = nullptr;
    if (pathItemCount(scene) == 1)
        connectedItem = dynamic_cast<QGraphicsPathItem *>(scene->items().first());

    if (!check(connectedItem != nullptr,
               "three lines should produce one connected path")
        || !check(connectedItem->path().elementCount() == originalElementCount,
                  "the connected path should contain every original point exactly once")) {
        return false;
    }

    // 高密度场景关闭抗锯齿，避免大量跨屏线段占用数秒进行光栅化。
    connectButton->click();
    actionButton->click();
    countBox->setValue(500);
    actionButton->click();

    if (!waitUntil([&] {
            return pathItemCount(scene) == 500 && actionButton->isEnabled();
        }, 10000)) {
        qCritical() << "500-line generation did not complete";
        return false;
    }

    if (!check(!view->renderHints().testFlag(QPainter::Antialiasing),
               "high-density scenes should disable antialiasing")) {
        return false;
    }

    connectButton->click();
    if (!waitUntil([&] {
            return connectButton->text() == QStringLiteral("取消连接");
        }, 10000)) {
        qCritical() << "500-line connection did not complete";
        return false;
    }

    return check(!view->renderHints().testFlag(QPainter::Antialiasing),
                 "connected high-density scenes should stay responsive");
}

} // namespace

/**
 * @brief 在离屏 Qt 环境中运行窗口生命周期测试。
 */
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    PracticeWindow window;
    window.show();
    application.processEvents();

    return exerciseConnectCancelLifecycle(window) ? 0 : 1;
}
