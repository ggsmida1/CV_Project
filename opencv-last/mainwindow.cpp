#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QApplication>
#include <cmath>

// ResultItemDelegate 实现：根据 item 的 UserRole 数据绘制绿/红背景
// 仅用于第 3 列（检测结果列），0 = 正常（绿色）, 1 = 残缺（红色）
void ResultItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    int status = index.data(Qt::UserRole).toInt();
    QColor bgColor = (status == 1) ? QColor("#c0392b") : QColor("#27ae60");
    painter->fillRect(option.rect, bgColor);
    painter->setPen(QColor("#ffffff"));
    QTextOption textOpt(Qt::AlignCenter);
    painter->drawText(option.rect, index.data(Qt::DisplayRole).toString(), textOpt);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isDraggingWindow(false)
    , m_titleBarHeight(38)
    , m_isMaximized(false)
    , m_currentROIIndex(-1)
    , m_isSelectingROI(false)
    , m_resultCount(0)
{
    // 隐藏系统标题栏（无边框），保留最小化/最大化/关闭能力
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    // 保留一些默认窗口特性（如可调节大小）
    setAttribute(Qt::WA_TranslucentBackground, false);

    ui->setupUi(this);

    // 标题栏按钮的槽函数（on_btnMinimize_clicked 等）由 setupUi 中的
    // QMetaObject::connectSlotsByName 自动连接，无需手动 connect。
    // 如果手动再 connect 一次会导致每个点击触发两次，造成闪一下的假象。

    // 刷新按钮样式
    ui->btnMinimize->style()->unpolish(ui->btnMinimize);
    ui->btnMinimize->style()->polish(ui->btnMinimize);
    ui->btnMaximize->style()->unpolish(ui->btnMaximize);
    ui->btnMaximize->style()->polish(ui->btnMaximize);
    ui->btnClose->style()->unpolish(ui->btnClose);
    ui->btnClose->style()->polish(ui->btnClose);

    // 设置按钮属性（用于QSS样式区分）
    ui->btnDeleteROI->setProperty("warningBtn", true);
    ui->btnSaveConfig->setProperty("successBtn", true);
    ui->btnLoadConfig->setProperty("secondaryBtn", true);
    ui->btnBrowseConfig->setProperty("secondaryBtn", true);
    ui->btnStartDetection->setProperty("successBtn", true);
    ui->btnExportResults->setProperty("successBtn", true);
    ui->btnClearResults->setProperty("warningBtn", true);

    // 刷新样式
    ui->btnDeleteROI->style()->unpolish(ui->btnDeleteROI);
    ui->btnDeleteROI->style()->polish(ui->btnDeleteROI);
    ui->btnSaveConfig->style()->unpolish(ui->btnSaveConfig);
    ui->btnSaveConfig->style()->polish(ui->btnSaveConfig);
    ui->btnLoadConfig->style()->unpolish(ui->btnLoadConfig);
    ui->btnLoadConfig->style()->polish(ui->btnLoadConfig);
    ui->btnBrowseConfig->style()->unpolish(ui->btnBrowseConfig);
    ui->btnBrowseConfig->style()->polish(ui->btnBrowseConfig);
    ui->btnStartDetection->style()->unpolish(ui->btnStartDetection);
    ui->btnStartDetection->style()->polish(ui->btnStartDetection);
    ui->btnExportResults->style()->unpolish(ui->btnExportResults);
    ui->btnExportResults->style()->polish(ui->btnExportResults);
    ui->btnClearResults->style()->unpolish(ui->btnClearResults);
    ui->btnClearResults->style()->polish(ui->btnClearResults);

    // 初始化状态栏
    statusBar()->showMessage(QString::fromUtf8(u8"就绪"));

    // 隐藏行号列（vertical header），避免与"序号"列重复
    ui->tableResults->verticalHeader()->setVisible(false);

    // 只把 delegate 安装到第 3 列（检测结果列），其余列保持默认样式
    // （QSS 会覆盖 item->setBackground()，所以必须用 delegate 来保证颜色生效）
    ui->tableResults->setItemDelegateForColumn(3, new ResultItemDelegate(ui->tableResults));

    // 设置列宽
    ui->tableResults->setColumnWidth(0, 80);
    ui->tableResults->setColumnWidth(2, 200);
    ui->tableResults->setColumnWidth(3, 120);
    ui->tableResults->setColumnWidth(4, 200);
    // 第1列（图片名称）自动拉伸占据剩余空间
    ui->tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);

    // 设置表头为固定高度，避免不同系统默认值差异
    ui->tableResults->horizontalHeader()->setFixedHeight(32);
    // 设置默认行高，使表格视觉更协调
    ui->tableResults->verticalHeader()->setDefaultSectionSize(32);

    // 启用鼠标追踪
    setMouseTracking(true);

    // 关闭 scaledContents：确保 pixmap 以原始比例居中显示（而不是拉伸填满label）
    // 否则宽高比被破坏 + 鼠标坐标无法正确映射到图像像素
    ui->lblTemplateImage->setScaledContents(false);
    ui->lblTestImage->setScaledContents(false);
    ui->lblResultImage->setScaledContents(false);
}

// 最小化窗口
void MainWindow::on_btnMinimize_clicked()
{
    showMinimized();
}

// 最大化/还原窗口
// FramelessWindowHint + QMainWindow 组合下，setGeometry/resize 经常被布局系统强制还原
// 解决方案：先解除 minimumSize/maximumSize 限制，再用两种方式双重设置大小
void MainWindow::on_btnMaximize_clicked()
{
    if (m_isMaximized) {
        // 还原
        if (m_normalGeometry.isValid() && m_normalGeometry.width() > 100) {
            setMinimumSize(0, 0);
            setMaximumSize(16777215, 16777215);
            move(m_normalGeometry.topLeft());
            resize(m_normalGeometry.size());
            setGeometry(m_normalGeometry);
        } else {
            move(100, 100);
            resize(1400, 900);
            setGeometry(100, 100, 1400, 900);
        }
        ui->btnMaximize->setText(QString::fromUtf8(u8"▢"));
        m_isMaximized = false;
    } else {
        // 最大化
        m_normalGeometry = QRect(pos(), size());

        // 拿到屏幕大小
        int w = 1920;
        int h = 1040;
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            QRect r = screen->availableGeometry();
            w = r.width();
            h = r.height();
        }

        // 关键：先解除最小/最大尺寸限制，否则 resize 被布局系统强制还原
        setMinimumSize(0, 0);
        setMaximumSize(16777215, 16777215);

        // 两种方式设置大小，互相兜底
        move(0, 0);
        resize(w, h);
        setGeometry(0, 0, w, h);

        ui->btnMaximize->setText(QString::fromUtf8(u8"❐"));
        m_isMaximized = true;
    }

    // 强制刷新布局
    if (centralWidget()) {
        centralWidget()->setMinimumSize(0, 0);
        centralWidget()->setMaximumSize(16777215, 16777215);
        centralWidget()->updateGeometry();
    }
    update();
    repaint();
}

// 关闭窗口
void MainWindow::on_btnClose_clicked()
{
    close();
}

// 双击标题栏 - 切换最大化
void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= m_titleBarHeight) {
        on_btnMaximize_clicked();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ==================== 辅助函数 ====================

// OpenCV Mat 转 QImage
QImage MainWindow::mat2QImage(const cv::Mat &mat)
{
    if (mat.empty()) {
        return QImage();
    }
    
    if (mat.type() == CV_8UC1) {
        // 灰度图
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        // BGR转RGB
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        // BGRA转RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    
    return QImage();
}

// QImage 转 OpenCV Mat
cv::Mat MainWindow::QImage2Mat(const QImage &image)
{
    if (image.isNull()) {
        return cv::Mat();
    }
    
    cv::Mat mat;
    switch (image.format()) {
    case QImage::Format_Grayscale8:
        mat = cv::Mat(image.height(), image.width(), CV_8UC1, const_cast<uchar*>(image.bits()), image.bytesPerLine()).clone();
        break;
    case QImage::Format_RGB888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC3, const_cast<uchar*>(image.bits()), image.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        break;
    case QImage::Format_RGBA8888:
        mat = cv::Mat(image.height(), image.width(), CV_8UC4, const_cast<uchar*>(image.bits()), image.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGRA);
        break;
    default:
        QImage converted = image.convertToFormat(QImage::Format_RGB888);
        mat = cv::Mat(converted.height(), converted.width(), CV_8UC3, const_cast<uchar*>(converted.bits()), converted.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
        break;
    }
    
    return mat;
}

// 安全读取图片 —— 用 QFile 把文件读到内存，再用 cv::imdecode 解码
// 彻底绕过 OpenCV 在 Windows 上不支持中文路径的问题
cv::Mat MainWindow::imreadSafe(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return cv::Mat();
    }
    QByteArray data = file.readAll();
    file.close();
    if (data.isEmpty()) {
        return cv::Mat();
    }
    std::vector<uchar> buffer(data.begin(), data.end());
    return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

// 计算图像在 label 里的缩放比和偏移（图像按比例居中显示）
void MainWindow::computeImageTransform(QLabel *label, const cv::Mat &image,
                                       double &outScale, int &outOffsetX, int &outOffsetY)
{
    if (!label || image.empty()) {
        outScale = 1.0;
        outOffsetX = 0;
        outOffsetY = 0;
        return;
    }
    QSize labelSize = label->size();
    double scale = qMin(static_cast<double>(labelSize.width()) / image.cols,
                        static_cast<double>(labelSize.height()) / image.rows);
    int displayW = static_cast<int>(image.cols * scale);
    int displayH = static_cast<int>(image.rows * scale);
    outScale = scale;
    outOffsetX = (labelSize.width() - displayW) / 2;    // 图像在label里的x偏移
    outOffsetY = (labelSize.height() - displayH) / 2;   // 图像在label里的y偏移
}

// label 控件坐标 -> 图像像素坐标；返回 (-1,-1) 表示点不在图像区域
QPoint MainWindow::labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos)
{
    double scale = 1.0;
    int offsetX = 0, offsetY = 0;
    computeImageTransform(label, image, scale, offsetX, offsetY);

    // 减去图像在label内的偏移，再除以缩放比，得到图像像素坐标
    double imgX = (labelPos.x() - offsetX) / scale;
    double imgY = (labelPos.y() - offsetY) / scale;

    if (imgX < 0 || imgY < 0 || imgX >= image.cols || imgY >= image.rows) {
        return QPoint(-1, -1);
    }
    return QPoint(static_cast<int>(imgX), static_cast<int>(imgY));
}

// 显示模板图像
void MainWindow::displayTemplateImage()
{
    if (m_templateDisplay.empty()) {
        ui->lblTemplateImage->setText(QString::fromUtf8(u8"请加载模板图片"));
        return;
    }
    
    // 缩放图像以适应标签大小
    QSize labelSize = ui->lblTemplateImage->size();
    cv::Mat display;
    double scale = qMin(static_cast<double>(labelSize.width()) / m_templateDisplay.cols,
                        static_cast<double>(labelSize.height()) / m_templateDisplay.rows);
    cv::resize(m_templateDisplay, display, cv::Size(), scale, scale, cv::INTER_AREA);
    
    QImage qImg = mat2QImage(display);
    ui->lblTemplateImage->setPixmap(QPixmap::fromImage(qImg));
}

// 显示待测图像
void MainWindow::displayTestImage()
{
    if (m_testImage.empty()) {
        ui->lblTestImage->setText(QString::fromUtf8(u8"待测图片"));
        return;
    }
    
    QSize labelSize = ui->lblTestImage->size();
    cv::Mat display;
    double scale = qMin(static_cast<double>(labelSize.width()) / m_testImage.cols,
                        static_cast<double>(labelSize.height()) / m_testImage.rows);
    cv::resize(m_testImage, display, cv::Size(), scale, scale, cv::INTER_AREA);
    
    QImage qImg = mat2QImage(display);
    ui->lblTestImage->setPixmap(QPixmap::fromImage(qImg));
}

// 显示结果图像
void MainWindow::displayResultImage()
{
    if (m_resultImage.empty()) {
        ui->lblResultImage->setText(QString::fromUtf8(u8"检测结果"));
        return;
    }
    
    QSize labelSize = ui->lblResultImage->size();
    cv::Mat display;
    double scale = qMin(static_cast<double>(labelSize.width()) / m_resultImage.cols,
                        static_cast<double>(labelSize.height()) / m_resultImage.rows);
    cv::resize(m_resultImage, display, cv::Size(), scale, scale, cv::INTER_AREA);
    
    QImage qImg = mat2QImage(display);
    ui->lblResultImage->setPixmap(QPixmap::fromImage(qImg));
}

// 更新ROI列表
void MainWindow::updateROIList()
{
    // 记住当前选中行（删除后尝试保持选择）
    int currentRow = ui->listROI->currentRow();
    ui->listROI->blockSignals(true);  // 暂时屏蔽信号，避免 clear 触发 currentRowChanged
    ui->listROI->clear();
    for (int i = 0; i < m_roiList.size(); ++i) {
        QString itemText = QString("ROI %1: (%2,%3) %4x%5 - %6")
            .arg(i + 1)
            .arg(m_roiList[i].rect.x)
            .arg(m_roiList[i].rect.y)
            .arg(m_roiList[i].rect.width)
            .arg(m_roiList[i].rect.height)
            .arg(m_roiList[i].name);
        ui->listROI->addItem(itemText);
    }
    ui->listROI->blockSignals(false);

    // 重建后尝试选中合理的行
    if (m_roiList.size() > 0) {
        int selectRow = qBound(0, currentRow, m_roiList.size() - 1);
        ui->listROI->setCurrentRow(selectRow);
        m_currentROIIndex = selectRow;
    } else {
        m_currentROIIndex = -1;
    }

    ui->lblROIInfo->setText(QString::fromUtf8(u8"当前共 %1 个检测区域").arg(m_roiList.size()));
}

// 在模板图像上绘制ROI
void MainWindow::drawROIsOnTemplate()
{
    if (m_templateImage.empty()) {
        return;
    }
    
    m_templateDisplay = m_templateImage.clone();
    
    // 绘制所有ROI
    for (int i = 0; i < m_roiList.size(); ++i) {
        cv::Scalar color = (i == m_currentROIIndex) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::rectangle(m_templateDisplay, m_roiList[i].rect, color, 2);
        
        // 添加标签
        QString label = QString("ROI %1").arg(i + 1);
        cv::putText(m_templateDisplay, label.toStdString(), 
                    cv::Point(m_roiList[i].rect.x, m_roiList[i].rect.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
    
    displayTemplateImage();
}

// 添加结果到表格
void MainWindow::addResultToTable(const DetectionResult &result)
{
    int row = ui->tableResults->rowCount();
    ui->tableResults->insertRow(row);

    ui->tableResults->setItem(row, 0, new QTableWidgetItem(QString::number(result.id)));
    ui->tableResults->setItem(row, 1, new QTableWidgetItem(result.imageName));
    ui->tableResults->setItem(row, 2, new QTableWidgetItem(result.roiName));

    QTableWidgetItem *resultItem = new QTableWidgetItem(result.isDefective ? QString::fromUtf8(u8"残缺") : QString::fromUtf8(u8"正常"));
    // 用 UserRole 存储状态：0 = 正常（绿）, 1 = 残缺（红）
    // delegate 根据此值绘制绿/红背景色
    resultItem->setData(Qt::UserRole, result.isDefective ? 1 : 0);
    ui->tableResults->setItem(row, 3, resultItem);

    ui->tableResults->setItem(row, 4, new QTableWidgetItem(result.detectionTime));
}

// ==================== 模板设计模块 ====================

// 读取模板图片
bool MainWindow::loadTemplateImage(const QString &path)
{
    m_templateImage = imreadSafe(path);
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法加载模板图片！\n路径：%1\n\n提示：请确认文件格式为 PNG/JPG/BMP/TIF。").arg(path));
        return false;
    }
    
    m_templateImagePath = path;
    m_roiList.clear();
    m_currentROIIndex = -1;
    updateROIList();
    drawROIsOnTemplate();
    
    statusBar()->showMessage(QString::fromUtf8(u8"已加载模板: %1").arg(path));
    return true;
}

// 添加ROI区域
void MainWindow::addROI(const cv::Rect &rect)
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载模板图片！"));
        return;
    }
    
    // 验证ROI是否有效
    if (rect.width < 10 || rect.height < 10) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"检测区域太小，请重新选择！"));
        return;
    }
    
    // 确保ROI在图像范围内
    cv::Rect validRect = rect & cv::Rect(0, 0, m_templateImage.cols, m_templateImage.rows);
    
    ROIRect roi;
    roi.id = m_roiList.size() + 1;
    roi.rect = validRect;
    roi.name = QString::fromUtf8(u8"区域%1").arg(roi.id);
    roi.templateImage = m_templateImage(validRect).clone();
    
    m_roiList.append(roi);
    updateROIList();
    drawROIsOnTemplate();
    
    statusBar()->showMessage(QString::fromUtf8(u8"已添加检测区域: %1").arg(roi.name));
}

// 删除ROI
void MainWindow::deleteROI(int index)
{
    if (index >= 0 && index < m_roiList.size()) {
        m_roiList.removeAt(index);
        m_currentROIIndex = -1;
        updateROIList();
        drawROIsOnTemplate();
        statusBar()->showMessage(QString::fromUtf8(u8"已删除检测区域"));
    }
}

// 保存配置文件
bool MainWindow::saveConfigFile(const QString &path)
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载模板图片！"));
        return false;
    }
    
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先添加检测区域！"));
        return false;
    }

    QJsonObject root;

    // 将模板路径存为相对路径（相对于 samples 目录），便于跨机器共享
    // 例如：templates/template.png
    QFileInfo templateInfo(m_templateImagePath);
    QString relativePath = "templates/" + templateInfo.fileName();
    root["templatePath"] = relativePath;

    root["templateWidth"] = m_templateImage.cols;
    root["templateHeight"] = m_templateImage.rows;

    QJsonArray roiArray;
    for (const ROIRect &roi : m_roiList) {
        QJsonObject roiObj;
        roiObj["id"] = roi.id;
        roiObj["name"] = roi.name;
        roiObj["x"] = roi.rect.x;
        roiObj["y"] = roi.rect.y;
        roiObj["width"] = roi.rect.width;
        roiObj["height"] = roi.rect.height;

        // 将模板图像保存为Base64
        std::vector<uchar> buf;
        cv::imencode(".png", roi.templateImage, buf);
        QByteArray base64 = QByteArray::fromRawData(reinterpret_cast<const char*>(buf.data()), buf.size()).toBase64();
        roiObj["templateImage"] = QString(base64);

        roiArray.append(roiObj);
    }
    root["roiList"] = roiArray;

    QJsonDocument doc(root);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法保存配置文件！"));
        return false;
    }

    file.write(doc.toJson());
    file.close();

    statusBar()->showMessage(QString::fromUtf8(u8"配置已保存: %1").arg(path));
    return true;
}

// 加载配置文件
bool MainWindow::loadConfigFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法加载配置文件！"));
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"配置文件格式错误！"));
        return false;
    }

    QJsonObject root = doc.object();

    // 加载模板图片（路径是相对于 samples 目录的相对路径）
    QString templateRelativePath = root["templatePath"].toString();

    // 从可执行文件所在目录往上定位项目根目录
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }

    QString templateAbsolutePath = projectRoot + "/samples/" + templateRelativePath;

    if (!loadTemplateImage(templateAbsolutePath)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"),
            QString::fromUtf8(u8"无法加载模板图片！\n路径：%1\n\n提示：请确认 templates 目录下存在该图片。")
            .arg(templateAbsolutePath));
        return false;
    }
    
    // 加载ROI列表
    m_roiList.clear();
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
        
        // 从Base64恢复模板图像
        QByteArray base64 = roiObj["templateImage"].toString().toLatin1();
        QByteArray imageData = QByteArray::fromBase64(base64);
        std::vector<uchar> buf(imageData.begin(), imageData.end());
        roi.templateImage = cv::imdecode(buf, cv::IMREAD_COLOR);
        
        m_roiList.append(roi);
    }
    
    updateROIList();
    drawROIsOnTemplate();
    
    m_configFilePath = path;
    ui->txtConfigPath->setText(path);
    statusBar()->showMessage(QString::fromUtf8(u8"已加载配置: %1").arg(path));
    return true;
}

// ==================== 检测模块 ====================

// 加载待测图片
bool MainWindow::loadTestImage(const QString &path)
{
    m_testImage = imreadSafe(path);
    if (m_testImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法加载待测图片！\n路径：%1").arg(path));
        return false;
    }

    m_testImagePath = path;
    displayTestImage();
    statusBar()->showMessage(QString::fromUtf8(u8"已加载待测图片: %1").arg(path));
    return true;
}

// 检测倾斜并矫正
bool MainWindow::detectSkewAndCorrect(cv::Mat &image)
{
    // 转换为灰度图
    cv::Mat gray;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }

    // 二值化
    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    // 使用霍夫变换检测直线（更严格的参数，避免虚假倾斜）
    std::vector<cv::Vec4i> lines;
    int minLineLength = std::max(150, image.cols / 8);  // 线至少占图宽的1/8
    cv::HoughLinesP(binary, lines, 1, CV_PI / 180, 200, minLineLength, 20);

    if (lines.empty()) {
        return false;  // 未检测到倾斜
    }

    // 计算平均角度（只考虑接近水平的线，且多条线角度要一致）
    double angle = 0;
    int count = 0;
    std::vector<double> angles;
    for (const auto &line : lines) {
        double dx = line[2] - line[0];
        double dy = line[3] - line[1];
        double lineAngle = atan2(dy, dx) * 180 / CV_PI;

        // 只考虑接近水平的线（倾斜角度小于45度）
        if (fabs(lineAngle) < 45) {
            angles.push_back(lineAngle);
            angle += lineAngle;
            count++;
        }
    }

    if (count < 3) {  // 至少检测到3条线才认为是真实倾斜
        return false;
    }

    angle /= count;

    // 只有倾斜角度 >=3 度才矫正（避免拍摄抖动导致的虚假旋转）
    if (fabs(angle) < 3.0) {
        return false;
    }

    // 旋转矫正
    cv::Point2f center(image.cols / 2.0, image.rows / 2.0);
    cv::Mat rotMat = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::warpAffine(image, image, rotMat, image.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

    return true;
}

// 模板匹配定位
cv::Point MainWindow::templateMatch(const cv::Mat &testImage, const cv::Mat &templateImage)
{
    cv::Mat result;
    cv::matchTemplate(testImage, templateImage, result, cv::TM_CCOEFF_NORMED);
    
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    
    return maxLoc;
}

// 计算检测区域
cv::Rect MainWindow::calculateDetectionRegion(const cv::Point &matchPoint, const cv::Rect &templateROI)
{
    return cv::Rect(matchPoint.x + templateROI.x, 
                    matchPoint.y + templateROI.y,
                    templateROI.width, 
                    templateROI.height);
}

// 字符残缺检测（用 NCC 归一化互相关，对亮度/对比度/轻微位移鲁棒）
bool MainWindow::detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore)
{
    if (testRegion.empty() || templateRegion.empty()) {
        defectScore = 1.0;
        return true;
    }

    // 转灰度图
    cv::Mat testGray, templateGray;
    if (testRegion.channels() == 3) {
        cv::cvtColor(testRegion, testGray, cv::COLOR_BGR2GRAY);
    } else {
        testGray = testRegion.clone();
    }
    if (templateRegion.channels() == 3) {
        cv::cvtColor(templateRegion, templateGray, cv::COLOR_BGR2GRAY);
    } else {
        templateGray = templateRegion.clone();
    }

    // 确保尺寸一致（只有当差异明显时才resize，正常情况不需要）
    cv::Mat testResized;
    if (testGray.size() != templateGray.size()) {
        cv::resize(testGray, testResized, templateGray.size());
    } else {
        testResized = testGray;
    }

    // 轻度高斯模糊去噪（消除JPEG压缩噪声，但保留笔画结构）
    cv::GaussianBlur(testResized, testResized, cv::Size(3, 3), 0);
    cv::GaussianBlur(templateGray, templateGray, cv::Size(3, 3), 0);

    // 用归一化互相关 (NCC / TM_CCOEFF_NORMED) 做相似度比较
    // 分数范围 [-1, 1]，1 表示完全相同，0 表示不相关，-1 表示反相关
    // NCC 对亮度变化、对比度变化鲁棒，不像OTSU+absdiff对轻微位移极度敏感
    cv::Mat nccResult;
    cv::matchTemplate(testResized, templateGray, nccResult, cv::TM_CCOEFF_NORMED);
    double minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);

    // 残缺程度 = 1 - NCC分数，范围 [0, 2]
    //   完全相同的图 → maxVal ≈ 1.0 → score ≈ 0
    //   笔画有缺失   → maxVal ≈ 0.6~0.85 → score ≈ 0.15~0.4
    //   完全不同的图 → maxVal ≈ 0 → score ≈ 1
    defectScore = 1.0 - maxVal;

    // 阈值：NCC < 0.85 判残缺
    // （同一张图、轻微压缩噪声时 NCC 通常 > 0.95；真实残缺会低于 0.85）
    return defectScore > 0.15;
}

// 执行检测
void MainWindow::performDetection(const QString &imagePath)
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载配置文件或设置检测区域！"));
        return;
    }

    // 加载待测图片
    cv::Mat testImage = imreadSafe(imagePath);
    if (testImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法加载待测图片！\n路径：%1").arg(imagePath));
        return;
    }

    // 倾斜矫正（仅当检测到明显倾斜时才矫正）
    detectSkewAndCorrect(testImage);

    // 复制用于显示结果
    m_resultImage = testImage.clone();

    // 模板匹配粗略定位（用整个模板图在待测图中搜索）
    cv::Point matchPoint = templateMatch(testImage, m_templateImage);

    // 在结果图像上标记匹配位置
    cv::rectangle(m_resultImage, matchPoint,
                  cv::Point(matchPoint.x + m_templateImage.cols, matchPoint.y + m_templateImage.rows),
                  cv::Scalar(255, 0, 0), 2);

    // 对每个ROI进行检测
    QFileInfo fileInfo(imagePath);
    QString imageName = fileInfo.fileName();
    QString detectionTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    for (const ROIRect &roi : m_roiList) {
        // 粗略位置：matchPoint + roi.rect
        cv::Point roughCenter(matchPoint.x + roi.rect.x + roi.rect.width / 2,
                              matchPoint.y + roi.rect.y + roi.rect.height / 2);

        // --- ROI 级精对齐：在粗略位置附近 ±N 像素内做小范围模板匹配 ---
        // 搜索范围：粗略位置中心 ± halfSearch 的正方形区域
        int halfSearch = std::min(30, std::min(roi.rect.width, roi.rect.height) / 3);

        // 提取搜索区域（待测图上的一块更大的区域）
        cv::Rect searchRect(
            std::max(0, roughCenter.x - roi.rect.width / 2 - halfSearch),
            std::max(0, roughCenter.y - roi.rect.height / 2 - halfSearch),
            roi.rect.width + 2 * halfSearch,
            roi.rect.height + 2 * halfSearch
        );
        // 确保不越界
        searchRect = searchRect & cv::Rect(0, 0, testImage.cols, testImage.rows);

        // 在搜索区域内匹配 roi.templateImage
        cv::Mat testSearch = testImage(searchRect);
        cv::Mat roiSearch;
        if (roi.templateImage.channels() == 3) {
            roiSearch = roi.templateImage.clone();
        } else {
            roiSearch = roi.templateImage.clone();
        }

        cv::Point preciseOffset(0, 0);
        if (searchRect.width >= roi.rect.width && searchRect.height >= roi.rect.height) {
            cv::Mat nccResult;
            cv::matchTemplate(testSearch, roiSearch, nccResult, cv::TM_CCOEFF_NORMED);
            double minVal, maxVal;
            cv::Point minLoc, maxLoc;
            cv::minMaxLoc(nccResult, &minVal, &maxVal, &minLoc, &maxLoc);
            preciseOffset = maxLoc;  // roi.templateImage 在 searchRect 内的最佳匹配位置
        }

        // 精对齐后的 ROI
        cv::Rect testROI(
            searchRect.x + preciseOffset.x,
            searchRect.y + preciseOffset.y,
            roi.rect.width,
            roi.rect.height
        );
        testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);

        if (testROI.width != roi.rect.width || testROI.height != roi.rect.height) {
            // 越界了，退回到粗略位置
            testROI = cv::Rect(
                std::max(0, matchPoint.x + roi.rect.x),
                std::max(0, matchPoint.y + roi.rect.y),
                roi.rect.width,
                roi.rect.height
            );
            testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);
        }

        if (testROI.area() <= 0) {
            continue;
        }

        // 提取检测区域
        cv::Mat testRegion = testImage(testROI);

        // 进行残缺检测
        double defectScore;
        bool isDefective = detectCharacterDefect(testRegion, roi.templateImage, defectScore);

        // 在结果图像上标记
        cv::Scalar color = isDefective ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
        cv::rectangle(m_resultImage, testROI, color, 2);

        QString label = isDefective
            ? QString("Defect: %1%").arg(defectScore * 100, 0, 'f', 1)
            : QString("OK (%1%)").arg((1.0 - defectScore) * 100, 0, 'f', 1);
        cv::putText(m_resultImage, label.toStdString(),
                    cv::Point(testROI.x, testROI.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);

        // 记录结果
        DetectionResult result;
        result.id = ++m_resultCount;
        result.imageName = imageName;
        result.roiName = roi.name;
        result.isDefective = isDefective;
        result.defectScore = defectScore;
        result.detectionTime = detectionTime;

        m_resultList.append(result);
        addResultToTable(result);
    }

    displayResultImage();
    statusBar()->showMessage(QString::fromUtf8(u8"检测完成: %1").arg(imageName));
}

// ==================== 槽函数实现 ====================

// 模板设计模块槽函数
void MainWindow::on_btnLoadTemplate_clicked()
{
    // 从可执行文件所在目录往上定位 samples 目录
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/templates").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/templates";

    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"选择模板图片"),
                                                 defaultDir,
                                                 QString::fromUtf8(u8"图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (!path.isEmpty()) {
        loadTemplateImage(path);
    }
}

void MainWindow::on_btnAddROI_clicked()
{
    if (m_templateImage.empty()) {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载模板图片，然后在图片上拖动鼠标选择检测区域。"));
        return;
    }
    m_isSelectingROI = true;
    statusBar()->showMessage(QString::fromUtf8(u8"请在模板图片上拖动鼠标选择检测区域..."));
}

void MainWindow::on_btnDeleteROI_clicked()
{
    int currentRow = ui->listROI->currentRow();
    if (currentRow >= 0) {
        deleteROI(currentRow);
    } else {
        QMessageBox::information(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先选择要删除的检测区域！"));
    }
}

void MainWindow::on_btnSaveConfig_clicked()
{
    // 定位 samples/configs/ 目录（从 exe 目录往上找项目根）
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/configs").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/configs";

    // 确保 configs 目录存在
    QDir().mkpath(defaultDir);

    // 文件名使用模板图片名
    QString defaultName;
    if (!m_templateImagePath.isEmpty()) {
        QFileInfo templateInfo(m_templateImagePath);
        defaultName = templateInfo.completeBaseName() + ".json";
    } else {
        defaultName = "config.json";
    }

    QString defaultPath = defaultDir + "/" + defaultName;

    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8(u8"保存配置文件"),
                                                 defaultPath,
                                                 QString::fromUtf8(u8"JSON文件 (*.json)"));
    if (!path.isEmpty()) {
        if (!path.endsWith(".json")) {
            path += ".json";
        }
        saveConfigFile(path);
    }
}

void MainWindow::on_btnLoadConfig_clicked()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/configs").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/configs";

    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"选择配置文件"),
                                                 defaultDir,
                                                 QString::fromUtf8(u8"JSON文件 (*.json)"));
    if (!path.isEmpty()) {
        loadConfigFile(path);
    }
}

void MainWindow::on_listROI_currentRowChanged(int currentRow)
{
    m_currentROIIndex = currentRow;
    drawROIsOnTemplate();
}

// 检测模块槽函数
void MainWindow::on_btnLoadTestImage_clicked()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/test_images").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/test_images";

    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"选择待测图片"),
                                                 defaultDir,
                                                 QString::fromUtf8(u8"图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (!path.isEmpty()) {
        loadTestImage(path);
    }
}

void MainWindow::on_btnBrowseConfig_clicked()
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/configs").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/configs";

    QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8(u8"选择配置文件"),
                                                 defaultDir,
                                                 QString::fromUtf8(u8"JSON文件 (*.json)"));
    if (!path.isEmpty()) {
        loadConfigFile(path);
    }
}

void MainWindow::on_btnStartDetection_clicked()
{
    if (m_testImage.empty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载待测图片！"));
        return;
    }
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载配置文件！"));
        return;
    }

    performDetection(m_testImagePath);
}

void MainWindow::on_btnBatchDetection_clicked()
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8(u8"提示"), QString::fromUtf8(u8"请先加载配置文件！"));
        return;
    }

    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples/test_images").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    QString defaultDir = projectRoot + "/samples/test_images";

    QStringList paths = QFileDialog::getOpenFileNames(this, QString::fromUtf8(u8"选择待测图片"),
                                                      defaultDir,
                                                      QString::fromUtf8(u8"图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (paths.isEmpty()) {
        return;
    }

    for (const QString &path : paths) {
        performDetection(path);
    }

    QMessageBox::information(this, QString::fromUtf8(u8"完成"), QString::fromUtf8(u8"批量检测完成，共检测 %1 张图片。").arg(paths.size()));
}

// 结果模块槽函数
void MainWindow::on_btnExportResults_clicked()
{
    QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8(u8"导出结果"), "",
                                                 QString::fromUtf8(u8"CSV文件 (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    if (!path.endsWith(".csv")) {
        path += ".csv";
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"错误"), QString::fromUtf8(u8"无法创建导出文件！"));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << QString::fromUtf8(u8"序号,图片名称,检测区域,检测结果,残缺程度,检测时间\n");

    for (const DetectionResult &result : m_resultList) {
        out << result.id << ","
            << result.imageName << ","
            << result.roiName << ","
            << (result.isDefective ? QString::fromUtf8(u8"残缺") : QString::fromUtf8(u8"正常")) << ","
            << QString::number(result.defectScore * 100, 'f', 2) << "%,"
            << result.detectionTime << "\n";
    }

    file.close();
    QMessageBox::information(this, QString::fromUtf8(u8"完成"), QString::fromUtf8(u8"结果已导出到: %1").arg(path));
}

void MainWindow::on_btnClearResults_clicked()
{
    m_resultList.clear();
    m_resultCount = 0;
    ui->tableResults->setRowCount(0);
    statusBar()->showMessage(QString::fromUtf8(u8"结果已清空"));
}

// ==================== 鼠标事件处理 ====================

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    // 检查是否点击在标题栏区域（用于拖动窗口）
    if (event->button() == Qt::LeftButton && event->pos().y() <= m_titleBarHeight) {
        m_isDraggingWindow = true;
        m_dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    // ROI选择逻辑 - 把label坐标先转换为图像像素坐标（考虑居中偏移+缩放）
    if (m_isSelectingROI && event->button() == Qt::LeftButton) {
        QLabel *lblTemplate = ui->lblTemplateImage;
        QPoint labelPos = lblTemplate->mapFromGlobal(event->globalPos());
        QPoint imgPos = labelToImagePos(lblTemplate, m_templateDisplay, labelPos);
        if (imgPos.x() >= 0) {
            m_roiStartPoint = imgPos;   // 存储为图像像素坐标
            m_roiEndPoint = imgPos;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 窗口拖动逻辑
    if (m_isDraggingWindow && (event->buttons() & Qt::LeftButton)) {
        if (!isMaximized()) {
            move(event->globalPos() - m_dragStartPosition);
        }
        event->accept();
        return;
    }

    // ROI选择逻辑 - 实时绘制选择框（坐标是图像像素坐标）
    if (m_isSelectingROI && !m_roiStartPoint.isNull() && !m_templateDisplay.empty()) {
        QLabel *lblTemplate = ui->lblTemplateImage;
        QPoint labelPos = lblTemplate->mapFromGlobal(event->globalPos());
        QPoint imgPos = labelToImagePos(lblTemplate, m_templateDisplay, labelPos);
        if (imgPos.x() >= 0) {
            m_roiEndPoint = imgPos;
        }

        // 在原图上画ROI框（已存成图像像素坐标，直接画即可）
        cv::Mat display = m_templateDisplay.clone();
        int x1 = qMin(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y1 = qMin(m_roiStartPoint.y(), m_roiEndPoint.y());
        int x2 = qMax(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y2 = qMax(m_roiStartPoint.y(), m_roiEndPoint.y());
        cv::rectangle(display, cv::Point(x1, y1), cv::Point(x2, y2),
                      cv::Scalar(0, 0, 255), 2);

        // 按比例缩放到label大小后显示
        double scale = 1.0;
        int offX = 0, offY = 0;
        computeImageTransform(lblTemplate, m_templateDisplay, scale, offX, offY);
        cv::Mat resized;
        cv::resize(display, resized, cv::Size(), scale, scale, cv::INTER_AREA);
        QImage qImg = mat2QImage(resized);
        lblTemplate->setPixmap(QPixmap::fromImage(qImg));
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    // 结束窗口拖动
    if (event->button() == Qt::LeftButton && m_isDraggingWindow) {
        m_isDraggingWindow = false;
        event->accept();
        return;
    }

    // ROI选择逻辑 - 坐标已是图像像素坐标，直接用
    if (m_isSelectingROI && event->button() == Qt::LeftButton && !m_roiStartPoint.isNull()) {
        m_isSelectingROI = false;

        int x1 = qMin(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y1 = qMin(m_roiStartPoint.y(), m_roiEndPoint.y());
        int x2 = qMax(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y2 = qMax(m_roiStartPoint.y(), m_roiEndPoint.y());
        if (x2 > x1 && y2 > y1) {
            cv::Rect rect(x1, y1, x2 - x1, y2 - y1);
            addROI(rect);
        }

        m_roiStartPoint = QPoint();
        m_roiEndPoint = QPoint();
    }
    QMainWindow::mouseReleaseEvent(event);
}