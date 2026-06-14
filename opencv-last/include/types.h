/**
 * @file types.h
 * @brief 系统核心数据类型定义
 *
 * 本文件定义了系统中使用的两个核心数据结构：
 * - ROIRect: 检测区域（Region of Interest）的描述
 * - DetectionResult: 单次检测结果的记录
 */

#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QList>
#include <opencv2/opencv.hpp>

/**
 * @struct ROIRect
 * @brief 检测区域结构体
 *
 * 描述模板图像上一个待检测的矩形区域。
 * 包含区域的编号、坐标、名称以及从模板图像中截取的子图像。
 */
struct ROIRect {
    int id;                     ///< 区域编号（从1开始递增）
    cv::Rect rect;              ///< 矩形区域坐标（x, y, width, height）
    QString name;               ///< 区域名称（如"区域1"、"编号区"等）
    cv::Mat templateImage;      ///< 从模板图像中截取的该区域子图像（用于后续NCC比对）
};

/**
 * @struct DetectionResult
 * @brief 检测结果结构体
 *
 * 记录一次检测的完整结果，包含图片信息、区域信息、判定结果和时间戳。
 */
struct DetectionResult {
    int id;                     ///< 结果编号（全局递增）
    QString imageName;          ///< 待测图片的文件名
    QString roiName;            ///< 对应的检测区域名称
    bool isDefective;           ///< 是否判定为残缺（true=残缺，false=正常）
    double defectScore;         ///< 残缺程度分数（0~1，越接近1表示残缺越严重）
    QString detectionTime;      ///< 检测时间（格式：yyyy-MM-dd hh:mm:ss）
};

#endif // TYPES_H
