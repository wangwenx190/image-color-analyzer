#include "mainwindow.h"
#include <QShortcut>
#include <QPainter>
#include <QFileDialog>
#include <QFont>
#include <QMouseEvent>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QVariant>
#include <QColor>
#include <QList>
#include <QUrl>
#include <QHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QSettings>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QClipboard>
#include <random>

using namespace Qt::StringLiterals;

#ifdef _DEBUG
static constexpr const bool IS_DEBUG_BUILD{ true };
#else
static constexpr const bool IS_DEBUG_BUILD{ false };
#endif

static constexpr const auto INVALID_COLOR_DISTANCE{ std::numeric_limits<qreal>::max() };

struct Pixel final {
    quint8 r{ 0 };
    quint8 g{ 0 };
    quint8 b{ 0 };

    friend bool operator==(Pixel lhs, Pixel rhs);
    friend bool operator!=(Pixel lhs, Pixel rhs);

    friend std::size_t qHash(Pixel key, std::size_t seed);
};

[[nodiscard]] static inline bool operator==(Pixel lhs, Pixel rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

[[nodiscard]] static inline bool operator!=(Pixel lhs, Pixel rhs) {
    return !operator==(lhs, rhs);
}

[[nodiscard]] static inline std::size_t qHash(Pixel key, std::size_t seed = 0) {
    return qHashMulti(seed, key.r, key.g, key.b);
}

[[nodiscard]] static inline qreal colorDistance(Pixel lhs, Pixel rhs) {
    const auto dr{ lhs.r - rhs.r };
    const auto dg{ lhs.g - rhs.g };
    const auto db{ lhs.b - rhs.b };
    return qSqrt(qreal(dr * dr) + qreal(dg * dg) + qreal(db * db));
}

[[nodiscard]] static inline bool extractColorsFromImage(
    QList<std::pair<QColor, qreal>>& resultOut,
    QImage imageIn,
    const qsizetype k = 5, // 4~8 is best, don't be too large (eg. > 20)! We want to get the most "attractive" color, if k is too large, the result would be distracted!
    const qsizetype maxIterations = 50, // most of the time the iteration will stop at < 20.
    const int maxWidth = 100, // if > 0, the image will be shrinked to not exceed this width. The image won't be changed if <= 0.
    const int maxHeight = 100, // same as above, just related to height.
    const int alphaThreshold = 180 // if > 0 and < 255, only the pixels whose alpha >= this value are accepted.
) {
    QElapsedTimer timer{};
    timer.start();
    if constexpr (IS_DEBUG_BUILD) {
        qInfo() << "------------------------------------------------------";
        qDebug() << "Checking whether there are any in-appropriate function parameters ...";
        qDebug().nospace() << "k=" << k << ", maxIterations=" << maxIterations << ", maxWidth=" << maxWidth << ", maxHeight=" << maxHeight << ", alphaThreshold=" << alphaThreshold;
    }
    Q_ASSERT(!imageIn.isNull());
    Q_ASSERT(k > 1);
    Q_ASSERT(maxIterations > 0);
    if (Q_UNLIKELY(imageIn.isNull() || k <= 1 || maxIterations <= 0)) {
        qWarning() << "Function parameter not valid, algorithm forcely exited. Please try again with appropriate ones.";
        return false;
    }
    QImage image(std::move(imageIn));
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "All function parameters seems to be valid.";
        qDebug().nospace() << "Image information: size: " << image.width() << "x" << image.height();
        qDebug() << "Checking whether we need to shrink the image size to speed up the whole process ...";
    }
    const qsizetype originalImageTotalPixelCount{ image.width() * image.height() };
    if (Q_LIKELY(maxWidth > 0 || maxHeight > 0)) {
        int targetWidth{ image.width() };
        if (maxWidth > 0) {
            targetWidth = qMin(targetWidth, maxWidth);
        }
        int targetHeight{ image.height() };
        if (maxHeight > 0) {
            targetHeight = qMin(targetHeight, maxHeight);
        }
        if (Q_LIKELY(targetWidth != image.width() || targetHeight != image.height())) {
            image = std::move(image.scaled(targetWidth, targetHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            Q_ASSERT(!image.isNull());
            if constexpr (IS_DEBUG_BUILD) {
                qDebug().nospace() << "Image size shrinked to: " << targetWidth << "x" << targetHeight;
            }
        }
    }
    const qsizetype nowImageTotalPixelCount{ image.width() * image.height() };
    if constexpr (IS_DEBUG_BUILD) {
        if (nowImageTotalPixelCount == originalImageTotalPixelCount) {
            qDebug() << "The image size is not shrinked, we will process the original image as-is.";
        }
        qDebug() << "Preparing the pixel list ...";
    }
    QList<Pixel> pixelList{};
    pixelList.reserve(nowImageTotalPixelCount);
    for (int y{ 0 }; y < image.height(); ++y) {
        for (int x{ 0 }; x < image.width(); ++x) {
            const QRgb rgba{ image.pixel(x, y) };
            const int a{ qAlpha(rgba) };
            if (Q_LIKELY(alphaThreshold <= std::numeric_limits<quint8>::min() || alphaThreshold >= std::numeric_limits<quint8>::max() || a >= alphaThreshold)) {
                const auto r{ static_cast<quint8>(qRed(rgba)) };
                const auto g{ static_cast<quint8>(qGreen(rgba)) };
                const auto b{ static_cast<quint8>(qBlue(rgba)) };
                // The Pixel struct is VERY small (only 3 bytes in total), move or copy doesn't have much difference in reality.
                pixelList.push_back(Pixel{ r, g, b });
            }
        }
    }
    Q_ASSERT(!pixelList.isEmpty());
    if (Q_UNLIKELY(pixelList.isEmpty())) {
        qWarning() << "No valid pixels found, please check the image file and/or the alpha threshold.";
        return false;
    }
    // No longer needed from now on and it may use much memory in many cases, so release
    // it's memory as soon as possible.
    image = {};
    const qsizetype totalValidPixelCount{ pixelList.size() };
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "Pixel list generated.";
        const qsizetype invalidPixelCount{ nowImageTotalPixelCount - totalValidPixelCount };
        qDebug().nospace() << "Total pixel count: " << nowImageTotalPixelCount << ", valid pixel count: " << totalValidPixelCount << " ("
                           << qreal(totalValidPixelCount) / qreal(nowImageTotalPixelCount) * qreal(100)
                           << "%), invalid pixel count: " << invalidPixelCount << " ("
                           << qreal(invalidPixelCount) / qreal(nowImageTotalPixelCount) * qreal(100) << "%)";
    }
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "Start building random centroid list ...";
    }
    QList<Pixel> centroidList(k);
    const auto& generateRandomCentroidList{ [k, totalValidPixelCount, &pixelList, &centroidList](){
        QList<qsizetype> indiceList(totalValidPixelCount);
        for (qsizetype index{ 0 }; index < indiceList.size(); ++index) {
            indiceList[index] = index;
        }
        {
            std::random_device rd{};
            std::mt19937_64 mt64(rd());
            std::shuffle(indiceList.begin(), indiceList.end(), mt64);
        }
        for (qsizetype index{ 0 }; index < k && index < totalValidPixelCount; ++index) {
            const auto randomIndex{ indiceList[index] };
            centroidList[index] = pixelList[randomIndex];
        }
    } };
    generateRandomCentroidList();
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "Random centroid list generated.";
        qDebug() << "Start building cluster list ...";
    }
    QList<QList<Pixel>> clusterList(k);
    for (auto& cluster : clusterList) {
        cluster.reserve(totalValidPixelCount);
    }
    qsizetype badClusterTimes{ 0 };
    while (true) {
        Q_ASSERT(badClusterTimes <= 10);
        if (badClusterTimes > 10) {
            // Critical message, always output, no matter whether this is a debug build or not.
            qCritical() << "Failed too many times, algorithm forcely exited. Please try again.";
            return false;
        }
        if constexpr (IS_DEBUG_BUILD) {
            qDebug() << "Start iterating.";
        }
        bool badClusterDetected{ false };
        for (qsizetype iteration{ 0 }; iteration < maxIterations; ++iteration) {
            if constexpr (IS_DEBUG_BUILD) {
                qDebug() << "Current iteration:" << iteration + 1;
            }
            for (auto& cluster : clusterList) {
                cluster.clear();
            }
            for (const Pixel pixel : std::as_const(pixelList)) {
                auto minimumDistance{ INVALID_COLOR_DISTANCE };
                qsizetype closestIndex{ -1 };
                for (qsizetype index{ 0 }; index < k; ++index) {
                    const auto distance{ colorDistance(pixel, centroidList[index]) };
                    Q_ASSERT(qFuzzyIsNull(distance) || distance > qreal(0));
                    Q_ASSERT(distance < INVALID_COLOR_DISTANCE);
                    if (distance < minimumDistance) {
                        minimumDistance = distance;
                        closestIndex = index;
                    }
                }
                Q_ASSERT(closestIndex >= 0);
                Q_ASSERT(closestIndex < k);
                clusterList[closestIndex].push_back(pixel);
            }
            bool changed{ false };
            QList<Pixel> newCentroidList(k);
            for (qsizetype index{ 0 }; index < k; ++index) {
                const auto& cluster{ clusterList[index] };
                //Q_ASSERT(!cluster.isEmpty());
                //Q_ASSERT(cluster.size() < totalValidPixelCount);
                if (cluster.isEmpty() || cluster.size() >= totalValidPixelCount) {
                    badClusterDetected = true;
                    break;
                }
                quint64 r{ 0 };
                quint64 g{ 0 };
                quint64 b{ 0 };
                for (const Pixel pixel : std::as_const(cluster)) {
                    r += pixel.r;
                    g += pixel.g;
                    b += pixel.b;
                }
                const auto totalPixelCount{ qreal(cluster.size()) };
                r = qRound64(qreal(r) / totalPixelCount);
                Q_ASSERT(r >= std::numeric_limits<quint8>::min() && r <= std::numeric_limits<quint8>::max());
                g = qRound64(qreal(g) / totalPixelCount);
                Q_ASSERT(g >= std::numeric_limits<quint8>::min() && g <= std::numeric_limits<quint8>::max());
                b = qRound64(qreal(b) / totalPixelCount);
                Q_ASSERT(b >= std::numeric_limits<quint8>::min() && b <= std::numeric_limits<quint8>::max());
                newCentroidList[index] = Pixel{ static_cast<quint8>(r), static_cast<quint8>(g), static_cast<quint8>(b) };
                if (colorDistance(centroidList[index], newCentroidList[index]) > qreal(1)) {
                    changed = true;
                }
            }
            if (badClusterDetected) {
                if constexpr (IS_DEBUG_BUILD) {
                    qWarning() << "Found bad cluster. Iteration forcely ended.";
                }
                break;
            }
            if (!changed) {
                if constexpr (IS_DEBUG_BUILD) {
                    qDebug() << "Result seems to be stable enough now. Iteration ended normally. Final iteration count:" << iteration + 1;
                }
                break;
            }
            centroidList = std::move(newCentroidList);
            //qSwap(centroidList, newCentroidList);
        }
        if (badClusterDetected) {
            ++badClusterTimes;
            generateRandomCentroidList();
            if constexpr (IS_DEBUG_BUILD) {
                qDebug() << "Centroid list regenerated. Re-starting iteration now ...";
            }
            continue;
        }
        break;
    }
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "Cluster list stablized, start re-ordering them by their pixel count ...";
    }
    // No longer needed from now on and it may use much memory depending on the image and user options,
    // so release it's memory as soon as possible.
    pixelList = {};
    QList<qsizetype> clusterSizeList(k);
    for (qsizetype index{ 0 }; index < k; ++index) {
        clusterSizeList[index] = clusterList[index].size();
    }
    // No longer needed from now on and it may use much memory depending on the image and user options,
    // so release it's memory as soon as possible.
    clusterList = {};
    QList<qsizetype> clusterIndexList(k);
    for (qsizetype index{ 0 }; index < k; ++index) {
        clusterIndexList[index] = index;
    }
    std::sort(clusterIndexList.begin(), clusterIndexList.end(),
              [&clusterSizeList](qsizetype indexLHS, qsizetype indexRHS){
                  return clusterSizeList[indexLHS] < clusterSizeList[indexRHS];
              });
    const auto& generateResultForIndex{ [k, totalValidPixelCount, &centroidList, &clusterSizeList](const qsizetype clusterIndex){
        Q_ASSERT(clusterIndex >= 0);
        Q_ASSERT(clusterIndex < k);
        const Pixel pixel{ centroidList[clusterIndex] };
        QColor color{ std::move(QColor::fromRgb(static_cast<int>(pixel.r), static_cast<int>(pixel.g), static_cast<int>(pixel.b))) };
        const qsizetype clusterSize{ clusterSizeList[clusterIndex] };
        Q_ASSERT(clusterSize > 0);
        Q_ASSERT(clusterSize < totalValidPixelCount);
        const qreal ratio{ qreal(clusterSize) / qreal(totalValidPixelCount) };
        return std::move(std::make_pair(std::move(color), ratio));
    } };
    if constexpr (IS_DEBUG_BUILD) {
        const qsizetype clusterIndex{ clusterIndexList.constLast() };
        const auto result{ generateResultForIndex(clusterIndex) };
        qDebug().noquote().nospace() << "Re-ordering done. The most dominant color is: " << std::move(result.first.name().toUpper()) << ", ratio: " << result.second * qreal(100) << "%";
        qDebug() << "Start generating result ...";
    }
    resultOut.resize(k);
    for (qsizetype index{ 0 }; index < k; ++index) {
        const qsizetype clusterIndex{ clusterIndexList[index] };
        auto result{ generateResultForIndex(clusterIndex) };
        resultOut[index] = std::move(result);
    }
    if constexpr (IS_DEBUG_BUILD) {
        qDebug() << "Result ready. Everything DONE now.";
        qDebug() << "Total elapsed time:" << timer.elapsed() << "milliseconds.";
    }
    return true;
}

[[nodiscard]] static inline bool extractImageDataFromMimeData(const QMimeData* md, QVariant* dataOut = nullptr) {
    Q_ASSERT(md);
    if (!md->hasImage() && !md->hasUrls() && !md->hasText()) {
        return false;
    }
    if (md->hasImage()) {
        if (dataOut) {
            *dataOut = std::move(md->imageData());
        }
        return true;
    }
    QString maybeFilePath{};
    if (md->hasText()) {
        maybeFilePath = std::move(md->text());
    } else {
        const QList<QUrl> urlList{ std::move(md->urls()) };
        Q_ASSERT(!urlList.isEmpty());
        maybeFilePath = std::move(urlList[0].toLocalFile());
    }
    Q_ASSERT(!maybeFilePath.isEmpty());
    if (Q_UNLIKELY(maybeFilePath.isEmpty())) {
        return false;
    }
    if (Q_LIKELY(maybeFilePath.startsWith(u"file:"_s))) {
        maybeFilePath.remove(0, 5);
    }
    while (maybeFilePath.startsWith(u"/"_s)) {
        maybeFilePath.remove(0, 1);
    }
    const QFileInfo fileInfo(maybeFilePath);
    if (Q_UNLIKELY(!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable())) {
        return false;
    }
    const QString extName{ std::move(fileInfo.suffix()) };
    if (Q_LIKELY(extName.compare(u"png"_s, Qt::CaseInsensitive) == 0 ||
                 extName.compare(u"jpg"_s, Qt::CaseInsensitive) == 0 ||
                 extName.compare(u"jpeg"_s, Qt::CaseInsensitive) == 0 ||
                 extName.compare(u"bmp"_s, Qt::CaseInsensitive) == 0)) {
        if (dataOut) {
            *dataOut = std::move(fileInfo.canonicalFilePath());
        }
        return true;
    }
    return false;
}

[[nodiscard]] static inline bool isColorLight(const QColor& color) {
    Q_ASSERT(color.isValid());
    const auto& toLinear = [](const qreal value) {
        Q_ASSERT(qFuzzyIsNull(value) || value > qreal(0));
        Q_ASSERT(qFuzzyCompare(value, qreal(1)) || value < qreal(1));
        static constexpr const qreal magic{ 0.03928 };
        return (qFuzzyCompare(value, magic) || value < magic) ? (value / 12.92) : qPow((value + 0.055) / 1.055, 2.4);
    };
    const auto linearR{ toLinear(color.redF()) };
    const auto linearG{ toLinear(color.greenF()) };
    const auto linearB{ toLinear(color.blueF()) };
    const auto luminance{ 0.2126 * linearR + 0.7152 * linearG + 0.0722 * linearB };
    return luminance > 0.5;
}

[[nodiscard]] static inline bool isPointInPieSlice(const QPointF& point, const QPointF& center, const qreal radius, qreal startAngleDeg, qreal endAngleDeg) {
    Q_ASSERT(radius > qreal(0));
    Q_ASSERT(!qFuzzyCompare(startAngleDeg, endAngleDeg));
    const qreal dx{ point.x() - center.x() };
    const qreal dy{ point.y() - center.y() };
    const qreal distance{ qSqrt(dx * dx + dy * dy) };
    if (qFuzzyIsNull(distance) || qFuzzyCompare(distance, radius) || distance > radius) {
        return false;
    }
    const auto& normalizeAngle{ [](qreal angle){
        while (angle < qreal(0)) {
            angle += qreal(360);
        }
        while (angle > qreal(360)) {
            angle -= qreal(360);
        }
        return angle;
    } };
    const qreal angleRad{ qAtan2(-dy, dx) };
    const qreal angleDeg{ normalizeAngle(qRadiansToDegrees(angleRad)) };
    Q_ASSERT(qFuzzyIsNull(angleDeg) || angleDeg > qreal(0));
    Q_ASSERT(qFuzzyCompare(angleDeg, qreal(360)) || angleDeg < qreal(360));
    startAngleDeg = normalizeAngle(startAngleDeg);
    Q_ASSERT(qFuzzyIsNull(startAngleDeg) || startAngleDeg > qreal(0));
    Q_ASSERT(qFuzzyCompare(startAngleDeg, qreal(360)) || startAngleDeg < qreal(360));
    endAngleDeg = normalizeAngle(endAngleDeg);
    Q_ASSERT(qFuzzyIsNull(endAngleDeg) || endAngleDeg > qreal(0));
    Q_ASSERT(qFuzzyCompare(endAngleDeg, qreal(360)) || endAngleDeg < qreal(360));
    Q_ASSERT(!qFuzzyCompare(startAngleDeg, endAngleDeg));
    if (qFuzzyCompare(angleDeg, startAngleDeg) || qFuzzyCompare(angleDeg, endAngleDeg)) {
        return false;
    }
    if (startAngleDeg > endAngleDeg) {
        return angleDeg > startAngleDeg || angleDeg < endAngleDeg;
    }
    return angleDeg > startAngleDeg && angleDeg < endAngleDeg;
}

struct UserOptions final {
    QString filePath{};
    qsizetype k{ 5 };
    qsizetype maxIterations{ 50 };
    int maxWidth{ 100 };
    int maxHeight{ 100 };
    int alphaThreshold{ 180 };
};

class OptionsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent = nullptr, Qt::WindowFlags f = {});
    ~OptionsDialog() override;

    [[nodiscard]] const UserOptions& userOptions() const;
    [[nodiscard]] QSettings& settings();

private:
    QLineEdit* m_filePathEdit{ nullptr };
    QSpinBox* m_kSpin{ nullptr };
    QSpinBox* m_maxIterationsSpin{ nullptr };
    QSpinBox* m_maxWidthSpin{ nullptr };
    QSpinBox* m_maxHeightSpin{ nullptr };
    QSpinBox* m_alphaThresholdSpin{ nullptr };
    UserOptions m_options{};
    QSettings m_settings{};
};

class MainWindowPrivate final {
    Q_DISABLE_COPY(MainWindowPrivate)
    Q_DECLARE_PUBLIC(MainWindow)

public:
    static inline constexpr const qreal s_margin{ 50 };
    static inline constexpr const auto s_backgroundColor{ QColorConstants::Transparent };

    MainWindowPrivate(MainWindow* qq);
    ~MainWindowPrivate();

    [[nodiscard]] bool parseImage(QImage image);
    [[nodiscard]] bool parseImage(QString filePath = {});
    [[nodiscard]] QRectF pieRect() const;
    [[nodiscard]] QPixmap grabResultImage();

    MainWindow* q_ptr{ nullptr };
    qsizetype highlightedSliceIndex{ -1 };
    QList<std::pair<QColor, qreal>> colorList{};
    QString imageFilePath{};
    bool isGrabbing{ false };
    OptionsDialog* optionsDialog{ nullptr };
};

OptionsDialog::OptionsDialog(QWidget* parent, Qt::WindowFlags f) : QDialog{ parent, f } {
    setAttribute(Qt::WA_DontCreateNativeAncestors);

    setWindowTitle(tr("Options"));

    setModal(true);

    auto formLayout{ new QFormLayout() };

    m_filePathEdit = new QLineEdit(this);
    m_filePathEdit->setMinimumWidth(400);
    m_filePathEdit->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);

    auto browseButton{ new QPushButton(this) };
    browseButton->setText(tr("&Browse"));
    connect(browseButton, &QPushButton::clicked, this, [this](){
        static const QString openDirKey{ std::move(u"open_dir"_s) };
        QString lastDirPath{ std::move(m_settings.value(openDirKey, u"."_s).toString()) };
        {
            const QFileInfo fileInfo(lastDirPath);
            if (!fileInfo.exists() || !fileInfo.isDir() || !fileInfo.isReadable()) {
                lastDirPath = std::move(u"."_s);
            }
        }
        const QString filePath{ std::move(QFileDialog::getOpenFileName(this, MainWindow::tr("Please select an image file to analyze"), lastDirPath, MainWindow::tr("Image Files (*.png *.jpg *.jpeg *.bmp);;All Files (*)"))) };
        if (filePath.isEmpty()) {
            return;
        }
        // The selected file surely exist apparently, so we can use "canonicalPath()" safely here.
        // "filePath" here points to a file, so we use "canonicalPath()" to get it's directory path.
        lastDirPath = std::move(QFileInfo(filePath).canonicalPath());
        m_settings.setValue(openDirKey, std::move(lastDirPath));
        m_filePathEdit->setText(QDir::toNativeSeparators(filePath));
    });

    auto filePathLayout{ new QHBoxLayout() };
    filePathLayout->addWidget(m_filePathEdit);
    filePathLayout->addWidget(browseButton);
    formLayout->addRow(tr("File path:"), filePathLayout);

    m_kSpin = new QSpinBox(this);
    m_kSpin->setRange(2, 9999);
    m_kSpin->setValue(5);
    formLayout->addRow(tr("k:"), m_kSpin);

    m_maxIterationsSpin = new QSpinBox(this);
    m_maxIterationsSpin->setRange(1, 9999);
    m_maxIterationsSpin->setValue(50);
    formLayout->addRow(tr("Maximum iteration count:"), m_maxIterationsSpin);

    m_maxWidthSpin = new QSpinBox(this);
    m_maxWidthSpin->setRange(-9999, 999999);
    m_maxWidthSpin->setValue(100);
    formLayout->addRow(tr("Maximum image width:"), m_maxWidthSpin);

    m_maxHeightSpin = new QSpinBox(this);
    m_maxHeightSpin->setRange(-9999, 999999);
    m_maxHeightSpin->setValue(100);
    formLayout->addRow(tr("Maximum image height:"), m_maxHeightSpin);

    m_alphaThresholdSpin = new QSpinBox(this);
    m_alphaThresholdSpin->setRange(-9999, 9999);
    m_alphaThresholdSpin->setValue(180);
    formLayout->addRow(tr("Maximum image height:"), m_alphaThresholdSpin);

    auto okButton{ new QPushButton(this) };
    okButton->setText(tr("&OK"));
    connect(okButton, &QPushButton::clicked, this, [this](){
        const QString filePath{ std::move(m_filePathEdit->text()) };
        if (filePath.isEmpty()) {
            QMessageBox::warning(this, tr("ERROR"), tr("You MUST set an valid local file path!"));
            return;
        }
        if (!QUrl::fromUserInput(filePath).isLocalFile()) {
            QMessageBox::warning(this, tr("ERROR"), tr("Only local file paths can be accepted, URLs are not allowed."));
            return;
        }
        const QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
            QMessageBox::warning(this, tr("ERROR"), tr("The file path points to an invalid location!"));
            return;
        }
        const qsizetype k{ m_kSpin->value() };
        if (k < 4 || k > 8) {
            if (QMessageBox::question(this, tr("WARNING"), tr("k's recommended range is [4,8], however, your input doesn't seem to be appropriate.\nDo you still wish to continue?")) == QMessageBox::No) {
                return;
            }
        }
        const qsizetype maxIterations{ m_maxIterationsSpin->value() };
        if (maxIterations < 20) {
            if (QMessageBox::question(this, tr("WARNING"), tr("The maximum iteration count is less than 20 which may make the result less accurate.\nDo you still wish to continue?")) == QMessageBox::No) {
                return;
            }
        }
        const int maxWidth{ m_maxWidthSpin->value() };
        const int maxHeight{ m_maxHeightSpin->value() };
        const int alphaThreshold{ m_alphaThresholdSpin->value() };
        m_options.filePath = std::move(fileInfo.canonicalFilePath());
        m_options.k = k;
        m_options.maxIterations = maxIterations;
        m_options.maxWidth = maxWidth;
        m_options.maxHeight = maxHeight;
        m_options.alphaThreshold = alphaThreshold;
        accept();
    });

    auto mainLayout{ new QVBoxLayout(this) };
    mainLayout->setSizeConstraint(QVBoxLayout::SetFixedSize);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(okButton);
}

OptionsDialog::~OptionsDialog() = default;

const UserOptions& OptionsDialog::userOptions() const {
    return m_options;
}

QSettings& OptionsDialog::settings() {
    return m_settings;
}

MainWindowPrivate::MainWindowPrivate(MainWindow* qq) : q_ptr{ qq } {
    Q_ASSERT(q_ptr);
    colorList.reserve(100);
    optionsDialog = new OptionsDialog(q_ptr);
}

MainWindowPrivate::~MainWindowPrivate() = default;

bool MainWindowPrivate::parseImage(QImage image) {
    Q_Q(MainWindow);
    if (image.isNull()) {
        QMessageBox::critical(q, MainWindow::tr("ERROR"), MainWindow::tr("The selected image file cannot be loaded successfully!"));
        return false;
    }
    const UserOptions& options{ optionsDialog->userOptions() };
    if (!extractColorsFromImage(colorList, std::move(image), options.k, options.maxIterations, options.maxWidth, options.maxHeight, options.alphaThreshold)) {
        QMessageBox::critical(q, MainWindow::tr("ERROR"), MainWindow::tr("Failed to analyze image color!"));
        return false;
    }
    imageFilePath.clear();
    q->update();
    return true;
}

bool MainWindowPrivate::parseImage(QString filePath) {
    Q_Q(MainWindow);
    if (filePath.isEmpty()) {
        QMessageBox::critical(q, MainWindow::tr("ERROR"), MainWindow::tr("The image file path MUST not be empty!"));
        return false;
    }
    qDebug() << "Trying to process:" << std::move(QDir::toNativeSeparators(filePath));
    QImage image{ filePath };
    if (!parseImage(std::move(image))) {
        return false;
    }
    imageFilePath = std::move(filePath);
    return true;
}

QRectF MainWindowPrivate::pieRect() const {
    Q_Q(const MainWindow);
    const auto width{ qreal(q->width()) };
    const auto height{ qreal(q->height()) };
    const qreal diameter{ qMin(width, height) - s_margin * qreal(2) };
    return QRectF{ (width - diameter) / qreal(2), (height - diameter) / qreal(2), diameter, diameter };
}

QPixmap MainWindowPrivate::grabResultImage() {
    Q_Q(MainWindow);
    isGrabbing = true;
    q->update();
    QPixmap pixmap{ q->grab(pieRect().toRect().marginsAdded(QMarginsF(s_margin, s_margin, s_margin, s_margin).toMargins())) };
    isGrabbing = false;
    return std::move(pixmap);
}

MainWindow::MainWindow(QWidget* parent) : QWidget{ parent }, d_ptr{ std::make_unique<MainWindowPrivate>(this) } {
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);
    setMouseTracking(true); // We need all mouse events no matter whether any mouse buttons are pressed or not.
    setAutoFillBackground(false); // We'll handle the window background ourself.
    setMinimumSize(600, 600);

    setWindowTitle(tr("Image Color Analyzer"));

    {
        QFont f{ font() };
        f.setBold(true);
        f.setPixelSize(25);
        setFont(f);
    }

    {
        Q_D(MainWindow);
        connect(d->optionsDialog, &OptionsDialog::finished, this, [this, d](const int result){
            if (result == OptionsDialog::Rejected) {
                return;
            }
            std::ignore = d->parseImage(d->optionsDialog->userOptions().filePath);
        });
    }

    new QShortcut(QKeySequence::Open, this, this, [this](){
        Q_D(MainWindow);
        d->optionsDialog->open();
    });
    new QShortcut(QKeySequence::Refresh, this, this, [this](){
        Q_D(MainWindow);
        if (!d->imageFilePath.isEmpty()) {
            std::ignore = d->parseImage(d->imageFilePath);
        }
    });
    new QShortcut(QKeySequence::Save, this, this, [this](){
        Q_D(MainWindow);
        QSettings& settings{ d->optionsDialog->settings() };
        static const QString saveDirKey{ std::move(u"save_dir"_s) };
        QString lastDirPath{ std::move(settings.value(saveDirKey, u"."_s).toString()) };
        {
            const QFileInfo fileInfo(lastDirPath);
            if (!fileInfo.exists() || !fileInfo.isDir() || !fileInfo.isWritable()) {
                if (QDir(lastDirPath) == QDir(u"."_s)) {
                    lastDirPath = std::move(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
                    if (lastDirPath.isEmpty()) {
                        QMessageBox::warning(this, tr("ERROR"), tr("Cannot determine any writable location. Operation aborted."));
                        return;
                    }
                } else {
                    lastDirPath = std::move(u"."_s);
                }
            }
        }
        const QString filePath{ std::move(QFileDialog::getSaveFileName(this, tr("Please select a save location"), lastDirPath, tr("PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)"))) };
        if (filePath.isEmpty()) {
            return;
        }
        // "filePath" here points to a file which may be generated later, so it may not exist currently
        // if we are not overwriting some existing file. But "QFileInfo::canonicalPath()" will return
        // an empty string if the file doesn't exist, so we can't use it here apparently.
        lastDirPath = std::move(QDir::cleanPath(QFileInfo(filePath).absolutePath()));
        settings.setValue(saveDirKey, std::move(lastDirPath));
        const QPixmap pixmap{ std::move(d->grabResultImage()) };
        Q_ASSERT(!pixmap.isNull());
        if (Q_UNLIKELY(pixmap.isNull())) {
            QMessageBox::warning(this, tr("ERROR"), tr("Failed to grab the image of current result."));
            return;
        }
        if (pixmap.save(filePath)) {
            QMessageBox::information(this, tr("INFORMATION"), tr("Result saved to: %1").arg(QDir::toNativeSeparators(filePath)));
        } else {
            QMessageBox::warning(this, tr("ERROR"), tr("Failed to write the grabbed image to disk."));
        }
    });
    new QShortcut(QKeySequence::Copy, this, this, [this](){
        Q_D(MainWindow);
        const QPixmap pixmap{ std::move(d->grabResultImage()) };
        Q_ASSERT(!pixmap.isNull());
        if (Q_UNLIKELY(pixmap.isNull())) {
            QMessageBox::warning(this, tr("ERROR"), tr("Failed to grab the image of current result."));
            return;
        }
        QGuiApplication::clipboard()->setPixmap(pixmap);
        QMessageBox::information(this, tr("INFORMATION"), tr("The current result image has been copied to the clipboard."));
    });
    new QShortcut(QKeySequence::Cancel, this, this, [](){ QCoreApplication::quit(); });
    new QShortcut(QKeySequence::Close, this, this, [](){ QCoreApplication::quit(); });
    new QShortcut(QKeySequence::Quit, this, this, [](){ QCoreApplication::quit(); });
}

MainWindow::~MainWindow() = default;

QSize MainWindow::sizeHint() const {
    return { 800, 800 };
}

void MainWindow::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    update();
}

void MainWindow::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    update();
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
    Q_D(MainWindow);
    qsizetype nowHighlightedSliceIndex{ -1 };
    do {
        if (d->colorList.isEmpty()) {
            break;
        }
        const QPointF mousePos{ event->position() };
        const QRectF pieRect{ d->pieRect() };
        const QPointF pieCenter{ pieRect.center() };
        const qreal pieRadius{ pieRect.width() / qreal(2) };
        {
            const qreal distance{ QLineF(pieCenter, mousePos).length() };
            if (qFuzzyCompare(distance, pieRadius) || distance > pieRadius) {
                break;
            }
        }
        qreal currentAngle{ 90 }; // 0 degree is +x direction, positive degree is counter-wise.
        for (qsizetype index{ 0 }; index < d->colorList.size(); ++index) {
            const auto& slice{ d->colorList[index] };
            Q_ASSERT(slice.second > qreal(0));
            Q_ASSERT(slice.second < qreal(1));
            const qreal spanAngle{ slice.second * qreal(360) };
            const qreal startAngleDeg{ currentAngle };
            const qreal endAngleDeg{ currentAngle + spanAngle };
            if (isPointInPieSlice(mousePos, pieCenter, pieRadius, startAngleDeg, endAngleDeg)) {
                nowHighlightedSliceIndex = index;
                break;
            }
            currentAngle = endAngleDeg;
        }
    } while (false);
    if (nowHighlightedSliceIndex == d->highlightedSliceIndex) {
        return;
    }
    d->highlightedSliceIndex = nowHighlightedSliceIndex;
    update();
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) {
        Q_D(MainWindow);
        d->optionsDialog->open();
    }
}

void MainWindow::paintEvent(QPaintEvent*) {
    Q_D(MainWindow);
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), MainWindowPrivate::s_backgroundColor);
    if (d->colorList.isEmpty()) {
        return;
    }
    const bool hasHighlightedSlice{ !d->isGrabbing && d->highlightedSliceIndex >= 0 };
    const QRectF pieRect{ d->pieRect() };
    const QPointF pieCenter{ pieRect.center() };
    const qreal textRadius{ pieRect.width() / qreal(2) * 0.7 };
    qreal currentAngle{ 90 }; // 0 degree is +x direction, positive degree is counter-wise.
    for (qsizetype index{ 0 }; index < d->colorList.size(); ++index) {
        const auto& slice{ d->colorList[index] };
        Q_ASSERT(slice.second > qreal(0));
        Q_ASSERT(slice.second < qreal(1));
        const QColor& color{ slice.first };
        Q_ASSERT(color.isValid());
        Q_ASSERT(color.alpha() == 255);
        const qreal ratio{ slice.second };
        const bool lightColor{ isColorLight(color) };
        const bool highlightCurrentSlice{ hasHighlightedSlice && d->highlightedSliceIndex == index };
        QPen pen{};
        if (index == d->colorList.size() - 1) {
            const QColor reversedColor{ std::move(QColor::fromRgb(255 - color.red(), 255 - color.green(), 255 - color.blue())) };
            pen.setColor(highlightCurrentSlice ? (isColorLight(reversedColor) ? reversedColor.darker(130) : reversedColor.lighter(130)) : reversedColor);
            pen.setWidthF(qreal(10));
        } else {
            pen.setColor(MainWindowPrivate::s_backgroundColor);
            pen.setWidthF(qreal(1));
        }
        painter.setPen(pen);
        painter.setBrush(highlightCurrentSlice ? (lightColor ? color.darker(130) : color.lighter(130)) : color);
        const qreal spanAngle{ ratio * qreal(360) };
        painter.drawPie(pieRect, currentAngle * qreal(16), spanAngle * qreal(16));
        const qreal middleAngleDeg{ currentAngle + spanAngle / qreal(2) };
        const qreal middleAngleRad{ qDegreesToRadians(middleAngleDeg) };
        const QPointF textCenterPos{ pieCenter.x() + textRadius * qCos(middleAngleRad), pieCenter.y() - textRadius * qSin(middleAngleRad) };
        const QFontMetricsF fm(painter.fontMetrics());
        QRectF textRect{};
        textRect.setWidth(fm.horizontalAdvance(u"#RRGGBB"_s));
        textRect.setHeight(fm.height() * qreal(2)); // 2 lines: 1 line for the color hex text and another line for the ratio text.
        textRect.moveCenter(textCenterPos);
        painter.setPen(lightColor ? QColorConstants::Black : QColorConstants::White);
        const QString sliceText{ u"%1\n%2%"_s.arg(color.name().toUpper(), QString::number(ratio * qreal(100))) };
        painter.drawText(textRect, Qt::AlignCenter | Qt::TextDontClip, sliceText);
        currentAngle += spanAngle;
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    QWidget::dragEnterEvent(event);
    if (extractImageDataFromMimeData(event->mimeData())) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    QWidget::dropEvent(event);
    Q_D(MainWindow);
    QVariant data{};
    const bool hasData{ extractImageDataFromMimeData(event->mimeData(), &data) };
    Q_ASSERT(hasData);
    Q_ASSERT(data.isValid());
    if (data.typeId() == QMetaType::QImage) {
        QImage image{ std::move(qvariant_cast<QImage>(data)) };
        std::ignore = d->parseImage(std::move(image));
    } else {
        Q_ASSERT(data.typeId() == QMetaType::QString);
        std::ignore = d->parseImage(std::move(data.toString()));
    }
}

#include "mainwindow.moc"
