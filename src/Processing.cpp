#include "Processing.h"
#include <QPdfDocument>
#include <QPdfWriter>
#include <QPainter>
#include <QPageLayout>
#include <QSaveFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QMimeDatabase>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr int maxPixels = 24000000;
constexpr int maxSide = 20000;
constexpr int previewSide = 3200;

bool exceedsImageLimit(double width, double height) {
    return width > maxSide || height > maxSide || width * height > maxPixels;
}

QString loadError(QPdfDocument::Error error) {
    if(error==QPdfDocument::Error::IncorrectPassword)
        return QStringLiteral("这是加密 PDF，请先用阅读器解锁并另存后再打开。");
    return QStringLiteral("无法读取 PDF，请确认文件完整且不是加密或损坏的文件。");
}
QImage render(QPdfDocument &document,int page,int dpi) {
    QSizeF points=document.pagePointSize(page);
    if(points.isEmpty() || !std::isfinite(points.width()) || !std::isfinite(points.height()))
        throw std::runtime_error("无法读取页面尺寸");
    double width=std::ceil(points.width()*dpi/72.0),height=std::ceil(points.height()*dpi/72.0);
    if(width<1 || height<1 || exceedsImageLimit(width, height))
        throw std::runtime_error("页面像素过大，请降低导出分辨率或选择较小的页面范围");
    QImage image=document.render(page,QSize(int(width),int(height)));
    if(image.isNull())throw std::runtime_error("页面渲染失败");
    return image;
}
bool cancelled(const Scan::Cancel &cancel){return cancel && cancel->load();}

QImage readImage(const QString &path) {
    QImageReader reader(path);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    // A top-down Windows bitmap can report a negative height before decoding.
    const double height = reader.format() == "bmp" ? std::abs(double(size.height())) : size.height();
    if (size.width() <= 0 || height <= 0)
        throw std::runtime_error("无法读取图片尺寸，请确认格式受支持且文件完整");
    if (exceedsImageLimit(size.width(), height))
        throw std::runtime_error("图片像素过大，支持的上限为 2400 万像素且每边不超过 20000 像素");
    QImage image = reader.read();
    if (image.isNull())
        throw std::runtime_error("无法读取图片，请确认文件完整");
    return image;
}

double imageProcessingDpi(const QImage &image) {
    // Camera scans rarely have useful DPI metadata. Scale the filter to the image.
    return std::clamp(std::max(image.width(), image.height()) / 11.7, 30.0, 600.0);
}

QString destinationError(const QFileInfo &from, const QFileInfo &to) {
    if (to.filePath().isEmpty())
        return "请选择保存位置";
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    if (from.absoluteFilePath().compare(to.absoluteFilePath(), sensitivity) == 0 ||
        (!to.canonicalFilePath().isEmpty() &&
         from.canonicalFilePath().compare(to.canonicalFilePath(), sensitivity) == 0))
        return "请另存为新文件，不能覆盖正在处理的原稿";
    return {};
}
}

bool Scan::isPdf(const QString &path) {
    return QFileInfo(path).suffix().compare("pdf", Qt::CaseInsensitive) == 0 ||
        QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name() == "application/pdf";
}

Scan::Options Scan::Options::fromMap(const QVariantMap &values){
    Options out;
    out.blackWhite=values.value("mode","gray").toString()!="gray";
    out.background=std::clamp(values.value("background",100).toInt(),0,100);
    out.darkness=std::clamp(values.value("darkness",0).toInt(),-20,20);
    out.sharpness=std::clamp(values.value("sharpness",20).toInt(),0,100);
    out.dpi=std::clamp(values.value("dpi",300).toInt(),100,400);
    out.first=std::max(1,values.value("first",1).toInt());
    out.last=std::max(0,values.value("last",0).toInt());
    return out;
}

QImage Scan::cleanImage(const QImage &input,const Options &options,double dpi){
    if(input.isNull())return {};
    QImage opaque = input;
    if (input.hasAlphaChannel()) {
        // Flatten transparent margins against the white output background.
        opaque = QImage(input.size(), QImage::Format_RGB32);
        if (opaque.isNull()) return {};
        opaque.fill(Qt::white);
        QPainter painter(&opaque);
        painter.drawImage(0, 0, input);
    }
    QImage gray=opaque.convertToFormat(QImage::Format_Grayscale8);
    if (gray.isNull()) return {};
    cv::Mat pixels(gray.height(),gray.width(),CV_8UC1,gray.bits(),gray.bytesPerLine());
    const int kernel=std::max(3,int(std::round(dpi*.15))|1);
    cv::Mat background,light,normalized;
    cv::morphologyEx(pixels,background,cv::MORPH_CLOSE,
                    cv::getStructuringElement(cv::MORPH_RECT,{kernel,kernel}));
    cv::GaussianBlur(background,background,{0,0},std::max(1.0,dpi*.025));
    pixels.convertTo(light,CV_32F);background.convertTo(background,CV_32F);
    cv::max(background,1.0,background);
    cv::divide(light,background,normalized,255.0);
    const double mix=std::clamp(options.background,0,100)/100.0;
    normalized=normalized*mix+light*(1.0-mix);
    cv::min(normalized,255.0,normalized);cv::max(normalized,0.0,normalized);
    if(options.sharpness>0){
        cv::Mat blurred;
        cv::GaussianBlur(normalized,blurred,{0,0},std::max(.45,dpi*.003));
        const double amount=std::clamp(options.sharpness,0,100)/125.0;
        // Sharpen existing edges gently; never generate or redraw characters.
        cv::addWeighted(normalized,1.0+amount,blurred,-amount,0,normalized);
        cv::min(normalized,255.0,normalized);cv::max(normalized,0.0,normalized);
    }
    cv::Mat result;
    if(options.blackWhite){
        // Sauvola local thresholding; physical window size follows the render DPI.
        const int window=std::max(7,int(std::round(dpi*.17))|1);
        cv::Mat mean,square,variance,deviation,threshold;
        cv::boxFilter(normalized,mean,CV_32F,{window,window},{-1,-1},true,cv::BORDER_REPLICATE);
        cv::sqrBoxFilter(normalized,square,CV_32F,{window,window},{-1,-1},true,cv::BORDER_REPLICATE);
        variance=square-mean.mul(mean);cv::max(variance,0.0,variance);cv::sqrt(variance,deviation);
        const double k=.20-std::clamp(options.darkness,-20,20)*.006;
        threshold=mean.mul(1.0+k*(deviation/128.0-1.0));
        cv::compare(normalized,threshold,result,cv::CMP_GT);
    }else{
        const double black=65.0+std::clamp(options.darkness,-20,20)*2;
        normalized.convertTo(result,CV_8U,255.0/(245.0-black),-black*255.0/(245.0-black));
    }
    return QImage(result.data,result.cols,result.rows,int(result.step),QImage::Format_Grayscale8).copy();
}

Scan::Preview Scan::preview(const QString &path,int requested,const Options &options){
    Preview out;
    try{
        if (!isPdf(path)) {
            QImage image = readImage(path);
            const double dpi = imageProcessingDpi(image);
            const int longEdge = std::max(image.width(), image.height());
            const double scale = std::min(1.0, double(previewSide) / longEdge);
            out.original = scale == 1.0 ? image :
                image.scaled(previewSide, previewSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            out.cleaned = cleanImage(out.original, options, dpi * scale);
            if (out.cleaned.isNull()) throw std::runtime_error("图片预览处理失败");
            out.count = 1;
            return out;
        }
        QPdfDocument document;auto error=document.load(path);
        if(error!=QPdfDocument::Error::None){out.error=loadError(error);return out;}
        out.count=document.pageCount();if(!out.count){out.error="PDF 没有可读取的页面";return out;}
        out.page=std::clamp(requested,1,out.count);
        auto size=document.pagePointSize(out.page-1);
        if(size.isEmpty()||!std::isfinite(size.width())||!std::isfinite(size.height())){
            out.error="无法读取页面尺寸";return out;
        }
        int dpi=std::clamp(int(previewSide*72/std::max(size.width(),size.height())),30,300);
        out.original=render(document,out.page-1,dpi);
        out.cleaned=cleanImage(out.original,options,dpi);
        if (out.cleaned.isNull()) throw std::runtime_error("页面预览处理失败");
    }catch(const std::exception &e){out.error=QString::fromUtf8(e.what());}
    return out;
}

Scan::Result Scan::exportPdf(const QString &source,const QString &destination,const Options &options,
                            const Cancel &cancel,std::function<void(int,int)> progress){
    Result out;
    const QFileInfo from(source);
    out.error = destinationError(from, QFileInfo(destination));
    if (!out.error.isEmpty()) return out;
    if(options.dpi<100||options.dpi>400){out.error="导出分辨率须为 100 至 400 DPI";return out;}
    try{
        if (cancelled(cancel)) { out.cancelled = true; return out; }
        const bool pdfSource = isPdf(source);
        QPdfDocument document;
        QImage sourceImage;
        int pageCount = 1;
        if (pdfSource) {
            const auto error = document.load(source);
            if (error != QPdfDocument::Error::None) { out.error = loadError(error); return out; }
            pageCount = document.pageCount();
        } else {
            sourceImage = readImage(source);
        }
        const int first=options.first,last=options.last?options.last:pageCount;
        if(first<1 || first>last || last>pageCount){
            out.error="页码范围无效，请选择原稿内的起止页码";return out;
        }
        if(cancelled(cancel)){out.cancelled=true;return out;}
        QSaveFile file(destination);file.setDirectWriteFallback(false);
        if(!file.open(QIODevice::WriteOnly)){out.error="无法创建文件："+file.errorString();return out;}
        {
            QPdfWriter writer(&file);writer.setResolution(options.dpi);
            writer.setTitle(from.completeBaseName()+" - 净页");writer.setCreator("ScanCleaner");
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            writer.setColorModel(QPdfWriter::ColorModel::Grayscale);
#endif
            QPainter painter;
            for(int page=first-1;page<last;++page){
                if(cancelled(cancel)){out.cancelled=true;break;}
                if(progress)progress(page-first+1,last-first+1);
                QImage image = pdfSource ? render(document, page, options.dpi) : sourceImage;
                auto points = pdfSource ? document.pagePointSize(page) :
                    QSizeF(image.size()) * (72.0 / options.dpi);
                if (!pdfSource && std::min(points.width(), points.height()) < 1)
                    points *= 1 / std::min(points.width(), points.height());
                image = cleanImage(image, options, pdfSource ? options.dpi : imageProcessingDpi(image));
                if(image.isNull())throw std::runtime_error("图像处理失败");
                if(options.blackWhite)image=image.convertToFormat(QImage::Format_Mono,Qt::ThresholdDither);
                if(cancelled(cancel)){out.cancelled=true;break;}
                const QPageLayout layout(QPageSize(points, QPageSize::Point, QString(), QPageSize::ExactMatch),
                                         QPageLayout::Portrait, QMarginsF(0, 0, 0, 0));
                if (!writer.setPageLayout(layout)) throw std::runtime_error("无法设置 PDF 页面尺寸");
                if(page==first-1){
                    if(!painter.begin(&writer))throw std::runtime_error("无法开始写入 PDF");
                    painter.setRenderHint(QPainter::LosslessImageRendering);
                }
                else if(!writer.newPage())throw std::runtime_error("无法写入下一页");
                painter.drawImage(QRect(0,0,writer.width(),writer.height()),image);
                ++out.pages;
            }
            if(painter.isActive()&&!painter.end())throw std::runtime_error("PDF 写入失败");
        } // Finalize the PDF trailer before committing the atomic save.
        if(out.cancelled || cancelled(cancel)){file.cancelWriting();out.cancelled=true;return out;}
        if(!file.commit()){out.error="保存失败："+file.errorString();return out;}
        out.success=true;if(progress)progress(last-first+1,last-first+1);
    }catch(const std::exception &e){out.error=QString::fromUtf8(e.what());}
    return out;
}

Scan::Result Scan::exportFile(const QString &source, const QString &destination, const Options &options,
                             const Cancel &cancel, std::function<void(int,int)> progress) {
    const QFileInfo target(destination);
    const auto suffix = target.suffix().toLower();
    if (suffix == "pdf" || (suffix.isEmpty() && isPdf(source)))
        return exportPdf(source, destination, options, cancel, progress);

    Result out;
    out.error = destinationError(QFileInfo(source), target);
    if (!out.error.isEmpty()) return out;
    const bool jpeg = suffix == "jpg" || suffix == "jpeg";
    if (suffix != "png" && !jpeg) {
        out.error = "请选择 PDF、PNG 或 JPG 格式";
        return out;
    }
    if (options.first < 1 || (options.last != 0 && options.last != options.first)) {
        out.error = "导出图片时请选择单页";
        return out;
    }
    if (cancelled(cancel)) { out.cancelled = true; return out; }

    try {
        QImage image;
        double dpi;
        const bool pdfSource = isPdf(source);
        if (pdfSource) {
            dpi = options.dpi;
            if (dpi < 100 || dpi > 400) { out.error = "导出分辨率须为 100 至 400 DPI"; return out; }
            QPdfDocument document;
            const auto error = document.load(source);
            if (error != QPdfDocument::Error::None) { out.error = loadError(error); return out; }
            if (options.first > document.pageCount()) { out.error = "页码超出原稿范围"; return out; }
            image = render(document, options.first - 1, options.dpi);
        } else {
            if (options.first != 1) { out.error = "单张图片只有一页"; return out; }
            image = readImage(source);
            dpi = imageProcessingDpi(image);
        }
        const int xResolution = pdfSource ? qRound(dpi / 0.0254) : image.dotsPerMeterX();
        const int yResolution = pdfSource ? xResolution : image.dotsPerMeterY();
        image = cleanImage(image, options, dpi);
        if (image.isNull()) throw std::runtime_error("图像处理失败");
        image.setDotsPerMeterX(xResolution);
        image.setDotsPerMeterY(yResolution);
        if (cancelled(cancel)) { out.cancelled = true; return out; }

        QSaveFile file(destination);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) { out.error = "无法创建文件：" + file.errorString(); return out; }
        QImageWriter writer(&file, jpeg ? "jpeg" : "png");
        if (jpeg) {
            writer.setQuality(95);
        } else if (options.blackWhite) {
            image = image.convertToFormat(QImage::Format_Mono, Qt::ThresholdDither);
        }
        if (!writer.write(image)) { out.error = "图片写入失败：" + writer.errorString(); return out; }
        if (cancelled(cancel)) { out.cancelled = true; return out; }
        if (!file.commit()) { out.error = "保存失败：" + file.errorString(); return out; }
        out.success = true;
        out.pages = 1;
        if (progress) progress(1, 1);
    } catch (const std::exception &e) {
        out.error = QString::fromUtf8(e.what());
    }
    return out;
}
