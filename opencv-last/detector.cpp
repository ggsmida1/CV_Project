#include "detector.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Detector {

double detectSkewAndCorrect(cv::Mat &image)
{
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    cv::Mat blurred;
    cv::GaussianBlur(gray, blurred, cv::Size(SkewConstants::GAUSS_KERNEL, SkewConstants::GAUSS_KERNEL), 0);

    cv::Mat edges;
    cv::Canny(blurred, edges, SkewConstants::CANNY_LOW, SkewConstants::CANNY_HIGH);

    std::vector<cv::Vec4i> lines;
    int minLineLength = std::max(100, image.cols / 8);
    cv::HoughLinesP(edges, lines, 1, CV_PI / 180,
                     SkewConstants::HOUGH_THRESHOLD, minLineLength,
                     SkewConstants::HOUGH_MAX_GAP);

    if (lines.empty()) return 0.0;

    std::vector<double> angles;
    for (const auto &ln : lines) {
        double dx = ln[2] - ln[0];
        double dy = ln[3] - ln[1];
        double a = std::atan2(dy, dx) * 180.0 / CV_PI;
        if (a > 90) a -= 180;
        if (a <= -90) a += 180;
        if (std::abs(a) < SkewConstants::MAX_LINE_ANGLE) {
            angles.push_back(a);
        }
    }

    if (angles.empty()) return 0.0;

    std::sort(angles.begin(), angles.end());
    double medianAngle = angles[angles.size() / 2];
    if (std::abs(medianAngle) < SkewConstants::MIN_ANGLE) return 0.0;

    cv::Point2f center(image.cols / 2.0f, image.rows / 2.0f);
    cv::Mat rotMat = cv::getRotationMatrix2D(center, medianAngle, 1.0);

    double absCos = std::abs(rotMat.at<double>(0, 0));
    double absSin = std::abs(rotMat.at<double>(0, 1));
    int newW = static_cast<int>(image.rows * absSin + image.cols * absCos);
    int newH = static_cast<int>(image.rows * absCos + image.cols * absSin);
    rotMat.at<double>(0, 2) += (newW / 2.0) - center.x;
    rotMat.at<double>(1, 2) += (newH / 2.0) - center.y;

    cv::Mat rotated;
    cv::warpAffine(image, rotated, rotMat, cv::Size(newW, newH),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));
    image = rotated;
    return medianAngle;
}

cv::Point templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage)
{
    cv::Mat result;
    cv::matchTemplate(testImage, templateImage, result, cv::TM_CCOEFF_NORMED);
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    return maxLoc;
}

cv::Mat alignROI(const cv::Mat &testImage, const cv::Mat &roiTemplate,
                 const cv::Point &roughMatchPoint, const cv::Rect &roiRect,
                 cv::Rect &outAlignedRect)
{
    cv::Point roughCenter(roughMatchPoint.x + roiRect.x + roiRect.width / 2,
                          roughMatchPoint.y + roiRect.y + roiRect.height / 2);

    int halfSearch = std::min(30, std::min(roiRect.width, roiRect.height) / 3);

    cv::Rect searchRect(
        std::max(0, roughCenter.x - roiRect.width / 2 - halfSearch),
        std::max(0, roughCenter.y - roiRect.height / 2 - halfSearch),
        roiRect.width + 2 * halfSearch,
        roiRect.height + 2 * halfSearch
    );
    searchRect = searchRect & cv::Rect(0, 0, testImage.cols, testImage.rows);

    cv::Point preciseOffset(0, 0);
    if (searchRect.width >= roiRect.width && searchRect.height >= roiRect.height) {
        cv::Mat testSearch = testImage(searchRect);
        cv::Mat nccResult;
        cv::matchTemplate(testSearch, roiTemplate, nccResult, cv::TM_CCOEFF_NORMED);
        double minVal, maxVal;
        cv::Point minLoc, maxLoc;
        cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);
        preciseOffset = maxLoc;
    }

    cv::Rect testROI(
        searchRect.x + preciseOffset.x,
        searchRect.y + preciseOffset.y,
        roiRect.width,
        roiRect.height
    );
    testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);

    if (testROI.width != roiRect.width || testROI.height != roiRect.height) {
        testROI = cv::Rect(
            std::max(0, roughMatchPoint.x + roiRect.x),
            std::max(0, roughMatchPoint.y + roiRect.y),
            roiRect.width,
            roiRect.height
        );
        testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);
    }

    outAlignedRect = testROI;
    return (testROI.area() > 0) ? testImage(testROI) : cv::Mat();
}

bool detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore)
{
    if (testRegion.empty() || templateRegion.empty()) {
        defectScore = 1.0;
        return true;
    }

    cv::Mat testGray, templateGray;
    if (testRegion.channels() == 3) {
        cv::cvtColor(testRegion, testGray, cv::COLOR_BGR2GRAY);
    } else {
        testGray = testRegion.clone();
    }
    if (templateRegion.channels() == 3) {
        cv::cvtColor(templateRegion, templateGray, cv::COLOR_BGR2GRAY);
    } else {
        templateGray = templateRegion.clone();
    }

    cv::Mat testResized;
    if (testGray.size() != templateGray.size()) {
        cv::resize(testGray, testResized, templateGray.size());
    } else {
        testResized = testGray;
    }

    cv::GaussianBlur(testResized, testResized,
                     cv::Size(DefectConstants::GAUSS_KERNEL, DefectConstants::GAUSS_KERNEL), 0);
    cv::GaussianBlur(templateGray, templateGray,
                     cv::Size(DefectConstants::GAUSS_KERNEL, DefectConstants::GAUSS_KERNEL), 0);

    cv::Mat nccResult;
    cv::matchTemplate(testResized, templateGray, nccResult, cv::TM_CCOEFF_NORMED);
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);

    defectScore = 1.0 - maxVal;
    return defectScore > DefectConstants::DEFECT_SCORE_THRESHOLD;
}

}
