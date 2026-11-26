#include "Processing.h"
#include <QtTest>
#include <QPainter>
#include <QPdfWriter>
#include <QPdfDocument>
#include <QPageLayout>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>

namespace {
QImage sample(){
    QImage image(800,1000,QImage::Format_RGB32);
    for(int y=0;y<image.height();++y)for(int x=0;x<image.width();++x){
        int shade=x*18/image.width()+y*12/image.height();image.setPixelColor(x,y,QColor(210+shade,199+shade,177+shade));
    }
    QPainter p(&image);p.setPen(Qt::NoPen);p.setBrush(QColor(70,55,45));
    p.drawRect(90,100,5,120);p.drawRect(120,100,70,4);p.drawRect(210,180,2,2);
    p.setBrush(QColor(125,110,100));p.drawRect(260,100,4,120);
    p.setPen(QColor(65,55,45));p.setFont(QFont("Times New Roman",18));
    p.drawText(QRect(80,280,650,600),Qt::TextWordWrap,"A scanned book. Fine print, punctuation: 1.25, (a+b), x = 3.\n\nThe background should become white while text remains intact.");
    return image;
}
QByteArray bytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
void fixture(const QString &path){
    QPdfWriter writer(path);writer.setResolution(100);writer.setPageLayout(QPageLayout(QPageSize(QSizeF(210,297),QPageSize::Millimeter),QPageLayout::Portrait,QMarginsF(0,0,0,0)));
    QPainter painter(&writer);painter.drawImage(QRect(0,0,writer.width(),writer.height()),sample());
    writer.setPageLayout(QPageLayout(QPageSize(QSizeF(210,148),QPageSize::Millimeter),QPageLayout::Portrait,QMarginsF(0,0,0,0)));
    writer.newPage();painter.drawImage(QRect(0,0,writer.width(),writer.height()),sample());painter.end();
}
}

class ScanTests:public QObject{
    Q_OBJECT
private slots:
    void backgroundAndThinMarks(){
        auto image=sample(),original=image.copy();Scan::Options options;options.blackWhite=true;
        auto result=Scan::cleanImage(image,options,150);QCOMPARE(image,original);QCOMPARE(result.size(),image.size());
        QCOMPARE(result.pixelColor(20,20).red(),255);QCOMPARE(result.pixelColor(780,970).red(),255);
        QCOMPARE(result.pixelColor(92,150).red(),0);QCOMPARE(result.pixelColor(145,102).red(),0);
        QCOMPARE(result.pixelColor(210,180).red(),0);QCOMPARE(result.pixelColor(261,140).red(),0);
        for(int y=0;y<result.height();++y)for(int x=0;x<result.width();++x){int value=result.constScanLine(y)[x];QVERIFY(value==0||value==255);}
        options.blackWhite=false;auto gray=Scan::cleanImage(image,options,150);
        QCOMPARE(gray.pixelColor(20,20).red(),255);QVERIFY(gray.pixelColor(92,150).red()<50);
        QVERIFY(gray.pixelColor(261,140).red()>0);QVERIFY(gray.pixelColor(261,140).red()<180);
    }
    void transparentMarginsAndDarkness(){
        QImage transparent(300,400,QImage::Format_ARGB32);transparent.fill(Qt::transparent);
        {QPainter p(&transparent);p.fillRect(90,120,4,80,Qt::black);}
        Scan::Options options;options.blackWhite=true;auto result=Scan::cleanImage(transparent,options,150);
        QCOMPARE(result.pixelColor(5,5),QColor(Qt::white));QCOMPARE(result.pixelColor(91,130),QColor(Qt::black));
        auto source=sample();options.darkness=-20;auto lighter=Scan::cleanImage(source,options,150);
        options.darkness=20;auto darker=Scan::cleanImage(source,options,150);int light=0,dark=0;
        for(int y=0;y<source.height();++y)for(int x=0;x<source.width();++x){light+=lighter.constScanLine(y)[x]==0;dark+=darker.constScanLine(y)[x]==0;}
        QVERIFY(dark>=light);QVERIFY(light>100);
    }
    void exportPreservesSizesAndSource(){
        QTemporaryDir dir;QString source=dir.filePath("source.pdf"),dest=dir.filePath("cleaned.pdf");fixture(source);auto original=bytes(source);
        Scan::Options options;options.blackWhite=true;options.dpi=100;auto result=Scan::exportPdf(source,dest,options);
        QVERIFY2(result.success,qPrintable(result.error));QCOMPARE(result.pages,2);QCOMPARE(bytes(source),original);QVERIFY(!bytes(dest).contains("/DCTDecode"));
        QPdfDocument before,after;QCOMPARE(before.load(source),QPdfDocument::Error::None);QCOMPARE(after.load(dest),QPdfDocument::Error::None);QCOMPARE(after.pageCount(),2);
        for(int page=0;page<2;++page){auto a=before.pagePointSize(page),b=after.pagePointSize(page);QVERIFY(qAbs(a.width()-b.width())<1);QVERIFY(qAbs(a.height()-b.height())<1);}
        QVERIFY(after.pagePointSize(1).width()>after.pagePointSize(1).height());
        auto preview=Scan::preview(dest,2,options);QVERIFY2(preview.error.isEmpty(),qPrintable(preview.error));QCOMPARE(preview.page,2);QCOMPARE(preview.count,2);QVERIFY(!preview.cleaned.isNull());
        options.first=2;options.last=2;QString single=dir.filePath("single.pdf");QVERIFY(Scan::exportPdf(source,single,options).success);
        QPdfDocument selected;QCOMPARE(selected.load(single),QPdfDocument::Error::None);QCOMPARE(selected.pageCount(),1);QVERIFY(selected.pagePointSize(0).width()>selected.pagePointSize(0).height());
    }
    void cancellationAndErrorsNeverReplaceExistingFiles(){
        QTemporaryDir dir;QString source=dir.filePath("source.pdf"),dest=dir.filePath("existing.pdf");fixture(source);QFile existing(dest);QVERIFY(existing.open(QIODevice::WriteOnly));existing.write("keep this file");existing.close();auto original=bytes(source);
        Scan::Options options;options.dpi=100;auto cancel=std::make_shared<std::atomic_bool>(false);
        auto result=Scan::exportPdf(source,dest,options,cancel,[cancel](int done,int){if(done==1)cancel->store(true);});
        QVERIFY(result.cancelled);QVERIFY(!result.success);QCOMPARE(bytes(dest),QByteArray("keep this file"));QCOMPARE(bytes(source),original);
        QVERIFY(!Scan::exportPdf(source,source,options).success);QCOMPARE(bytes(source),original);
#ifdef Q_OS_UNIX
        QString alias=dir.filePath("alias.pdf");QVERIFY(QFile::link(source,alias));QVERIFY(!Scan::exportPdf(source,alias,options).success);QCOMPARE(bytes(source),original);
#endif
        options.first=3;QVERIFY(!Scan::exportPdf(source,dest,options).success);QCOMPARE(bytes(dest),QByteArray("keep this file"));
        QVERIFY(!Scan::exportPdf(dir.filePath("missing.pdf"),dest,options).success);QCOMPARE(bytes(dest),QByteArray("keep this file"));
        QVERIFY(!Scan::preview(dir.filePath("missing.pdf"),1,options).error.isEmpty());
    }
    void oversizedPageAndInvalidDpiDoNotAllocateOrCommit(){
        QTemporaryDir dir;QString source=dir.filePath("huge.pdf"),dest=dir.filePath("output.pdf");
        {QPdfWriter writer(source);writer.setPageSize(QPageSize(QSizeF(10000,10000),QPageSize::Point));QPainter painter(&writer);painter.drawLine(10,10,20,20);painter.end();}
        Scan::Options options;options.dpi=100;auto result=Scan::exportPdf(source,dest,options);
        QVERIFY(!result.success);QVERIFY(result.error.contains("像素过大"));QVERIFY(!QFile::exists(dest));
        options.dpi=900;result=Scan::exportPdf(source,dest,options);QVERIFY(!result.success);QVERIFY(result.error.contains("分辨率"));QVERIFY(!QFile::exists(dest));
    }
    void imagePreviewAndExportPreservePixels(){
        QTemporaryDir dir;
        const QString source = dir.filePath("scan.png");
        const QImage input = sample();
        QVERIFY(input.save(source));
        const auto original = bytes(source);

        Scan::Options options;
        options.blackWhite = true;
        const auto preview = Scan::preview(source, 3, options);
        QVERIFY2(preview.error.isEmpty(), qPrintable(preview.error));
        QCOMPARE(preview.count, 1);
        QCOMPARE(preview.page, 1);
        QCOMPARE(preview.original.size(), input.size());
        QCOMPARE(preview.cleaned.pixelColor(20, 20), QColor(Qt::white));
        QCOMPARE(preview.cleaned.pixelColor(92, 150), QColor(Qt::black));

        const QString png = dir.filePath("cleaned.png");
        const auto pngResult = Scan::exportFile(source, png, options);
        QVERIFY2(pngResult.success, qPrintable(pngResult.error));
        const QImage pngImage(png);
        QCOMPARE(pngImage.depth(), 1);
        QCOMPARE(pngImage.convertToFormat(QImage::Format_Grayscale8), preview.cleaned);

        const QString jpeg = dir.filePath("cleaned.jpeg");
        QVERIFY(Scan::exportFile(source, jpeg, options).success);
        QCOMPARE(QImage(jpeg).size(), input.size());
        QCOMPARE(bytes(source), original);
        QVERIFY(!Scan::exportFile(source, source, options).success);
        auto cancel = std::make_shared<std::atomic_bool>(true);
        const auto beforeCancel = bytes(png);
        QVERIFY(Scan::exportFile(source, png, options, cancel).cancelled);
        QCOMPARE(bytes(png), beforeCancel);
    }
    void exportImagePdfAndPdfPagePng(){
        QTemporaryDir dir;
        const QString source = dir.filePath("scan.png");
        QVERIFY(sample().save(source));
        Scan::Options options;
        options.dpi = 200;
        const QString pdf = dir.filePath("scan-cleaned.pdf");
        QVERIFY(Scan::exportFile(source, pdf, options).success);
        QPdfDocument converted;
        QCOMPARE(converted.load(pdf), QPdfDocument::Error::None);
        QCOMPARE(converted.pageCount(), 1);
        const auto points = converted.pagePointSize(0);
        QVERIFY(qAbs(points.width() - 288) < 1);
        QVERIFY(qAbs(points.height() - 360) < 1);

        const QString document = dir.filePath("book.pdf");
        fixture(document);
        const QString png = dir.filePath("book-page-2-cleaned.png");
        options.first = 2;
        options.last = 2;
        QVERIFY(Scan::exportFile(document, png, options).success);
        const QImage page(png);
        QVERIFY(page.width() > page.height());
        QCOMPARE(page.dotsPerMeterX(), qRound(200 / 0.0254));
        options.last = 3;
        QVERIFY(!Scan::exportFile(document, png, options).success);
    }
    void previewTopDownBitmap(){
        QTemporaryDir dir;
        const QString path = dir.filePath("top-down.bmp");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto bmp = QByteArray::fromHex(
            "424d460000000000000036000000" // Windows BMP, pixel data starts at byte 54.
            "2800000002000000feffffff01001800000000001000000000000000000000000000000000000000"
            "0000ff00ff000000" // Top row: red, green, padding.
            "ff0000ffffff0000"); // Bottom row: blue, white, padding.
        QCOMPARE(file.write(bmp), 70);
        file.close();

        const auto preview = Scan::preview(path, 1, {});
        QVERIFY2(preview.error.isEmpty(), qPrintable(preview.error));
        QCOMPARE(preview.original.size(), QSize(2, 2));
        QCOMPARE(preview.original.pixelColor(0, 0), QColor(Qt::red));
        QCOMPARE(preview.original.pixelColor(0, 1), QColor(Qt::blue));
    }
};
QTEST_MAIN(ScanTests)
#include "tst_processing.moc"
