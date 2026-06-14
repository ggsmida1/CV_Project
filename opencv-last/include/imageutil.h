/**
 * @file imageutil.h
 * @brief 图像处理工具函数集合
 *
 * 提供跨 OpenCV/Qt 的图像格式转换、图像在 QLabel 上的自适应显示、
 * 以及基于二进制流的图像读写（支持中文路径）等通用工具函数。
 */

#ifndef IMAGEUTIL_H
#define IMAGEUTIL_H

#include <QImage>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <opencv2/opencv.hpp>

namespace ImageUtil {

/**
 * @brief OpenCV Mat → QImage 格式转换
 * @param mat 输入图像（支持 CV_8UC1、CV_8UC3、CV_8UC4）
 * @return 转换后的 QImage
 *
 * 注意：Qt 的 RGB 顺序与 OpenCV 默认的 BGR 相反，需要交换通道。
 */
QImage mat2QImage(const cv::Mat &mat);

/**
 * @brief QImage → OpenCV Mat 格式转换
 * @param image 输入图像
 * @return 转换后的 OpenCV Mat（通道顺序为 BGR/BGRA）
 */
cv::Mat QImage2Mat(const QImage &image);

/**
 * @brief 以二进制流方式读取图像（避开 OpenCV imread 的中文路径问题）
 * @param path 图像路径
 * @return 解码后的 Mat；失败返回空 Mat
 *
 * 原理：通过 QFile 读取原始字节 → 喂给 cv::imdecode 解码。
 * 由于 Qt 的文件读写 UTF-8 兼容，因此可以稳定读取中文路径。
 */
cv::Mat imreadSafe(const QString &path);

/**
 * @brief 计算图像在 Label 上缩放显示时的缩放比例和偏移
 * @param label 目标显示控件
 * @param image 原始图像
 * @param outScale 输出缩放比例
 * @param outOffsetX 输出水平偏移（图像左上角 x）
 * @param outOffsetY 输出垂直偏移（图像左上角 y）
 *
 * 用于在鼠标点击 Label 时，将点击坐标换算为原始图像像素坐标。
 */
void computeImageTransform(QLabel *label, const cv::Mat &image,
                           double &outScale, int &outOffsetX, int &outOffsetY);

/**
 * @brief 将鼠标在 Label 上的点击坐标转换为原始图像像素坐标
 * @param label 目标显示控件
 * @param image 原始图像
 * @param labelPos 鼠标在 Label 上的点击坐标
 * @return 对应图像像素坐标；若点击在图像显示区域外返回 (-1, -1)
 */
QPoint labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos);

/**
 * @brief 将 OpenCV 图像自适应显示到 QLabel 上（等比例缩放）
 * @param label 目标 QLabel
 * @param image 输入图像
 * @param placeholder 图像为空时显示的占位文本
 *
 * 缩放策略：取 min(label.width/image.width, label.height/image.height)，
 * 确保图像完整显示并居中。
 */
void displayImageOnLabel(QLabel *label, const cv::Mat &image, const QString &placeholder);

/**
 * @brief 获取 samples 目录的绝对路径
 * @param subdir 子目录名（如 "templates"、"configs" 等）
 * @return samples/subdir 的绝对路径
 *
 * 从应用运行目录向上查找，找到包含 samples 目录的项目根，
 * 然后拼接上 subdir。便于在不同运行环境下定位资源文件。
 */
QString getSamplesDir(const QString &subdir);

} // namespace ImageUtil

#endif // IMAGEUTIL_H
