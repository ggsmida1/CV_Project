#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QList>
#include <opencv2/opencv.hpp>
#include "types.h"

namespace ConfigManager {

struct ConfigData {
    cv::Mat templateImage;
    QString templatePath;
    QList<ROIRect> roiList;
};

bool save(const QString &path, const cv::Mat &templateImage,
          const QString &templateImagePath, const QList<ROIRect> &roiList);
bool load(const QString &path, ConfigData &outData);

}

#endif
