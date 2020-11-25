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

#include "fullscreenbackground.h"

#include <QApplication>
#include <QScreen>
#include <QDesktopWidget>
#include <QPainter>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <QKeyEvent>
#include <QCryptographicHash>
#include <QWindow>
#include <QDir>
#include <DGuiApplicationHelper>
#include <unistd.h>
#include "src/session-widgets/framedatabind.h"

DGUI_USE_NAMESPACE
#define  DEFAULT_BACKGROUND "/usr/share/backgrounds/default_background.jpg"

FullscreenBackground::FullscreenBackground(QWidget *parent)
    : QWidget(parent)
    , m_fadeOutAni(new QVariantAnimation(this))
    , m_imageEffectInter(new ImageEffectInter("com.deepin.daemon.ImageEffect", "/com/deepin/daemon/ImageEffect", QDBusConnection::systemBus(), this))
    , m_displayInter(new DisplayInter("com.deepin.daemon.Display", "/com/deepin/daemon/Display", QDBusConnection::sessionBus(), this))
{
#ifndef QT_DEBUG
//    if(DGuiApplicationHelper::isXWindowPlatform()) {
//        setWindowFlags(Qt::WindowStaysOnTopHint | Qt::X11BypassWindowManagerHint);
//    } else {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Window);
//    }
#endif

    setAttribute(Qt::WA_NativeWindow);
    windowHandle()->setProperty("_d_dwayland_window-type", "session-shell");
    QPalette pal(this->palette());
    pal.setColor(QPalette::Background, QColor(0,0,0,0));
    this->setAutoFillBackground(true);
    this->setPalette(pal);

    m_fadeOutAni->setEasingCurve(QEasingCurve::InOutCubic);
    m_fadeOutAni->setDuration(1000);
    m_fadeOutAni->setStartValue(1.0f);
    m_fadeOutAni->setEndValue(0.0f);

    installEventFilter(this);

    QWindow * window = windowHandle();
    QObject::connect(qApp, &QGuiApplication::screenRemoved, window, [window,this] (QScreen *screen) {
        if (screen == m_screen) {
            m_screen = qApp->primaryScreen();
        }
    });

    connect(m_fadeOutAni, &QVariantAnimation::valueChanged, this, static_cast<void (FullscreenBackground::*)()>(&FullscreenBackground::update));
}

FullscreenBackground::~FullscreenBackground()
{
}

bool FullscreenBackground::contentVisible() const
{
    return m_content && m_content->isVisible();
}

void FullscreenBackground::updateBackground(const QPixmap &background)
{
    // show old background fade out
    m_fakeBackground = m_background;
    m_background = background;

    m_backgroundCache = pixmapHandle(m_background);
    m_fakeBackgroundCache = pixmapHandle(m_fakeBackground);

    m_fadeOutAni->start();
}

void FullscreenBackground::updateBackground(const QString &file)
{
    //前后设置的图片一致的时候，不用加载图片，减少加载时间
    if (!file.isEmpty() && m_bgPath.compare(file, Qt::CaseSensitive) == 0) {
        return;
    }

    m_bgPath = file;

    //mark 会导致延greeter的背景及用户头像, 有比较明显延时刷出
    QPixmap image;
    QSharedMemory memory (file);
    if (memory.attach()) {
        image.loadFromData ( (const unsigned char *) memory.data(), memory.size());
        if (image.isNull()) {
            qDebug() << "input background: " << file << " is invalid image file.";
            image.load(getBlurBackground(file));
        }
        memory.detach();
    } else {
        image.load(getBlurBackground(file));
    }

    updateBackground (image);
}

QString FullscreenBackground::getBlurBackground (const QString &file)
{
    auto isPicture = [] (const QString & filePath) {
        return QFile::exists (filePath) && QFile (filePath).size() && !QPixmap (filePath).isNull() ;
    };

    QString bg_path = file;
    if (!isPicture(bg_path)) {
        QDir dir ("/usr/share/wallpapers/deepin");
        if (dir.exists()) {
            dir.setFilter (QDir::Files);
            QFileInfoList list = dir.entryInfoList();
            foreach (QFileInfo f, list) {
                if (f.baseName() == "desktop") {
                    bg_path = f.filePath();
                    break;
                }
            }
        }

        if (!QFile::exists (bg_path)) {
            bg_path = DEFAULT_BACKGROUND;
        }
    }

    QString imageEffect = m_imageEffectInter->Get ("", bg_path);
    if (!isPicture (imageEffect)) {
        imageEffect = DEFAULT_BACKGROUND;
    }

    return imageEffect;
}


void FullscreenBackground::setScreen(QScreen *screen)
{
    QScreen *primary_screen = QGuiApplication::primaryScreen();
    if(primary_screen == screen) {
        m_content->show();
        m_primaryShowFinished = true;
        emit contentVisibleChanged(true);
    } else {
        m_content->hide();
        m_primaryShowFinished = true;
        setMouseTracking(true);
    }

    updateScreen(screen);
}

void FullscreenBackground::setMonitor(Monitor *monitor)
{
    QDBusMessage getRealDisplayMode = QDBusMessage::createMethodCall("com.deepin.daemon.Display",
                                                                     "/com/deepin/daemon/Display",
                                                                     "com.deepin.daemon.Display",
                                                                     "GetRealDisplayMode");
    QDBusMessage msg = QDBusConnection::sessionBus().call(getRealDisplayMode);
    if (DisplayMode::CopyMode == msg.arguments().first().toUInt()) {
        if (m_displayInter->primary() == monitor->name()) {
            emit contentVisibleChanged(true);
            m_content->show();
        }
    } else {
        emit contentVisibleChanged(false);
        m_content->hide();
    }

    m_primaryShowFinished = monitor->enable();
    updateMonitor(monitor);
}

void FullscreenBackground::setContentVisible(bool contentVisible)
{
    if (this->contentVisible() == contentVisible)
        return;

    if (!m_content)
        return;

    if (!isVisible() && !contentVisible)
        return;

    m_content->setVisible(contentVisible);

    emit contentVisibleChanged(contentVisible);
}

void FullscreenBackground::setContent(QWidget *const w)
{
    //Q_ASSERT(m_content.isNull());

    m_content = w;
    m_content->setParent(this);
    m_content->raise();
    if (1 == m_displayInter->displayMode()) {
        m_content->show();
    }
    m_content->move(0, 0);
}

void FullscreenBackground::setIsBlackMode(bool is_black)
{
    if(m_isBlackMode == is_black) return;

    m_isBlackMode = is_black;
    FrameDataBind::Instance()->updateValue("PrimaryShowFinished", !is_black);
    m_content->setVisible(!is_black);
    emit contentVisibleChanged(!is_black);

    update();
}

void FullscreenBackground::setIsHibernateMode(){
    updateGeometry();
    m_content->show();
    emit contentVisibleChanged(true);
}

void FullscreenBackground::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const float current_ani_value = m_fadeOutAni->currentValue().toFloat();

    QRect backRect;
    if (m_monitor)
        backRect = QRect(m_monitor->rect().x(), m_monitor->rect().y(),
                         m_monitor->rect().width()/qApp->primaryScreen()->devicePixelRatio(),
                         m_monitor->rect().height()/qApp->primaryScreen()->devicePixelRatio());
    else
        backRect = QRect(QPoint(0, 0), size());

    const QRect trueRect(QPoint(0, 0), QSize(backRect.size()));
    if(!m_isBlackMode) {
        if (!m_background.isNull()) {
            // tr is need redraw rect, sourceRect need correct upper left corner
            painter.drawPixmap(trueRect,
                               m_backgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_backgroundCache.devicePixelRatioF()));
        }

        if (!m_fakeBackground.isNull()) {
            // draw background
            painter.setOpacity(current_ani_value);
            painter.drawPixmap(trueRect,
                               m_fakeBackgroundCache,
                               QRect(trueRect.topLeft(), trueRect.size() * m_fakeBackgroundCache.devicePixelRatioF()));
            painter.setOpacity(1);
        }
    } else {
        painter.fillRect(trueRect, Qt::black);
    }
}

void FullscreenBackground::enterEvent(QEvent *event)
{
    if(m_primaryShowFinished) {
        m_content->show();
        emit contentVisibleChanged(true);
    }

    return QWidget::enterEvent(event);
}

void FullscreenBackground::leaveEvent(QEvent *event)
{
    m_content->hide();
    return QWidget::leaveEvent(event);
}

void FullscreenBackground::resizeEvent(QResizeEvent *event)
{
    if (m_monitor) {
        QRect backRect(m_monitor->rect().x(), m_monitor->rect().y(),
                       m_monitor->rect().width()/qApp->primaryScreen()->devicePixelRatio(),
                       m_monitor->rect().height()/qApp->primaryScreen()->devicePixelRatio());
        m_content->resize(backRect.size());
        setGeometry(backRect);
    } else {
        m_content->resize(size());
    }

    m_backgroundCache = pixmapHandle(m_background);
    m_fakeBackgroundCache = pixmapHandle(m_fakeBackground);

    return QWidget::resizeEvent(event);
}

void FullscreenBackground::mouseMoveEvent(QMouseEvent *event)
{
    m_content->show();
    emit contentVisibleChanged(true);

    return QWidget::mouseMoveEvent(event);
}

void FullscreenBackground::keyPressEvent(QKeyEvent *e)
{
    QWidget::keyPressEvent(e);

    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:        qApp->quit();       break;
#endif
    default:;
    }
}

void FullscreenBackground::showEvent(QShowEvent *event)
{
    if (QWindow *w = windowHandle()) {
        if (m_screen) {
            if (w->screen() != m_screen) {
                w->setScreen(m_screen);
            }

            // 更新窗口位置和大小
            setFixedSize(m_screen->geometry().width(), m_screen->geometry().height());
            updateGeometry();
        }
    }

    return QWidget::showEvent(event);
}

const QPixmap FullscreenBackground::pixmapHandle(const QPixmap &pixmap)
{
    const QSize trueSize { size() *devicePixelRatioF() };
    QPixmap pix;
    if (!pixmap.isNull())
        pix = pixmap.scaled(trueSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    pix = pix.copy(QRect((pix.width() - trueSize.width()) / 2,
                         (pix.height() - trueSize.height()) / 2,
                         trueSize.width(),
                         trueSize.height()));

    // draw pix to widget, so pix need set pixel ratio from qwidget devicepixelratioF
    pix.setDevicePixelRatio(devicePixelRatioF());

    return pix;
}

void FullscreenBackground::updateScreen(QScreen *screen)
{
//    qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "UpdateScree: " << screen << screen->geometry();
//    if (screen == m_screen)
//        return;

//    if (m_screen) {
//        disconnect(m_screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry);
//    }

//    if (screen) {
//        connect(screen, &QScreen::geometryChanged, this, &FullscreenBackground::updateGeometry);
//    }

    m_screen = screen;

    if (m_screen)
        updateGeometry();
}

void FullscreenBackground::updateMonitor(Monitor *monitor)
{
    qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "UpdateScree: " << monitor << monitor->rect();
    if (monitor == m_monitor)
        return;

    if (m_monitor) {
        disconnect(m_monitor, &Monitor::geometryChanged, this, &FullscreenBackground::updateMonitorGeometry);
        disconnect(m_monitor, &Monitor::enableChanged, this, &FullscreenBackground::setVisible);
    }

    if (monitor) {
        connect(monitor, &Monitor::geometryChanged, this, &FullscreenBackground::updateMonitorGeometry);
        connect(monitor, &Monitor::enableChanged, this, [&](bool isEnable){
            if(!isEnable)
                this->setVisible(isEnable);
        });
    }

    m_monitor = monitor;

    if (m_monitor)
        updateMonitorGeometry();
}

void FullscreenBackground::updateGeometry()
{
    qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "updateGeometry: " << m_screen << m_screen->geometry() << size();
    //setGeometry(m_screen->geometry());
    QTimer::singleShot(500, this, [&](){
        setGeometry(m_screen->geometry());
    });
}

void FullscreenBackground::updateMonitorGeometry()
{
    qDebug() << __FILE__ << __LINE__ << __FUNCTION__ << "updateGeometry: " << m_monitor << m_monitor->rect() << size();
    QTimer::singleShot(200, this, [&](){
        for (auto m : m_monitor->modes()) {
            if (m.width() != m_monitor->rect().width() || m.height() != m_monitor->rect().height())
                continue;
        //setGeometry(m_monitor->rect());
        setGeometry(m_monitor->rect().x(), m_monitor->rect().y(),
                        m_monitor->rect().width()/qApp->primaryScreen()->devicePixelRatio(),
                            m_monitor->rect().height()/qApp->primaryScreen()->devicePixelRatio());
        break;
        }
    });
}

/********************************************************
 * 监听主窗体属性。
 * 用户登录界面，主窗体在某时刻会被设置为WindowDeactivate，
 * 此时登录界面获取不到焦点，需要调用requestActivate激活窗体。
********************************************************/
bool FullscreenBackground::eventFilter(QObject *watched, QEvent *e)
{
#ifndef QT_DEBUG
    if (e->type() == QEvent::WindowDeactivate) {
        if (m_content->isVisible())
            windowHandle()->requestActivate();
    }
#endif

    return QWidget::eventFilter(watched, e);
}
