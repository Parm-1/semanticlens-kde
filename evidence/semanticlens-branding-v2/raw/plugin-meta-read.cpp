#include <KPluginMetaData>
#include <QPluginLoader>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cstdio>
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    const QString path = QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    QPluginLoader loader(path);
    const auto qt = loader.metaData();
    const KPluginMetaData metadata(path);
    QJsonObject result{{"module", path}, {"exists", QFileInfo::exists(path)}, {"qt_metadata", qt}, {"valid", metadata.isValid()}, {"plugin_id", metadata.pluginId()}, {"name", metadata.name()}, {"description", metadata.description()}, {"raw_metadata", metadata.rawData()}, {"loader_error", loader.errorString()}};
    const auto bytes = QJsonDocument(result).toJson(QJsonDocument::Compact);
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stdout); std::fputc('\n', stdout);
    return metadata.isValid() && metadata.name() == QStringLiteral("SemanticLens for KDE") && metadata.pluginId() == QStringLiteral("kde-index-runner") ? 0 : 1;
}
