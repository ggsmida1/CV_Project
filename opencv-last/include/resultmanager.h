/**
 * @file resultmanager.h
 * @brief 检测结果管理模块
 *
 * 负责将检测结果添加到界面表格、导出为 CSV 文件、以及清空重置。
 * 一张图片上可能有多个 ROI，因此采用 "图片级跨行合并" 的展示方式：
 * - 图片名列和检测时间列在多个 ROI 之间合并为一行
 * - 检测区域和检测结果按每个 ROI 分别展示
 */

#ifndef RESULTMANAGER_H
#define RESULTMANAGER_H

#include <QTableWidget>
#include <QList>
#include <QString>
#include "types.h"

namespace ResultManager {

/**
 * @brief 将一次图片检测的结果添加到表格
 * @param table 目标表格控件
 * @param imageId 图片级序号（第几张检测图）
 * @param roiResults 该图片上所有 ROI 的检测结果
 *
 * 当一张图片有多个 ROI 时，图片序号/图片名/检测时间三列会被
 * 跨行合并，表格呈现出层次分明的视觉效果。
 */
void addToTable(QTableWidget *table, int imageId, const QList<DetectionResult> &roiResults);

/**
 * @brief 将所有检测结果导出为 CSV 文件
 * @param path 导出文件路径
 * @param resultList 全部检测结果列表
 * @return true 成功，false 失败
 *
 * CSV 列格式：序号, 图片名称, 检测区域, 检测结果, 残缺程度(%), 检测时间
 * 以 UTF-8 编码写入，支持中文。
 */
bool exportToCsv(const QString &path, const QList<DetectionResult> &resultList);

/**
 * @brief 清空所有检测结果
 * @param table 表格控件（清空内容）
 * @param resultList 结果列表（清空内存）
 * @param resultCount 全局 ROI 级计数（重置为0）
 * @param imageResultCount 全局图片级计数（重置为0）
 */
void clearAll(QTableWidget *table, QList<DetectionResult> &resultList,
              int &resultCount, int &imageResultCount);

} // namespace ResultManager

#endif // RESULTMANAGER_H
