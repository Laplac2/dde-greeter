#include "dbuslockagent.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/global_util/public_func.h"

#include <QGSettings>

DBusLockAgent::DBusLockAgent(QObject *parent) : QObject(parent)
{

}

void DBusLockAgent::setModel(SessionBaseModel *const model)
{
    m_model = model;
}

void DBusLockAgent::Show()
{
    InternalShow();
}

void DBusLockAgent::ShowAuth(bool active)
{
    Show();
    emit m_model->activeAuthChanged(!active);
}

// 待机，enable=true：进入待机；enable=false：待机恢复
void DBusLockAgent::Suspend(bool enable)
{
    if (enable) {
        m_model->setIsBlackModel(true);
        m_model->setIsShow(true);
    } else {
        QDBusInterface infc("com.deepin.daemon.Power","/com/deepin/daemon/Power","com.deepin.daemon.Power");
        // 待机恢复需要密码
        bool bSuspendLock = infc.property("SleepLock").toBool();

        if (bSuspendLock) {
            m_model->setIsBlackModel(false);
            m_model->setIsShow(true);
        } else {
            m_model->setIsShow(false);
            emit m_model->visibleChanged(false);
        }
    }
}

void DBusLockAgent::Hibernate(bool enable)
{
    m_model->setIsHibernateModel(enable);
    m_model->setIsShow(true);
}

void DBusLockAgent::InternalShow()
{
    m_model->setIsBlackModel(false);
    m_model->setIsHibernateModel(false);
    m_model->setIsShow(true);
}

void DBusLockAgent::ShowUserList()
{
    emit m_model->showUserList();
}


