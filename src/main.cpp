#include "ui/practice_window.h"

#include <QApplication>

/**
 * @brief 程序入口：创建 Qt 应用和主窗口，进入事件循环。
 */
int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    PracticeWindow window;
    window.show();
    return application.exec();
}
