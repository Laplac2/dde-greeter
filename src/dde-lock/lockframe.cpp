/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
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

#include "lockframe.h"
#include "src/session-widgets/lockcontent.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/userinfo.h"

#include <QApplication>
#include <QWindow>

LockFrame::LockFrame(SessionBaseModel *const model, QWidget *parent)
    : FullscreenBackground(parent)
    , m_model(model)
{
    qDebug() << "LockFrame geometry:" << geometry();

    QTimer::singleShot(0, this, [ = ] {
        auto user = model->currentUser();
        if (user != nullptr) updateBackground(user->greeterBackgroundPath());
    });

    m_content = new LockContent(model);
    setContent(m_content);
    m_content->hide();
    connect(m_content, &LockContent::requestSwitchToUser, this, &LockFrame::requestSwitchToUser);
    connect(m_content, &LockContent::requestAuthUser, this, &LockFrame::requestAuthUser);
    connect(m_content, &LockContent::requestSetLayout, this, &LockFrame::requestSetLayout);
    connect(m_content, &LockContent::requestBackground, this, static_cast<void (LockFrame::*)(const QString &)>(&LockFrame::updateBackground));
    connect(model, &SessionBaseModel::showUserList, this, &LockFrame::showUserList);
    connect(m_content, &LockContent::unlockActionFinish,this, [ = ]() {
        Q_EMIT requestEnableHotzone(true);
        hide();
    });
    connect(model, &SessionBaseModel::authFinished, this, [ = ](bool success){
        m_content->beforeUnlockAction(success);
    });
}

void LockFrame::showUserList()
{
    show();
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
}

void LockFrame::visibleChangedFrame(bool isVisible)
{
    QDBusInterface *inter = nullptr;
    QDBusInterface *inter1 = nullptr;
    if (qEnvironmentVariable("XDG_SESSION_TYPE").toLower().contains("wayland")) {
        inter = new QDBusInterface("org.kde.KWin", "/kglobalaccel", "org.kde.KGlobalAccel",
                                                  QDBusConnection::sessionBus(), this);
        inter1 = new QDBusInterface("org.kde.KWin", "/KWin", "org.kde.KWin",
                                                  QDBusConnection::sessionBus(), this);
    }

    if (inter) {
        auto req = inter->call("blockGlobalShortcuts", isVisible);
        auto req1 = inter1->call("disableHotKeysForClient", isVisible);
    }
    QDBusInterface launcherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", "com.deepin.dde.Launcher"
                                 , QDBusConnection::sessionBus());
    launcherInter.call("Hide");
    if (isVisible) {
        updateMonitorGeometry();
        show();
    } else {
        setVisible(isVisible);
    }
}

void LockFrame::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:    qApp->quit();       break;
#endif
    }
}

void LockFrame::showEvent(QShowEvent *event)
{
    emit requestEnableHotzone(false);

    m_model->setIsShow(true);

    return FullscreenBackground::showEvent(event);
}

void LockFrame::hideEvent(QHideEvent *event)
{
    emit requestEnableHotzone(true);

    m_model->setIsShow(false);

    return FullscreenBackground::hideEvent(event);
}

LockFrame::~LockFrame()
{
    //+ 防止插拔HDMI显示屏出现崩溃问题，需要析构时调用对应delete释放资源；
    delete m_content;
}
