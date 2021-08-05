#ifndef AUTHINTERFACE_H
#define AUTHINTERFACE_H

#include "src/global_util/public_func.h"
#include "src/global_util/constants.h"
#include "src/global_util/dbus/dbuslogin1manager.h"

#include <com_deepin_daemon_accounts.h>
#include <com_deepin_daemon_logined.h>

#include <QJsonArray>
#include <QObject>
#include <QGSettings>
#include <memory>

using AccountsInter = com::deepin::daemon::Accounts;
using LoginedInter = com::deepin::daemon::Logined;

class User;
class SessionBaseModel;

namespace Auth {
class AuthInterface : public QObject {
    Q_OBJECT
public:
    explicit AuthInterface(SessionBaseModel *const model, QObject *parent = nullptr);

    virtual void authUser(const QString &password)        = 0;

    virtual void switchToUser(std::shared_ptr<User> user);
    virtual void setLayout(std::shared_ptr<User> user, const QString &layout);
    virtual void onUserListChanged(const QStringList &list);
    virtual void onUserAdded(const QString &user);
    virtual void onUserRemove(const QString &user);

    enum SwitchUser {
        Always = 0,
        Ondemand,
        Disabled
    };

protected:
    void initDBus();
    void initData();
    void onLastLogoutUserChanged(uint uid);
    void onLoginUserListChanged(const QString &list);

    bool checkHaveDisplay(const QJsonArray &array);
    bool isLogined(uint uid);
    void checkConfig();
    void checkPowerInfo();
    void checkVirtualKB();
    void checkSwap();
    bool isDeepin();
    QString getFileContent(QString path = "/proc/cmdline ");
    bool gsettingsExist(const QString& key);
    QVariant getGSettings(const QString& key);

    template <typename T>
    T valueByQSettings(const QString & group,
                       const QString & key,
                       const QVariant &failback) {
        return findValueByQSettings<T>(DDESESSIONCC::session_ui_configs,
                                       group,
                                       key,
                                       failback);
    }

    //判断是否加入AD域
    bool checkIsADDomain();

protected:
    SessionBaseModel*  m_model;
    AccountsInter *    m_accountsInter;
    LoginedInter*      m_loginedInter;
    DBusLogin1Manager* m_login1Inter;
    QGSettings*        m_gsettings = nullptr;
    uint               m_lastLogoutUid;
    uint               m_currentUserUid;
    std::list<uint>    m_loginUserList;
};
}  // namespace Auth

#endif  // AUTHINTERFACE_H
