#ifndef IMAGEUTIL_H
#define IMAGEUTIL_H

#include <QImage>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <opencv2/opencv.hpp>

namespace ImageUtil {

QImage mat2QImage(const cv::Mat &mat);
cv::Mat QImage2Mat(const QImage &image);
cv::Mat imreadSafe(const QString &path);
void computeImageTransform(QLabel *label, const cv::Mat &image, double &outScale, int &outOffsetX, int &outOffsetY);
QPoint labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos);
void displayImageOnLabel(QLabel *label, const cv::Mat &image, const QString &placeholder);
QString getSamplesDir(const QString &subdir);

}

#endif
