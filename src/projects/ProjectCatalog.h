#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class ProjectCatalog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded CONSTANT)
    Q_PROPERTY(QString errorString READ errorString CONSTANT)
    Q_PROPERTY(QString currentArchitecture READ currentArchitecture CONSTANT)
    // The system RecompStation is running on, detected at startup from the
    // running kernel (not from how it was built): "linux" or "windows".
    Q_PROPERTY(QString currentSystem READ currentSystem CONSTANT)
    // "<system>-<architecture>", the key of the builds this copy installs
    // by default ("linux-x86_64").
    Q_PROPERTY(QString currentBuild READ currentBuild CONSTANT)

public:
    explicit ProjectCatalog(QObject *parent = nullptr);

    bool isLoaded() const;
    QString errorString() const;
    QString currentArchitecture() const;
    QString currentSystem() const;
    QString currentBuild() const;

    Q_INVOKABLE QVariantList projects() const;
    Q_INVOKABLE QStringList supportedPlatforms() const;
    Q_INVOKABLE QVariantMap project(const QString &projectId) const;
    Q_INVOKABLE QVariantList projectsForGame(const QString &metadataTitle,
                                              const QString &platform) const;
    // Why a release has nothing to install here, in a sentence for Game
    // Details: what the release offers instead (Windows, macOS, ARM, a
    // Flatpak, source only) and for which system it was checked.
    Q_INVOKABLE QString explainNoCompatibleAsset(const QString &projectId,
                                                 const QVariantList &assets,
                                                 const QString &build = QString()) const;
    // Assets of a release that match one build of the project; the current
    // build when none is given.
    Q_INVOKABLE QVariantList compatibleAssets(const QString &projectId,
                                               const QVariantList &assets,
                                               const QString &build = QString()) const;
    // build: a build key ("windows-x86_64"), or an architecture alone
    // ("x86_64"), which means that architecture on the current system.
    Q_INVOKABLE bool isAssetCompatible(const QString &projectId,
                                       const QString &build,
                                       const QString &assetName) const;
    // The build keys a project publishes ("linux-x86_64", "windows-x86_64").
    Q_INVOKABLE QStringList availableBuilds(const QString &projectId) const;
    // Download rules for one build (assetNameRegex, packageFormat,
    // executablePath, system, architecture), or empty.
    Q_INVOKABLE QVariantMap buildRules(const QString &projectId, const QString &build) const;
    // "64-bit Linux", "64-bit Windows", "Linux on aarch64".
    Q_INVOKABLE QString buildLabel(const QString &build) const;

    // Replaces the bundled catalog (tests use small catalogs of their own).
    bool loadFromJson(const QByteArray &json);

    static bool parseCatalog(const QByteArray &json,
                             QVariantList *projects,
                             QString *errorMessage = nullptr);
    static QString normalizeGameTitle(const QString &title);
    static QString normalizeArchitecture(const QString &architecture);
    // "linux" for a Linux kernel, "windows" for Windows ("winnt"), else empty.
    static QString systemForKernelType(const QString &kernelType);
    // "x86_64" -> "<current system>-x86_64"; a full build key is normalized.
    QString resolveBuild(const QString &build) const;

private:
    QVariantList m_projects;
    QString m_errorString;
    bool m_loaded = false;
};
