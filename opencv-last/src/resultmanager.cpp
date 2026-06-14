/**
 * @file resultmanager.cpp
 * @brief 检测结果管理实现
 */

#include "resultmanager.h"
#include <QTableWidgetItem>
#include <QFile>
#include <QTextStream>
#include <QMap>

namespace ResultManager {

/**
 * @brief 将一次图片检测的结果添加到表格
 *
 * 实现"图片级跨行合并"：
 * 一张图片可能有多个 ROI 检测结果 → 序号/图片名/时间三列跨多行合并。
 */
void addToTable(QTableWidget *table, int imageId, const QList<DetectionResult> &roiResults)
{
    // 空结果直接返回
    if (roiResults.isEmpty()) return;

    // --- 步骤1: 在表格末尾插入新行（行数=本次检测的 ROI 数量）
    int startRow = table->rowCount();
    int rowCount = roiResults.size();
    for (int i = 0; i < rowCount; ++i) {
        table->insertRow(startRow + i);
    }

    // --- 步骤2: 设置"序号"列，并在多行之间合并
    QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(imageId));
    idItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 0, idItem);
    if (rowCount > 1) table->setSpan(startRow, 0, rowCount, 1);

    // --- 步骤3: 设置"图片名称"列，并在多行之间合并
    QTableWidgetItem *nameItem = new QTableWidgetItem(roiResults.first().imageName);
    nameItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 1, nameItem);
    if (rowCount > 1) table->setSpan(startRow, 1, rowCount, 1);

    // --- 步骤4: 设置"检测时间"列，并在多行之间合并
    QTableWidgetItem *timeItem = new QTableWidgetItem(roiResults.first().detectionTime);
    timeItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 4, timeItem);
    if (rowCount > 1) table->setSpan(startRow, 4, rowCount, 1);

    // --- 步骤5: 逐行填充每个 ROI 的检测区域和检测结果
    for (int i = 0; i < rowCount; ++i) {
        const DetectionResult &r = roiResults[i];

        // 检测区域名
        QTableWidgetItem *roiItem = new QTableWidgetItem(r.roiName);
        roiItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(startRow + i, 2, roiItem);

        // 检测结果（在 mainwindow.cpp 中通过 ResultItemDelegate 进一步渲染颜色）
        // 这里先写入文本，isDefective 存入 UserRole 供自定义绘制委托读取
        QTableWidgetItem *resultItem = new QTableWidgetItem(
            r.isDefective ? QStringLiteral("残缺") : QStringLiteral("正常"));
        resultItem->setTextAlignment(Qt::AlignCenter);
        resultItem->setData(Qt::UserRole, r.isDefective ? 1 : 0);
        table->setItem(startRow + i, 3, resultItem);
    }
}

/**
 * @brief 将所有检测结果导出为 CSV 文件
 *
 * 注意：导出时需要从扁平的 ROI 结果反向构建图片级序号，
 * 用 "imageName + detectionTime" 作为 key 维护一个递增 id。
 */
bool exportToCsv(const QString &path, const QList<DetectionResult> &resultList)
{
    // --- 步骤1: 创建文件
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    // --- 步骤2: 以 UTF-8 写入表头
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QStringLiteral("序号,图片名称,检测区域,检测结果,残缺程度,检测时间\n");

    // --- 步骤3: 构建图片级序号（从扁平结果反向聚合）
    QMap<QString, int> imageIdMap;
    int nextImageId = 0;

    for (const DetectionResult &result : resultList) {
        // key = 图片名 + 检测时间，确保每张检测图拥有唯一序号
        QString key = result.imageName + "|" + result.detectionTime;
        if (!imageIdMap.contains(key)) imageIdMap[key] = ++nextImageId;
        int imageId = imageIdMap[key];

        // --- 步骤4: 逐行写入 CSV
        out << imageId << ","
            << result.imageName << ","
            << result.roiName << ","
            << (result.isDefective ? QStringLiteral("残缺") : QStringLiteral("正常")) << ","
            << QString::number(result.defectScore * 100, 'f', 2) << "%,"
            << result.detectionTime << "\n";
    }

    file.close();
    return true;
}

/**
 * @brief 清空所有检测结果
 *
 * 同时清理：
 * - 内存中的检测结果列表
 * - 两个计数变量
 * - 界面表格所有行
 */
void clearAll(QTableWidget *table, QList<DetectionResult> &resultList,
              int &resultCount, int &imageResultCount)
{
    resultList.clear();
    resultCount = 0;
    imageResultCount = 0;
    table->setRowCount(0);
}

} // namespace ResultManager
