
#include <sys/time.h>
#define TRACE_ME_IN struct timeval tp ; gettimeofday ( &tp , nullptr ); printf("[%4ld.%4ld] In: %s\n",tp.tv_sec , tp.tv_usec,__PRETTY_FUNCTION__);
#define TRACE_ME_OUT gettimeofday (const_cast<timeval *>(&tp) , nullptr ); printf("[%4ld.%4ld] Out: %s\n",tp.tv_sec , tp.tv_usec,__PRETTY_FUNCTION__);

/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#include "inhibitwarnview.h"
#include "src/session-widgets/framedatabind.h"

#include <QHBoxLayout>
#include <QPushButton>

const int ButtonIconSize = 28;
const int ButtonWidth = 200;
const int ButtonHeight = 64;

InhibitorRow::InhibitorRow(QString who, QString why, const QIcon &icon, QWidget *parent)
    : QWidget(parent)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *whoLabel = new QLabel(who);
    QLabel *whyLabel = new QLabel("-" + why);
    whoLabel->setStyleSheet("color: white; font: bold 12px;");
    whyLabel->setStyleSheet("color: white;");

    layout->addStretch();

    if (!icon.isNull()) {
        QLabel *iconLabel = new QLabel(this);
        QPixmap pixmap = icon.pixmap(topLevelWidget()->windowHandle(), QSize(48, 48));
        iconLabel->setPixmap(pixmap);
        iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        layout->addWidget(iconLabel);
    }

    layout->addWidget(whoLabel);
    layout->addWidget(whyLabel);
    layout->addStretch();
    this->setFixedHeight(ButtonHeight);
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    this->setLayout(layout);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

InhibitorRow::~InhibitorRow()
{

}

void InhibitorRow::paintEvent(QPaintEvent *event)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    QWidget::paintEvent(event);
    QPainter painter(this);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 25));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawRoundedRect(this->rect(), 18, 18);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

InhibitWarnView::InhibitWarnView(Actions inhibitType, QWidget *parent)
    : WarningView(parent)
    , m_inhibitType(inhibitType)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_acceptBtn = new QPushButton(QString());
    m_acceptBtn->setObjectName("AcceptButton");
    m_acceptBtn->setIconSize(QSize(ButtonIconSize, ButtonIconSize));
    m_acceptBtn->setFixedSize(ButtonWidth, ButtonHeight);
    m_acceptBtn->setCheckable(true);
    m_acceptBtn->setAutoExclusive(true);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName("CancelButton");
    m_cancelBtn->setIconSize(QSize(ButtonIconSize, ButtonIconSize));
    m_cancelBtn->setFixedSize(ButtonWidth, ButtonHeight);
    m_cancelBtn->setCheckable(true);
    m_cancelBtn->setAutoExclusive(true);

    const auto ratio = devicePixelRatioF();
    QIcon icon_pix = QIcon::fromTheme(":/img/cancel_normal.svg").pixmap(m_cancelBtn->iconSize() * ratio);
    m_cancelBtn->setIcon(icon_pix);

    m_confirmTextLabel = new QLabel;

    m_inhibitorListLayout = new QVBoxLayout;

    std::function<void (QVariant)> buttonChanged = std::bind(&InhibitWarnView::onOtherPageDataChanged, this, std::placeholders::_1);
    m_dataBindIndex = FrameDataBind::Instance()->registerFunction("InhibitWarnView", buttonChanged);

    m_confirmTextLabel->setText("The reason of inhibit.");
    m_confirmTextLabel->setAlignment(Qt::AlignCenter);
    m_confirmTextLabel->setStyleSheet("color:white;");

    QVBoxLayout *cancelLayout = new QVBoxLayout;
    cancelLayout->addWidget(m_cancelBtn);

    QVBoxLayout *acceptLayout = new QVBoxLayout;
    acceptLayout->addWidget(m_acceptBtn);

    QVBoxLayout *centralLayout = new QVBoxLayout;
    centralLayout->addStretch();
    centralLayout->addLayout(m_inhibitorListLayout);
    centralLayout->addSpacing(20);
    centralLayout->addWidget(m_confirmTextLabel);
    centralLayout->addSpacing(20);
    centralLayout->addWidget(m_cancelBtn, 0, Qt::AlignHCenter);
    centralLayout->addSpacing(20);
    centralLayout->addWidget(m_acceptBtn, 0, Qt::AlignHCenter);
    centralLayout->addStretch();

    setLayout(centralLayout);

    m_cancelBtn->setChecked(true);
    m_currentBtn = m_cancelBtn;

    connect(m_cancelBtn, &QPushButton::clicked, this, &InhibitWarnView::cancelled);
    connect(m_acceptBtn, &QPushButton::clicked, [this] {emit actionInvoked(m_action);});
    TRACE_ME_OUT;	//<<==--TracePoint!

}

InhibitWarnView::~InhibitWarnView()
{
    TRACE_ME_IN;	//<<==--TracePoint!
    FrameDataBind::Instance()->unRegisterFunction("InhibitWarnView", m_dataBindIndex);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::setInhibitorList(const QList<InhibitorData> &list)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    for (QWidget *widget : m_inhibitorPtrList) {
        m_inhibitorListLayout->removeWidget(widget);
        widget->deleteLater();
    };
    m_inhibitorPtrList.clear();

    for (const InhibitorData &inhibitor : list) {
        QIcon icon;

        if (inhibitor.icon.isEmpty() && inhibitor.pid) {
            QFileInfo executable_info(QFile::readLink(QString("/proc/%1/exe").arg(inhibitor.pid)));

            if (executable_info.exists()) {
                icon = QIcon::fromTheme(executable_info.fileName());
            }
        } else {
            icon = QIcon::fromTheme(inhibitor.icon, QIcon::fromTheme("application-x-desktop"));
        }

        if (icon.isNull()) {
            icon = QIcon::fromTheme("application-x-desktop");
        }

        QWidget *inhibitorWidget = new InhibitorRow(inhibitor.who, inhibitor.why, icon, this);

        m_inhibitorPtrList.append(inhibitorWidget);
        m_inhibitorListLayout->addWidget(inhibitorWidget, 0, Qt::AlignHCenter);
    }
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::setInhibitConfirmMessage(const QString &text)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_confirmTextLabel->setText(text);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::setAcceptReason(const QString &reason)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_acceptBtn->setText(reason);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::setAction(const Actions action)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_action = action;

    QString icon_string;
    switch (action) {
    case Actions::Shutdown:
        icon_string = ":/img/poweroff_warning_normal.svg";
        break;
    case Actions::Logout:
        icon_string = ":/img/logout_warning_normal.svg";
        break;
    default:
        icon_string = ":/img/reboot_warning_normal.svg";
        break;
    }

    const auto ratio = devicePixelRatioF();
    QIcon icon_pix = QIcon::fromTheme(icon_string).pixmap(m_acceptBtn->iconSize() * ratio);
    m_acceptBtn->setIcon(icon_pix);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::setAcceptVisible(const bool acceptable)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_acceptBtn->setVisible(acceptable);
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::toggleButtonState()
{
    TRACE_ME_IN;	//<<==--TracePoint!
    if (m_cancelBtn->isChecked() && m_acceptBtn->isVisible()) {
        m_cancelBtn->setChecked(false);
        m_acceptBtn->setChecked(true);
        m_currentBtn = m_acceptBtn;
    } else {
        m_acceptBtn->setChecked(false);
        m_cancelBtn->setChecked(true);
        m_currentBtn = m_cancelBtn;
    }

    FrameDataBind::Instance()->updateValue("InhibitWarnView", m_currentBtn->objectName());
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void InhibitWarnView::buttonClickHandle()
{
    TRACE_ME_IN;	//<<==--TracePoint!
    emit m_currentBtn->clicked();
    TRACE_ME_OUT;	//<<==--TracePoint!

}

Actions InhibitWarnView::inhibitType() const
{
    TRACE_ME_IN;	//<<==--TracePoint!
    TRACE_ME_OUT;	//<<==--TracePoint!
    return m_inhibitType;
}

void InhibitWarnView::onOtherPageDataChanged(const QVariant &value)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    const QString objectName { value.toString() };

    if (objectName == "AcceptButton") {
        m_cancelBtn->setChecked(false);
        m_acceptBtn->setChecked(true);
        m_currentBtn = m_acceptBtn;
    } else {
        m_acceptBtn->setChecked(false);
        m_cancelBtn->setChecked(true);
        m_currentBtn = m_cancelBtn;
    }
    TRACE_ME_OUT;	//<<==--TracePoint!

}
