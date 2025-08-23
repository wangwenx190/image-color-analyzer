#include "mainwindow.h"
#include <QDir>
#include <QLocale>
#include <QApplication>
#include <QFontDatabase>
#include <clocale>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QCoreApplication::setApplicationName(u"Image Color Analyzer"_s);
    QCoreApplication::setApplicationVersion(u"1.0.0.0"_s);
    QCoreApplication::setOrganizationName(u"wangwenx190"_s);
    QCoreApplication::setOrganizationDomain(u"https://wangwenx190.github.io/"_s);

    QApplication application(argc, argv);

    std::setlocale(LC_ALL, "C.UTF-8");
    QLocale::setDefault(QLocale::c());
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    {
        const QString preferredFontFamily{ std::move(u"JetBrains Mono"_s) };
        bool hasOurRequiredFont{ false };
        const QStringList systemFontFamilyList{ std::move(QFontDatabase::families()) };
        for (auto&& systemFontFamily : systemFontFamilyList) {
            if (systemFontFamily == preferredFontFamily) {
                hasOurRequiredFont = true;
                break;
            }
        }
        if (!hasOurRequiredFont) {
            int result{ QFontDatabase::addApplicationFont(u":/fonts/JetBrainsMono-Regular.ttf"_s) };
            Q_ASSERT(result >= 0);
            result = QFontDatabase::addApplicationFont(u":/fonts/JetBrainsMono-Bold.ttf"_s);
            Q_ASSERT(result >= 0);
            result = QFontDatabase::addApplicationFont(u":/fonts/JetBrainsMono-Italic.ttf"_s);
            Q_ASSERT(result >= 0);
            result = QFontDatabase::addApplicationFont(u":/fonts/JetBrainsMono-BoldItalic.ttf"_s);
            Q_ASSERT(result >= 0);
            Q_UNUSED(result);
        }
        QFont font{ QGuiApplication::font() };
        font.setStyleStrategy(static_cast<QFont::StyleStrategy>(QFont::PreferQuality | QFont::PreferAntialias));
        font.setFamily(preferredFontFamily);
        font.setPixelSize(14);
        QGuiApplication::setFont(font);
    }

    MainWindow mainWindow{};
    mainWindow.show();

    return application.exec();
}
