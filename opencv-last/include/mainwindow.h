#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMouseEvent>
#include <opencv2/opencv.hpp>
#include "types.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ResultItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ResultItemDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnLoadTemplate_clicked();
    void on_btnAddROI_clicked();
    void on_btnDeleteROI_clicked();
    void on_btnSaveConfig_clicked();
    void on_btnLoadConfig_clicked();
    void on_listROI_currentRowChanged(int currentRow);
    void on_btnLoadTestImage_clicked();
    void on_btnBrowseConfig_clicked();
    void on_btnStartDetection_clicked();
    void on_btnBatchDetection_clicked();
    void on_btnExportResults_clicked();
    void on_btnClearResults_clicked();
    void on_btnMinimize_clicked();
    void on_btnMaximize_clicked();
    void on_btnClose_clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    Ui::MainWindow *ui;

    bool m_isDraggingWindow;
    QPoint m_dragStartPosition;
    int m_titleBarHeight;
    bool m_isMaximized;
    QRect m_normalGeometry;

    cv::Mat m_templateImage;
    cv::Mat m_templateDisplay;
    QString m_templateImagePath;
    QList<ROIRect> m_roiList;
    int m_currentROIIndex;
    bool m_isSelectingROI;
    QPoint m_roiStartPoint;
    QPoint m_roiEndPoint;

    cv::Mat m_testImage;
    cv::Mat m_resultImage;
    QString m_testImagePath;
    QString m_configFilePath;
    QList<DetectionResult> m_resultList;
    int m_resultCount;
    int m_imageResultCount;

    bool loadTemplateImage(const QString &path);
    bool loadTestImage(const QString &path);
    void addROI(const cv::Rect &rect);
    void deleteROI(int index);
    void drawROIsOnTemplate();
    void updateROIList(int preferredIndex = -1);
    void performDetection(const QString &imagePath);
};

#endif
