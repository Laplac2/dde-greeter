
#include <sys/time.h>
#define TRACE_ME_IN struct timeval tp ; gettimeofday ( &tp , nullptr ); printf("[%4ld.%4ld] In: %s\n",tp.tv_sec , tp.tv_usec,__PRETTY_FUNCTION__);
#define TRACE_ME_OUT gettimeofday (const_cast<timeval *>(&tp) , nullptr ); printf("[%4ld.%4ld] Out: %s\n",tp.tv_sec , tp.tv_usec,__PRETTY_FUNCTION__);

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

#include "useravatar.h"
#include "dthememanager.h"
#include <QUrl>
#include <QFile>
#include <QPainterPath>

UserAvatar::UserAvatar(QWidget *parent, bool deleteable) :
    QPushButton(parent), m_deleteable(deleteable)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    setCheckable(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    mainLayout->setAlignment(Qt::AlignCenter);

    m_iconLabel = new QLabel;
    m_iconLabel->setObjectName("UserAvatar");
    m_iconLabel->setAlignment(Qt::AlignCenter);

    mainLayout->addWidget(m_iconLabel);
    setLayout(mainLayout);

    initDeleteButton();
    m_borderColor = QColor(255, 255, 255, 255);
    setStyleSheet("background-color: rgba(255, 255, 255, 0);\
                                    color: #b4b4b4;\
                                    border: none;");
                                    TRACE_ME_OUT;	//<<==--TracePoint!

}

void UserAvatar::setIcon(const QString &iconPath, const QSize &size)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    QUrl url(iconPath);

    if (url.isLocalFile())
        m_iconPath = url.path();
    else
        m_iconPath = iconPath;

    if (size.isEmpty())
        m_iconLabel->setFixedSize(NORMAL_ICON_SIZE, NORMAL_ICON_SIZE);
    else
        m_iconLabel->setFixedSize(size);

    update();
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void UserAvatar::paintEvent(QPaintEvent *)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    int iconSize = NORMAL_ICON_SIZE;
    switch (m_avatarSize){
    case AvatarSmallSize:
        iconSize = SMALL_ICON_SIZE;
        break;
    case AvatarLargeSize:
        iconSize = LARGE_ICON_SIZE;
        break;
    default:
        break;
    }

    QPainter painter(this);
    QRect roundedRect((width() -iconSize) / 2.0, (height() - iconSize) / 2.0, iconSize, iconSize);
    QPainterPath path;
    path.addRoundedRect(roundedRect, AVATAR_ROUND_RADIUS, AVATAR_ROUND_RADIUS);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setClipPath(path);

    const auto ratio = devicePixelRatioF();
    QString imgPath = m_iconPath;
    if (ratio > 1.0)
        imgPath.replace("icons/", "icons/bigger/");
    if (!QFile(imgPath).exists())
        imgPath = m_iconPath;

    QImage tmpImg(imgPath);

    painter.drawImage(roundedRect, this->isEnabled() ? tmpImg : imageToGray(tmpImg));

    QColor penColor = m_selected ? m_borderSelectedColor : m_borderColor;

    if (m_borderWidth) {
        QPen pen;
        pen.setColor(penColor);
        pen.setWidth(m_borderWidth);
        painter.setPen(pen);
        painter.drawPath(path);
        painter.end();
    }
    TRACE_ME_OUT;	//<<==--TracePoint!

}

QImage UserAvatar::imageToGray(const QImage &image)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    int height = image.height();
    int width = image.width();
    QImage targetImg(width, height, QImage::Format_Indexed8);
    targetImg.setColorCount(256);
    for(int i = 0; i < 256; i++)
        targetImg.setColor(i, qRgb(i, i, i));

    switch(image.format())
    {
    case QImage::Format_Indexed8:
        for(int i = 0; i < height; i ++)
        {
            const uchar *pSrc = (uchar *)image.constScanLine(i);
            uchar *pDest = (uchar *)targetImg.scanLine(i);
            memcpy(pDest, pSrc, width);
        }
        break;
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        for(int i = 0; i < height; i ++)
        {
            const QRgb *pSrc = (QRgb *)image.constScanLine(i);
            uchar *pDest = (uchar *)targetImg.scanLine(i);

            for( int j = 0; j < width; j ++)
            {
                 pDest[j] = qGray(pSrc[j]);
            }
        }
        break;
    default:
        break;
    }
    TRACE_ME_OUT;	//<<==--TracePoint!
    return targetImg;
}

void UserAvatar::initDeleteButton()
{
//    m_deleteButton = new AvatarDeleteButton(this);
//    m_deleteButton->hide();
//    m_deleteButton->raise();
//    m_deleteButton->setFixedSize(24, 24); //image's size
//    m_deleteButton->move(width()  - m_deleteButton->width(), 0);
//    connect(m_deleteButton, &AvatarDeleteButton::clicked, this, &UserAvatar::requestDelete);
}
bool UserAvatar::deleteable() const
{
    TRACE_ME_IN;	//<<==--TracePoint!
    TRACE_ME_OUT;	//<<==--TracePoint!
    return m_deleteable;
}

void UserAvatar::setDeleteable(bool deleteable)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_deleteable = deleteable;
    TRACE_ME_OUT;	//<<==--TracePoint!

}


void UserAvatar::setAvatarSize(const AvatarSize &avatarSize)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    int tmpSize = NORMAL_ICON_SIZE;
    switch (avatarSize){
    case AvatarSmallSize:
        tmpSize = SMALL_ICON_SIZE;
        break;
    case AvatarLargeSize:
        tmpSize = LARGE_ICON_SIZE;
        break;
    default:
        break;
    }
    m_iconLabel->setFixedSize(tmpSize, tmpSize);

    m_avatarSize = avatarSize;
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void UserAvatar::setDisabled(bool disable)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    setEnabled(!disable);
    repaint();
    TRACE_ME_OUT;	//<<==--TracePoint!

}

void UserAvatar::setSelected(bool selected)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_selected = selected;

    repaint();
    TRACE_ME_OUT;	//<<==--TracePoint!

}


int UserAvatar::borderWidth() const
{
    TRACE_ME_IN;	//<<==--TracePoint!
    TRACE_ME_OUT;	//<<==--TracePoint!
    return m_borderWidth;
}

void UserAvatar::setBorderWidth(int borderWidth)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_borderWidth = borderWidth;
    TRACE_ME_OUT;	//<<==--TracePoint!

}

QColor UserAvatar::borderSelectedColor() const
{
    TRACE_ME_IN;	//<<==--TracePoint!
    TRACE_ME_OUT;	//<<==--TracePoint!
    return m_borderSelectedColor;
}

void UserAvatar::setBorderSelectedColor(const QColor &borderSelectedColor)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_borderSelectedColor = borderSelectedColor;
    TRACE_ME_OUT;	//<<==--TracePoint!

}

QColor UserAvatar::borderColor() const
{
    TRACE_ME_IN;	//<<==--TracePoint!
    TRACE_ME_OUT;	//<<==--TracePoint!
    return m_borderColor;
}

void UserAvatar::setBorderColor(const QColor &borderColor)
{
    TRACE_ME_IN;	//<<==--TracePoint!
    m_borderColor = borderColor;
    TRACE_ME_OUT;	//<<==--TracePoint!

}
//AvatarDeleteButton::AvatarDeleteButton(QWidget *parent) : DImageButton(parent)
//{

//}
//void UserAvatar::showUserAvatar() {
//    qDebug() << "UserAvatar" << "showButton";
//    showButton();
//}

void UserAvatar::setColor(QColor color) {
    TRACE_ME_IN;	//<<==--TracePoint!
    m_palette.setColor(QPalette::WindowText, color);
    this->setPalette(m_palette);
    TRACE_ME_OUT;	//<<==--TracePoint!

}
