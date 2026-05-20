#include "app/UpdateChecker.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <memory>

namespace
{
struct ReleaseAsset
{
    QString name;
    QString downloadUrl;
};

QVector<int> parseVersionParts(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }

    const int suffixIndex = version.indexOf(QRegularExpression(QStringLiteral("[+-]")));
    if (suffixIndex >= 0) {
        version.truncate(suffixIndex);
    }

    QVector<int> parts;
    const QStringList tokens = version.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    for (const QString &token : tokens) {
        const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("^(\\d+)")).match(token);
        parts.append(match.hasMatch() ? match.captured(1).toInt() : 0);
    }
    return parts;
}

int compareVersions(const QString &left, const QString &right)
{
    const QVector<int> leftParts = parseVersionParts(left);
    const QVector<int> rightParts = parseVersionParts(right);
    const int partCount = std::max(leftParts.size(), rightParts.size());
    for (int i = 0; i < partCount; ++i) {
        const int leftValue = i < leftParts.size() ? leftParts.at(i) : 0;
        const int rightValue = i < rightParts.size() ? rightParts.at(i) : 0;
        if (leftValue != rightValue) {
            return leftValue < rightValue ? -1 : 1;
        }
    }
    return 0;
}

QString compactReleaseNotes(QString body)
{
    body = body.trimmed();
    if (body.size() > 1200) {
        body = body.left(1200).trimmed() + QStringLiteral("...");
    }
    return body;
}

ReleaseAsset releaseAsset(const QJsonObject &release, const QRegularExpression &namePattern)
{
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &assetValue : assets) {
        if (!assetValue.isObject()) {
            continue;
        }

        const QJsonObject asset = assetValue.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!namePattern.match(name).hasMatch()) {
            continue;
        }

        const QString downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
        if (!downloadUrl.isEmpty()) {
            return ReleaseAsset { name, downloadUrl };
        }
    }

    return ReleaseAsset {};
}

QString sha256FromChecksumText(const QString &checksumText)
{
    const QRegularExpression hashPattern(QStringLiteral("\\b([A-Fa-f0-9]{64})\\b"));
    const QRegularExpressionMatch match = hashPattern.match(checksumText);
    if (!match.hasMatch()) {
        return QString {};
    }
    return match.captured(1).toLower();
}

QString sha256ForFile(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return QString {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Failed to read file for SHA256 hashing.");
        }
        return QString {};
    }

    return QString::fromLatin1(hash.result().toHex());
}

QString safeInstallerFileName(QString fileName)
{
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("ZStreamEye-update-setup.exe");
    }
    fileName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    if (!fileName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        fileName.append(QStringLiteral(".exe"));
    }
    return fileName;
}
}

UpdateChecker::UpdateChecker(QWidget *dialogParent, QObject *parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
}

QNetworkAccessManager *UpdateChecker::networkManager()
{
    if (m_networkManager == nullptr) {
        m_networkManager = new QNetworkAccessManager(this);
    }
    return m_networkManager;
}

void UpdateChecker::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(m_busy);
}

void UpdateChecker::checkForUpdates()
{
    setBusy(true);

    const QUrl latestReleaseUrl(QStringLiteral("https://api.github.com/repos/FreddieGeorge/ZStreamEye/releases/latest"));
    QNetworkRequest request(latestReleaseUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    emit statusMessage(tr("Checking for updates..."), 3000);
    emit logMessage(tr("[Info] Checking GitHub Releases for updates."));

    QNetworkReply *reply = networkManager()->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);

        if (reply->error() != QNetworkReply::NoError) {
            const QString message = tr("Update check failed: %1").arg(reply->errorString());
            emit statusMessage(message, 5000);
            emit logMessage(tr("[Warning] %1").arg(message));
            QMessageBox::warning(m_dialogParent, tr("Check for Updates"), message);
            return;
        }

        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            const QString message = tr("GitHub returned an invalid release response.");
            emit statusMessage(message, 5000);
            emit logMessage(tr("[Warning] %1").arg(message));
            QMessageBox::warning(m_dialogParent, tr("Check for Updates"), message);
            return;
        }

        const QJsonObject release = document.object();
        const QString tagName = release.value(QStringLiteral("tag_name")).toString().trimmed();
        const QString releasePage = release.value(QStringLiteral("html_url")).toString().trimmed();
        const QString releaseNotes = compactReleaseNotes(release.value(QStringLiteral("body")).toString());
        const ReleaseAsset installerAsset = releaseAsset(
            release,
            QRegularExpression(QStringLiteral("^ZStreamEye-.+-windows-ucrt64-setup\\.exe$")));
        const ReleaseAsset portableAsset = releaseAsset(
            release,
            QRegularExpression(QStringLiteral("^ZStreamEye-.+-windows-ucrt64\\.zip$")));
        const ReleaseAsset installerChecksumAsset = installerAsset.name.isEmpty()
            ? ReleaseAsset {}
            : releaseAsset(
                  release,
                  QRegularExpression(QStringLiteral("^%1\\.sha256$")
                                         .arg(QRegularExpression::escape(installerAsset.name))));
        const QString currentVersion = QCoreApplication::applicationVersion();

        if (tagName.isEmpty() || releasePage.isEmpty()) {
            const QString message = tr("GitHub release response did not include a release tag or page URL.");
            emit statusMessage(message, 5000);
            emit logMessage(tr("[Warning] %1").arg(message));
            QMessageBox::warning(m_dialogParent, tr("Check for Updates"), message);
            return;
        }

        if (compareVersions(tagName, currentVersion) <= 0) {
            emit statusMessage(tr("ZStreamEye is up to date."), 4000);
            emit logMessage(tr("[Info] No update available. Current version: %1, latest release: %2.")
                                .arg(currentVersion, tagName));
            QMessageBox::information(
                m_dialogParent,
                tr("Check for Updates"),
                tr("You are using the latest version.\n\nCurrent version: %1\nLatest release: %2")
                    .arg(currentVersion, tagName));
            return;
        }

        QMessageBox messageBox(m_dialogParent);
        messageBox.setWindowTitle(tr("Update Available"));
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setText(tr("A new ZStreamEye release is available."));
        messageBox.setInformativeText(
            tr("Current version: %1\nLatest release: %2\n\nDownload the installer, verify SHA256, then close ZStreamEye and start the installer.")
                .arg(currentVersion, tagName));
        if (!releaseNotes.isEmpty()) {
            messageBox.setDetailedText(releaseNotes);
        }

        QPushButton *downloadInstallerButton = nullptr;
        QPushButton *downloadPortableButton = nullptr;
        if (!installerAsset.downloadUrl.isEmpty() && !installerChecksumAsset.downloadUrl.isEmpty()) {
            downloadInstallerButton = messageBox.addButton(tr("Download and Install"), QMessageBox::AcceptRole);
        } else if (!installerAsset.downloadUrl.isEmpty()) {
            messageBox.setInformativeText(
                messageBox.informativeText()
                + tr("\n\nThe installer is available, but its SHA256 checksum asset is missing. Open the release page to download it manually."));
        }
        if (!portableAsset.downloadUrl.isEmpty()) {
            downloadPortableButton = messageBox.addButton(tr("Download Portable ZIP"), QMessageBox::ActionRole);
        }
        QPushButton *openButton = messageBox.addButton(tr("Open Release Page"), QMessageBox::AcceptRole);
        messageBox.addButton(QMessageBox::Close);
        messageBox.exec();

        if (messageBox.clickedButton() == downloadInstallerButton) {
            downloadAndInstallUpdate(QUrl(installerAsset.downloadUrl),
                                     QUrl(installerChecksumAsset.downloadUrl),
                                     installerAsset.name,
                                     tagName);
        } else if (messageBox.clickedButton() == downloadPortableButton) {
            QDesktopServices::openUrl(QUrl(portableAsset.downloadUrl));
        } else if (messageBox.clickedButton() == openButton) {
            QDesktopServices::openUrl(QUrl(releasePage));
        }

        emit statusMessage(tr("Update available: %1").arg(tagName), 5000);
        emit logMessage(tr("[Info] Update available. Current version: %1, latest release: %2.")
                            .arg(currentVersion, tagName));
    });
}

void UpdateChecker::downloadAndInstallUpdate(const QUrl &installerUrl,
                                             const QUrl &checksumUrl,
                                             const QString &installerFileName,
                                             const QString &tagName)
{
    const QString tempRoot = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir updateDir(QDir(tempRoot).filePath(QStringLiteral("ZStreamEye-updates")));
    if (!updateDir.exists() && !QDir().mkpath(updateDir.absolutePath())) {
        QMessageBox::warning(m_dialogParent,
                             tr("Update ZStreamEye"),
                             tr("Failed to create update download directory:\n%1").arg(updateDir.absolutePath()));
        return;
    }

    const QString safeFileName = safeInstallerFileName(installerFileName);
    const QString installerPath = updateDir.filePath(safeFileName);
    if (QFile::exists(installerPath) && !QFile::remove(installerPath)) {
        QMessageBox::warning(m_dialogParent,
                             tr("Update ZStreamEye"),
                             tr("Failed to replace existing installer:\n%1").arg(installerPath));
        return;
    }

    setBusy(true);

    auto canceled = std::make_shared<bool>(false);
    auto *progress = new QProgressDialog(tr("Downloading checksum..."), tr("Cancel"), 0, 0, m_dialogParent);
    progress->setWindowTitle(tr("Update ZStreamEye"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->show();

    QNetworkRequest checksumRequest(checksumUrl);
    checksumRequest.setHeader(QNetworkRequest::UserAgentHeader,
                              QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
    QNetworkReply *checksumReply = networkManager()->get(checksumRequest);
    connect(progress, &QProgressDialog::canceled, checksumReply, [checksumReply, canceled]() {
        *canceled = true;
        checksumReply->abort();
    });

    connect(checksumReply, &QNetworkReply::finished, this, [this, checksumReply, installerUrl, installerPath, tagName, progress, canceled]() {
        checksumReply->deleteLater();

        const auto finishWithWarning = [this, progress](const QString &message) {
            setBusy(false);
            progress->close();
            progress->deleteLater();
            emit statusMessage(message, 5000);
            emit logMessage(tr("[Warning] %1").arg(message));
            QMessageBox::warning(m_dialogParent, tr("Update ZStreamEye"), message);
        };
        const auto finishCanceled = [this, progress]() {
            setBusy(false);
            progress->close();
            progress->deleteLater();
            emit statusMessage(tr("Update canceled."), 3000);
            emit logMessage(tr("[Info] Update canceled by user."));
        };

        if (checksumReply->error() != QNetworkReply::NoError) {
            if (*canceled) {
                finishCanceled();
            } else {
                finishWithWarning(tr("Failed to download SHA256 checksum. Please try again.\n\n%1")
                                      .arg(checksumReply->errorString()));
            }
            return;
        }

        const QString expectedSha256 = sha256FromChecksumText(QString::fromUtf8(checksumReply->readAll()));
        if (expectedSha256.isEmpty()) {
            finishWithWarning(tr("The release checksum file did not contain a valid SHA256 hash. Open the release page and download the installer manually."));
            return;
        }

        progress->setLabelText(tr("Downloading installer..."));
        progress->setRange(0, 0);
        progress->setValue(0);

        auto *installerFile = new QFile(installerPath, progress);
        if (!installerFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            finishWithWarning(tr("Failed to write installer: %1").arg(installerFile->errorString()));
            installerFile->deleteLater();
            return;
        }

        QNetworkRequest installerRequest(installerUrl);
        installerRequest.setHeader(QNetworkRequest::UserAgentHeader,
                                   QStringLiteral("ZStreamEye/%1").arg(QCoreApplication::applicationVersion()));
        QNetworkReply *installerReply = networkManager()->get(installerRequest);
        connect(progress, &QProgressDialog::canceled, installerReply, [installerReply, canceled]() {
            *canceled = true;
            installerReply->abort();
        });
        connect(installerReply, &QNetworkReply::readyRead, installerFile, [installerReply, installerFile]() {
            installerFile->write(installerReply->readAll());
        });
        connect(installerReply, &QNetworkReply::downloadProgress, progress, [progress](qint64 bytesReceived, qint64 bytesTotal) {
            if (bytesTotal > 0) {
                progress->setRange(0, 100);
                progress->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
            } else {
                progress->setRange(0, 0);
            }
        });

        connect(installerReply, &QNetworkReply::finished, this, [this, installerReply, installerFile, installerPath, expectedSha256, tagName, progress, canceled]() {
            installerReply->deleteLater();
            installerFile->write(installerReply->readAll());
            installerFile->flush();
            installerFile->close();
            installerFile->deleteLater();

            const auto finishWithWarning = [this, progress, installerPath](const QString &message) {
                setBusy(false);
                QFile::remove(installerPath);
                progress->close();
                progress->deleteLater();
                emit statusMessage(message, 5000);
                emit logMessage(tr("[Warning] %1").arg(message));
                QMessageBox::warning(m_dialogParent, tr("Update ZStreamEye"), message);
            };
            const auto finishCanceled = [this, progress, installerPath]() {
                setBusy(false);
                QFile::remove(installerPath);
                progress->close();
                progress->deleteLater();
                emit statusMessage(tr("Update canceled."), 3000);
                emit logMessage(tr("[Info] Update canceled by user."));
            };

            if (installerReply->error() != QNetworkReply::NoError) {
                if (*canceled) {
                    finishCanceled();
                } else {
                    finishWithWarning(tr("Failed to download installer. Please try again.\n\n%1")
                                          .arg(installerReply->errorString()));
                }
                return;
            }

            progress->setLabelText(tr("Verifying SHA256..."));
            progress->setRange(0, 0);
            QString hashError;
            const QString actualSha256 = sha256ForFile(installerPath, &hashError);
            if (actualSha256.isEmpty()) {
                finishWithWarning(tr("Failed to verify installer: %1").arg(hashError));
                return;
            }
            if (actualSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
                finishWithWarning(tr("Installer SHA256 verification failed. The downloaded file was removed."));
                return;
            }

            progress->close();
            progress->deleteLater();

            const QMessageBox::StandardButton choice = QMessageBox::question(
                m_dialogParent,
                tr("Install Update"),
                tr("ZStreamEye %1 was downloaded and verified.\n\nThe installer will now start, and ZStreamEye will close. Continue?")
                    .arg(tagName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
            if (choice != QMessageBox::Yes) {
                setBusy(false);
                emit statusMessage(tr("Update installer downloaded and verified."), 5000);
                emit logMessage(tr("[Info] Update installer downloaded and verified: %1")
                                    .arg(QDir::toNativeSeparators(installerPath)));
                return;
            }

            const bool started = QProcess::startDetached(
                QDir::toNativeSeparators(installerPath),
                QStringList {},
                QFileInfo(installerPath).absolutePath());
            if (!started) {
                setBusy(false);
                const QString message = tr("Failed to start installer: %1").arg(QDir::toNativeSeparators(installerPath));
                emit statusMessage(message, 5000);
                emit logMessage(tr("[Warning] %1").arg(message));
                QMessageBox::warning(m_dialogParent, tr("Update ZStreamEye"), message);
                return;
            }

            emit logMessage(tr("[Info] Started verified update installer: %1")
                                .arg(QDir::toNativeSeparators(installerPath)));
            emit applicationCloseRequested();
        });
    });
}
