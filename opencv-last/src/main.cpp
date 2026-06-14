/**
 * @file main.cpp
 * @brief 应用程序入口
 *
 * 流程：创建 QApplication → 加载样式文件（QSS） → 创建 MainWindow → 显示 → 进入事件循环
 */

#include "mainwindow.h"
#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    // --- 步骤1: 创建 Qt 应用程序对象
    QApplication a(argc, argv);

    // --- 步骤2: 加载 QSS 样式文件（通过 Qt 资源系统，":/style.qss" 指向资源中的文件）
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        a.setStyleSheet(styleSheet);   // 将样式应用到整个应用
        styleFile.close();
    }

    // --- 步骤3: 创建主窗口并显示
    MainWindow w;
    w.show();

    // --- 步骤4: 进入 Qt 事件循环（exec() 直到窗口关闭才返回）
    return a.exec();
}
