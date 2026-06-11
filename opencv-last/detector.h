#ifndef DETECTOR_H
#define DETECTOR_H

#include <opencv2/opencv.hpp>

namespace Detector {

struct SkewConstants {
    static constexpr double MIN_ANGLE = 1.0;
    static constexpr double MAX_LINE_ANGLE = 45.0;
    static constexpr int GAUSS_KERNEL = 5;
    static constexpr double CANNY_LOW = 50.0;
    static constexpr double CANNY_HIGH = 150.0;
    static constexpr int HOUGH_THRESHOLD = 100;
    static constexpr int HOUGH_MAX_GAP = 20;
};

struct DefectConstants {
    static constexpr double NCC_THRESHOLD = 0.85;
    static constexpr double DEFECT_SCORE_THRESHOLD = 0.15;
    static constexpr int GAUSS_KERNEL = 3;
};

bool detectSkewAndCorrect(cv::Mat &image);
cv::Point templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage);
cv::Rect calculateDetectionRegion(const cv::Point &matchPoint, const cv::Rect &templateROI);
bool detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore);

}

#endif
