#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ProjectCatalog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(QString currentArchitecture READ currentArchitecture CONSTANT)

public:
    explicit ProjectCatalog(QObject *parent = nullptr);

    bool isLoaded() const;
    QString errorString() const;
    QString currentArchitecture() const;

    Q_INVOKABLE QVariantList projects() const;
    Q_INVOKABLE QVariantMap project(const QString &projectId) const;
    Q_INVOKABLE QVariantList projectsForGame(const QString &metadataTitle,
                                              const QString &platform) const;
    Q_INVOKABLE bool isAssetCompatible(const QString &projectId,
                                       const QString &architecture,
                                       const QString &assetName) const;

    static bool parseCatalog(const QByteArray &json,
                             QVariantList *projects,
                             QString *errorMessage = nullptr);
    static QString normalizeGameTitle(const QString &title);
    static QString normalizeArchitecture(const QString &architecture);

private:
    QVariantList m_projects;
    QString m_errorString;
    bool m_loaded = false;
};
