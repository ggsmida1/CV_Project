#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QMouseEvent>
#include <QPainter>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// ROI区域结构体
struct ROIRect {
    int id;
    cv::Rect rect;
    QString name;
    cv::Mat templateImage;  // 该区域的模板图像
};

// 检测结果结构体
struct DetectionResult {
    int id;
    QString imageName;
    QString roiName;
    bool isDefective;      // 是否残缺
    double defectScore;    // 残缺程度评分
    QString detectionTime;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 模板设计模块槽函数
    void on_btnLoadTemplate_clicked();
    void on_btnAddROI_clicked();
    void on_btnDeleteROI_clicked();
    void on_btnSaveConfig_clicked();
    void on_btnLoadConfig_clicked();
    void on_listROI_currentRowChanged(int currentRow);

    // 检测模块槽函数
    void on_btnLoadTestImage_clicked();
    void on_btnBrowseConfig_clicked();
    void on_btnStartDetection_clicked();
    void on_btnBatchDetection_clicked();

    // 结果模块槽函数
    void on_btnExportResults_clicked();
    void on_btnClearResults_clicked();

    // 菜单动作
    void on_actionOpenTemplate_triggered();
    void on_actionOpenTestImage_triggered();
    void on_actionSaveConfig_triggered();
    void on_actionLoadConfig_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();

protected:
    // 鼠标事件处理（用于ROI选择）
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Ui::MainWindow *ui;

    // 模板设计相关变量
    cv::Mat m_templateImage;           // 模板图像
    cv::Mat m_templateDisplay;         // 用于显示的模板图像（带ROI标记）
    QString m_templateImagePath;       // 模板图片路径
    QList<ROIRect> m_roiList;          // ROI列表
    int m_currentROIIndex;             // 当前选中的ROI索引
    bool m_isSelectingROI;             // 是否正在选择ROI
    QPoint m_roiStartPoint;            // ROI选择起始点
    QPoint m_roiEndPoint;              // ROI选择终点

    // 检测模块相关变量
    cv::Mat m_testImage;               // 待测图像
    cv::Mat m_resultImage;             // 结果图像
    QString m_testImagePath;           // 待测图片路径
    QString m_configFilePath;          // 配置文件路径
    QList<DetectionResult> m_resultList; // 检测结果列表
    int m_resultCount;                 // 结果计数

    // 辅助函数
    QImage mat2QImage(const cv::Mat &mat);
    cv::Mat QImage2Mat(const QImage &image);
    void displayTemplateImage();
    void displayTestImage();
    void displayResultImage();
    void updateROIList();
    void drawROIsOnTemplate();
    void addResultToTable(const DetectionResult &result);

    // 模板设计模块函数
    bool loadTemplateImage(const QString &path);
    void addROI(const cv::Rect &rect);
    void deleteROI(int index);
    bool saveConfigFile(const QString &path);
    bool loadConfigFile(const QString &path);

    // 检测模块函数
    bool loadTestImage(const QString &path);
    bool detectSkewAndCorrect(cv::Mat &image);
    cv::Point templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage);
    cv::Rect calculateDetectionRegion(const cv::Point &matchPoint, const cv::Rect &templateROI);
    bool detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore);
    void performDetection(const QString &imagePath);
};

#endif // MAINWINDOW_H