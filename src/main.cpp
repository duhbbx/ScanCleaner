#include "CleanerService.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#ifdef SCAN_LEGACY_WINDOWS
#include <QQuickWindow>
#endif
#include <QCommandLineParser>
#include <QTimer>
#include <QDebug>

int main(int argc,char *argv[]){
    QGuiApplication app(argc,argv);
    app.setApplicationName("ScanCleaner");app.setApplicationDisplayName("净页");
    app.setOrganizationName("Skyler");app.setApplicationVersion(SCAN_VERSION);
    QCommandLineParser parser;parser.setApplicationDescription("净页：本地 PDF 和图片去底色与黑白增强");
    parser.addHelpOption();parser.addVersionOption();
    parser.addOptions({{{"i","input"},"输入 PDF 或图片","path"},{{"o","output"},"命令行导出 PDF、PNG 或 JPG","path"},
                       {"mode","bw 或 gray","mode","gray"},{"dpi","导出 DPI，100 至 400","dpi","300"},
                       {"first","起始页或导出图片的页码，从 1 开始","page","1"},
                       {"last","最后一页，0 表示 PDF 输出到末页，图片输出为单页","page","0"},
                       {"background","去底色强度，0 至 100","value","100"},
                       {"darkness","文字深浅，-20 至 20","value","0"},
                       {"sharpness","轻度锐化，0 至 100","value","20"}});
    parser.process(app);
    QVariantMap values{{"mode",parser.value("mode")},{"dpi",parser.value("dpi")},
                       {"first",parser.value("first")},{"last",parser.value("last")},
                       {"background",parser.value("background")},{"darkness",parser.value("darkness")},
                       {"sharpness",parser.value("sharpness")}};
    if(parser.isSet("output")){
        if(!parser.isSet("input")){qCritical().noquote()<<"请使用 --input 指定 PDF 或图片";return 2;}
        for(const auto &key:{"dpi","first","last","background","darkness","sharpness"}){
            bool valid=false;values[key].toString().toInt(&valid);if(!valid){qCritical()<<"无效参数"<<key;return 2;}
        }
        if(!QStringList{"bw","gray"}.contains(parser.value("mode"))){qCritical()<<"--mode 必须是 bw 或 gray";return 2;}
        const QMap<QString,QPair<int,int>> limits{{"dpi",{100,400}},{"first",{1,1000000}},{"last",{0,1000000}},
            {"background",{0,100}},{"darkness",{-20,20}},{"sharpness",{0,100}}};
        for(auto it=limits.cbegin();it!=limits.cend();++it)if(values[it.key()].toInt()<it->first||values[it.key()].toInt()>it->second){qCritical()<<"参数超出范围"<<it.key();return 2;}
        auto result=Scan::exportFile(parser.value("input"),parser.value("output"),Scan::Options::fromMap(values),{},
                                    [](int done,int total){qInfo().noquote()<<QString("%1 / %2").arg(done).arg(total);});
        if(!result.success){qCritical().noquote()<<result.error;return 1;}return 0;
    }
#ifdef SCAN_LEGACY_WINDOWS
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
#endif
    QQuickStyle::setStyle("Basic");
    QQmlApplicationEngine engine;auto images=new PreviewImages;
    CleanerService service(images);engine.addImageProvider("scan",images);
    engine.rootContext()->setContextProperty("cleaner",&service);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[]{QCoreApplication::exit(1);},Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/ScanCleaner/qml/Main.qml")));
    if(parser.isSet("input"))QTimer::singleShot(0,&service,[&]{service.open(QUrl::fromLocalFile(parser.value("input")));});
    return app.exec();
}
