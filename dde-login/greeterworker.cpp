#include "greeterworker.h"

#include "auth_common.h"
#include "keyboardmonitor.h"
#include "userinfo.h"

#include <DSysInfo>

#include <QGSettings>

#include <com_deepin_system_systempower.h>
#include <pwd.h>

#define LOCKSERVICE_PATH "/com/deepin/dde/LockService"
#define LOCKSERVICE_NAME "com.deepin.dde.LockService"

using PowerInter = com::deepin::system::Power;
using namespace Auth;
using namespace AuthCommon;
DCORE_USE_NAMESPACE

class UserNumlockSettings
{
public:
    UserNumlockSettings(const QString &username)
        : m_username(username)
        , m_settings(QSettings::UserScope, "deepin", "greeter")
    {
    }

    int get(const int defaultValue) { return m_settings.value(m_username, defaultValue).toInt(); }
    void set(const int value) { m_settings.setValue(m_username, value); }

private:
    QString m_username;
    QSettings m_settings;
};

GreeterWorker::GreeterWorker(SessionBaseModel *const model, QObject *parent)
    : AuthInterface(model, parent)
    , m_greeter(new QLightDM::Greeter(this))
    , m_authFramework(new DeepinAuthFramework(this))
    , m_lockInter(new DBusLockService(LOCKSERVICE_NAME, LOCKSERVICE_PATH, QDBusConnection::systemBus(), this))
    , m_resetSessionTimer(new QTimer(this))
    , m_limitsUpdateTimer(new QTimer(this))
    , m_haveRespondedToLightdm(false)
{
#ifndef QT_DEBUG
    if (!m_greeter->connectSync()) {
        qCritical() << "greeter connect fail !!!";
        exit(1);
    }
#endif

    checkDBusServer(m_accountsInter->isValid());

    initConnections();
    initData();
    initConfiguration();

    m_limitsUpdateTimer->setSingleShot(true);
    m_limitsUpdateTimer->setInterval(50);

    //认证超时重启
    m_resetSessionTimer->setInterval(15000);
    if (QGSettings::isSchemaInstalled("com.deepin.dde.session-shell")) {
        QGSettings gsetting("com.deepin.dde.session-shell", "/com/deepin/dde/session-shell/", this);
        if (gsetting.keys().contains("authResetTime")) {
            int resetTime = gsetting.get("auth-reset-time").toInt();
            if (resetTime > 0)
                m_resetSessionTimer->setInterval(resetTime);
        }
    }

    m_resetSessionTimer->setSingleShot(true);
    connect(m_resetSessionTimer, &QTimer::timeout, this, [=] {
        endAuthentication(m_account, AT_All);
        m_model->updateAuthStatus(AT_All, AS_Cancel, "Cancel");
        destoryAuthentication(m_account);
        createAuthentication(m_account);
    });
}

GreeterWorker::~GreeterWorker()
{
}

void GreeterWorker::initConnections()
{
    /* greeter */
    connect(m_greeter, &QLightDM::Greeter::showPrompt, this, &GreeterWorker::showPrompt);
    connect(m_greeter, &QLightDM::Greeter::showMessage, this, &GreeterWorker::showMessage);
    connect(m_greeter, &QLightDM::Greeter::authenticationComplete, this, &GreeterWorker::authenticationComplete);
    /* com.deepin.daemon.Accounts */
    connect(m_accountsInter, &AccountsInter::UserAdded, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::addUser));
    connect(m_accountsInter, &AccountsInter::UserDeleted, m_model, static_cast<void (SessionBaseModel::*)(const QString &)>(&SessionBaseModel::removeUser));
    // connect(m_accountsInter, &AccountsInter::UserListChanged, m_model, &SessionBaseModel::updateUserList); // UserListChanged信号的处理， 改用UserAdded和UserDeleted信号替代
    connect(m_loginedInter, &LoginedInter::LastLogoutUserChanged, m_model, static_cast<void (SessionBaseModel::*)(const uid_t)>(&SessionBaseModel::updateLastLogoutUser));
    connect(m_loginedInter, &LoginedInter::UserListChanged, m_model, &SessionBaseModel::updateLoginedUserList);
    /* com.deepin.daemon.Authenticate */
    connect(m_authFramework, &DeepinAuthFramework::FramworkStateChanged, m_model, &SessionBaseModel::updateFrameworkState);
    connect(m_authFramework, &DeepinAuthFramework::LimitsInfoChanged, this, [this](const QString &account) {
        qDebug() << "GreeterWorker::initConnections LimitsInfoChanged:" << account;
        if (account == m_model->currentUser()->name()) {
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::SupportedEncryptsChanged, m_model, &SessionBaseModel::updateSupportedEncryptionType);
    connect(m_authFramework, &DeepinAuthFramework::SupportedMixAuthFlagsChanged, m_model, &SessionBaseModel::updateSupportedMixAuthFlags);
    /* com.deepin.daemon.Authenticate.Session */
    connect(m_authFramework, &DeepinAuthFramework::AuthStatusChanged, this, &GreeterWorker::handleAuthStatusChanged);
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    /* org.freedesktop.login1.Session */
    connect(m_login1SessionSelf, &Login1SessionSelf::ActiveChanged, this, [=](bool active) {
        qDebug() << "Login1SessionSelf::ActiveChanged:" << active;
        if (m_model->currentUser() == nullptr || m_model->currentUser()->name().isEmpty()) {
            return;
        }
        if (active) {
            if (!m_model->isServerModel() && !m_model->currentUser()->isNoPasswordLogin()) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            endAuthentication(m_account, AT_All);
            destoryAuthentication(m_account);
        }
    });
    /* org.freedesktop.login1.Manager */
    connect(m_login1Inter, &DBusLogin1Manager::PrepareForSleep, this, [=](bool isSleep) {
        qDebug() << "DBusLogin1Manager::PrepareForSleep:" << isSleep;
        // 登录界面待机或休眠时提供显示假黑屏，唤醒时显示正常界面
        m_model->setIsBlackModel(isSleep);

        if (isSleep) {
            endAuthentication(m_account, AT_All);
            destoryAuthentication(m_account);
        } else {
            createAuthentication(m_model->currentUser()->name());
        }
    });
    /* com.deepin.dde.LockService */
    connect(m_lockInter, &DBusLockService::UserChanged, this, [=](const QString &json) {
        qDebug() << "DBusLockService::UserChanged:" << json;
        m_resetSessionTimer->stop();
        m_model->updateCurrentUser(json);
        std::shared_ptr<User> user_ptr = m_model->currentUser();
        const QString &account = user_ptr->name();
        if (user_ptr.get()->isNoPasswordLogin()) {
            emit m_model->authTypeChanged(AT_None);
            m_account = account;
        }
        emit m_model->switchUserFinished();
    });
    /* model */
    connect(m_model, &SessionBaseModel::authTypeChanged, this, [ = ](const int type) {
        if (type > 0 && !m_model->currentUser()->limitsInfo()->value(type).locked && m_model->getAuthProperty().MFAFlag) {
            startAuthentication(m_account, m_model->getAuthProperty().AuthType);
        }
        m_limitsUpdateTimer->start();
    });
    connect(m_model, &SessionBaseModel::onPowerActionChanged, this, &GreeterWorker::doPowerAction);
    connect(m_model, &SessionBaseModel::currentUserChanged, this, &GreeterWorker::recoveryUserKBState);
    connect(m_model, &SessionBaseModel::visibleChanged, this, [=](bool visible) {
        if (visible) {
            if (!m_model->isServerModel() && (!m_model->currentUser()->isNoPasswordLogin()
                || m_model->currentUser()->expiredStatus() == User::ExpiredAlready)) {
                createAuthentication(m_model->currentUser()->name());
            }
        } else {
            m_resetSessionTimer->stop();
        }
    });
    /* others */
    connect(KeyboardMonitor::instance(), &KeyboardMonitor::numlockStatusChanged, this, [=](bool on) {
        saveNumlockStatus(m_model->currentUser(), on);
    });
    connect(m_limitsUpdateTimer, &QTimer::timeout, this, [this] {
        if (m_authFramework->isDeepinAuthValid())
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_account));
    });
}

void GreeterWorker::initData()
{
    /* com.deepin.daemon.Accounts */
    m_model->updateUserList(m_accountsInter->userList());
    m_model->updateLastLogoutUser(m_loginedInter->lastLogoutUser());
    m_model->updateLoginedUserList(m_loginedInter->userList());

    /* com.deepin.udcp.iam */
    QDBusInterface ifc("com.deepin.udcp.iam", "/com/deepin/udcp/iam", "com.deepin.udcp.iam", QDBusConnection::systemBus(), this);
    const bool allowShowCustomUser = valueByQSettings<bool>("", "loginPromptInput", false) || ifc.property("Enable").toBool();
    m_model->setAllowShowCustomUser(allowShowCustomUser);

    /* init cureent user */
    if (DSysInfo::deepinType() == DSysInfo::DeepinServer || m_model->allowShowCustomUser()) {
        std::shared_ptr<User> user(new User());
        m_model->setIsServerModel(DSysInfo::deepinType() == DSysInfo::DeepinServer);
        m_model->addUser(user);
        if (DSysInfo::deepinType() == DSysInfo::DeepinServer) {
            m_model->updateCurrentUser(user);
        } else {
            /* com.deepin.dde.LockService */
            m_model->updateCurrentUser(m_lockInter->CurrentUser());
        }
    } else {
        connect(m_login1Inter, &DBusLogin1Manager::SessionRemoved, this, [=] {
            qDebug() << "DBusLogin1Manager::SessionRemoved";
            // lockservice sometimes fails to call on olar server
            QDBusPendingReply<QString> replay = m_lockInter->CurrentUser();
            replay.waitForFinished();

            if (!replay.isError()) {
                const QJsonObject obj = QJsonDocument::fromJson(replay.value().toUtf8()).object();
                auto user_ptr = m_model->findUserByUid(static_cast<uint>(obj["Uid"].toInt()));

                m_model->updateCurrentUser(user_ptr);
            }
        });

        /* com.deepin.dde.LockService */
        m_model->updateCurrentUser(m_lockInter->CurrentUser());
    }

    /* com.deepin.daemon.Authenticate */
    if (m_authFramework->isDeepinAuthValid()) {
        m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
        m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
        m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
    }
}

void GreeterWorker::initConfiguration()
{
    m_model->setAlwaysShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Always);
    m_model->setAllowShowUserSwitchButton(getGSettings("", "switchuser").toInt() == AuthInterface::Ondemand);

    checkPowerInfo();

    if (QFile::exists("/etc/deepin/no_suspend")) {
        m_model->setCanSleep(false);
    }

    //当这个配置不存在是，如果是不是笔记本就打开小键盘，否则就关闭小键盘 0关闭键盘 1打开键盘 2默认值（用来判断是不是有这个key）
    if (m_model->currentUser() != nullptr && UserNumlockSettings(m_model->currentUser()->name()).get(2) == 2) {
        PowerInter powerInter("com.deepin.system.Power", "/com/deepin/system/Power", QDBusConnection::systemBus(), this);
        if (powerInter.hasBattery()) {
            saveNumlockStatus(m_model->currentUser(), 0);
        } else {
            saveNumlockStatus(m_model->currentUser(), 1);
        }
        recoveryUserKBState(m_model->currentUser());
    }
}

void GreeterWorker::doPowerAction(const SessionBaseModel::PowerAction action)
{
    switch (action) {
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        break;
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        break;
    // 在登录界面请求待机或者休眠时，通过显示假黑屏挡住输入密码界面，防止其闪现
    case SessionBaseModel::PowerAction::RequireSuspend:
        m_model->setIsBlackModel(true);
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        m_login1Inter->Suspend(true);
        break;
    case SessionBaseModel::PowerAction::RequireHibernate:
        m_model->setIsBlackModel(true);
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
        m_login1Inter->Hibernate(true);
        break;
    default:
        break;
    }

    m_model->setPowerAction(SessionBaseModel::PowerAction::None);
}

/**
 * @brief 将当前用户的信息保存到 LockService 服务
 *
 * @param user
 */
void GreeterWorker::setCurrentUser(const std::shared_ptr<User> user)
{
    QJsonObject json;
    json["Name"] = user->name();
    json["Type"] = user->type();
    json["Uid"] = static_cast<int>(user->uid());
    m_lockInter->SwitchToUser(QString(QJsonDocument(json).toJson(QJsonDocument::Compact))).waitForFinished();
}

void GreeterWorker::switchToUser(std::shared_ptr<User> user)
{
    if (user->name() == m_account) {
        if (!m_authFramework->authSessionExist(m_account))
            createAuthentication(m_account);

        return;
    }
    qInfo() << "switch user from" << m_account << " to " << user->name() << user->uid() << user->isLogin();
    endAuthentication(m_account, AT_All);

    if (user->uid() == INT_MAX) {
        startGreeterAuth();
        m_model->setAuthType(AT_None);
    }
    setCurrentUser(user);
    if (user->isLogin()) { // switch to user Xorg
        startGreeterAuth();
        QProcess::startDetached("dde-switchtogreeter", QStringList() << user->name());
    } else {
        m_model->updateAuthStatus(AT_All, AS_Cancel, "Cancel");
        destoryAuthentication(m_account);
        m_model->updateCurrentUser(user);
        if (!user->isNoPasswordLogin()) {
            createAuthentication(user->name());
        }
    }
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void GreeterWorker::createAuthentication(const QString &account)
{
    qDebug() << "GreeterWorker::createAuthentication:" << account;
    m_account = account;
    if (account.isEmpty()) {
        m_model->setAuthType(AT_None);
        return;
    }

    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    if (user_ptr) {
        user_ptr->updatePasswordExpiredInfo();
    }
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), Login);
        m_authFramework->SetAuthQuitFlag(account, DeepinAuthFramework::ManualQuit);
        if (!m_authFramework->SetPrivilegesEnable(account, QString("/usr/sbin/lightdm"))) {
            qWarning() << "Failed to set privileges!";
        }
        startGreeterAuth(account);
        break;
    default:
        startGreeterAuth(account);
        m_model->setAuthType(AT_PAM);
        break;
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void GreeterWorker::destoryAuthentication(const QString &account)
{
    qDebug() << "GreeterWorker::destoryAuthentication:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->DestoryAuthController(account);
        break;
    default:
        break;
    }
}

/**
 * @brief 开启认证服务    -- 作为接口提供给上层，屏蔽底层细节
 *
 * @param account   账户
 * @param authType  认证类型（可传入一种或多种）
 * @param timeout   设定超时时间（默认 -1）
 */
void GreeterWorker::startAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorker::startAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (!m_authFramework->authSessionExist(account))
            createAuthentication(account);

        m_authFramework->EndAuthentication(account, authType);
        m_authFramework->StartAuthentication(account, authType, -1);
        break;
    default:
        startGreeterAuth(account);
        break;
    }
}

/**
 * @brief 将密文发送给认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void GreeterWorker::sendTokenToAuth(const QString &account, const int authType, const QString &token)
{
    qDebug() << Q_FUNC_INFO
             << "account: " << account
             << "auth type: " << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (!m_authFramework->authSessionExist(account))
            createAuthentication(m_account);

        m_authFramework->SendTokenToAuth(account, authType, token);
        if (authType == AT_Password) {
            m_password = token; // 用于解锁密钥环
        }
        break;
    default:
        m_greeter->respond(token);
        m_haveRespondedToLightdm = true;
        break;
    }
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 */
void GreeterWorker::endAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorker::endAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (authType == AT_All)
            m_authFramework->SetPrivilegesDisable(account);

        m_authFramework->EndAuthentication(account, authType);
        break;
    default:
        break;
    }
}

/**
 * @brief 检查用户输入的账户是否合理
 *
 * @param account
 */
void GreeterWorker::checkAccount(const QString &account)
{
    qDebug() << "GreeterWorker::checkAccount:" << account;
    if (m_greeter->authenticationUser() == account) {
        return;
    }

    std::shared_ptr<User> user_ptr = m_model->findUserByName(account);
    // 当用户登录成功后，判断用户输入帐户有效性逻辑改为后端去做处理
    const QString userPath = m_accountsInter->FindUserByName(account);
    if (userPath.startsWith("/")) {
        user_ptr = std::make_shared<NativeUser>(userPath);

        // 对于没有设置密码的账户,直接认定为错误账户
        if (!user_ptr->isPasswordValid()) {
            qWarning() << userPath;
            emit m_model->authFaildTipsMessage(tr("Wrong account"));
            m_model->setAuthType(AT_None);
            startGreeterAuth();
            return;
        }
    } else if (user_ptr == nullptr) {
        // 判断账户第一次登录时的有效性
        std::string str = account.toStdString();
        passwd *pw = getpwnam(str.c_str());
        if (pw) {
            QString userName = pw->pw_name;
            QString userFullName = userName.leftRef(userName.indexOf(QString("@"))).toString();
            user_ptr = std::make_shared<ADDomainUser>(INT_MAX - 1);

            dynamic_cast<ADDomainUser *>(user_ptr.get())->setName(userName);
            dynamic_cast<ADDomainUser *>(user_ptr.get())->setFullName(userFullName);
        } else {
            qWarning() << userPath;
            emit m_model->authFaildTipsMessage(tr("Wrong account"));
            m_model->setAuthType(AT_None);
            startGreeterAuth();
            return;
        }
    }

    m_model->updateCurrentUser(user_ptr);
    if (user_ptr->isNoPasswordLogin()) {
        if (user_ptr->expiredStatus() == User::ExpiredAlready) {
            m_model->setAuthType(AT_PAM);
        }
        startGreeterAuth(user_ptr->name());
    } else {
        m_resetSessionTimer->stop();
        endAuthentication(m_account, AT_All);
        m_model->updateAuthStatus(AT_All, AS_Cancel, "Cancel");
        destoryAuthentication(m_account);
        createAuthentication(user_ptr->name());
    }
}

void GreeterWorker::checkDBusServer(bool isvalid)
{
    if (isvalid) {
        m_accountsInter->userList();
    } else {
        // FIXME: 我不希望这样做，但是QThread::msleep会导致无限递归
        QTimer::singleShot(300, this, [=] {
            qWarning() << "com.deepin.daemon.Accounts is not start, rechecking!";
            checkDBusServer(m_accountsInter->isValid());
        });
    }
}

/**
 * @brief 显示提示信息
 *
 * @param text
 * @param type
 */
void GreeterWorker::showPrompt(const QString &text, const QLightDM::Greeter::PromptType type)
{
    qDebug() << "GreeterWorker::showPrompt:" << text
             << ", type: " << type
             << ", is authenticated: " << m_greeter->isAuthenticated()
             << ", current user passwd expired status: " << m_model->currentUser()->expiredStatus();
    switch (type) {
    case QLightDM::Greeter::PromptTypeSecret:
        // 已经回应lightdm且当前用户的密码已过期，按照提示修改密码
        if (m_haveRespondedToLightdm && User::ExpiredAlready == m_model->currentUser()->expiredStatus()) {
            m_model->setCurrentModeState(SessionBaseModel::ResetPasswdMode);
            emit requestShowPrompt(text);
        } else if (!m_authFramework->isDeepinAuthValid()){
            handleAuthStatusChanged(AT_PAM, AS_Prompt, text);
        }
        break;
    case QLightDM::Greeter::PromptTypeQuestion:
        handleAuthStatusChanged(AT_PAM, AS_Prompt, text);
        break;
    }
}

/**
 * @brief 显示认证成功/失败的信息
 *
 * @param text
 * @param type
 */
void GreeterWorker::showMessage(const QString &text, const QLightDM::Greeter::MessageType type)
{
    qDebug() << Q_FUNC_INFO
             << ", message: " << text
             << ", type: " << type;
    switch (type) {
    case QLightDM::Greeter::MessageTypeInfo:
        m_model->updateAuthStatus(AT_PAM, AS_Success, text);
        break;
    case QLightDM::Greeter::MessageTypeError:
        // 验证完成且未验证通过的情况发送验证失败信息
        if (!m_greeter->isAuthenticated() && !m_greeter->inAuthentication())
            handleAuthStatusChanged(AT_PAM, AS_Failure, text);
        else
            emit requestShowMessage(text);

        break;
    }
}

/**
 * @brief 认证完成
 */
void GreeterWorker::authenticationComplete()
{
    const bool result = m_greeter->isAuthenticated();
    qInfo() << "Authentication result:" << result;

    if (!result) {
        showMessage(tr("Wrong Password"), QLightDM::Greeter::MessageTypeError);
        return;
    }

    emit m_model->authFinished(true);

    m_password.clear();

    switch (m_model->powerAction()) {
    case SessionBaseModel::PowerAction::RequireRestart:
        m_login1Inter->Reboot(true);
        return;
    case SessionBaseModel::PowerAction::RequireShutdown:
        m_login1Inter->PowerOff(true);
        return;
    default:
        break;
    }

    qInfo() << "start session = " << m_model->sessionKey();

    auto startSessionSync = [=]() {
        setCurrentUser(m_model->currentUser());
        m_greeter->startSessionSync(m_model->sessionKey());
    };

    emit requestUpdateBackground(m_model->currentUser()->greeterBackground());
#ifndef DISABLE_LOGIN_ANI
    QTimer::singleShot(1000, this, startSessionSync);
#else
    startSessionSync();
#endif
    endAuthentication(m_account, AT_All);
    destoryAuthentication(m_account);
}

void GreeterWorker::onAuthFinished()
{
    if (m_greeter->inAuthentication()) {
        m_haveRespondedToLightdm = true;
        m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
    } else {
        qWarning() << "The lightdm is not in authentication!";
    }
}

void GreeterWorker::handleAuthStatusChanged(const int type, const int status, const QString &message)
{
    qDebug() << Q_FUNC_INFO
             << ", type: " << type
             << ", status: " << status
             << ", message: " << message;

    if (m_model->getAuthProperty().MFAFlag) {
        if (type == AT_All) {
            switch (status) {
            case AS_Success:
                m_model->updateAuthStatus(type, status, message);
                m_resetSessionTimer->stop();
                if (m_greeter->inAuthentication()) {
                    m_haveRespondedToLightdm = true;
                    m_greeter->respond(m_authFramework->AuthSessionPath(m_account) + QString(";") + m_password);
                } else {
                    qWarning() << "The lightdm is not in authentication!";
                }
                break;
            case AS_Cancel:
                m_model->updateAuthStatus(type, status, message);
                destoryAuthentication(m_account);
                break;
            default:
                break;
            }
        } else {
            switch (status) {
            case AS_Success:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                m_resetSessionTimer->start();
                m_model->updateAuthStatus(type, status, message);
                break;
            case AS_Failure:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode) {
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                }
                m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
                endAuthentication(m_account, type);
                // 人脸和虹膜需要手动重启验证
                if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                    QTimer::singleShot(50, this, [=] {
                        startAuthentication(m_account, type);
                    });
                }
                QTimer::singleShot(50, this, [=] {
                    m_model->updateAuthStatus(type, status, message);
                });
                break;
            case AS_Locked:
                if (m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
                    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);
                endAuthentication(m_account, type);
                // TODO: 信号时序问题,考虑优化,Bug 89056
                QTimer::singleShot(50, this, [=] {
                    m_model->updateAuthStatus(type, status, message);
                });
                break;
            case AS_Timeout:
            case AS_Error:
                m_model->updateAuthStatus(type, status, message);
                endAuthentication(m_account, type);
                break;
            default:
                m_model->updateAuthStatus(type, status, message);
                break;
            }
        }
    } else {
        if (m_model->currentModeState() != SessionBaseModel::ModeStatus::PasswordMode
                && (status == AS_Success || status == AS_Failure)
                && m_model->currentModeState() != SessionBaseModel::ResetPasswdMode)
            m_model->setCurrentModeState(SessionBaseModel::ModeStatus::PasswordMode);

        m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
        m_model->updateAuthStatus(type, status, message);
        switch (status) {
        case AS_Failure:
            if (AT_All != type) {
                endAuthentication(m_account, type);
                // 人脸和虹膜需要手动重新开启验证
                if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                    QTimer::singleShot(50, this, [this, type] {
                        startAuthentication(m_account, type);
                    });
                }
            }
            break;
        case AS_Cancel:
            destoryAuthentication(m_account);
            break;
        default:
            break;
        }
    }
}

void GreeterWorker::onPasswdRespond(const QString &passwd)
{
    qDebug() << Q_FUNC_INFO;
    m_greeter->respond(passwd);
}

void GreeterWorker::saveNumlockStatus(std::shared_ptr<User> user, const bool &on)
{
    UserNumlockSettings(user->name()).set(on);
}

void GreeterWorker::recoveryUserKBState(std::shared_ptr<User> user)
{
    //FIXME(lxz)
    //    PowerInter powerInter("com.deepin.system.Power", "/com/deepin/system/Power", QDBusConnection::systemBus(), this);
    //    const BatteryPresentInfo info = powerInter.batteryIsPresent();
    //    const bool defaultValue = !info.values().first();
    if (user.get() == nullptr)
        return;

    const bool enabled = UserNumlockSettings(user->name()).get(false);

    qWarning() << "restore numlock status to " << enabled;

    // Resync numlock light with numlock status
    bool cur_numlock = KeyboardMonitor::instance()->isNumlockOn();
    KeyboardMonitor::instance()->setNumlockStatus(!cur_numlock);
    KeyboardMonitor::instance()->setNumlockStatus(cur_numlock);

    KeyboardMonitor::instance()->setNumlockStatus(enabled);
}

void GreeterWorker::restartResetSessionTimer()
{
    if (m_model->visible() && m_resetSessionTimer->isActive()) {
        m_resetSessionTimer->start();
    }
}

void GreeterWorker::startGreeterAuth(const QString &account)
{
    m_greeter->authenticate(account);
    m_haveRespondedToLightdm = false;
}
