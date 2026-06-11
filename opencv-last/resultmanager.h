#ifndef RESULTMANAGER_H
#define RESULTMANAGER_H

#include <QTableWidget>
#include <QList>
#include <QString>
#include "types.h"

namespace ResultManager {

void addToTable(QTableWidget *table, int imageId, const QList<DetectionResult> &roiResults);
bool exportToCsv(const QString &path, const QList<DetectionResult> &resultList);
void clearAll(QTableWidget *table, QList<DetectionResult> &resultList,
              int &resultCount, int &imageResultCount);

}

#endif
