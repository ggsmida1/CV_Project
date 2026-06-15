/**
 * @file configmanager.cpp
 * @brief 配置文件管理实现
 */

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

/**
 * @brief 将当前配置保存为 JSON 文件
 *
 * @param path 保存文件路径
 * @param templateImage 模板图像
 * @param templateImagePath 模板图像原始路径
 * @param roiList ROI 列表
 * @return true 保存成功，false 失败
 */
bool save(const QString &path, const cv::Mat &templateImage,
          const QString &templateImagePath, const QList<ROIRect> &roiList)
{
    // --- 步骤1: 基本合法性校验（模板图像为空或无 ROI 则不保存）
    if (templateImage.empty() || roiList.isEmpty()) return false;

    // --- 步骤2: 构建 JSON 根对象
    QJsonObject root;

    // 保存模板路径（以 samples/templates/xxx.jpg 的相对路径形式）
    QFileInfo templateInfo(templateImagePath);
    root["templatePath"] = "templates/" + templateInfo.fileName();
    // 保存模板原始尺寸（便于调试和校验）
    root["templateWidth"] = templateImage.cols;
    root["templateHeight"] = templateImage.rows;

    // 将模板图像本身也 base64 编码嵌入 JSON，使配置文件完全自包含
    // 这样即使 samples/templates/ 下找不到原始图片，也能从 JSON 中还原
    std::vector<uchar> tplBuf;
    cv::imencode(".jpg", templateImage, tplBuf);
    QByteArray tplBase64 = QByteArray::fromRawData(
        reinterpret_cast<const char *>(tplBuf.data()), tplBuf.size()).toBase64();
    root["templateImage"] = QString(tplBase64);

    // 同时把模板图片复制到 samples/templates/ 目录（便于调试和与旧版本兼容）
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString tplDir = projectRoot + "/samples/templates";
    QDir().mkpath(tplDir);
    QString tplDst = tplDir + "/" + templateInfo.fileName();
    if (!QFile::exists(tplDst) && !templateImagePath.isEmpty()) {
        QFile::copy(templateImagePath, tplDst);
    }

    // --- 步骤3: 遍历 ROI 列表，将每个 ROI 的信息和子图像写入 JSON
    QJsonArray roiArray;
    for (const ROIRect &roi : roiList) {
        QJsonObject roiObj;
        roiObj["id"] = roi.id;
        roiObj["name"] = roi.name;
        roiObj["x"] = roi.rect.x;
        roiObj["y"] = roi.rect.y;
        roiObj["width"] = roi.rect.width;
        roiObj["height"] = roi.rect.height;

        // --- 关键点：将 ROI 子图像编码为 PNG → base64 嵌入 JSON
        // 这样配置文件是"自包含"的，即使原始模板图片丢失，
        // 仍能从 JSON 中还原出各 ROI 区域的子图像用于后续检测
        std::vector<uchar> buf;
        cv::imencode(".png", roi.templateImage, buf);
        QByteArray base64 = QByteArray::fromRawData(
            reinterpret_cast<const char *>(buf.data()), buf.size()).toBase64();
        roiObj["templateImage"] = QString(base64);
        roiArray.append(roiObj);
    }
    root["roiList"] = roiArray;

    // --- 步骤4: QJsonDocument 写入文件
    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(doc.toJson());
    file.close();
    return true;
}

/**
 * @brief 从 JSON 文件加载配置
 *
 * @param path 配置文件路径
 * @param outData 输出配置数据
 * @return true 加载成功，false 失败
 */
bool load(const QString &path, ConfigData &outData)
{
    // --- 步骤1: 打开并读取 JSON 文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    // --- 步骤2: 解析 JSON 文档
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) return false;

    QJsonObject root = doc.object();

    // --- 步骤3: 还原模板图像（优先从 JSON 内嵌的 base64 数据还原，退而从外部文件读取，
    //          最后尝试从 ROI 子图像合成。三重保障确保不会因图片丢失而加载失败。）
    cv::Mat templateImg;
    QString templateAbsolutePath;

    // 方式1: 新格式——JSON 中已经把模板图用 base64 编码直接嵌入了
    if (root.contains("templateImage") && !root["templateImage"].toString().isEmpty()) {
        QByteArray tplBase64 = root["templateImage"].toString().toLatin1();
        QByteArray tplData = QByteArray::fromBase64(tplBase64);
        std::vector<uchar> tplBuf(tplData.begin(), tplData.end());
        templateImg = cv::imdecode(tplBuf, cv::IMREAD_COLOR);
        if (!templateImg.empty()) {
            outData.templateImage = templateImg;
            outData.templatePath = root["templatePath"].toString();
        }
    }

    // 方式2: 旧格式——从 templatePath 指定的外部路径加载（向后兼容）
    if (templateImg.empty()) {
        QString templateRelativePath = root["templatePath"].toString();

        QDir appDir(QCoreApplication::applicationDirPath());
        QString projectRoot = appDir.absolutePath();
        if (!QDir(projectRoot + "/samples").exists()) {
            appDir.cdUp();
            projectRoot = appDir.absolutePath();
        }
        templateAbsolutePath = projectRoot + "/samples/" + templateRelativePath;

        templateImg = ImageUtil::imreadSafe(templateAbsolutePath);
        if (!templateImg.empty()) {
            outData.templateImage = templateImg;
            outData.templatePath = templateAbsolutePath;
        }
    }

    // 方式3: 兜底——从 JSON 中已有的 ROI 子图像合成一个简易模板图
    // 当模板图彻底丢失时，用各 ROI 子图在其坐标位置合成一个最小可用图
    if (templateImg.empty()) {
        int tplW = root["templateWidth"].toInt();
        int tplH = root["templateHeight"].toInt();
        if (tplW > 0 && tplH > 0) {
            templateImg = cv::Mat(tplH, tplW, CV_8UC3, cv::Scalar(200, 200, 200));
            QJsonArray roiArray2 = root["roiList"].toArray();
            for (const QJsonValue &v : roiArray2) {
                QJsonObject obj = v.toObject();
                int x = obj["x"].toInt(), y = obj["y"].toInt();
                int w = obj["width"].toInt(), h = obj["height"].toInt();
                QByteArray subBase64 = obj["templateImage"].toString().toLatin1();
                QByteArray subData = QByteArray::fromBase64(subBase64);
                std::vector<uchar> subBuf(subData.begin(), subData.end());
                cv::Mat subImg = cv::imdecode(subBuf, cv::IMREAD_COLOR);
                if (!subImg.empty() && x >= 0 && y >= 0 &&
                    x + w <= tplW && y + h <= tplH) {
                    cv::Mat dstRoi = templateImg(cv::Rect(x, y, w, h));
                    cv::resize(subImg, dstRoi, dstRoi.size());
                }
            }
            if (!templateImg.empty()) {
                outData.templateImage = templateImg;
                outData.templatePath = root["templatePath"].toString();
            }
        }
    }

    // 三种方式都失败才返回 false
    if (templateImg.empty()) return false;

    // --- 步骤4: 遍历 ROI 数组，还原每个 ROI
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

        // base64 解码 → imdecode 还原图像
        QByteArray base64 = roiObj["templateImage"].toString().toLatin1();
        QByteArray imageData = QByteArray::fromBase64(base64);
        std::vector<uchar> buf(imageData.begin(), imageData.end());
        roi.templateImage = cv::imdecode(buf, cv::IMREAD_COLOR);

        outData.roiList.append(roi);
    }

    return true;
}

} // namespace ConfigManager
