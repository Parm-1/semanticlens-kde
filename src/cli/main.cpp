#include <QtCore>
#include <QtNetwork>
#include <unistd.h>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    QCommandLineParser p;
    p.addHelpOption();
    p.addOption({
        "socket","Local service socket","name","kde-index-"+QString::number(getuid())
    }
    );
    p.addOption({
        "timeout","Response timeout seconds","seconds","120"
    }
    );
    p.addPositionalArgument("request","JSON object; id added if absent");
    p.process(app);
    if(p.positionalArguments().size()!=1)p.showHelp(2);
    QJsonParseError error;
    auto doc=QJsonDocument::fromJson(p.positionalArguments()[0].toUtf8(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()){
        qCritical("invalid request JSON");
        return 2;
    }
    auto request=doc.object();
    if(!request.contains("id"))request["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
    QLocalSocket s;
    s.connectToServer(p.value("socket"));
    if(!s.waitForConnected(3000)){
        qCritical()<<s.errorString();
        return 2;
    }
    s.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+"\n");
    s.flush();
    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();
    int timeout=p.value("timeout").toInt()*1000;
    while(timer.elapsed()<timeout){
        s.waitForReadyRead(100);
        buffer+=s.readAll();
        while(buffer.contains('\n')){
            int n=buffer.indexOf('\n');
            auto line=buffer.left(n);
            buffer.remove(0,n+1);
            auto response=QJsonDocument::fromJson(line).object();
            if(response.value("id")==request.value("id")){
                QFile out;
                if(!out.open(stdout,QIODevice::WriteOnly))return 2;
                out.write(line+"\n");
                return response.value("ok").toBool()?0:1;
            }
        }
        if(s.state()==QLocalSocket::UnconnectedState)break;
    }
    qCritical("response timeout or disconnection");
    return 2;
}
