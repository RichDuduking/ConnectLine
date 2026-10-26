#include "ui/practice_window.h"

#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QThread>

#include <functional>

namespace {

QPushButton *findButton(QWidget *window, const QString &text)
{
    for (QPushButton *button : window->findChildren<QPushButton *>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

int pathItemCount(QGraphicsScene *scene)
{
    int count = 0;
    for (QGraphicsItem *item : scene->items()) {
        if (dynamic_cast<QGraphicsPathItem *>(item))
            ++count;
    }
    return count;
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 30000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return condition();
}

qint64 renderTime(QGraphicsView *view)
{
    QElapsedTimer timer;
    timer.start();
    view->viewport()->grab();
    return timer.nsecsElapsed();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    PracticeWindow window;
    window.show();
    application.processEvents();

    auto *view = window.findChild<QGraphicsView *>();
    auto *countBox = window.findChild<QSpinBox *>();
    auto *slider = window.findChild<QSlider *>();
    auto *actionButton = findButton(&window, QStringLiteral("生成"));
    auto *connectButton = findButton(&window, QStringLiteral("连接"));

    if (!view || !countBox || !slider || !actionButton || !connectButton)
        return 1;

    const int requestedLines =
        argc > 1 ? QByteArray(argv[1]).toInt() : 500;
    const int lineCount = requestedLines > 0 ? requestedLines : 500;

    countBox->setValue(lineCount);
    slider->setValue(50);

    QElapsedTimer totalTimer;
    totalTimer.start();
    actionButton->click();
    const bool generated = waitUntil([&] {
        return pathItemCount(view->scene()) == lineCount && actionButton->isEnabled();
    });
    const qint64 generationTotal = totalTimer.nsecsElapsed();
    const qint64 generationRender = renderTime(view);

    totalTimer.restart();
    connectButton->click();
    const bool connected = waitUntil([&] {
        return connectButton->text() == QStringLiteral("取消连接");
    });
    const qint64 connectionTotal = totalTimer.nsecsElapsed();
    const qint64 connectionRender = renderTime(view);

    qInfo().noquote()
        << QStringLiteral(
               "%1 lines | generated=%2 | generation=%3 ms | "
               "generation render=%4 ms | connected=%5 | connection=%6 ms | "
               "connection render=%7 ms | final items=%8")
               .arg(lineCount)
               .arg(generated)
               .arg(generationTotal / 1'000'000.0, 0, 'f', 2)
               .arg(generationRender / 1'000'000.0, 0, 'f', 2)
               .arg(connected)
               .arg(connectionTotal / 1'000'000.0, 0, 'f', 2)
               .arg(connectionRender / 1'000'000.0, 0, 'f', 2)
               .arg(pathItemCount(view->scene()));

    return generated && connected ? 0 : 1;
}
