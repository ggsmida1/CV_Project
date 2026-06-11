#include "imageutil.h"
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <algorithm>

namespace ImageUtil {

QImage mat2QImage(const cv::Mat &mat)
{
    if (mat.empty()) return QImage();
    if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    } else if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    } else if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    return QImage();
}

cv::Mat QImage2Mat(const QImage &image)
{
    if (image.isNull()) return cv::Mat();
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

cv::Mat imreadSafe(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return cv::Mat();
    QByteArray data = file.readAll();
    file.close();
    if (data.isEmpty()) return cv::Mat();
    std::vector<uchar> buffer(data.begin(), data.end());
    return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

void computeImageTransform(QLabel *label, const cv::Mat &image, double &outScale, int &outOffsetX, int &outOffsetY)
{
    if (!label || image.empty()) {
        outScale = 1.0; outOffsetX = 0; outOffsetY = 0;
        return;
    }
    QSize labelSize = label->size();
    double scale = qMin(static_cast<double>(labelSize.width()) / image.cols,
                        static_cast<double>(labelSize.height()) / image.rows);
    outScale = scale;
    outOffsetX = (labelSize.width() - static_cast<int>(image.cols * scale)) / 2;
    outOffsetY = (labelSize.height() - static_cast<int>(image.rows * scale)) / 2;
}

QPoint labelToImagePos(QLabel *label, const cv::Mat &image, const QPoint &labelPos)
{
    double scale = 1.0;
    int offsetX = 0, offsetY = 0;
    computeImageTransform(label, image, scale, offsetX, offsetY);
    double imgX = (labelPos.x() - offsetX) / scale;
    double imgY = (labelPos.y() - offsetY) / scale;
    if (imgX < 0 || imgY < 0 || imgX >= image.cols || imgY >= image.rows)
        return QPoint(-1, -1);
    return QPoint(static_cast<int>(imgX), static_cast<int>(imgY));
}

void displayImageOnLabel(QLabel *label, const cv::Mat &image, const QString &placeholder)
{
    if (image.empty()) {
        label->setText(placeholder);
        return;
    }
    QSize labelSize = label->size();
    cv::Mat display;
    double scale = qMin(static_cast<double>(labelSize.width()) / image.cols,
                        static_cast<double>(labelSize.height()) / image.rows);
    cv::resize(image, display, cv::Size(), scale, scale, cv::INTER_AREA);
    QImage qImg = mat2QImage(display);
    label->setPixmap(QPixmap::fromImage(qImg));
}

QString getSamplesDir(const QString &subdir)
{
    QDir appDir(QCoreApplication::applicationDirPath());
    QString projectRoot = appDir.absolutePath();
    if (!QDir(projectRoot + "/samples").exists()) {
        appDir.cdUp();
        projectRoot = appDir.absolutePath();
    }
    return projectRoot + "/samples/" + subdir;
}

}
