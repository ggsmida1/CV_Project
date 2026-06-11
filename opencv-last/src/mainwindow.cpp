#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "imageutil.h"
#include "detector.h"
#include "configmanager.h"
#include "resultmanager.h"
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QApplication>
#include <QTextOption>
#include <QPushButton>
#include <QtMath>

static void refreshStyle(QPushButton *btn)
{
    btn->style()->unpolish(btn);
    btn->style()->polish(btn);
}

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
    , m_imageResultCount(0)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    ui->    setupUi(this);

    refreshStyle(ui->btnMinimize);
    refreshStyle(ui->btnMaximize);
    refreshStyle(ui->btnClose);

    ui->btnDeleteROI->setProperty("warningBtn", true);

    ui->btnSaveConfig->setProperty("successBtn", true);
    ui->btnLoadConfig->setProperty("secondaryBtn", true);
    ui->btnBrowseConfig->setProperty("secondaryBtn", true);
    ui->btnStartDetection->setProperty("successBtn", true);
    ui->btnExportResults->setProperty("successBtn", true);
    ui->btnClearResults->setProperty("warningBtn", true);

    refreshStyle(ui->btnDeleteROI);
    refreshStyle(ui->btnSaveConfig);
    refreshStyle(ui->btnLoadConfig);
    refreshStyle(ui->btnBrowseConfig);
    refreshStyle(ui->btnStartDetection);
    refreshStyle(ui->btnExportResults);
    refreshStyle(ui->btnClearResults);

    statusBar()->showMessage(QStringLiteral("就绪"));

    ui->tableResults->verticalHeader()->setVisible(false);
    ui->tableResults->setItemDelegateForColumn(3, new ResultItemDelegate(ui->tableResults));

    ui->tableResults->setColumnWidth(0, 80);
    ui->tableResults->setColumnWidth(2, 200);
    ui->tableResults->setColumnWidth(3, 120);
    ui->tableResults->setColumnWidth(4, 200);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    ui->tableResults->horizontalHeader()->setFixedHeight(38);
    ui->tableResults->verticalHeader()->setDefaultSectionSize(32);

    setMouseTracking(true);

    ui->lblTemplateImage->setScaledContents(false);
    ui->lblTestImage->setScaledContents(false);
    ui->lblResultImage->setScaledContents(false);

    // 标题栏小星星
    QPixmap starPix(24, 24);
    starPix.fill(Qt::transparent);
    {
        QPainter p(&starPix);
        p.setRenderHint(QPainter::Antialiasing);
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            double r = (i % 2 == 0) ? 10.0 : 4.5;
            double a = -90.0 + i * 36.0;
            star << QPointF(12.0 + r * qCos(qDegreesToRadians(a)),
                            12.0 + r * qSin(qDegreesToRadians(a)));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#f39c12"));
        p.drawPolygon(star);
    }
    ui->lblTitleIcon->setPixmap(starPix);
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ==================== 窗口管理 ====================

void MainWindow::on_btnMinimize_clicked() { showMinimized(); }

void MainWindow::on_btnMaximize_clicked()
{
    if (m_isMaximized) {
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
        ui->btnMaximize->setText(QStringLiteral("▢"));
        m_isMaximized = false;
    } else {
        m_normalGeometry = QRect(pos(), size());
        int w = 1920, h = 1040;
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen) screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect r = screen->availableGeometry();
            w = r.width(); h = r.height();
        }
        setMinimumSize(0, 0);
        setMaximumSize(16777215, 16777215);
        move(0, 0);
        resize(w, h);
        setGeometry(0, 0, w, h);
        ui->btnMaximize->setText(QStringLiteral("❐"));
        m_isMaximized = true;
    }
    if (centralWidget()) {
        centralWidget()->setMinimumSize(0, 0);
        centralWidget()->setMaximumSize(16777215, 16777215);
        centralWidget()->updateGeometry();
    }
    update();
    repaint();
}

void MainWindow::on_btnClose_clicked() { close(); }

void MainWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= m_titleBarHeight) {
        on_btnMaximize_clicked();
        return;
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

// ==================== 模板设计 ====================

bool MainWindow::loadTemplateImage(const QString &path)
{
    m_templateImage = ImageUtil::imreadSafe(path);
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法加载模板图片！\n路径：%1\n\n提示：请确认文件格式为 PNG/JPG/BMP/TIF。").arg(path));
        return false;
    }
    m_templateImagePath = path;
    m_roiList.clear();
    m_currentROIIndex = -1;
    updateROIList();
    drawROIsOnTemplate();
    statusBar()->showMessage(QStringLiteral("已加载模板: %1").arg(path));
    return true;
}

void MainWindow::updateROIList(int preferredIndex)
{
    int currentRow = (preferredIndex >= 0) ? preferredIndex : ui->listROI->currentRow();
    ui->listROI->blockSignals(true);
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
    if (m_roiList.size() > 0) {
        int selectRow = qBound(0, currentRow, m_roiList.size() - 1);
        ui->listROI->setCurrentRow(selectRow);
        m_currentROIIndex = selectRow;
    } else {
        m_currentROIIndex = -1;
    }
    ui->lblROIInfo->setText(QStringLiteral("当前共 %1 个检测区域").arg(m_roiList.size()));
}

void MainWindow::drawROIsOnTemplate()
{
    if (m_templateImage.empty()) return;
    m_templateDisplay = m_templateImage.clone();
    for (int i = 0; i < m_roiList.size(); ++i) {
        cv::Scalar color = (i == m_currentROIIndex) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::rectangle(m_templateDisplay, m_roiList[i].rect, color, 2);
        QString label = QString("ROI %1").arg(i + 1);
        cv::putText(m_templateDisplay, label.toStdString(),
                    cv::Point(m_roiList[i].rect.x, m_roiList[i].rect.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
    }
    ImageUtil::displayImageOnLabel(ui->lblTemplateImage, m_templateDisplay,
                                   QStringLiteral("请加载模板图片"));
}

void MainWindow::addROI(const cv::Rect &rect)
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载模板图片！"));
        return;
    }
    if (rect.width < 10 || rect.height < 10) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("检测区域太小，请重新选择！"));
        return;
    }
    cv::Rect validRect = rect & cv::Rect(0, 0, m_templateImage.cols, m_templateImage.rows);
    ROIRect roi;
    roi.id = m_roiList.size() + 1;
    roi.rect = validRect;
    roi.name = QStringLiteral("区域%1").arg(roi.id);
    roi.templateImage = m_templateImage(validRect).clone();
    m_roiList.append(roi);
    updateROIList(m_roiList.size() - 1);
    drawROIsOnTemplate();
    statusBar()->showMessage(QStringLiteral("已添加检测区域: %1").arg(roi.name));
}

void MainWindow::deleteROI(int index)
{
    if (index >= 0 && index < m_roiList.size()) {
        m_roiList.removeAt(index);
        m_currentROIIndex = -1;
        updateROIList();
        drawROIsOnTemplate();
        statusBar()->showMessage(QStringLiteral("已删除检测区域"));
    }
}

// ==================== 检测模块 ====================

bool MainWindow::loadTestImage(const QString &path)
{
    m_testImage = ImageUtil::imreadSafe(path);
    if (m_testImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法加载待测图片！\n路径：%1").arg(path));
        return false;
    }
    m_testImagePath = path;
    ImageUtil::displayImageOnLabel(ui->lblTestImage, m_testImage,
                                   QStringLiteral("待测图片"));
    statusBar()->showMessage(QStringLiteral("已加载待测图片: %1").arg(path));
    return true;
}

void MainWindow::performDetection(const QString &imagePath)
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先加载配置文件或设置检测区域！"));
        return;
    }

    cv::Mat testImage = ImageUtil::imreadSafe(imagePath);
    if (testImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法加载待测图片！\n路径：%1").arg(imagePath));
        return;
    }

    double skewAngle = Detector::detectSkewAndCorrect(testImage);
    if (skewAngle != 0.0) {
        statusBar()->showMessage(QStringLiteral("倾斜矫正完成: %1°").arg(skewAngle, 0, 'f', 2));
    }

    m_resultImage = testImage.clone();
    cv::Point matchPoint = Detector::templateMatch(testImage, m_templateImage);

    cv::rectangle(m_resultImage, matchPoint,
                  cv::Point(matchPoint.x + m_templateImage.cols, matchPoint.y + m_templateImage.rows),
                  cv::Scalar(255, 0, 0), 2);

    QFileInfo fileInfo(imagePath);
    QString imageName = fileInfo.fileName();
    QString detectionTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

    QList<DetectionResult> imageRoiResults;

    for (const ROIRect &roi : m_roiList) {
        cv::Rect testROI;
        cv::Mat testRegion = Detector::alignROI(testImage, roi.templateImage,
                                                 matchPoint, roi.rect, testROI);
        if (testRegion.empty()) continue;
        double defectScore;
        bool isDefective = Detector::detectCharacterDefect(testRegion, roi.templateImage, defectScore);

        cv::Scalar color = isDefective ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
        cv::rectangle(m_resultImage, testROI, color, 2);

        QString label = isDefective
            ? QString("Defect: %1%").arg(defectScore * 100, 0, 'f', 1)
            : QString("OK (%1%)").arg((1.0 - defectScore) * 100, 0, 'f', 1);
        cv::putText(m_resultImage, label.toStdString(),
                    cv::Point(testROI.x, testROI.y - 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);

        DetectionResult result;
        result.id = ++m_resultCount;
        result.imageName = imageName;
        result.roiName = roi.name;
        result.isDefective = isDefective;
        result.defectScore = defectScore;
        result.detectionTime = detectionTime;

        m_resultList.append(result);
        imageRoiResults.append(result);
    }

    if (!imageRoiResults.isEmpty()) {
        ++m_imageResultCount;
        ResultManager::addToTable(ui->tableResults, m_imageResultCount, imageRoiResults);
    }

    ImageUtil::displayImageOnLabel(ui->lblResultImage, m_resultImage,
                                   QStringLiteral("检测结果"));
    statusBar()->showMessage(QStringLiteral("检测完成: %1").arg(imageName));

    // 更新检测结果摘要
    QStringList lines;
        lines << QStringLiteral("- %1").arg(imageName);
    for (const DetectionResult &r : imageRoiResults) {
        QString status = r.isDefective
            ? QStringLiteral("残缺 (%1%)").arg(r.defectScore * 100, 0, 'f', 1)
            : QStringLiteral("正常 (%1%)").arg((1.0 - r.defectScore) * 100, 0, 'f', 1);
        lines << QStringLiteral("  %1 → %2").arg(r.roiName, -8).arg(status);
    }
    QString prev = ui->lblDetectionSummary->text();
    if (prev == QStringLiteral("尚未检测")) prev.clear();
    ui->lblDetectionSummary->setText(prev + lines.join(QStringLiteral("\n")) + QStringLiteral("\n"));
}

// ==================== 槽函数 ====================

void MainWindow::on_btnLoadTemplate_clicked()
{
    QString defaultDir = ImageUtil::getSamplesDir("templates");
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择模板图片"),
                                                defaultDir,
                                                QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (!path.isEmpty()) loadTemplateImage(path);
}

void MainWindow::on_btnAddROI_clicked()
{
    if (m_templateImage.empty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先加载模板图片，然后在图片上拖动鼠标选择检测区域。"));
        return;
    }
    m_isSelectingROI = true;
    statusBar()->showMessage(QStringLiteral("请在模板图片上拖动鼠标选择检测区域..."));
}

void MainWindow::on_btnDeleteROI_clicked()
{
    int currentRow = ui->listROI->currentRow();
    if (currentRow >= 0) {
        deleteROI(currentRow);
    } else {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择要删除的检测区域！"));
    }
}

void MainWindow::on_btnSaveConfig_clicked()
{
    if (m_templateImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载模板图片！"));
        return;
    }
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加检测区域！"));
        return;
    }

    QString defaultDir = ImageUtil::getSamplesDir("configs");
    QDir().mkpath(defaultDir);
    QString defaultName = m_templateImagePath.isEmpty()
        ? "config.json"
        : QFileInfo(m_templateImagePath).completeBaseName() + ".json";
    QString defaultPath = defaultDir + "/" + defaultName;

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存配置文件"),
                                                defaultPath,
                                                QStringLiteral("JSON文件 (*.json)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".json")) path += ".json";

    if (ConfigManager::save(path, m_templateImage, m_templateImagePath, m_roiList)) {
        statusBar()->showMessage(QStringLiteral("配置已保存: %1").arg(path));
        m_configFilePath = path;
        ui->txtConfigPath->setText(path);
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法保存配置文件！"));
    }
}

void MainWindow::on_btnLoadConfig_clicked()
{
    QString defaultDir = ImageUtil::getSamplesDir("configs");
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择配置文件"),
                                                defaultDir,
                                                QStringLiteral("JSON文件 (*.json)"));
    if (path.isEmpty()) return;

    ConfigManager::ConfigData data;
    if (!ConfigManager::load(path, data)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法加载配置文件！\n路径：%1\n\n请检查文件格式和模板图片路径。").arg(path));
        return;
    }

    m_templateImage = data.templateImage;
    m_templateImagePath = data.templatePath;
    m_roiList = data.roiList;
    m_currentROIIndex = -1;
    updateROIList();
    drawROIsOnTemplate();

    m_configFilePath = path;
    ui->txtConfigPath->setText(path);
    statusBar()->showMessage(QStringLiteral("已加载配置: %1").arg(path));
}

void MainWindow::on_listROI_currentRowChanged(int currentRow)
{
    m_currentROIIndex = currentRow;
    drawROIsOnTemplate();
}

void MainWindow::on_btnLoadTestImage_clicked()
{
    QString defaultDir = ImageUtil::getSamplesDir("test_images");
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择待测图片"),
                                                defaultDir,
                                                QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (!path.isEmpty()) loadTestImage(path);
}

void MainWindow::on_btnBrowseConfig_clicked()
{
    on_btnLoadConfig_clicked();
}

void MainWindow::on_btnStartDetection_clicked()
{
    if (m_testImage.empty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载待测图片！"));
        return;
    }
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载配置文件！"));
        return;
    }
    ui->lblDetectionSummary->setText(QString());
    performDetection(m_testImagePath);
}

void MainWindow::on_btnBatchDetection_clicked()
{
    if (m_roiList.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先加载配置文件！"));
        return;
    }

    QString defaultDir = ImageUtil::getSamplesDir("test_images");
    QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("选择待测图片"),
                                                      defaultDir,
                                                      QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif)"));
    if (paths.isEmpty()) return;

    ui->lblDetectionSummary->setText(QString());
    for (const QString &path : paths) {
        performDetection(path);
    }
    QMessageBox::information(this, QStringLiteral("完成"),
                             QStringLiteral("批量检测完成，共检测 %1 张图片。").arg(paths.size()));
}

void MainWindow::on_btnExportResults_clicked()
{
    QString defaultDir = ImageUtil::getSamplesDir("results");
    QDir().mkpath(defaultDir);
    QString defaultName = QStringLiteral("检测结果_%1.csv")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QString defaultPath = defaultDir + "/" + defaultName;

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出结果"), defaultPath,
                                                QStringLiteral("CSV文件 (*.csv)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".csv")) path += ".csv";

    if (ResultManager::exportToCsv(path, m_resultList)) {
        QMessageBox::information(this, QStringLiteral("完成"),
                                 QStringLiteral("结果已导出到: %1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法创建导出文件！"));
    }
}

void MainWindow::on_btnClearResults_clicked()
{
    ResultManager::clearAll(ui->tableResults, m_resultList, m_resultCount, m_imageResultCount);
    statusBar()->showMessage(QStringLiteral("结果已清空"));
}

// ==================== 鼠标事件 ====================

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= m_titleBarHeight) {
        m_isDraggingWindow = true;
        m_dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
        return;
    }

    if (m_isSelectingROI && event->button() == Qt::LeftButton) {
        QLabel *lbl = ui->lblTemplateImage;
        QPoint imgPos = ImageUtil::labelToImagePos(lbl, m_templateDisplay,
                                                    lbl->mapFromGlobal(event->globalPos()));
        if (imgPos.x() >= 0) {
            m_roiStartPoint = imgPos;
            m_roiEndPoint = imgPos;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDraggingWindow && (event->buttons() & Qt::LeftButton)) {
        if (!isMaximized()) {
            move(event->globalPos() - m_dragStartPosition);
        }
        event->accept();
        return;
    }

    if (m_isSelectingROI && !m_roiStartPoint.isNull() && !m_templateDisplay.empty()) {
        QLabel *lbl = ui->lblTemplateImage;
        QPoint imgPos = ImageUtil::labelToImagePos(lbl, m_templateDisplay,
                                                    lbl->mapFromGlobal(event->globalPos()));
        if (imgPos.x() >= 0) m_roiEndPoint = imgPos;

        ImageUtil::displayImageOnLabel(lbl, m_templateDisplay, "");
        const QPixmap *pixPtr = lbl->pixmap();
        if (!pixPtr || pixPtr->isNull()) return;
        QPixmap pix = *pixPtr;

        double scale = static_cast<double>(pix.width()) / m_templateDisplay.cols;
        int sx1 = static_cast<int>(qMin(m_roiStartPoint.x(), m_roiEndPoint.x()) * scale);
        int sy1 = static_cast<int>(qMin(m_roiStartPoint.y(), m_roiEndPoint.y()) * scale);
        int sx2 = static_cast<int>(qMax(m_roiStartPoint.x(), m_roiEndPoint.x()) * scale);
        int sy2 = static_cast<int>(qMax(m_roiStartPoint.y(), m_roiEndPoint.y()) * scale);
        QPainter painter(&pix);
        painter.setPen(QPen(QColor(255, 0, 0), 2));
        painter.drawRect(sx1, sy1, sx2 - sx1, sy2 - sy1);
        painter.end();
        lbl->setPixmap(pix);
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDraggingWindow) {
        m_isDraggingWindow = false;
        event->accept();
        return;
    }

    if (m_isSelectingROI && event->button() == Qt::LeftButton && !m_roiStartPoint.isNull()) {
        m_isSelectingROI = false;
        int x1 = qMin(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y1 = qMin(m_roiStartPoint.y(), m_roiEndPoint.y());
        int x2 = qMax(m_roiStartPoint.x(), m_roiEndPoint.x());
        int y2 = qMax(m_roiStartPoint.y(), m_roiEndPoint.y());
        if (x2 > x1 && y2 > y1) {
            addROI(cv::Rect(x1, y1, x2 - x1, y2 - y1));
        }
        m_roiStartPoint = QPoint();
        m_roiEndPoint = QPoint();
    }
    QMainWindow::mouseReleaseEvent(event);
}
