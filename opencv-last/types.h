#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QList>
#include <opencv2/opencv.hpp>

struct ROIRect {
    int id;
    cv::Rect rect;
    QString name;
    cv::Mat templateImage;
};

struct DetectionResult {
    int id;
    QString imageName;
    QString roiName;
    bool isDefective;
    double defectScore;
    QString detectionTime;
};

#endif
