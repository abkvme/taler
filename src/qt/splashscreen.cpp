// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#include <boost/bind.hpp>

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/splashscreen.h>

#include <qt/brandbanner.h>
#include <qt/networkstyle.h>

#include <clientversion.h>
#include <interfaces/handler.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <util.h>
#include <ui_interface.h>
#include <version.h>

#include <QApplication>
#include <QCloseEvent>
#include <QDesktopWidget>
#include <QPainter>
#include <QRadialGradient>

SplashScreen::SplashScreen(interfaces::Node& node, Qt::WindowFlags f, const NetworkStyle *networkStyle) :
    QWidget(0, f), curAlignment(0), m_node(node)
{
    float devicePixelRatio = 1.0;
#if QT_VERSION > 0x050100
    devicePixelRatio = static_cast<QGuiApplication*>(QCoreApplication::instance())->devicePixelRatio();
#endif

    const QPixmap artwork = brand::Artwork();
    const QString titleText = tr(PACKAGE_NAME);
    const QString titleAddText = networkStyle->getTitleAddText();

    // The artwork carries the whole brand; the strip beneath it is ours, so the
    // start-up progress has somewhere to live that does not sit on top of the
    // picture. Height follows the artwork's own aspect ratio rather than a fixed
    // number, so replacing the images cannot letterbox or stretch them.
    const int imageWidth = brand::ARTWORK_WIDTH;
    const int imageHeight = artwork.isNull() ? 400
                                             : imageWidth * artwork.height() / artwork.width();
    const int stripHeight = brand::ARTWORK_STRIP_HEIGHT;
    const int margin = 12;
    const QSize logicalSize(imageWidth, imageHeight + stripHeight);

    pixmap = QPixmap(logicalSize * devicePixelRatio);
#if QT_VERSION > 0x050100
    pixmap.setDevicePixelRatio(devicePixelRatio);
#endif

    QPainter pixPaint(&pixmap);
    pixPaint.setRenderHint(QPainter::SmoothPixmapTransform);
    pixPaint.setRenderHint(QPainter::Antialiasing);

    const QRect imageRect(0, 0, imageWidth, imageHeight);
    if (artwork.isNull()) {
        pixPaint.fillRect(imageRect, QColor(10, 25, 48));
    } else {
        pixPaint.drawPixmap(imageRect, artwork);
    }

    // Take the strip's colour from the artwork's own bottom edge so it reads as
    // part of the picture rather than a bar bolted underneath, whatever artwork
    // is dropped in later.
    const QColor edge = brand::ArtworkEdge(artwork);
    const bool dark_artwork = brand::ArtworkIsDark(artwork);
    m_message_color = dark_artwork ? QColor(198, 210, 228) : QColor(55, 55, 55);

    const QRect stripRect(0, imageHeight, imageWidth, stripHeight);
    pixPaint.fillRect(stripRect, edge);
    m_message_rect = stripRect.adjusted(margin, 0, -margin, 0);

    // Version in the top-right corner, where every one of the images leaves room.
    brand::DrawVersion(pixPaint, imageRect, margin, dark_artwork, titleAddText);

    pixPaint.end();

    setWindowTitle(titleText + " " + titleAddText);

    QRect r(QPoint(), logicalSize);
    resize(r.size());
    setFixedSize(r.size());
    move(QApplication::desktop()->screenGeometry().center() - r.center());

    subscribeToCoreSignals();
    installEventFilter(this);
}

SplashScreen::~SplashScreen()
{
    unsubscribeFromCoreSignals();
}

bool SplashScreen::eventFilter(QObject * obj, QEvent * ev) {
    if (ev->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(ev);
        if(keyEvent->text()[0] == 'q') {
            m_node.startShutdown();
        }
    }
    return QObject::eventFilter(obj, ev);
}

void SplashScreen::slotFinish(QWidget *mainWin)
{
    Q_UNUSED(mainWin);

    /* If the window is minimized, hide() will be ignored. */
    /* Make sure we de-minimize the splashscreen window before hiding */
    if (isMinimized())
        showNormal();
    hide();
    deleteLater(); // No more need for this
}

static void InitMessage(SplashScreen *splash, const std::string &message)
{
    QMetaObject::invokeMethod(splash, "showMessage",
        Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(int, Qt::AlignVCenter|Qt::AlignHCenter),
        Q_ARG(QColor, splash->messageColor()));
}

static void ShowProgress(SplashScreen *splash, const std::string &title, int nProgress, bool resume_possible)
{
    // One line: the progress strip is a single line high. The hint used to sit on
    // a line of its own and is now folded in. Both strings are left exactly as
    // they were, so nothing changes for translators.
    InitMessage(splash, strprintf("%s %d%%  -  %s", title, nProgress,
            resume_possible ? _("(press q to shutdown and continue later)")
                            : _("press q to shutdown")));
}
#ifdef ENABLE_WALLET
void SplashScreen::ConnectWallet(std::unique_ptr<interfaces::Wallet> wallet)
{
    m_connected_wallet_handlers.emplace_back(wallet->handleShowProgress(boost::bind(ShowProgress, this, _1, _2, false)));
    m_connected_wallets.emplace_back(std::move(wallet));
}
#endif

void SplashScreen::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_init_message = m_node.handleInitMessage(boost::bind(InitMessage, this, _1));
    m_handler_show_progress = m_node.handleShowProgress(boost::bind(ShowProgress, this, _1, _2, _3));
#ifdef ENABLE_WALLET
    m_handler_load_wallet = m_node.handleLoadWallet([this](std::unique_ptr<interfaces::Wallet> wallet) { ConnectWallet(std::move(wallet)); });
#endif
}

void SplashScreen::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_init_message->disconnect();
    m_handler_show_progress->disconnect();
    for (auto& handler : m_connected_wallet_handlers) {
        handler->disconnect();
    }
    m_connected_wallet_handlers.clear();
    m_connected_wallets.clear();
}

void SplashScreen::showMessage(const QString &message, int alignment, const QColor &color)
{
    curMessage = message;
    curAlignment = alignment;
    curColor = color;
    update();
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(0, 0, pixmap);
    painter.setPen(curColor);
    QFont messageFont = painter.font();
    messageFont.setPointSize(10);
    painter.setFont(messageFont);
    // Elided rather than wrapped: the strip is one line high, and a message that
    // overflowed it used to paint across the artwork.
    const QString text = QFontMetrics(messageFont).elidedText(curMessage, Qt::ElideRight,
                                                              m_message_rect.width());
    painter.drawText(m_message_rect, curAlignment, text);
}

void SplashScreen::closeEvent(QCloseEvent *event)
{
    m_node.startShutdown(); // allows an "emergency" shutdown during startup
    event->ignore();
}
