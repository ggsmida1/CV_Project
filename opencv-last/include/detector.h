/**
 * @file detector.h
 * @brief 检测核心算法模块
 *
 * 本文件定义了字符残缺检测系统的核心算法：
 * 1. 倾斜矫正（Skew Detection & Correction）
 * 2. 模板匹配定位（Template Matching）
 * 3. ROI精对齐（ROI Alignment）
 * 4. 字符残缺检测（Character Defect Detection）
 */

#ifndef DETECTOR_H
#define DETECTOR_H

#include <opencv2/opencv.hpp>

namespace Detector {

/**
 * @struct SkewConstants
 * @brief 倾斜矫正模块的常量参数集合
 *
 * 集中管理倾斜检测相关的参数，便于后期调优。
 */
struct SkewConstants {
    static constexpr double MIN_ANGLE = 1.0;          ///< 最小矫正角度（度），小于此值不进行矫正，避免图像质量损失
    static constexpr double MAX_LINE_ANGLE = 45.0;     ///< 最大有效直线角度（度），过滤掉过于倾斜的异常直线
    static constexpr int GAUSS_KERNEL = 5;              ///< 高斯模糊核大小，用于去噪
    static constexpr double CANNY_LOW = 50.0;           ///< Canny边缘检测低阈值
    static constexpr double CANNY_HIGH = 150.0;         ///< Canny边缘检测高阈值
    static constexpr int HOUGH_THRESHOLD = 100;         ///< 霍夫变换直线检测阈值（累计点数）
    static constexpr int HOUGH_MAX_GAP = 20;            ///< 霍夫变换允许的最大线段间隙（像素）
};

/**
 * @struct DefectConstants
 * @brief 残缺检测模块的常量参数集合
 *
 * 集中管理残缺检测相关的参数，便于后期调优。
 */
struct DefectConstants {
    static constexpr double NCC_THRESHOLD = 0.85;       ///< NCC相似度阈值（低于此值视为残缺）
    static constexpr double DEFECT_SCORE_THRESHOLD = 0.15; ///< 残缺分数阈值（defectScore = 1-NCC，大于此值视为残缺）
    static constexpr int GAUSS_KERNEL = 3;              ///< 残缺检测前高斯模糊核大小
};

/**
 * @brief 检测图像倾斜并进行旋转矫正
 * @param image 输入图像，同时作为矫正后的输出（原地修改）
 * @return 矫正角度（度），0.0 表示无需矫正
 *
 * 处理流程：
 * 1. 灰度化 → 高斯模糊去噪
 * 2. Canny边缘检测
 * 3. HoughLinesP提取直线
 * 4. 计算所有直线角度的中位数（对离群值鲁棒）
 * 5. 角度绝对值 > MIN_ANGLE 时执行 warpAffine 旋转，自动扩展画布避免裁剪
 */
double detectSkewAndCorrect(cv::Mat &image);

/**
 * @brief 在待测图像中搜索模板的最佳匹配位置
 * @param testImage 待测图像
 * @param templateImage 模板图像
 * @return 模板左上角的最佳匹配点坐标
 *
 * 使用归一化互相关（TM_CCOEFF_NORMED）进行全图搜索，
 * 对光照和对比度变化不敏感，适合工业检测场景。
 */
cv::Point templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage);

/**
 * @brief ROI精对齐：在粗定位基础上做局部窗口的精细匹配
 * @param testImage 待测图像
 * @param roiTemplate 该ROI的模板子图像
 * @param roughMatchPoint 粗定位的匹配点（模板匹配输出）
 * @param roiRect ROI在模板中的相对位置
 * @param outAlignedRect 输出精对齐后的ROI坐标
 * @return 对齐后从待测图像中裁剪的ROI子图像
 *
 * 搜索窗口大小：min(30, roiWidth/3, roiHeight/3)，即围绕粗定位中心做局部搜索。
 * 若搜索窗口过小或失败，则回退至粗定位位置。
 */
cv::Mat alignROI(const cv::Mat &testImage, const cv::Mat &roiTemplate,
                 const cv::Point &roughMatchPoint, const cv::Rect &roiRect,
                 cv::Rect &outAlignedRect);

/**
 * @brief 对单个ROI进行字符残缺检测
 * @param testRegion 待测图像中对齐后的ROI子图
 * @param templateRegion 模板图像中对应的ROI子图
 * @param defectScore 输出残缺程度分数（0~1，越接近1越残缺）
 * @return true 表示检测到残缺，false 表示正常
 *
 * 处理流程：
 * 1. 空值校验（任一为空直接返回残缺）
 * 2. 统一转灰度图
 * 3. resize 对齐到模板尺寸
 * 4. 高斯模糊去噪（压制轻微差异）
 * 5. NCC相似度比对，取 maxVal
 * 6. defectScore = 1 - maxVal，与阈值比较输出判定结果
 */
bool detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore);

} // namespace Detector

#endif // DETECTOR_H
