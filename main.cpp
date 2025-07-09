#include <QGuiApplication>
#include <QWindow>
#include <QPainter>
#include <QBackingStore>
#include <QResizeEvent>
#include <QXmlStreamReader>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include <QDebug>
#include <QFileInfo>
#include <QStringList>
#include <LayerShellQt/window.h>

struct StaticEvent {
    int duration;
    QString file;
};

struct TransitionEvent {
    int duration;
    QString fromFile;
    QString toFile;
};

struct Event {
    enum Type { Static, Transition } type;
    QVariant data;
};

Q_DECLARE_METATYPE(StaticEvent)
Q_DECLARE_METATYPE(TransitionEvent)

class WallpaperWindow : public QWindow {
    Q_OBJECT

public:
    WallpaperWindow(const QString &xmlPath);
    ~WallpaperWindow();

protected:
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateWallpaper();

private:
    void renderWallpaper();
    bool loadXml(const QString &xmlPath);
    int totalDuration() const;
    void cacheImage(const QString &path);

    QBackingStore *m_backingStore;

    QDateTime m_startTime;
    QVector<Event> m_events;

    QHash<QString, QImage> m_imageCache;
    QStringList m_cacheOrder;
    const int m_cacheLimit = 3;

    QTimer m_timer;

    QImage m_currentStaticImage;
    QImage m_transitionFromImage;
    QImage m_transitionToImage;

    bool m_inTransition = false;
    int m_currentEventIndex = 0;
    qint64 m_elapsedInEvent = 0;
};

WallpaperWindow::WallpaperWindow(const QString &xmlPath)
: QWindow()
, m_backingStore(new QBackingStore(this))
{
    auto layerWindow = LayerShellQt::Window::get(this);
    layerWindow->setLayer(LayerShellQt::Window::LayerBackground);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setAnchors(
        QFlags<LayerShellQt::Window::Anchor>(
            LayerShellQt::Window::AnchorTop |
            LayerShellQt::Window::AnchorBottom |
            LayerShellQt::Window::AnchorLeft |
            LayerShellQt::Window::AnchorRight
        )
    );

    layerWindow->setExclusiveZone(-1);

    setFlags(Qt::FramelessWindowHint);
    setSurfaceType(QSurface::RasterSurface);
    resize(1920, 1080);
    show();

    QTimer::singleShot(0, this, [this]() {
        this->lower();
    });

    QFileInfo fi(xmlPath);
    if (fi.exists() && (fi.suffix().toLower() == "png" || fi.suffix().toLower() == "jpg" || fi.suffix().toLower() == "jpeg")) {
        QImage img(xmlPath);
        if (img.isNull()) {
            qWarning() << "Failed to load image:" << xmlPath;
            QCoreApplication::exit(1);
            return;
        }
        m_currentStaticImage = img;
        m_inTransition = false;
        m_timer.stop();
        updateWallpaper();
        return;
    }

    if (!loadXml(xmlPath)) {
        qWarning() << "Failed to load XML, exiting.";
        QCoreApplication::exit(1);
        return;
    }

    connect(&m_timer, &QTimer::timeout, this, &WallpaperWindow::updateWallpaper);
    m_timer.start(5 * 60 * 1000); // default interval
    updateWallpaper();
}

WallpaperWindow::~WallpaperWindow() {
    delete m_backingStore;
}

void WallpaperWindow::cacheImage(const QString &path) {
    if (m_imageCache.contains(path)) {
        m_cacheOrder.removeAll(path);
        m_cacheOrder.append(path);
        return;
    }

    QImage img(path);
    if (img.isNull()) {
        qWarning() << "Failed to load image:" << path;
        return;
    }

    m_imageCache.insert(path, img);
    m_cacheOrder.append(path);

    while (m_cacheOrder.size() > m_cacheLimit) {
        QString oldest = m_cacheOrder.takeFirst();
        m_imageCache.remove(oldest);
    }
}

bool WallpaperWindow::loadXml(const QString &xmlPath) {
    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot open XML file:" << xmlPath;
        return false;
    }

    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == "starttime") {
                int year=0, month=0, day=0, hour=0, minute=0, second=0;
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "starttime")) {
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == "year") year = xml.readElementText().toInt();
                        else if (xml.name() == "month") month = xml.readElementText().toInt();
                        else if (xml.name() == "day") day = xml.readElementText().toInt();
                        else if (xml.name() == "hour") hour = xml.readElementText().toInt();
                        else if (xml.name() == "minute") minute = xml.readElementText().toInt();
                        else if (xml.name() == "second") second = xml.readElementText().toInt();
                    }
                    xml.readNext();
                }
                m_startTime = QDateTime(QDate(year, month, day), QTime(hour, minute, second));
            } else if (xml.name() == "static") {
                StaticEvent se;
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "static")) {
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == "duration") se.duration = xml.readElementText().toInt();
                        else if (xml.name() == "file") se.file = xml.readElementText();
                    }
                    xml.readNext();
                }
                Event e;
                e.type = Event::Static;
                e.data = QVariant::fromValue(se);
                m_events.append(e);
            } else if (xml.name() == "transition") {
                TransitionEvent te;
                while (!(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "transition")) {
                    if (xml.tokenType() == QXmlStreamReader::StartElement) {
                        if (xml.name() == "duration") te.duration = xml.readElementText().toInt();
                        else if (xml.name() == "from") te.fromFile = xml.readElementText();
                        else if (xml.name() == "to") te.toFile = xml.readElementText();
                    }
                    xml.readNext();
                }
                Event e;
                e.type = Event::Transition;
                e.data = QVariant::fromValue(te);
                m_events.append(e);
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "XML parse error:" << xml.errorString();
        return false;
    }

    return true;
}

int WallpaperWindow::totalDuration() const {
    int total = 0;
    for (const Event &e : m_events) {
        if (e.type == Event::Static)
            total += e.data.value<StaticEvent>().duration;
        else
            total += e.data.value<TransitionEvent>().duration;
    }
    return total;
}

void WallpaperWindow::updateWallpaper() {
    if (m_events.isEmpty()) return;

    qint64 secondsSinceStart = m_startTime.secsTo(QDateTime::currentDateTime());
    int cycleDuration = totalDuration();
    if (cycleDuration == 0) return;

    qint64 loopSec = secondsSinceStart % cycleDuration;

    int accumulated = 0;
    int index = 0;
    for (; index < m_events.size(); ++index) {
        int dur = (m_events[index].type == Event::Static)
        ? m_events[index].data.value<StaticEvent>().duration
        : m_events[index].data.value<TransitionEvent>().duration;

        if (loopSec < accumulated + dur)
            break;

        accumulated += dur;
    }

    if (index >= m_events.size()) index = 0;

    m_currentEventIndex = index;
    m_elapsedInEvent = loopSec - accumulated;

    const Event &event = m_events[index];

    if (event.type == Event::Static) {
        StaticEvent se = event.data.value<StaticEvent>();
        cacheImage(se.file);
        m_currentStaticImage = m_imageCache.value(se.file);
        m_inTransition = false;
    } else {
        TransitionEvent te = event.data.value<TransitionEvent>();
        cacheImage(te.fromFile);
        cacheImage(te.toFile);
        m_transitionFromImage = m_imageCache.value(te.fromFile);
        m_transitionToImage = m_imageCache.value(te.toFile);
        m_inTransition = true;
    }

    if (isExposed())
        renderWallpaper();
}

void WallpaperWindow::renderWallpaper() {
    QRect rect = geometry();
    m_backingStore->beginPaint(rect);

    QPaintDevice *device = m_backingStore->paintDevice();
    QPainter painter(device);
    painter.fillRect(rect, Qt::black);

    // Draw image scaled to fill the window while preserving aspect ratio (crop excess)
    auto drawImagePreserveAspectCrop = [&](const QImage &img, qreal opacity = 1.0) {
        if (img.isNull())
            return;

        QSize targetSize = rect.size();
        QSize sourceSize = img.size();

        qreal scale = qMax(
            qreal(targetSize.width()) / sourceSize.width(),
                           qreal(targetSize.height()) / sourceSize.height()
        );

        QSize scaledSize = sourceSize * scale;

        QRectF targetRect(
            rect.x() + (rect.width() - scaledSize.width()) / 2.0,
                          rect.y() + (rect.height() - scaledSize.height()) / 2.0,
                          scaledSize.width(),
                          scaledSize.height()
        );

        painter.setOpacity(opacity);
        painter.drawImage(targetRect, img);
        painter.setOpacity(1.0);
    };

    if (m_inTransition) {
        TransitionEvent te = m_events[m_currentEventIndex].data.value<TransitionEvent>();
        double progress = double(m_elapsedInEvent) / te.duration;
        progress = qBound(0.0, progress, 1.0);

        drawImagePreserveAspectCrop(m_transitionFromImage, 1.0);
        drawImagePreserveAspectCrop(m_transitionToImage, progress);
    } else {
        drawImagePreserveAspectCrop(m_currentStaticImage);
    }

    painter.end();
    m_backingStore->endPaint();
    m_backingStore->flush(rect);
}


void WallpaperWindow::exposeEvent(QExposeEvent *event) {
    Q_UNUSED(event);
    if (isExposed())
        renderWallpaper();
}

void WallpaperWindow::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event);
    m_backingStore->resize(event->size());
    if (isExposed())
        renderWallpaper();
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);

    if (argc < 2) {
        qWarning() << "Usage:" << argv[0] << "path_to_xml_or_image";
        return 1;
    }

    WallpaperWindow window(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}

#include "main.moc"
