#include <QApplication>
#include <QCoreApplication>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QWidget>

// Include the real application so the test exercises its actual buttons,
// scene, lambda state, and object ownership instead of a duplicate test model.
#define main resumePracticeMain
#include "../main.cpp"
#undef main

namespace {

QPushButton *findButtonByText(QWidget *window, const QString &text)
{
    const QList<QPushButton *> buttons = window->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
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

void exerciseConnectCancelLifecycle()
{
    QWidget *window = nullptr;
    for (QWidget *candidate : QApplication::topLevelWidgets()) {
        if (candidate->findChild<QGraphicsView *>()) {
            window = candidate;
            break;
        }
    }

    auto check = [](bool condition, const char *message) {
        if (!condition)
            qCritical().noquote() << message;
        return condition;
    };

    if (!check(window != nullptr, "application window was not created")) {
        QCoreApplication::exit(1);
        return;
    }

    QGraphicsView *view = window->findChild<QGraphicsView *>();
    QGraphicsScene *scene = view ? view->scene() : nullptr;
    QSpinBox *countBox = window->findChild<QSpinBox *>();
    QPushButton *actionButton = findButtonByText(window, QStringLiteral("生成"));
    QPushButton *connectButton = findButtonByText(window, QStringLiteral("连接"));

    if (!check(scene && countBox && actionButton && connectButton,
               "required controls or scene are missing")) {
        QCoreApplication::exit(1);
        return;
    }

    countBox->setValue(2);
    actionButton->click();

    if (!check(pathItemCount(scene) == 2, "two original segments should be generated")) {
        QCoreApplication::exit(1);
        return;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 1, "connecting should replace the originals with one line")
        || !check(connectButton->text() == QStringLiteral("取消连接"),
                  "connect button should enter connected mode")) {
        QCoreApplication::exit(1);
        return;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 2,
               "cancelling should leave exactly the two original segments")) {
        QCoreApplication::exit(1);
        return;
    }

    if (!check(actionButton->isEnabled() && actionButton->text() == QStringLiteral("清除"),
               "cancelling should restore the generated-segments UI state")
        || !check(connectButton->text() == QStringLiteral("连接"),
                  "cancelling should restore the connect button")) {
        QCoreApplication::exit(1);
        return;
    }

    for (QGraphicsItem *item : scene->items()) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (!check(pathItem != nullptr && pathItem->pen().width() == 2,
                   "restored segments should keep their original pen width")) {
            QCoreApplication::exit(1);
            return;
        }
    }

    // Reconnecting without regenerating exercises the saved geometry and also
    // ensures no QGraphicsItem pointers from the previous cycle are reused.
    connectButton->click();
    if (!check(pathItemCount(scene) == 1, "cancelled originals should be connectable again")) {
        QCoreApplication::exit(1);
        return;
    }

    connectButton->click();
    if (!check(pathItemCount(scene) == 2,
               "second cancellation should restore exactly two segments")) {
        QCoreApplication::exit(1);
        return;
    }

    // A three-line case exposes the old all-pairs behavior: every original
    // point must appear exactly once in the final single chain.
    actionButton->click();
    countBox->setValue(3);
    actionButton->click();

    int originalElementCount = 0;
    for (QGraphicsItem *item : scene->items()) {
        QGraphicsPathItem *pathItem = dynamic_cast<QGraphicsPathItem *>(item);
        if (pathItem)
            originalElementCount += pathItem->path().elementCount();
    }

    if (!check(pathItemCount(scene) == 3 && originalElementCount > 0,
               "three original segments should be generated")) {
        QCoreApplication::exit(1);
        return;
    }

    connectButton->click();
    QGraphicsPathItem *connectedItem = nullptr;
    if (pathItemCount(scene) == 1)
        connectedItem = dynamic_cast<QGraphicsPathItem *>(scene->items().first());

    if (!check(connectedItem != nullptr,
               "three lines should produce one connected path")
        || !check(connectedItem->path().elementCount() == originalElementCount,
                  "the connected path should contain every original point exactly once")) {
        QCoreApplication::exit(1);
        return;
    }

    QCoreApplication::exit(0);
}

void scheduleLifecycleTest()
{
    QTimer::singleShot(0, [] {
        exerciseConnectCancelLifecycle();
    });
}

Q_COREAPP_STARTUP_FUNCTION(scheduleLifecycleTest)

} // namespace

int main(int argc, char *argv[])
{
    return resumePracticeMain(argc, argv);
}
