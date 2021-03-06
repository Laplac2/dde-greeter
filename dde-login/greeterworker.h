#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include <QLightDM/Greeter>
#include <QLightDM/SessionsModel>
#include <QObject>

#include "dbuslockservice.h"
#include "authinterface.h"
#include "deepinauthframework.h"
#include "dbuslogin1manager.h"
#include "sessionbasemodel.h"
#include <com_deepin_daemon_authenticate.h>

using com::deepin::daemon::Authenticate;

class GreeterWorker : public Auth::AuthInterface
{
    Q_OBJECT
public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    explicit GreeterWorker(SessionBaseModel *const model, QObject *parent = nullptr);
    ~GreeterWorker();

    void switchToUser(std::shared_ptr<User> user) override;

signals:
    void requestUpdateBackground(const QString &path);
    void requestShowPrompt(const QString &prompt);
    void requestShowMessage(const QString &message);

public slots:
    /* New authentication framework */
    void createAuthentication(const QString &account);
    void destoryAuthentication(const QString &account);
    void startAuthentication(const QString &account, const int authType);
    void endAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);

    void checkAccount(const QString &account);
    void restartResetSessionTimer();
    void onAuthFinished();
    void handleAuthStatusChanged(const int type, const int status, const QString &message);
    void onPasswdRespond(const QString &passwd);

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void doPowerAction(const SessionBaseModel::PowerAction action);
    void setCurrentUser(const std::shared_ptr<User> user);

    void checkDBusServer(bool isvalid);
    void showPrompt(const QString &text, const QLightDM::Greeter::PromptType type);
    void showMessage(const QString &text, const QLightDM::Greeter::MessageType type);
    void authenticationComplete();
    void saveNumlockStatus(std::shared_ptr<User> user, const bool &on);
    void recoveryUserKBState(std::shared_ptr<User> user);
    void startGreeterAuth(const QString &account = QString());

private:
    QLightDM::Greeter *m_greeter;
    DeepinAuthFramework *m_authFramework;
    DBusLockService *m_lockInter;
    QTimer *m_resetSessionTimer;
    QTimer *m_limitsUpdateTimer;
    QString m_account;
    QString m_password;
    bool m_haveRespondedToLightdm;
};

#endif  // GREETERWORKEK_H
