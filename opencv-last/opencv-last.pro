QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = CharacterDefectDetection
TEMPLATE = app

# 允许使用 C++11 特性
CONFIG += c++11

# 构建输出目录
DESTDIR = $$PWD/bin
OBJECTS_DIR = $$PWD/build/obj
MOC_DIR = $$PWD/build/moc
RCC_DIR = $$PWD/build/rcc
UI_DIR = $$PWD/build/ui

# 源文件
SOURCES += \
    main.cpp \
    mainwindow.cpp

# 头文件
HEADERS += \
    mainwindow.h

# UI文件
FORMS += \
    mainwindow.ui

# OpenCV 配置（使用环境变量）
OPENCV_DIR = $$(OPENCV_DIR)
isEmpty(OPENCV_DIR) {
    # 默认路径（你的本地配置）
    OPENCV_DIR = E:/software-e/opencv-4.10.0/build_mingw/install
}
INCLUDEPATH += $$OPENCV_DIR/include
LIBS += -L$$OPENCV_DIR/x64/mingw/lib -lopencv_world4100