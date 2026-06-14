/**
 * @file configmanager.h
 * @brief 配置文件管理模块
 *
 * 负责将模板图像和 ROI 信息保存为 JSON 配置文件，
 * 以及从 JSON 文件中加载配置还原到界面。
 * 配置文件是"自包含"的——将 ROI 子图像以 base64 编码嵌入 JSON，
 * 使配置文件可以脱离原始图片独立迁移使用。
 */

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QList>
#include <opencv2/opencv.hpp>
#include "types.h"

namespace ConfigManager {

/**
 * @struct ConfigData
 * @brief 配置数据结构体
 *
 * 用于从 JSON 文件加载后输出还原的配置信息。
 */
struct ConfigData {
    cv::Mat templateImage;          ///< 模板图像
    QString templatePath;           ///< 模板图像路径
    QList<ROIRect> roiList;         ///< ROI 列表
};

/**
 * @brief 将当前配置保存为 JSON 文件
 * @param path 保存文件路径
 * @param templateImage 模板图像
 * @param templateImagePath 模板图像路径（保存相对路径，便于迁移）
 * @param roiList ROI 列表
 * @return true 保存成功，false 保存失败
 *
 * JSON 结构：
 * {
 *   "templatePath": "templates/身份证背部.jpg",
 *   "templateWidth": 800,
 *   "templateHeight": 500,
 *   "roiList": [
 *     {
 *       "id": 1, "name": "区域1",
 *       "x": 100, "y": 200, "width": 160, "height": 40,
 *       "templateImage": "iVBORw0KGgoAAAA..." (base64 编码 PNG)
 *     },
 *     ...
 *   ]
 * }
 */
bool save(const QString &path, const cv::Mat &templateImage,
          const QString &templateImagePath, const QList<ROIRect> &roiList);

/**
 * @brief 从 JSON 文件加载配置
 * @param path 配置文件路径
 * @param outData 输出配置数据
 * @return true 加载成功，false 加载失败
 *
 * 加载过程：
 * 1. 读取 JSON 文件 → 解析 QJsonDocument
 * 2. 根据 templatePath 还原模板图像（支持 samples 目录下的相对路径）
 * 3. 遍历 roiList 数组 → base64 解码还原每个 ROI 的子图像
 * 4. 填充 ConfigData 输出到调用者
 */
bool load(const QString &path, ConfigData &outData);

} // namespace ConfigManager

#endif // CONFIGMANAGER_H
