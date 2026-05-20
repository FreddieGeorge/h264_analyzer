#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QWidget;

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QWidget *dialogParent, QObject *parent = nullptr);

    void checkForUpdates();

signals:
    void busyChanged(bool busy);
    void statusMessage(const QString &message, int timeoutMs);
    void logMessage(const QString &message);
    void applicationCloseRequested();

private:
    QNetworkAccessManager *networkManager();
    void setBusy(bool busy);
    void downloadAndInstallUpdate(const QUrl &installerUrl,
                                  const QUrl &checksumUrl,
                                  const QString &installerFileName,
                                  const QString &tagName);

    QWidget *m_dialogParent = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_busy = false;
};
