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

    // --- 步骤3: 还原模板图像
    // 获取模板相对路径（如 "templates/身份证背部.jpg"）
    QString templateRelativePath = root["templatePath"].toString();

    // 从应用目录向上查找 samples 目录
    // （在 IDE 运行时 appDir 为构建目录，而 samples 在项目根目录）
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    // 拼出模板图像的绝对路径
    QString templateAbsolutePath = projectRoot + "/samples/" + templateRelativePath;

    // 使用 imreadSafe 读取（支持中文路径）
    cv::Mat templateImg = ImageUtil::imreadSafe(templateAbsolutePath);
    if (templateImg.empty()) return false;

    outData.templateImage = templateImg;
    outData.templatePath = templateAbsolutePath;

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
