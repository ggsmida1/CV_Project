/**
 * @file detector.cpp
 * @brief 检测核心算法实现
 */

#include "detector.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Detector {

/**
 * @brief 检测图像倾斜并进行旋转矫正
 *
 * 使用 Canny 边缘检测 + HoughLinesP 概率霍夫变换提取直线，
 * 取所有直线角度的中位数作为最终旋转角度，
 * 通过 warpAffine 进行仿射变换旋转，自动扩展画布避免裁剪。
 *
 * @param image 输入/输出图像（原地修改）
 * @return 矫正角度（度），0.0 表示无需矫正
 */
double detectSkewAndCorrect(cv::Mat &image)
{
    // --- 步骤1: 图像灰度化（若为彩色图）
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // --- 步骤2: 高斯模糊去噪，减少后续边缘检测的误报
    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred,
                       cv::Size(SkewConstants::GAUSS_KERNEL, SkewConstants::GAUSS_KERNEL),
                       0);

    // --- 步骤3: Canny 边缘检测，提取图像中清晰的边缘信息
    cv::Mat edges;
    cv::Canny(blurred, edges, SkewConstants::CANNY_LOW, SkewConstants::CANNY_HIGH);

    // --- 步骤4: 概率霍夫变换（HoughLinesP）检测直线段
    // minLineLength 随图像大小动态调整，避免在小图上产生过多噪点直线
    std::vector<cv::Vec4i> lines;
    int minLineLength = std::max(100, image.cols / 8);
    cv::HoughLinesP(edges, lines, 1, CV_PI / 180,
                     SkewConstants::HOUGH_THRESHOLD, minLineLength,
                     SkewConstants::HOUGH_MAX_GAP);

    // 若未检测到任何直线，无法判断倾斜，直接返回 0（不矫正）
    if (lines.empty()) return 0.0;

    // --- 步骤5: 逐条直线计算与水平方向的夹角
    std::vector<double> angles;
    for (const auto &ln : lines) {
        // 计算直线向量与水平方向夹角（度）
        double dx = ln[2] - ln[0];
        double dy = ln[3] - ln[1];
        double a = std::atan2(dy, dx) * 180.0 / CV_PI;
        // 将角度归一化到 (-90, 90] 区间
        if (a > 90) a -= 180;
        if (a <= -90) a += 180;
        // 过滤掉过于倾斜的异常直线（如一些孤立的斜线噪声）
        if (std::abs(a) < SkewConstants::MAX_LINE_ANGLE) {
            angles.push_back(a);
        }
    }

    // 过滤后也可能为空（比如图像完全是垂直/水平线的情况）
    if (angles.empty()) return 0.0;

    // --- 步骤6: 取角度的中位数（对个别错误直线产生的离群值鲁棒）
    std::sort(angles.begin(), angles.end());
    double medianAngle = angles[angles.size() / 2];

    // 若倾斜角度过小（< 1°），跳过矫正避免不必要的图像质量损失
    if (std::abs(medianAngle) < SkewConstants::MIN_ANGLE) return 0.0;

    // --- 步骤7: 以图像中心为旋转中心，计算旋转矩阵
    cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
    cv::Mat rotMat = cv::getRotationMatrix2D(center, medianAngle, 1.0);

    // --- 步骤8: 计算旋转后的新画布尺寸，确保图像不被裁剪
    double absCos = std::abs(rotMat.at<double>(0, 0));
    double absSin = std::abs(rotMat.at<double>(0, 1));
    int newW = static_cast<int>(image.rows * absSin + image.cols * absCos);
    int newH = static_cast<int>(image.rows * absCos + image.cols * absSin);
    // 调整平移分量，使旋转后的图像完整显示在新画布中心
    rotMat.at<double>(0, 2) += (newW / 2.0) - center.x;
    rotMat.at<double>(1, 2) += (newH / 2.0) - center.y;

    // --- 步骤9: warpAffine 仿射变换旋转
    cv::Mat rotated;
    cv::warpAffine(image, rotated, rotMat, cv::Size(newW, newH),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    image = rotated;
    return medianAngle;
}

/**
 * @brief 模板匹配定位
 *
 * 使用归一化互相关（NCC）在待测图像中搜索模板的最佳匹配位置。
 *
 * @param testImage 待测图像
 * @param templateImage 模板图像
 * @return 模板左上角的最佳匹配点坐标
 */
cv::Point templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage)
{
    // 执行归一化互相关匹配
    cv::Mat result;
    cv::matchTemplate(testImage, templateImage, result, cv::TM_CCOEFF_NORMED);

    // 获取最大匹配分数和位置（最大匹配分数代表最佳匹配点）
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    return maxLoc;
}

/**
 * @brief ROI 精对齐
 *
 * 在粗定位结果基础上，在局部窗口再次做NCC搜索，获取更精确的ROI坐标。
 *
 * @param testImage 待测图像
 * @param roiTemplate 该ROI的模板子图像
 * @param roughMatchPoint 粗定位的匹配点（模板左上角坐标）
 * @param roiRect ROI在模板中的相对位置
 * @param outAlignedRect 输出精对齐后的ROI坐标
 * @return 对齐后从待测图像中裁剪的ROI子图像
 */
cv::Mat alignROI(const cv::Mat &testImage, const cv::Mat &roiTemplate,
                 const cv::Point &roughMatchPoint, const cv::Rect &roiRect,
                 cv::Rect &outAlignedRect)
{
    // --- 步骤1: 根据粗匹配点计算ROI粗定位中心
    cv::Point roughCenter(
        roughMatchPoint.x + roiRect.x + roiRect.width / 2,
        roughMatchPoint.y + roiRect.y + roiRect.height / 2);

    // --- 步骤2: 计算局部搜索窗口大小
    // 搜索范围：以粗定位中心 ± halfSearch 像素
    int halfSearch = std::min({30, roiRect.width / 3, roiRect.height / 3});

    // --- 步骤3: 定义搜索矩形并确保在图像边界内
    cv::Rect searchRect(
        std::max(0, roughCenter.x - roiRect.width / 2 - halfSearch),
        std::max(0, roughCenter.y - roiRect.height / 2 - halfSearch),
        roiRect.width + 2 * halfSearch,
        roiRect.height + 2 * halfSearch);
    // 与图像边界求交集，确保不越界
    searchRect = searchRect & cv::Rect(0, 0, testImage.cols, testImage.rows);

    // --- 步骤4: 在搜索窗口内做局部NCC匹配
    cv::Point preciseOffset(0, 0);
    if (searchRect.width >= roiRect.width && searchRect.height >= roiRect.height) {
        // 从待测图像中裁剪出搜索区域
        cv::Mat testSearch = testImage(searchRect);
        cv::Mat nccResult;
        // 在搜索区域内再次做 NCC 匹配
        cv::matchTemplate(testSearch, roiTemplate, nccResult, cv::TM_CCOEFF_NORMED);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);
        preciseOffset = maxLoc; // 精匹配后的偏移（相对于 searchRect 左上角）
    }

    // --- 步骤5: 根据精匹配结果计算最终对齐后的ROI坐标
    cv::Rect testROI(
        searchRect.x + preciseOffset.x,
        searchRect.y + preciseOffset.y,
        roiRect.width,
        roiRect.height);
    testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);

    // --- 步骤6: 若局部搜索失败（尺寸不匹配），回退到粗定位位置
    if (testROI.width != roiRect.width || testROI.height != roiRect.height) {
        testROI = cv::Rect(
            std::max(0, roughMatchPoint.x + roiRect.x),
            std::max(0, roughMatchPoint.y + roiRect.y),
            roiRect.width,
            roiRect.height);
        testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);
    }

    // --- 步骤7: 返回精对齐后的ROI坐标和子图像
    outAlignedRect = testROI;
    return (testROI.area() > 0) ? testImage(testROI) : cv::Mat();
}

/**
 * @brief 字符残缺检测
 *
 * 通过比对测试区域和模板区域的NCC相似度，判定该ROI是否存在字符残缺。
 *
 * @param testRegion 待测图像中对齐后的ROI子图
 * @param templateRegion 模板图像中对应的ROI子图
 * @param defectScore 输出残缺程度分数（0~1）
 * @return true 表示检测到残缺，false 表示正常
 */
bool detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore)
{
    // --- 步骤1: 空值校验，区域为空直接判定为残缺
    if (testRegion.empty() || templateRegion.empty()) {
        defectScore = 1.0;
        return true;
    }

    // --- 步骤2: Lambda 函数：将图像统一转为灰度图
    // 使用 lambda 便于在本函数内复用
    auto toGray = [](const cv::Mat &m) -> cv::Mat {
        if (m.channels() == 3) {
            cv::Mat g;
            cv::cvtColor(m, g, cv::COLOR_BGR2GRAY);
            return g;
        }
        return m.clone();
    };
    cv::Mat testGray = toGray(testRegion);
    cv::Mat templateGray = toGray(templateRegion);

    // --- 步骤3: resize 对齐，确保两张图尺寸一致
    cv::Mat testResized;
    if (testGray.size() != templateGray.size()) {
        cv::resize(testGray, testResized, templateGray.size());
    } else {
        testResized = testGray;
    }

    // --- 步骤4: 高斯模糊进一步压制轻微噪声和细微差异
    cv::GaussianBlur(testResized, testResized,
                     cv::Size(DefectConstants::GAUSS_KERNEL, DefectConstants::GAUSS_KERNEL), 0);
    cv::GaussianBlur(templateGray, templateGray,
                     cv::Size(DefectConstants::GAUSS_KERNEL, DefectConstants::GAUSS_KERNEL), 0);

    // --- 步骤5: NCC相似度比对
    cv::Mat nccResult;
    cv::matchTemplate(testResized, templateGray, nccResult, cv::TM_CCOEFF_NORMED);
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);

    // --- 步骤6: 计算残缺分数并与阈值比较
    // defectScore = 1 - NCC最大值
    // 越接近1表示与模板差异越大，残缺越严重
    defectScore = 1.0 - maxVal;
    // 大于阈值 0.15 判定为残缺
    return defectScore > DefectConstants::DEFECT_SCORE_THRESHOLD;
}

} // namespace Detector
