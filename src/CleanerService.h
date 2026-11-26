#pragma once
#include "Processing.h"
#include <QObject>
#include <QQuickImageProvider>
#include <QMutex>
#include <QFutureWatcher>
#include <QTimer>
#include <QUrl>

class PreviewImages:public QQuickImageProvider{
public:
    PreviewImages():QQuickImageProvider(QQuickImageProvider::Image){}
    QImage requestImage(const QString &id,QSize *size,const QSize &requested)override;
    void set(const QImage &original,const QImage &cleaned);
private:QMutex mutex;QImage before,after;
};
class CleanerService:public QObject{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY changed)
    Q_PROPERTY(bool imageInput READ imageInput NOTIFY changed)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
    Q_PROPERTY(bool hasPreview READ hasPreview NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(double ratio READ ratio NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit CleanerService(PreviewImages *images,QObject *parent=nullptr);
    ~CleanerService()override;
    QString fileName()const;
    bool imageInput()const{return sourceIsImage;}
    int pageCount()const{return count;}int page()const{return current;}
    bool previewBusy()const{return pendingPreview;}bool exporting()const{return exportWatcher.isRunning();}
    bool hasPreview()const{return ready;}int revision()const{return imageRevision;}
    double ratio()const{return imageRatio;}int completed()const{return done;}int total()const{return pageTotal;}
    QString status()const{return message;}
    Q_INVOKABLE void open(const QUrl &url);
    Q_INVOKABLE void setPage(int page);
    Q_INVOKABLE void setOptions(const QVariantMap &options);
    Q_INVOKABLE QUrl suggestedOutput(const QString &suffix)const;
    Q_INVOKABLE void exportFile(const QUrl &url,int first,int last,int dpi,const QString &suffix);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void revealOutput()const;
signals:
    void changed();void error(const QString &message);void exportFinished(bool success,bool cancelled);
private:
    PreviewImages *images;QString path,output,message="打开 PDF 或图片，预览处理效果后导出";
    Scan::Options options;int count=0,current=1,imageRevision=0,done=0,pageTotal=0;
    double imageRatio=.707;bool pendingPreview=false,ready=false,sourceIsImage=false;
    quint64 generation=0,activeGeneration=0;
    QTimer debounce;QFutureWatcher<Scan::Preview> previewWatcher;QFutureWatcher<Scan::Result> exportWatcher;
    Scan::Cancel cancellation;
    void schedule();void startPreview();
};
