#include "resultmanager.h"
#include <QTableWidgetItem>
#include <QFile>
#include <QTextStream>
#include <QMap>

namespace ResultManager {

void addToTable(QTableWidget *table, int imageId, const QList<DetectionResult> &roiResults)
{
    if (roiResults.isEmpty()) return;

    int startRow = table->rowCount();
    int rowCount = roiResults.size();

    for (int i = 0; i < rowCount; ++i) {
        table->insertRow(startRow + i);
    }

    QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(imageId));
    idItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 0, idItem);
    if (rowCount > 1) table->setSpan(startRow, 0, rowCount, 1);

    QTableWidgetItem *nameItem = new QTableWidgetItem(roiResults.first().imageName);
    nameItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 1, nameItem);
    if (rowCount > 1) table->setSpan(startRow, 1, rowCount, 1);

    QTableWidgetItem *timeItem = new QTableWidgetItem(roiResults.first().detectionTime);
    timeItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(startRow, 4, timeItem);
    if (rowCount > 1) table->setSpan(startRow, 4, rowCount, 1);

    for (int i = 0; i < rowCount; ++i) {
        const DetectionResult &r = roiResults[i];

        QTableWidgetItem *roiItem = new QTableWidgetItem(r.roiName);
        roiItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(startRow + i, 2, roiItem);

        QTableWidgetItem *resultItem = new QTableWidgetItem(
            r.isDefective ? QStringLiteral("残缺") : QStringLiteral("正常"));
        resultItem->setTextAlignment(Qt::AlignCenter);
        resultItem->setData(Qt::UserRole, r.isDefective ? 1 : 0);
        table->setItem(startRow + i, 3, resultItem);
    }
}

bool exportToCsv(const QString &path, const QList<DetectionResult> &resultList)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QStringLiteral("序号,图片名称,检测区域,检测结果,残缺程度,检测时间\n");

    QMap<QString, int> imageIdMap;
    int nextImageId = 0;

    for (const DetectionResult &result : resultList) {
        QString key = result.imageName + "|" + result.detectionTime;
        if (!imageIdMap.contains(key)) imageIdMap[key] = ++nextImageId;
        int imageId = imageIdMap[key];

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

void clearAll(QTableWidget *table, QList<DetectionResult> &resultList,
              int &resultCount, int &imageResultCount)
{
    resultList.clear();
    resultCount = 0;
    imageResultCount = 0;
    table->setRowCount(0);
}

}
