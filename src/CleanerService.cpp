#include "CleanerService.h"
#include <QtConcurrent>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDir>
#include <QPointer>
#include <algorithm>

QImage PreviewImages::requestImage(const QString &id,QSize *size,const QSize &requested){
    QMutexLocker lock(&mutex);QImage image=id.startsWith("original")?before:after;
    if(size)*size=image.size();return requested.isValid()?image.scaled(requested,Qt::KeepAspectRatio,Qt::SmoothTransformation):image;
}
void PreviewImages::set(const QImage &original,const QImage &cleaned){QMutexLocker lock(&mutex);before=original;after=cleaned;}
CleanerService::CleanerService(PreviewImages *provider,QObject *parent):QObject(parent),images(provider){
    debounce.setSingleShot(true);debounce.setInterval(220);
    connect(&debounce,&QTimer::timeout,this,&CleanerService::startPreview);
    connect(&previewWatcher,&QFutureWatcher<Scan::Preview>::finished,this,[this]{
        auto result=previewWatcher.result();
        if(activeGeneration!=generation){startPreview();return;}
        pendingPreview=false;count=result.count;
        if(result.error.isEmpty()){
            current=result.page;images->set(result.original,result.cleaned);
            imageRatio=double(result.original.width())/result.original.height();++imageRevision;ready=true;
            if(!exporting())message="预览已更新，可放大检查细字、标点和公式";
        }else{ready=false;if(!exporting())message=result.error;emit error(result.error);}
        emit changed();
    });
    connect(&exportWatcher,&QFutureWatcher<Scan::Result>::finished,this,[this]{
        auto result=exportWatcher.result();
        if(result.success){done=pageTotal;message="已保存 "+QFileInfo(output).fileName();}
        else if(result.cancelled)message="已取消导出，原稿和已有目标文件均保留";
        else{message=result.error;emit error(result.error);}
        emit changed();emit exportFinished(result.success,result.cancelled);
    });
}
CleanerService::~CleanerService(){debounce.stop();if(cancellation)cancellation->store(true);previewWatcher.waitForFinished();exportWatcher.waitForFinished();}
QString CleanerService::fileName()const{return QFileInfo(path).fileName();}
void CleanerService::open(const QUrl &url){
    if(exporting())return;
    if(!url.isLocalFile()||!QFileInfo(url.toLocalFile()).isFile()){
        emit error("请选择本机上的 PDF 或图片文件");return;
    }
    path=QFileInfo(url.toLocalFile()).absoluteFilePath();current=1;count=0;ready=false;
    sourceIsImage = !Scan::isPdf(path);
    message = sourceIsImage ? "正在读取图片…" : "正在读取 PDF…";
    schedule();
}
void CleanerService::setPage(int value){
    if(exporting()||!count)return;value=std::clamp(value,1,count);if(value==current)return;
    current=value;schedule();
}
void CleanerService::setOptions(const QVariantMap &values){
    if(exporting())return;auto next=Scan::Options::fromMap(values);
    if(next.blackWhite==options.blackWhite&&next.background==options.background&&next.darkness==options.darkness&&next.sharpness==options.sharpness)return;
    options=next;if(!path.isEmpty())schedule();
}
void CleanerService::schedule(){++generation;pendingPreview=true;debounce.start();emit changed();}
void CleanerService::startPreview(){
    if(previewWatcher.isRunning()||path.isEmpty())return;
    activeGeneration=generation;const QString source=path;const int page=current;auto selected=options;
    previewWatcher.setFuture(QtConcurrent::run([source,page,selected]{return Scan::preview(source,page,selected);}));
}
QUrl CleanerService::suggestedOutput(const QString &suffix)const{
    const QFileInfo file(path);
    QString name = file.completeBaseName();
    if (!sourceIsImage && suffix != "pdf") name += QString("-page-%1").arg(current);
    return QUrl::fromLocalFile(file.dir().filePath(name + "-cleaned." + suffix));
}
void CleanerService::exportFile(const QUrl &url,int first,int last,int dpi,const QString &suffix){
    if(exporting()||pendingPreview||!ready||path.isEmpty())return;
    if(!url.isLocalFile()){emit error("请选择本机保存位置");return;}
    const auto actualSuffix = QFileInfo(url.toLocalFile()).suffix().toLower();
    if (actualSuffix != suffix && !(suffix == "jpg" && actualSuffix == "jpeg")) {
        emit error("文件扩展名应与所选格式一致（." + suffix + "）");
        return;
    }
    if(first<1||last<first||last>count){emit error("请检查起止页码");return;}
    if(dpi<100||dpi>400){emit error("请选择 100 至 400 DPI 的分辨率");return;}
    auto selected=options;selected.first=first;selected.last=last;selected.dpi=dpi;
    output=url.toLocalFile();const QString source=path,destination=output;
    cancellation=std::make_shared<std::atomic_bool>(false);auto cancel=cancellation;
    done=0;pageTotal=last-first+1;message=pageTotal==1 ? "正在处理并导出…" : "正在逐页处理，可随时取消…";
    QPointer<CleanerService> guard(this);
    auto progress=[guard](int done,int total){if(guard)QMetaObject::invokeMethod(guard,[guard,done,total]{if(guard){guard->done=done;guard->pageTotal=total;emit guard->changed();}},Qt::QueuedConnection);};
    exportWatcher.setFuture(QtConcurrent::run([source,destination,selected,cancel,progress]{return Scan::exportFile(source,destination,selected,cancel,progress);}));
    emit changed();
}
void CleanerService::cancelExport(){if(cancellation)cancellation->store(true);message="正在取消，当前页面结束后停止…";emit changed();}
void CleanerService::revealOutput()const{if(QFileInfo::exists(output))QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(output).absolutePath()));}
