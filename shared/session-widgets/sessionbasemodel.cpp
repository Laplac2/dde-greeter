#include "sessionbasemodel.h"

#include <DSysInfo>

#include <QDebug>
#include <QGSettings>

#define SessionManagerService "com.deepin.SessionManager"
#define SessionManagerPath "/com/deepin/SessionManager"

DCORE_USE_NAMESPACE

SessionBaseModel::SessionBaseModel(AuthType type, QObject *parent)
    : QObject(parent)
    , m_hasSwap(false)
    , m_visible(false)
    , m_isServerModel(false)
    , m_canSleep(false)
    , m_isLockNoPassword(false)
    , m_currentType(type)
    , m_currentUser(nullptr)
    , m_powerAction(PowerAction::RequireNormal)
    , m_currentModeState(ModeStatus::NoStatus)
    , m_users(new QMap<QString, std::shared_ptr<User>>())
    , m_loginedUsers(new QMap<QString, std::shared_ptr<User>>())
{
}

SessionBaseModel::~SessionBaseModel()
{
    delete m_users;
    delete m_loginedUsers;
}

std::shared_ptr<User> SessionBaseModel::findUserByUid(const uint uid) const
{
    for (auto user : qAsConst(*m_users)) {
        if (user->uid() == uid) {
            return user;
        }
    }
    return std::shared_ptr<User>(nullptr);
}

std::shared_ptr<User> SessionBaseModel::findUserByName(const QString &name) const
{
    if (name.isEmpty()) {
        return std::shared_ptr<User>(nullptr);
    }

    for (auto user : qAsConst(*m_users)) {
        if (user->name() == name) {
            return user;
        }
    }
    return std::shared_ptr<User>(nullptr);
}

void SessionBaseModel::setAppType(const AppType type)
{
    m_appType = type;
}

void SessionBaseModel::setSessionKey(const QString &sessionKey)
{
    qDebug() << "SessionBaseModel::setSessionKey:" << sessionKey;
    if (m_sessionKey == sessionKey) return;

    m_sessionKey = sessionKey;

    emit onSessionKeyChanged(sessionKey);
}

void SessionBaseModel::setPowerAction(const PowerAction &powerAction)
{
    qDebug() << "SessionBaseModel::setPowerAction:" << m_powerAction << powerAction;
    if (powerAction == m_powerAction) return;

    m_powerAction = powerAction;

    emit onPowerActionChanged(powerAction);
}

void SessionBaseModel::setCurrentModeState(const ModeStatus &currentModeState)
{
    qDebug() << "SessionBaseModel::setCurrentModeState:" << m_currentModeState << currentModeState;
    if (m_currentModeState == currentModeState) return;

    m_currentModeState = currentModeState;

    emit onStatusChanged(currentModeState);
}

void SessionBaseModel::setUserListSize(int users_size)
{
    if (m_userListSize == users_size) return;

    m_userListSize = users_size;

    emit onUserListSizeChanged(users_size);
}

void SessionBaseModel::setHasVirtualKB(bool hasVirtualKB)
{
    //?????????????????????????????????????????????onboard??????????????????????????????onboard??????
    if (hasVirtualKB) {
        bool b = QProcess::execute("which", QStringList() << "onboard") == 0;
        emit hasVirtualKBChanged(b);
    } else {
        emit hasVirtualKBChanged(false);
    }
}

void SessionBaseModel::setHasSwap(bool hasSwap)
{
    if (m_hasSwap == hasSwap) return;

    m_hasSwap = hasSwap;

    emit onHasSwapChanged(hasSwap);
}

/**
 * @brief ????????????????????????
 *
 * @param visible
 */
void SessionBaseModel::setVisible(const bool visible)
{
    qDebug() << "SessionBaseModel::setVisible:" << visible;
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;

    //????????????????????????????????????????????????????????????
    setHasVirtualKB(m_visible);

    emit visibleChanged(m_visible);
}

void SessionBaseModel::setCanSleep(bool canSleep)
{
    if (m_canSleep == canSleep) return;

    m_canSleep = canSleep;

    emit canSleepChanged(canSleep);
}

void SessionBaseModel::setAllowShowUserSwitchButton(bool allowShowUserSwitchButton)
{
    if (m_allowShowUserSwitchButton == allowShowUserSwitchButton) return;

    m_allowShowUserSwitchButton = allowShowUserSwitchButton;

    emit allowShowUserSwitchButtonChanged(allowShowUserSwitchButton);
}

void SessionBaseModel::setAlwaysShowUserSwitchButton(bool alwaysShowUserSwitchButton)
{
    m_alwaysShowUserSwitchButton = alwaysShowUserSwitchButton;
}

void SessionBaseModel::setIsServerModel(bool server_model)
{
    if (m_isServerModel == server_model) return;

    m_isServerModel = server_model;
}

void SessionBaseModel::setAbortConfirm(bool abortConfirm)
{
    m_abortConfirm = abortConfirm;
    emit abortConfirmChanged(abortConfirm);
}

void SessionBaseModel::setIsLockNoPassword(bool LockNoPassword)
{
    if (m_isLockNoPassword == LockNoPassword) return;

    m_isLockNoPassword = LockNoPassword;
}

void SessionBaseModel::setIsBlackModel(bool is_black)
{
    if (m_isBlackMode == is_black) return;

    m_isBlackMode = is_black;
    emit blackModeChanged(is_black);
}

void SessionBaseModel::setIsHibernateModel(bool is_Hibernate)
{
    if (m_isHibernateMode == is_Hibernate) return;
    m_isHibernateMode = is_Hibernate;
    emit HibernateModeChanged(is_Hibernate);
}

void SessionBaseModel::setIsCheckedInhibit(bool checked)
{
    if (m_isCheckedInhibit == checked) return;
    m_isCheckedInhibit = checked;
}

/**
 * @brief ?????????????????????
 *
 * @param allowShowCustomUser
 */
void SessionBaseModel::setAllowShowCustomUser(const bool allowShowCustomUser)
{
    if (allowShowCustomUser == m_allowShowCustomUser) {
        return;
    }
    m_allowShowCustomUser = allowShowCustomUser;
}

/**
 * @brief ????????????
 *
 * @param type
 */
void SessionBaseModel::setAuthType(const int type)
{
    qDebug() << Q_FUNC_INFO << type;
    if (type == m_authProperty.AuthType) {
        return;
    }
    if (m_currentUser->type() == User::Default) {
        m_authProperty.AuthType = type;
        emit authTypeChanged(AT_None);
    } else {
        m_authProperty.AuthType = type;
        emit authTypeChanged(type);
    }
}

/**
 * @brief ????????????
 *
 * @param path ??????????????? Uid
 */
void SessionBaseModel::addUser(const QString &path)
{
    qDebug() << "SessionBaseModel::addUser:" << path;
    if (m_users->contains(path)) {
        return;
    }
    std::shared_ptr<NativeUser> user(new NativeUser(path));
    m_users->insert(path, user);
    emit userAdded(user);
    emit userListChanged(m_users->values());
}

/**
 * @brief ????????????
 *
 * @param user
 */
void SessionBaseModel::addUser(const std::shared_ptr<User> user)
{
    qDebug() << "SessionBaseModel::addUser:" << user->path() << user->name();
    const QList<std::shared_ptr<User>> userList = m_users->values();
    if (userList.contains(user)) {
        return;
    }
    const QString path = user->path().isEmpty() ? QString::number(user->uid()) : user->path();
    m_users->insert(path, user);
    emit userAdded(user);
}

/**
 * @brief ????????????
 *
 * @param path ??????????????? Uid
 */
void SessionBaseModel::removeUser(const QString &path)
{
    if (!m_users->contains(path)) {
        return;
    }
    const std::shared_ptr<User> user = m_users->value(path);
    m_users->remove(path);
    emit userRemoved(user);
    emit userListChanged(m_users->values());
}

/**
 * @brief ????????????
 *
 * @param user
 */
void SessionBaseModel::removeUser(const std::shared_ptr<User> user)
{
    const QList<std::shared_ptr<User>> userList = m_users->values();
    if (!userList.contains(user)) {
        return;
    }
    qDebug() << "SessionBaseModel::removeUser:" << user->name() << user->uid();
    const QString path = user->path().isEmpty() ? QString::number(user->uid()) : user->path();
    m_users->remove(path);
    emit userRemoved(user);
}

/**
 * @brief ????????????????????????
 *
 * @param userJson
 */
void SessionBaseModel::updateCurrentUser(const QString &userJson)
{
    qDebug() << "SessionBaseModel::updateCurrentUser:" << userJson;
    std::shared_ptr<User> user_ptr;
    QJsonParseError jsonParseError;
    const QJsonDocument userDoc = QJsonDocument::fromJson(userJson.toUtf8(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError || userDoc.isEmpty()) {
        qWarning() << "Failed to obtain current user information from LockService!";
        user_ptr = m_lastLogoutUser ? m_lastLogoutUser : m_users->first();
    } else {
        const QJsonObject userObj = userDoc.object();
        const QString &name = userObj["Name"].toString();
        user_ptr = findUserByName(name);
        if (name.isEmpty() || user_ptr == nullptr) {
            const uid_t uid = static_cast<uid_t>(userObj["Uid"].toInt());
            user_ptr = findUserByUid(uid);
        }
        if (user_ptr == nullptr) {
            qWarning() << "Failed to find user!";
            user_ptr = m_lastLogoutUser ? m_lastLogoutUser : m_users->first();
        }
    }
    updateCurrentUser(user_ptr);
}

/**
 * @brief ????????????????????????
 *
 * @param user
 */
void SessionBaseModel::updateCurrentUser(const std::shared_ptr<User> user)
{
    if (user == nullptr) {
        qWarning() << "Failed to set current user!" << user.get();
        return;
    }
    qDebug() << "SessionBaseModel::updateCurrentUser:" << user->name();
    if (m_currentUser == user) {
        return;
    }
    m_currentUser = user;
    emit currentUserChanged(user);
}

/**
 * @brief ???????????????????????????????????????????????????????????????????????????????????????????????????
 *
 * @param list
 */
void SessionBaseModel::updateUserList(const QStringList &list)
{
    qDebug() << "SessionBaseModel::updateUserList:" << list;
    QStringList listTmp = m_users->keys();
    for (const QString &path : list) {
        if (m_users->contains(path)) {
            listTmp.removeAll(path);
        } else {
            std::shared_ptr<NativeUser> user(new NativeUser(path));
            m_users->insert(path, user);
        }
    }
    for (const QString &path : qAsConst(listTmp)) {
        m_users->remove(path);
    }
    emit userListChanged(m_users->values());
}

/**
 * @brief ???????????????????????????
 *
 * @param uid
 */
void SessionBaseModel::updateLastLogoutUser(const uid_t uid)
{
    qDebug() << "SessionBaseModel::updateLastLogoutUser:" << uid;
    for (const std::shared_ptr<User> &user : m_users->values()) {
        if (uid == user->uid()) {
            updateLastLogoutUser(user);
            break;
        }
    }
}

/**
 * @brief ??????????????????????????????
 *
 * @param lastLogoutUser
 */
void SessionBaseModel::updateLastLogoutUser(const std::shared_ptr<User> lastLogoutUser)
{
    qDebug() << "SessionBaseModel::updateLastLogoutUser:" << lastLogoutUser->name() << lastLogoutUser->uid();
    if (m_lastLogoutUser && m_lastLogoutUser == lastLogoutUser) {
        return;
    }
    m_lastLogoutUser = lastLogoutUser;
}

/**
 * @brief ???????????????????????????
 *
 * @param list
 */
void SessionBaseModel::updateLoginedUserList(const QString &list)
{
    qDebug() << "SessionBaseModel::updateLoginedUserList:" << list;
    QList<QString> loginedUsersTmp = m_loginedUsers->keys();
    QJsonParseError jsonParseError;
    const QJsonDocument loginedUserListDoc = QJsonDocument::fromJson(list.toUtf8(), &jsonParseError);
    if (jsonParseError.error != QJsonParseError::NoError) {
        qWarning() << "The logined user list is wrong!";
        return;
    }
    const QJsonObject loginedUserListDocObj = loginedUserListDoc.object();
    for (const QString &loginedUserListDocKey : loginedUserListDocObj.keys()) {
        const QJsonArray loginedUserListArr = loginedUserListDocObj.value(loginedUserListDocKey).toArray();
        for (const QJsonValue &loginedUserStr : loginedUserListArr) {
            const QJsonObject loginedUserListObj = loginedUserStr.toObject();
            const int uid = loginedUserListObj["Uid"].toInt();
            const QString &desktop = loginedUserListObj["Desktop"].toString();
            if ((uid != 0 && uid < 1000) || desktop.isEmpty()) {
                continue; // ???????????????????????????
            }
            const QString path = QString("/com/deepin/daemon/Accounts/User") + QString::number(uid);
            if (!m_loginedUsers->contains(path)) {
                // ??????????????????????????????????????????(?????????), ?????????????????????????????????????????????m_users->value(path)??????????????????????????????????????????
                // ???????????????????????????????????????????????????addUser????????????????????????????????????????????????????????????????????????
                addUser(path);
                m_loginedUsers->insert(path, m_users->value(path));
                m_users->value(path)->updateLoginStatus(true);
            } else {
                loginedUsersTmp.removeAll(path);
            }
        }
    }
    for (const QString &path : qAsConst(loginedUsersTmp)) {
        m_loginedUsers->remove(path);
        m_users->value(path)->updateLoginStatus(false);
    }
    qInfo() << "Logined users:" << m_loginedUsers->keys();
    emit loginedUserListChanged(m_loginedUsers->values());
}

/**
 * @brief ????????????????????????
 *
 * @param info
 */
void SessionBaseModel::updateLimitedInfo(const QString &info)
{
    qDebug() << "SessionBaseModel::updateLimitedInfo" << info;
    m_currentUser->updateLimitsInfo(info);
}

/**
 * @brief ??????????????????
 *
 * @param state
 */
void SessionBaseModel::updateFrameworkState(const int state)
{
    qDebug() << "SessionBaseModel::updateFrameworkState:" << state;
    if (state == m_authProperty.FrameworkState) {
        return;
    }
    m_authProperty.FrameworkState = state;
}

/**
 * @brief ?????????????????????
 *
 * @param flags
 */
void SessionBaseModel::updateSupportedMixAuthFlags(const int flags)
{
    if (flags == m_authProperty.MixAuthFlags) {
        return;
    }
    m_authProperty.MixAuthFlags = flags;
}

/**
 * @brief ?????????????????????
 *
 * @param type
 */
void SessionBaseModel::updateSupportedEncryptionType(const QString &type)
{
    if (type == m_authProperty.EncryptionType) {
        return;
    }
    m_authProperty.EncryptionType = type;
}

/**
 * @brief ??????????????????????????? PAM ?????????????????????
 *
 * @param fuzzMFA
 */
void SessionBaseModel::updateFuzzyMFA(const bool fuzzMFA)
{
    if (fuzzMFA == m_authProperty.FuzzyMFA) {
        return;
    }
    m_authProperty.FuzzyMFA = fuzzMFA;
}

/**
 * @brief ???????????????
 *
 * @param MFAFlag true: ???????????????  false: ???????????????
 */
void SessionBaseModel::updateMFAFlag(const bool MFAFlag)
{
    qDebug() << "SessionBaseModel::updateMFAFlag:" << m_authProperty.MFAFlag << MFAFlag;
    if (MFAFlag == m_authProperty.MFAFlag) {
        return;
    }
    m_authProperty.MFAFlag = MFAFlag;
    emit MFAFlagChanged(MFAFlag);
}

/**
 * @brief PIN ????????????
 * @param length
 */
void SessionBaseModel::updatePINLen(const int PINLen)
{
    if (PINLen == m_authProperty.PINLen) {
        return;
    }
    m_authProperty.PINLen = PINLen;
}

/**
 * @brief ???????????????
 *
 * @param prompt ?????????
 */
void SessionBaseModel::updatePrompt(const QString &prompt)
{
    if (prompt == m_authProperty.Prompt) {
        return;
    }
    m_authProperty.Prompt = prompt;
}

/**
 * @brief ?????????????????????
 *
 * @param info
 */
void SessionBaseModel::updateFactorsInfo(const MFAInfoList &infoList)
{
    qDebug() << Q_FUNC_INFO << infoList;
    m_authProperty.AuthType = AT_None;
    switch (m_authProperty.FrameworkState) {
    case Available:
        for (const MFAInfo &info : infoList) {
            m_authProperty.AuthType |= info.AuthType;
        }
        emit authTypeChanged(m_authProperty.AuthType);
        break;
    default:
        m_authProperty.AuthType = AT_PAM;
        emit authTypeChanged(AT_PAM);
        break;
    }
}

/**
 * @brief ??????????????????
 *
 * @param currentAuthType
 * @param status
 * @param result
 */
void SessionBaseModel::updateAuthStatus(const int type, const int status, const QString &result)
{
    qInfo() << "Authentication Service status:" << type << status << result;
    switch (m_authProperty.FrameworkState) {
    case Available:
        emit authStatusChanged(type, status, result);
        break;
    default:
        emit authStatusChanged(AT_PAM, status, result);
        break;
    }
}
