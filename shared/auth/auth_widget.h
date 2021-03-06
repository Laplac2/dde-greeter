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

#ifndef AUTHWIDGET_H
#define AUTHWIDGET_H

#include "userinfo.h"

#include <DArrowRectangle>
#include <DBlurEffectWidget>
#include <DClipEffectWidget>
#include <DFloatingButton>
#include <DLabel>

#include <QWidget>

class AuthSingle;
class AuthIris;
class AuthFace;
class AuthUKey;
class AuthFingerprint;
class AuthPassword;
class DLineEditEx;
class FrameDataBind;
class KbLayoutWidget;
class KeyboardMonitor;
class SessionBaseModel;
class UserAvatar;

DWIDGET_USE_NAMESPACE

class AuthWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AuthWidget(QWidget *parent = nullptr);
    ~AuthWidget() override;

    virtual void setModel(const SessionBaseModel *model);
    virtual void setAuthType(const int type);
    virtual void setAuthStatus(const int type, const int status, const QString &message);
    virtual void reset();
    virtual int getTopSpacing() const ;

    void setAccountErrorMsg(const QString &message);

signals:
    void requestCheckAccount(const QString &account);
    void requestSetKeyboardType(const QString &key);
    void requestStartAuthentication(const QString &account, const int authType);
    void sendTokenToAuth(const QString &account, const int authType, const QString &token);
    void requestEndAuthentication(const QString &account, const int authType);
    void authFinished();

protected:
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

protected:
    void initUI();
    void initConnections();

    virtual void checkAuthResult(const int type, const int status);

    void setUser(std::shared_ptr<User> user);
    void setLimitsInfo(const QMap<int, User::LimitsInfo> *limitsInfo);
    void setAvatar(const QString &avatar);
    void setName(const QString &name);
    void setNameFont(const QFont &font);
    void setPasswordHint(const QString &hint);
    void setKeyboardType(const QString &type);
    void setKeyboardTypeList(const QStringList &list);
    void setLockButtonType(const int type);

    void updateBlurEffectGeometry();
    void updateKeyboardTypeListGeometry();
    void updateExpiredStatus();

    void showKeyboardList();

    void registerSyncFunctions(const QString &flag, std::function<void(QVariant)> function);
    void syncSingle(const QVariant &value);
    void syncSingleResetPasswordVisibleChanged(const QVariant &value);
    void syncAccount(const QVariant &value);
    void syncPassword(const QVariant &value);
    void syncPasswordResetPasswordVisibleChanged(const QVariant &value);
    void syncUKey(const QVariant &value);

protected:
    const SessionBaseModel *m_model;
    FrameDataBind *m_frameDataBind;

    DBlurEffectWidget *m_blurEffectWidget; // ????????????
    DFloatingButton *m_lockButton;         // ????????????
    UserAvatar *m_userAvatar;              // ????????????

    DLabel *m_expiredStatusLabel; // ??????????????????
    QSpacerItem *m_expiredSpacerItem;      // ????????????????????????????????????
    DLabel *m_nameLabel;          // ?????????
    DLineEditEx *m_accountEdit;   // ??????????????????

    KeyboardMonitor *m_capslockMonitor;    // ?????????
    KbLayoutWidget *m_keyboardTypeWidget;  // ??????????????????
    DArrowRectangle *m_keyboardTypeBorder; // ??????????????????????????????
    DClipEffectWidget *m_keyboardTypeClip; // ???????????????????????????????????????

    AuthSingle *m_singleAuth;           // PAM
    AuthPassword *m_passwordAuth;       // ??????
    AuthFingerprint *m_fingerprintAuth; // ??????
    AuthUKey *m_ukeyAuth;               // UKey
    AuthFace *m_faceAuth;               // ??????
    AuthIris *m_irisAuth;               // ??????

    QString m_passwordHint;     // ????????????
    QString m_keyboardType;     // ??????????????????
    QStringList m_keyboardList; // ??????????????????

    std::shared_ptr<User> m_user; // ????????????

    QList<QMetaObject::Connection> m_connectionList;
    QMap<QString, int> m_registerFunctions;
};

#endif // AUTHWIDGET_H
