#include "configmanager.h"
#include "imageutil.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>

namespace ConfigManager {

bool save(const QString &path, const cv::Mat &templateImage,
          const QString &templateImagePath, const QList<ROIRect> &roiList)
{
    if (templateImage.empty() || roiList.isEmpty()) return false;

    QJsonObject root;
    QFileInfo templateInfo(templateImagePath);
    root["templatePath"] = "templates/" + templateInfo.fileName();
    root["templateWidth"] = templateImage.cols;
    root["templateHeight"] = templateImage.rows;

    QJsonArray roiArray;
    for (const ROIRect &roi : roiList) {
        QJsonObject roiObj;
        roiObj["id"] = roi.id;
        roiObj["name"] = roi.name;
        roiObj["x"] = roi.rect.x;
        roiObj["y"] = roi.rect.y;
        roiObj["width"] = roi.rect.width;
        roiObj["height"] = roi.rect.height;

        std::vector<uchar> buf;
        cv::imencode(".png", roi.templateImage, buf);
        QByteArray base64 = QByteArray::fromRawData(
            reinterpret_cast<const char*>(buf.data()), buf.size()).toBase64();
        roiObj["templateImage"] = QString(base64);
        roiArray.append(roiObj);
    }
    root["roiList"] = roiArray;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

bool load(const QString &path, ConfigData &outData)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) return false;

    QJsonObject root = doc.object();

    QString templateRelativePath = root["templatePath"].toString();
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString templateAbsolutePath = projectRoot + "/samples/" + templateRelativePath;

    cv::Mat templateImg = ImageUtil::imreadSafe(templateAbsolutePath);
    if (templateImg.empty()) return false;

    outData.templateImage = templateImg;
    outData.templatePath = templateAbsolutePath;

    QJsonArray roiArray = root["roiList"].toArray();
    for (const QJsonValue &value : roiArray) {
        QJsonObject roiObj = value.toObject();
        ROIRect roi;
        roi.id = roiObj["id"].toInt();
        roi.name = roiObj["name"].toString();
        roi.rect.x = roiObj["x"].toInt();
        roi.rect.y = roiObj["y"].toInt();
        roi.rect.width = roiObj["width"].toInt();
        roi.rect.height = roiObj["height"].toInt();

        QByteArray base64 = roiObj["templateImage"].toString().toLatin1();
        QByteArray imageData = QByteArray::fromBase64(base64);
        std::vector<uchar> buf(imageData.begin(), imageData.end());
        roi.templateImage = cv::imdecode(buf, cv::IMREAD_COLOR);

        outData.roiList.append(roi);
    }

    return true;
}

}
