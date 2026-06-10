#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_currentROIIndex(-1)
    , m_isSelectingROI(false)
    , m_resultCount(0)
{
    ui->setupUi(this);
    
    // 初始化状态栏
    statusBar()->showMessage("就绪");
    
    // 初始化表格
    ui->tableResults->setColumnWidth(0, 60);
    ui->tableResults->setColumnWidth(1, 200);
    ui->tableResults->setColumnWidth(2, 150);
    ui->tableResults->setColumnWidth(3, 100);
    ui->tableResults->setColumnWidth(4, 180);
    
    // 启用鼠标追踪
    setMouseTracking(true);
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

// 显示模板图像
void MainWindow::displayTemplateImage()
{
    if (m_templateDisplay.empty()) {
        ui->lblTemplateImage->setText("请加载模板图片");
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
        ui->lblTestImage->setText("待测图片");
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
        ui->lblResultImage->setText("检测结果");
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
    
    ui->lblROIInfo->setText(QString("检测区域列表：共 %1 个区域").arg(m_roiList.size()));
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
    
    QTableWidgetItem *resultItem = new QTableWidgetItem(result.isDefective ? "残缺" : "正常");
    resultItem->setBackground(result.isDefective ? QBrush(Qt::red) : QBrush(Qt::green));
    ui->tableResults->setItem(row, 3, resultItem);
    
    ui->tableResults->setItem(row, 4, new QTableWidgetItem(result.detectionTime));
}

// ==================== 模板设计模块 ====================

// 读取模板图片
bool MainWindow::loadTemplateImage(const QString &path)
{
    m_templateImage = cv::imread(path.toStdString());
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, "错误", "无法加载模板图片！");
        return false;
    }
    
    m_templateImagePath = path;
    m_roiList.clear();
    m_currentROIIndex = -1;
    updateROIList();
    drawROIsOnTemplate();
    
    statusBar()->showMessage(QString("已加载模板: %1").arg(path));
    return true;
}

// 添加ROI区域
void MainWindow::addROI(const cv::Rect &rect)
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, "提示", "请先加载模板图片！");
        return;
    }
    
    // 验证ROI是否有效
    if (rect.width < 10 || rect.height < 10) {
        QMessageBox::warning(this, "提示", "检测区域太小，请重新选择！");
        return;
    }
    
    // 确保ROI在图像范围内
    cv::Rect validRect = rect & cv::Rect(0, 0, m_templateImage.cols, m_templateImage.rows);
    
    ROIRect roi;
    roi.id = m_roiList.size() + 1;
    roi.rect = validRect;
    roi.name = QString("区域%1").arg(roi.id);
    roi.templateImage = m_templateImage(validRect).clone();
    
    m_roiList.append(roi);
    updateROIList();
    drawROIsOnTemplate();
    
    statusBar()->showMessage(QString("已添加检测区域: %1").arg(roi.name));
}

// 删除ROI
void MainWindow::deleteROI(int index)
{
    if (index >= 0 && index < m_roiList.size()) {
        m_roiList.removeAt(index);
        m_currentROIIndex = -1;
        updateROIList();
        drawROIsOnTemplate();
        statusBar()->showMessage("已删除检测区域");
    }
}

// 保存配置文件
bool MainWindow::saveConfigFile(const QString &path)
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, "提示", "请先加载模板图片！");
        return false;
    }
    
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先添加检测区域！");
        return false;
    }
    
    QJsonObject root;
    root["templatePath"] = m_templateImagePath;
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
        QMessageBox::warning(this, "错误", "无法保存配置文件！");
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    statusBar()->showMessage(QString("配置已保存: %1").arg(path));
    return true;
}

// 加载配置文件
bool MainWindow::loadConfigFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法加载配置文件！");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "错误", "配置文件格式错误！");
        return false;
    }
    
    QJsonObject root = doc.object();
    
    // 加载模板图片
    QString templatePath = root["templatePath"].toString();
    if (!loadTemplateImage(templatePath)) {
        // 尝试使用相对路径
        QFileInfo configInfo(path);
        QString absoluteTemplatePath = configInfo.absolutePath() + "/" + QFileInfo(templatePath).fileName();
        if (!loadTemplateImage(absoluteTemplatePath)) {
            QMessageBox::warning(this, "错误", "无法加载模板图片，请确保图片路径正确！");
            return false;
        }
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
    statusBar()->showMessage(QString("已加载配置: %1").arg(path));
    return true;
}

// ==================== 检测模块 ====================

// 加载待测图片
bool MainWindow::loadTestImage(const QString &path)
{
    m_testImage = cv::imread(path.toStdString());
    if (m_testImage.empty()) {
        QMessageBox::warning(this, "错误", "无法加载待测图片！");
        return false;
    }
    
    m_testImagePath = path;
    displayTestImage();
    statusBar()->showMessage(QString("已加载待测图片: %1").arg(path));
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
    
    // 使用霍夫变换检测直线
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(binary, lines, 1, CV_PI / 180, 100, 100, 10);
    
    if (lines.empty()) {
        return false;  // 未检测到倾斜
    }
    
    // 计算平均角度
    double angle = 0;
    int count = 0;
    for (const auto &line : lines) {
        double dx = line[2] - line[0];
        double dy = line[3] - line[1];
        double lineAngle = atan2(dy, dx) * 180 / CV_PI;
        
        // 只考虑接近水平的线（倾斜角度小于45度）
        if (fabs(lineAngle) < 45) {
            angle += lineAngle;
            count++;
        }
    }
    
    if (count == 0) {
        return false;
    }
    
    angle /= count;
    
    // 如果倾斜角度很小，不需要矫正
    if (fabs(angle) < 0.5) {
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

// 字符残缺检测
bool MainWindow::detectCharacterDefect(const cv::Mat &testRegion, const cv::Mat &templateRegion, double &defectScore)
{
    if (testRegion.empty() || templateRegion.empty()) {
        defectScore = 1.0;
        return true;  // 视为残缺
    }
    
    // 确保尺寸一致
    cv::Mat testResized;
    if (testRegion.size() != templateRegion.size()) {
        cv::resize(testRegion, testResized, templateRegion.size());
    } else {
        testResized = testRegion.clone();
    }
    
    // 转换为灰度图
    cv::Mat testGray, templateGray;
    if (testResized.channels() == 3) {
        cv::cvtColor(testResized, testGray, cv::COLOR_BGR2GRAY);
    } else {
        testGray = testResized;
    }
    if (templateRegion.channels() == 3) {
        cv::cvtColor(templateRegion, templateGray, cv::COLOR_BGR2GRAY);
    } else {
        templateGray = templateRegion;
    }
    
    // 高斯模糊去噪
    cv::GaussianBlur(testGray, testGray, cv::Size(3, 3), 0);
    cv::GaussianBlur(templateGray, templateGray, cv::Size(3, 3), 0);
    
    // 二值化
    cv::Mat testBinary, templateBinary;
    cv::threshold(testGray, testBinary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    cv::threshold(templateGray, templateBinary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    
    // 计算差异
    cv::Mat diff;
    cv::absdiff(testBinary, templateBinary, diff);
    
    // 形态学操作，去除噪声
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(diff, diff, cv::MORPH_OPEN, kernel);
    
    // 计算差异比例
    int totalPixels = diff.rows * diff.cols;
    int diffPixels = cv::countNonZero(diff);
    defectScore = static_cast<double>(diffPixels) / totalPixels;
    
    // 设定阈值，超过阈值视为残缺
    double threshold = 0.05;  // 5%差异阈值
    return defectScore > threshold;
}

// 执行检测
void MainWindow::performDetection(const QString &imagePath)
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先加载配置文件或设置检测区域！");
        return;
    }
    
    // 加载待测图片
    cv::Mat testImage = cv::imread(imagePath.toStdString());
    if (testImage.empty()) {
        QMessageBox::warning(this, "错误", "无法加载待测图片！");
        return;
    }
    
    // 倾斜矫正
    detectSkewAndCorrect(testImage);
    
    // 复制用于显示结果
    m_resultImage = testImage.clone();
    
    // 模板匹配定位（使用整个模板图像）
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
        // 计算待测图像中的检测区域
        cv::Rect testROI = calculateDetectionRegion(matchPoint, roi.rect);
        
        // 确保区域在图像范围内
        testROI = testROI & cv::Rect(0, 0, testImage.cols, testImage.rows);
        
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
        
        QString label = isDefective ? QString("Defect: %1%").arg(defectScore * 100, 0, 'f', 1) : "OK";
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
    statusBar()->showMessage(QString("检测完成: %1").arg(imageName));
}

// ==================== 槽函数实现 ====================

// 模板设计模块槽函数
void MainWindow::on_btnLoadTemplate_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "选择模板图片", "", 
                                                 "图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (!path.isEmpty()) {
        loadTemplateImage(path);
    }
}

void MainWindow::on_btnAddROI_clicked()
{
    if (m_templateImage.empty()) {
        QMessageBox::information(this, "提示", "请先加载模板图片，然后在图片上拖动鼠标选择检测区域。");
        return;
    }
    m_isSelectingROI = true;
    statusBar()->showMessage("请在模板图片上拖动鼠标选择检测区域...");
}

void MainWindow::on_btnDeleteROI_clicked()
{
    int currentRow = ui->listROI->currentRow();
    if (currentRow >= 0) {
        deleteROI(currentRow);
    } else {
        QMessageBox::information(this, "提示", "请先选择要删除的检测区域！");
    }
}

void MainWindow::on_btnSaveConfig_clicked()
{
    QString path = QFileDialog::getSaveFileName(this, "保存配置文件", "", 
                                                 "JSON文件 (*.json)");
    if (!path.isEmpty()) {
        if (!path.endsWith(".json")) {
            path += ".json";
        }
        saveConfigFile(path);
    }
}

void MainWindow::on_btnLoadConfig_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "选择配置文件", "", 
                                                 "JSON文件 (*.json)");
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
    QString path = QFileDialog::getOpenFileName(this, "选择待测图片", "", 
                                                 "图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (!path.isEmpty()) {
        loadTestImage(path);
    }
}

void MainWindow::on_btnBrowseConfig_clicked()
{
    QString path = QFileDialog::getOpenFileName(this, "选择配置文件", "", 
                                                 "JSON文件 (*.json)");
    if (!path.isEmpty()) {
        loadConfigFile(path);
    }
}

void MainWindow::on_btnStartDetection_clicked()
{
    if (m_testImage.empty()) {
        QMessageBox::warning(this, "提示", "请先加载待测图片！");
        return;
    }
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先加载配置文件！");
        return;
    }
    
    performDetection(m_testImagePath);
}

void MainWindow::on_btnBatchDetection_clicked()
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先加载配置文件！");
        return;
    }
    
    QStringList paths = QFileDialog::getOpenFileNames(this, "选择待测图片", "", 
                                                       "图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (paths.isEmpty()) {
        return;
    }
    
    for (const QString &path : paths) {
        performDetection(path);
    }
    
    QMessageBox::information(this, "完成", QString("批量检测完成，共检测 %1 张图片。").arg(paths.size()));
}

// 结果模块槽函数
void MainWindow::on_btnExportResults_clicked()
{
    QString path = QFileDialog::getSaveFileName(this, "导出结果", "", 
                                                 "CSV文件 (*.csv)");
    if (path.isEmpty()) {
        return;
    }
    
    if (!path.endsWith(".csv")) {
        path += ".csv";
    }
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建导出文件！");
        return;
    }
    
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "序号,图片名称,检测区域,检测结果,残缺程度,检测时间\n";
    
    for (const DetectionResult &result : m_resultList) {
        out << result.id << ","
            << result.imageName << ","
            << result.roiName << ","
            << (result.isDefective ? "残缺" : "正常") << ","
            << QString::number(result.defectScore * 100, 'f', 2) << "%,"
            << result.detectionTime << "\n";
    }
    
    file.close();
    QMessageBox::information(this, "完成", QString("结果已导出到: %1").arg(path));
}

void MainWindow::on_btnClearResults_clicked()
{
    m_resultList.clear();
    m_resultCount = 0;
    ui->tableResults->setRowCount(0);
    statusBar()->showMessage("结果已清空");
}

// 菜单动作
void MainWindow::on_actionOpenTemplate_triggered()
{
    on_btnLoadTemplate_clicked();
}

void MainWindow::on_actionOpenTestImage_triggered()
{
    on_btnLoadTestImage_clicked();
}

void MainWindow::on_actionSaveConfig_triggered()
{
    on_btnSaveConfig_clicked();
}

void MainWindow::on_actionLoadConfig_triggered()
{
    on_btnLoadConfig_clicked();
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "关于", 
        "残缺字符检测系统 v1.0\n\n"
        "功能说明：\n"
        "1. 模板设计：加载模板图片，设置检测区域，保存配置\n"
        "2. 字符检测：加载待测图片，自动进行倾斜矫正、模板匹配、残缺检测\n"
        "3. 结果导出：检测结果可导出为CSV文件\n\n"
        "开发者：计算机视觉课程大作业");
}

// ==================== 鼠标事件处理 ====================

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (m_isSelectingROI && event->button() == Qt::LeftButton) {
        // 检查是否点击在模板图像显示区域
        QLabel *lblTemplate = ui->lblTemplateImage;
        QPoint globalPos = event->globalPos();
        QPoint labelPos = lblTemplate->mapFromGlobal(globalPos);
        
        if (labelPos.x() >= 0 && labelPos.y() >= 0 && 
            labelPos.x() < lblTemplate->width() && labelPos.y() < lblTemplate->height()) {
            m_roiStartPoint = labelPos;
            m_roiEndPoint = labelPos;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelectingROI && !m_roiStartPoint.isNull()) {
        QLabel *lblTemplate = ui->lblTemplateImage;
        QPoint globalPos = event->globalPos();
        QPoint labelPos = lblTemplate->mapFromGlobal(globalPos);
        
        // 限制在标签范围内
        labelPos.setX(qBound(0, labelPos.x(), lblTemplate->width()));
        labelPos.setY(qBound(0, labelPos.y(), lblTemplate->height()));
        
        m_roiEndPoint = labelPos;
        
        // 绘制选择框
        if (!m_templateDisplay.empty()) {
            // 计算缩放比例
            QSize labelSize = lblTemplate->size();
            double scale = qMin(static_cast<double>(labelSize.width()) / m_templateImage.cols,
                                static_cast<double>(labelSize.height()) / m_templateImage.rows);
            
            // 在显示图像上绘制选择框
            cv::Mat display = m_templateDisplay.clone();
            cv::Rect scaledRect(
                static_cast<int>(qMin(m_roiStartPoint.x(), m_roiEndPoint.x()) / scale),
                static_cast<int>(qMin(m_roiStartPoint.y(), m_roiEndPoint.y()) / scale),
                static_cast<int>(abs(m_roiEndPoint.x() - m_roiStartPoint.x()) / scale),
                static_cast<int>(abs(m_roiEndPoint.y() - m_roiStartPoint.y()) / scale)
            );
            cv::rectangle(display, scaledRect, cv::Scalar(255, 255, 0), 2);
            
            // 缩放显示
            cv::Mat resized;
            cv::resize(display, resized, cv::Size(), scale, scale, cv::INTER_AREA);
            QImage qImg = mat2QImage(resized);
            lblTemplate->setPixmap(QPixmap::fromImage(qImg));
        }
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_isSelectingROI && event->button() == Qt::LeftButton && !m_roiStartPoint.isNull()) {
        m_isSelectingROI = false;
        
        // 计算实际的ROI坐标
        QLabel *lblTemplate = ui->lblTemplateImage;
        QSize labelSize = lblTemplate->size();
        double scale = qMin(static_cast<double>(labelSize.width()) / m_templateImage.cols,
                            static_cast<double>(labelSize.height()) / m_templateImage.rows);
        
        int x1 = static_cast<int>(qMin(m_roiStartPoint.x(), m_roiEndPoint.x()) / scale);
        int y1 = static_cast<int>(qMin(m_roiStartPoint.y(), m_roiEndPoint.y()) / scale);
        int x2 = static_cast<int>(qMax(m_roiStartPoint.x(), m_roiEndPoint.x()) / scale);
        int y2 = static_cast<int>(qMax(m_roiStartPoint.y(), m_roiEndPoint.y()) / scale);
        
        cv::Rect rect(x1, y1, x2 - x1, y2 - y1);
        addROI(rect);
        
        m_roiStartPoint = QPoint();
        m_roiEndPoint = QPoint();
    }
    QMainWindow::mouseReleaseEvent(event);
}