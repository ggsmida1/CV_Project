/**
 * @file imageutil.cpp
 * @brief 图像处理工具函数实现
 */

#include "imageutil.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <algorithm>

namespace ImageUtil {

/**
 * @brief OpenCV Mat → QImage 格式转换
 *
 * 根据图像通道数分别处理：
 * - 1 通道（灰度图）：直接拷贝数据
 * - 3 通道（BGR）：转成 RGB 后拷贝
 * - 4 通道（BGRA）：转成 RGBA 后拷贝
 *
 * 注意：QImage::Format_RGB888 使用 R-G-B 顺序存储，
 * 而 OpenCV 默认是 B-G-R，因此需要交换通道。
 */
QImage mat2QImage(const cv::Mat &mat)
{
    // 空矩阵直接返回空 QImage
    if (mat.empty()) return QImage();

    // --- 情况1: 单通道灰度图
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows,
                      static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    }
    // --- 情况2: 三通道彩色图（OpenCV 默认 BGR 顺序）
    else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);   // 通道交换：BGR → RGB
        return QImage(rgb.data, rgb.cols, rgb.rows,
                      static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    // --- 情况3: 四通道带 Alpha 的图像
    else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA); // 通道交换：BGRA → RGBA
        return QImage(rgba.data, rgba.cols, rgba.rows,
                      static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    return QImage();
}

/**
 * @brief QImage → OpenCV Mat 格式转换
 */
cv::Mat QImage2Mat(const QImage &image)
{
    if (image.isNull()) return cv::Mat();
    cv::Mat mat;
    switch (image.format()) {
    // --- 单通道灰度图：直接按字节拷贝
    case QImage::Format_Grayscale8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1,
                      const_cast<uchar *>(image.bits()), image.bytesPerLine()).clone();
        break;
    // --- 三通道 RGB：转成 BGR 供 OpenCV 使用
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3,
                      const_cast<uchar *>(image.bits()), image.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        break;
    // --- 四通道 RGBA：转成 BGRA 供 OpenCV 使用
    case QImage::Format_RGBA8888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4,
                      const_cast<uchar *>(image.bits()), image.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);
        break;
    // --- 其他格式：统一转为 RGB888 后再处理
    default: {
        QImage converted = image.convertToFormat(QImage::Format_RGB888);
        mat = cv::Mat(converted.height(), converted.width(), CV_8UC3,
                      const_cast<uchar *>(converted.bits()), converted.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        break;
    }
    }
    return mat;
}

/**
 * @brief 以二进制流方式读取图像
 *
 * 解决 Windows 下 OpenCV imread 对中文路径的兼容性问题。
 * 流程：QFile 以只读方式读字节 → cv::imdecode 解码。
 */
cv::Mat imreadSafe(const QString &path)
{
    // 以只读方式打开文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return cv::Mat();

    // 一次性读入所有字节
    QByteArray data = file.readAll();
    file.close();
    if (data.isEmpty()) return cv::Mat();

    // 将字节喂给 cv::imdecode（强制以彩色模式解码）
    std::vector<uchar> buffer(data.begin(), data.end());
    return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

/**
 * @brief 计算图像在 Label 上的缩放比例和偏移
 *
 * 图像等比例缩放显示时，需要将鼠标点击的 Label 坐标
 * 换算回原始图像坐标。
 */
void computeImageTransform(QLabel *label, const cv::Mat &image,
                           double &outScale, int &outOffsetX, int &outOffsetY)
{
    // 空控件/空图像返回默认值 1/0/0
    if (!label || image.empty()) {
        outScale = 1.0; outOffsetX = 0; outOffsetY = 0;
        return;
    }
    QSize labelSize = label->size();

    // 缩放比例 = min(Label 宽/图像宽, Label 高/图像高)
    // 确保图像完整显示而不是被裁剪
    double scale = std::min(
        static_cast<double>(labelSize.width()) / image.cols,
        static_cast<double>(labelSize.height()) / image.rows);
    outScale = scale;

    // 偏移使图像在 Label 中居中显示
    outOffsetX = (labelSize.width() - static_cast<int>(image.cols * scale)) / 2;
    outOffsetY = (labelSize.height() - static_cast<int>(image.rows * scale)) / 2;
}

/**
 * @brief Label 坐标 → 图像像素坐标
 *
 * 反向缩放：先减去偏移，再除以缩放比例。
 */
QPoint labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos)
{
    double scale = 1.0;
    int offsetX = 0, offsetY = 0;
    computeImageTransform(label, image, scale, offsetX, offsetY);

    // 反向换算：(labelPos - offset) / scale
    double imgX = (labelPos.x() - offsetX) / scale;
    double imgY = (labelPos.y() - offsetY) / scale;

    // 检查是否在图像范围内（范围外返回无效坐标 -1,-1）
    if (imgX < 0 || imgY < 0 || imgX >= image.cols || imgY >= image.rows)
        return QPoint(-1, -1);
    return QPoint(static_cast<int>(imgX), static_cast<int>(imgY));
}

/**
 * @brief 将 OpenCV 图像自适应显示到 QLabel
 *
 * 流程：等比例缩放 → Mat→QImage → QPixmap::fromImage → setPixmap
 */
void displayImageOnLabel(QLabel *label, const cv::Mat &image, const QString &placeholder)
{
    // 图像为空显示占位文本
    if (image.empty()) {
        label->setText(placeholder);
        return;
    }
    QSize labelSize = label->size();
    cv::Mat display;
    // 等比例缩放（INTER_AREA 用于缩小时效果更佳）
    double scale = std::min(
        static_cast<double>(labelSize.width()) / image.cols,
        static_cast<double>(labelSize.height()) / image.rows);
    cv::resize(image, display, cv::Size(), scale, scale, cv::INTER_AREA);

    // Mat → QImage → QPixmap，最后显示到 Label
    QImage qImg = mat2QImage(display);
    label->setPixmap(QPixmap::fromImage(qImg));
}

/**
 * @brief 获取 samples 目录的绝对路径
 *
 * 从应用可执行文件目录（Qt Creator 中是构建目录）
 * 向上查找 samples 目录。找到后再拼上子目录名。
 */
QString getSamplesDir(const QString &subdir)
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    // 若当前目录下无 samples，则尝试上一层
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    return projectRoot + "/samples/" + subdir;
}

} // namespace ImageUtil
