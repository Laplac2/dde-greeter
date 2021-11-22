/*
* Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co.,Ltd.
*
* Author:     Zhang Qipeng <zhangqipeng@uniontech.com>
*
* Maintainer: Zhang Qipeng <zhangqipeng@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "auth_password.h"

#include "authcommon.h"
#include "dlineeditex.h"

#include <DHiDPIHelper>
#include <DLabel>

#include <QKeyEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QApplication>
#include <QDesktopWidget>
#include <QDBusReply>
#include <QWindow>

#include <unistd.h>

#include <com_deepin_daemon_accounts_user.h>

#define Service "com.deepin.dialogs.ResetPassword"
#define Path "/com/deepin/dialogs/ResetPassword"
#define Interface "com.deepin.dialogs.ResetPassword"

using namespace AuthCommon;

AuthPassword::AuthPassword(QWidget *parent)
    : AuthModule(parent)
    , m_capsLock(new DLabel(this))
    , m_lineEdit(new DLineEditEx(this))
    , m_keyboardBtn(new DPushButton(this))
    , m_passwordHintBtn(new DIconButton(this))
    , m_resetPasswordMessageVisible(false)
    , m_resetPasswordFloatingMessage(nullptr)
{
    setObjectName(QStringLiteral("AuthPassword"));
    setAccessibleName(QStringLiteral("AuthPassword"));

    m_type = AT_Password;

    initUI();
    initConnections();

    m_lineEdit->installEventFilter(this);
    setFocusProxy(m_lineEdit);
}

/**
 * @brief 初始化界面
 */
void AuthPassword::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_lineEdit->setClearButtonEnabled(false);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setContextMenuPolicy(Qt::NoContextMenu);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    m_lineEdit->lineEdit()->setAlignment(Qt::AlignCenter);
    setLineEditInfo(tr("Password"), PlaceHolderText);

    QHBoxLayout *passwordLayout = new QHBoxLayout(m_lineEdit->lineEdit());
    passwordLayout->setContentsMargins(0, 0, 10, 0);
    passwordLayout->setSpacing(5);
    /* 键盘布局按钮 */
    m_keyboardBtn->setContentsMargins(5, 5, 5, 5);
    m_keyboardBtn->setFocusPolicy(Qt::NoFocus);
    m_keyboardBtn->setCursor(Qt::ArrowCursor);
    m_keyboardBtn->setFlat(true);
    passwordLayout->addWidget(m_keyboardBtn, 0, Qt::AlignLeft | Qt::AlignVCenter);
    /* 缩放因子 */
    passwordLayout->addStretch(1);
    /* 大小写状态 */
    QPixmap pixmap = DHiDPIHelper::loadNxPixmap(CAPS_LOCK);
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_capsLock->setPixmap(pixmap);
    passwordLayout->addWidget(m_capsLock, 0, Qt::AlignRight | Qt::AlignVCenter);
    /* 认证状态 */
    m_authStatusLabel = new DLabel(this);
    setAuthStatusStyle(LOGIN_WAIT);
    passwordLayout->addWidget(m_authStatusLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
    /* 密码提示 */
    m_passwordHintBtn->setAccessibleName(QStringLiteral("PasswordHint"));
    m_passwordHintBtn->setContentsMargins(0, 0, 0, 0);
    m_passwordHintBtn->setFocusPolicy(Qt::NoFocus);
    m_passwordHintBtn->setCursor(Qt::ArrowCursor);
    m_passwordHintBtn->setFlat(true);
    m_passwordHintBtn->setIcon(QIcon(PASSWORD_HINT));
    m_passwordHintBtn->setIconSize(QSize(16, 16));
    passwordLayout->addWidget(m_passwordHintBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(m_lineEdit);
}

/**
 * @brief 初始化信号连接
 */
void AuthPassword::initConnections()
{
    AuthModule::initConnections();
    /* 键盘布局按钮 */
    connect(m_keyboardBtn, &QPushButton::clicked, this, &AuthPassword::requestShowKeyboardList);
    /* 密码提示 */
    connect(m_passwordHintBtn, &DIconButton::clicked, this, &AuthPassword::showPasswordHint);
    /* 密码输入框 */
    connect(m_lineEdit, &DLineEditEx::focusChanged, this, [this](const bool focus) {
        if (!focus) m_lineEdit->setAlert(false);
        m_authStatusLabel->setVisible(!focus & m_showAuthStatus);
        emit focusChanged(focus);
    });
    connect(m_lineEdit, &DLineEditEx::textChanged, this, [this](const QString &text) {
        m_lineEdit->setAlert(false);
        emit lineEditTextChanged(text);
    });
    connect(m_lineEdit, &DLineEditEx::returnPressed, this, [ this ] {
        if (!m_lineEdit->lineEdit()->isReadOnly()) // 避免用户在验证的时候反复点击
            emit requestAuthenticate();
    });
}

/**
 * @brief AuthPassword::reset
 */
void AuthPassword::reset()
{
    m_lineEdit->clear();
    m_lineEdit->setAlert(false);
    m_lineEdit->hideAlertMessage();
    setLineEditEnabled(true);
    setLineEditInfo(tr("Password"), PlaceHolderText);
}

/**
 * @brief 设置认证状态
 *
 * @param status
 * @param result
 */
void AuthPassword::setAuthStatus(const int state, const QString &result)
{
    qDebug() << "AuthPassword::setAuthResult:" << state << result << m_currentUid;
    m_status = state;
    switch (state) {
    case AS_Success:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_CHECK);
        m_lineEdit->setAlert(false);
        m_lineEdit->clear();
        setLineEditEnabled(false);
        setLineEditInfo(tr("Verification successful"), PlaceHolderText);
        m_showPrompt = true;
        m_lineEdit->hideAlertMessage();
        emit authFinished(state);
        emit requestChangeFocus();
        break;
    case AS_Failure: {
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        m_lineEdit->clear();
        setLineEditEnabled(true);
        const int leftTimes = static_cast<int>(m_limitsInfo->maxTries - m_limitsInfo->numFailures);
        if (leftTimes > 1) {
            setLineEditInfo(tr("Verification failed, %n chances left", "", leftTimes), PlaceHolderText);
        } else if (leftTimes == 1) {
            setLineEditInfo(tr("Verification failed, only one chance left"), PlaceHolderText);
        }
        setLineEditInfo(tr("Wrong Password"), AlertText);
        m_showPrompt = false;
        emit authFinished(state);
        break;
    }
    case AS_Cancel:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        m_showPrompt = true;
        break;
    case AS_Timeout:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Error:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Verify:
        setAnimationStatus(true);
        setAuthStatusStyle(LOGIN_SPINNER);
        break;
    case AS_Exception:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditInfo(result, AlertText);
        break;
    case AS_Prompt:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        if (m_showPrompt) {
            setLineEditInfo(tr("Password"), PlaceHolderText);
        }
        break;
    case AS_Started:
        break;
    case AS_Ended:
        m_lineEdit->clear();
        break;
    case AS_Locked:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_LOCK);
        setLineEditEnabled(false);
        m_lineEdit->setAlert(false);
        m_lineEdit->hideAlertMessage();
        if (m_integerMinutes == 1) {
            setLineEditInfo(tr("Please try again 1 minute later"), PlaceHolderText);
        } else {
            setLineEditInfo(tr("Please try again %n minutes later", "", static_cast<int>(m_integerMinutes)), PlaceHolderText);
        }
        if (QFile::exists(ResetPassword_Exe_Path) && m_currentUid <= 9999 && isUserAccountBinded()) {
            qDebug() << "begin reset passoword";
            setResetPasswordMessageVisible(true);
            updateResetPasswordUI();
        }
        m_showPrompt = false;
        m_passwordHintBtn->hide();
        break;
    case AS_Recover:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        break;
    case AS_Unlocked:
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        m_showPrompt = true;
        break;
    default:
        setAnimationStatus(false);
        setAuthStatusStyle(LOGIN_WAIT);
        setLineEditEnabled(true);
        setLineEditInfo(result, AlertText);
        qWarning() << "Error! The status of Password Auth is wrong!" << state << result;
        break;
    }
    update();
}

/**
 * @brief 设置认证动画状态
 *
 * @param start
 */
void AuthPassword::setAnimationStatus(const bool start)
{
    start ? m_lineEdit->startAnimation() : m_lineEdit->stopAnimation();
}

/**
 * @brief 设置大小写图标状态
 *
 * @param on
 */
void AuthPassword::setCapsLockVisible(const bool on)
{
    m_capsLock->setVisible(on);
}

/**
 * @brief 更新认证受限信息
 *
 * @param info
 */
void AuthPassword::setLimitsInfo(const LimitsInfo &info)
{
    qDebug() << "AuthPassword::setLimitsInfo" << info.numFailures;
    AuthModule::setLimitsInfo(info);
    m_passwordHintBtn->setVisible(info.numFailures > 0 && !m_passwordHint.isEmpty());
}

/**
 * @brief 设置输入框中的文案
 *
 * @param text
 * @param type
 */
void AuthPassword::setLineEditInfo(const QString &text, const TextType type)
{
    switch (type) {
    case AlertText:
        m_lineEdit->showAlertMessage(text, this, 3000);
        m_lineEdit->setAlert(true);
        break;
    case InputText: {
        const int cursorPos = m_lineEdit->lineEdit()->cursorPosition();
        m_lineEdit->setText(text);
        m_lineEdit->lineEdit()->setCursorPosition(cursorPos);
        break;
    }
    case PlaceHolderText:
        m_lineEdit->setPlaceholderText(text);
        break;
    }
}

/**
 * @brief 密码提示
 * @param hint
 */
void AuthPassword::setPasswordHint(const QString &hint)
{
    if (hint == m_passwordHint) {
        return;
    }
    m_passwordHint = hint;
}

void AuthPassword::setCurrentUid(uid_t uid)
{
    m_currentUid = uid;
}

/**
 * @brief 设置键盘布局按钮显示文案
 *
 * @param text
 */
void AuthPassword::setKeyboardButtonInfo(const QString &text)
{
    QString currentText = text.split(";").first();
    /* special mark in Japanese */
    if (currentText.contains("/")) {
        currentText = currentText.split("/").last();
    }
    m_keyboardBtn->setText(currentText);
}

/**
 * @brief 设置键盘布局按钮显示状态
 *
 * @param visible
 */
void AuthPassword::setKeyboardButtonVisible(const bool visible)
{
    m_keyboardBtn->setVisible(visible);
}

/**
 * @brief 获取输入框中的文字
 *
 * @return QString
 */
QString AuthPassword::lineEditText() const
{
    return m_lineEdit->text();
}

/**
 * @brief 设置 LineEdit 是否可输入
 *
 * @param enable
 */
void AuthPassword::setLineEditEnabled(const bool enable)
{
    if (enable && !m_limitsInfo->locked) {
        m_lineEdit->setFocusPolicy(Qt::StrongFocus);
        m_lineEdit->setFocus();
        m_lineEdit->lineEdit()->setReadOnly(false);
    } else {
        m_lineEdit->setFocusPolicy(Qt::NoFocus);
        m_lineEdit->clearFocus();
        m_lineEdit->lineEdit()->setReadOnly(true);
    }
}

/**
 * @brief 更新认证锁定时的文案
 */
void AuthPassword::updateUnlockPrompt()
{
    AuthModule::updateUnlockPrompt();
    if (m_integerMinutes == 1) {
        m_lineEdit->setPlaceholderText(tr("Please try again 1 minute later"));
    } else if (m_integerMinutes > 1) {
        m_lineEdit->setPlaceholderText(tr("Please try again %n minute(s) later", "", static_cast<int>(m_integerMinutes)));
    } else {
        QTimer::singleShot(1000, this, [this] {
            emit activeAuth(m_type);
        });
        qInfo() << "Waiting authentication service...";
    }
    update();
}

/**
 * @brief 显示密码提示
 */
void AuthPassword::showPasswordHint()
{
    m_lineEdit->showAlertMessage(m_passwordHint, this, 3000);
}

/**
 * @brief 设置密码提示按钮的可见性
 * @param visible
 */
void AuthPassword::setPasswordHintBtnVisible(const bool isVisible)
{
    m_passwordHintBtn->setVisible(isVisible);
}

bool AuthPassword::eventFilter(QObject *watched, QEvent *event)
{
    if (qobject_cast<DLineEditEx *>(watched) == m_lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->matches(QKeySequence::Cut)
            || keyEvent->matches(QKeySequence::Copy)
            || keyEvent->matches(QKeySequence::Paste)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 设置重置密码消息框的显示状态数据
 *
 * @param visible
 */
void AuthPassword::setResetPasswordMessageVisible(const bool isVisible)
{
    m_resetPasswordMessageVisible = isVisible;
}

/**
 * @brief 显示重置密码消息框
 */
void AuthPassword::showResetPasswordMessage()
{
    if (m_resetPasswordFloatingMessage && m_resetPasswordFloatingMessage->isVisible()) {
        return;
    }

    QWidget *userLoginWidget = parentWidget();
    if (!userLoginWidget) {
        return;
    }

    QWidget *centerFrame = userLoginWidget->parentWidget();
    if (!centerFrame) {
        return;
    }

    QPalette pa;
    pa.setColor(QPalette::Background, QColor(247, 247, 247, 51));
    m_resetPasswordFloatingMessage = new DFloatingMessage(DFloatingMessage::MessageType::ResidentType);
    m_resetPasswordFloatingMessage->setPalette(pa);
    m_resetPasswordFloatingMessage->setIcon(QIcon::fromTheme("dialog-warning"));
    DSuggestButton *suggestButton = new DSuggestButton(tr("Reset Password"));
    m_resetPasswordFloatingMessage->setWidget(suggestButton);
    m_resetPasswordFloatingMessage->setMessage(tr("Forgot password?"));
    connect(suggestButton, &QPushButton::clicked, this, [ this ]{
        if (window()->windowHandle() && window()->windowHandle()->setKeyboardGrabEnabled(false)) {
            qDebug() << "setKeyboardGrabEnabled(false) success！";
        }
        const QString AccountsService("com.deepin.daemon.Accounts");
        const QString path = QString("/com/deepin/daemon/Accounts/User%1").arg(m_currentUid);
        com::deepin::daemon::accounts::User user(AccountsService, path, QDBusConnection::systemBus());
        auto reply = user.SetPassword("");
        reply.waitForFinished();
        qWarning() << "reply setpassword:" << reply.error().message();

        emit m_resetPasswordFloatingMessage->closeButtonClicked();
    });
    connect(m_resetPasswordFloatingMessage, &DFloatingMessage::closeButtonClicked, this, [this](){
        if (m_resetPasswordFloatingMessage) {
            delete  m_resetPasswordFloatingMessage;
            m_resetPasswordFloatingMessage = nullptr;
        }
        emit resetPasswordMessageVisibleChanged(false);
    });
    DMessageManager::instance()->sendMessage(centerFrame, m_resetPasswordFloatingMessage);
    emit resetPasswordMessageVisibleChanged(true);
}

/**
 * @brief 关闭重置密码消息框
 */
void AuthPassword::closeResetPasswordMessage()
{
    if (m_resetPasswordFloatingMessage) {
        m_resetPasswordFloatingMessage->close();
        delete  m_resetPasswordFloatingMessage;
        m_resetPasswordFloatingMessage = nullptr;
        emit resetPasswordMessageVisibleChanged(false);
    }
}

/**
 * @brief 当前账户是否绑定unionid
 */
bool AuthPassword::isUserAccountBinded()
{
    QDBusInterface syncHelperInter("com.deepin.sync.Helper",
                                   "/com/deepin/sync/Helper",
                                   "com.deepin.sync.Helper",
                                   QDBusConnection::systemBus());
    QDBusReply<QString> retUOSID = syncHelperInter.call("UOSID");
    if (!syncHelperInter.isValid()) {
        return false;
    }
    QString uosid;
    if (retUOSID.error().message().isEmpty()) {
        uosid = retUOSID.value();
    } else {
        qWarning() << retUOSID.error().message();
        return false;
    }

    QDBusInterface accountsInter("com.deepin.daemon.Accounts",
                                 QString("/com/deepin/daemon/Accounts/User%1").arg(m_currentUid),
                                 "com.deepin.daemon.Accounts.User",
                                 QDBusConnection::systemBus());
    QVariant retUUID = accountsInter.property("UUID");
    if (!accountsInter.isValid()) {
        return false;
    }
    QString uuid = retUUID.toString();

    QDBusReply<QString> retLocalBindCheck= syncHelperInter.call("LocalBindCheck", uosid, uuid);
    if (!syncHelperInter.isValid()) {
        return false;
    }
    QString ubid;
    if (retLocalBindCheck.error().message().isEmpty()) {
        ubid = retLocalBindCheck.value();
    } else {
        qWarning() << "UOSID:" << uosid << "uuid:" << uuid;
        qWarning() << retLocalBindCheck.error().message();
        return false;
    }

    return !ubid.isEmpty();
}

/**
 * @brief 更新重置密码UI相关状态
 */
void AuthPassword::updateResetPasswordUI()
{
    // >=10000 域管账户, 该功能屏蔽域管账户
    if (m_currentUid > 9999) {
        return;
    }

    if (m_resetPasswordMessageVisible) {
        showResetPasswordMessage();
    } else {
        closeResetPasswordMessage();
    }
}

void AuthPassword::hide()
{
    m_lineEdit->hideAlertMessage();
    AuthModule::hide();
}
