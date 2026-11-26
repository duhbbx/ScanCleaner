#pragma once
#include <QImage>
#include <QString>
#include <QVariantMap>
#include <atomic>
#include <functional>
#include <memory>

namespace Scan {
struct Options {
    bool blackWhite = false;
    int background = 100;
    int darkness = 0;
    int sharpness = 20;
    int dpi = 300;
    int first = 1;
    int last = 0; // Zero means the final page.
    static Options fromMap(const QVariantMap &values);
};
using Cancel = std::shared_ptr<std::atomic_bool>;
struct Preview {
    QImage original, cleaned;
    int count = 0, page = 1;
    QString error;
};
struct Result {
    bool success = false, cancelled = false;
    int pages = 0;
    QString error;
};
QImage cleanImage(const QImage &image, const Options &options, double dpi);
bool isPdf(const QString &path);
Preview preview(const QString &path, int page, const Options &options);
Result exportPdf(const QString &source, const QString &destination, const Options &options,
                 const Cancel &cancel = {}, std::function<void(int,int)> progress = {});
Result exportFile(const QString &source, const QString &destination, const Options &options,
                  const Cancel &cancel = {}, std::function<void(int,int)> progress = {});
}
