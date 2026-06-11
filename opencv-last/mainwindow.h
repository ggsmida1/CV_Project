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
#include <QRect>
#include <QPoint>
#include <QStyledItemDelegate>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 自定义 delegate：根据 item 的 Qt::UserRole 数据（检测结果值）来绘制绿/红背景
// 这是绕过 QSS 覆盖 item 背景色的最可靠方式
class ResultItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ResultItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

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

    // 自定义标题栏槽函数
    void on_btnMinimize_clicked();
    void on_btnMaximize_clicked();
    void on_btnClose_clicked();

protected:
    // 鼠标事件处理（用于ROI选择和窗口拖动）
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    Ui::MainWindow *ui;

    // 窗口拖动相关变量
    bool m_isDraggingWindow;
    QPoint m_dragStartPosition;
    int m_titleBarHeight;
    bool m_isMaximized;           // 当前是否最大化（手动追踪，避免FramelessWindowHint下isMaximized失效）
    QRect m_normalGeometry;       // 还原时用的窗口原始大小

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
    int m_resultCount;                 // ROI 结果计数（内部用）
    int m_imageResultCount;            // 图片检测次数（序号）

    // 辅助函数
    QImage mat2QImage(const cv::Mat &mat);
    cv::Mat QImage2Mat(const QImage &image);
    static cv::Mat imreadSafe(const QString &path); // 安全读取图片（支持中文路径）
    void displayTemplateImage();
    void displayTestImage();
    void displayResultImage();
    void updateROIList();
    void drawROIsOnTemplate();
    void addResultToTable(int imageId, const QList<DetectionResult> &roiResults);

    // 坐标映射：把 QLabel 控件坐标 -> 图像像素坐标（考虑缩放+居中偏移）
    // 返回 (-1,-1) 表示点击不在图像区域
    QPoint labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos);
    // 计算图像在 label 内的缩放比和偏移 (用于显示和坐标转换)
    void computeImageTransform(QLabel *label, const cv::Mat &image,
                               double &outScale, int &outOffsetX, int &outOffsetY);

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