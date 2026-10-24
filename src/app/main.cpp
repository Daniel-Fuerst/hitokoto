// SPDX-License-Identifier: GPL-3.0-or-later
#include "document_session.hpp"
#include "editor_widget.hpp"
#include "hitokoto/core/markdown.hpp"
#include "main_window.hpp"
#include "theme_manager.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QTimer>

#include <cstdio>
#include <memory>

using namespace hitokoto;

namespace {

/// S006 probe: with HITOKOTO_PROBE_FIRST_FRAME=1 the app prints the time from main() to the
/// first painted frame with the document loaded, then quits. Used by bench/startup.
void installStartupProbe(ui::MainWindow& window, const QElapsedTimer& sinceMain, bool waitForLoad) {
    auto loaded = std::make_shared<bool>(!waitForLoad);
    QObject::connect(window.session(), &ui::DocumentSession::firstChunkShown, &window, [loaded] { *loaded = true; });
    QObject::connect(window.session(), &ui::DocumentSession::loadFailed, &window, [loaded] { *loaded = true; });
    QObject::connect(window.editor(), &ui::EditorWidget::painted, &window, [&sinceMain, loaded] {
        if (!*loaded) {
            return;
        }
        *loaded = false;
        std::fprintf(stderr, "hitokoto-probe first-frame-ms=%.2f\n",
                     static_cast<double>(sinceMain.nsecsElapsed()) / 1e6);
        std::fflush(stderr);
        QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(0); });
    });
}

}  // namespace

int main(int argc, char* argv[]) {
    QElapsedTimer sinceMain;
    sinceMain.start();

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("hitokoto"));
    QApplication::setApplicationVersion(QStringLiteral(HITOKOTO_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A single-file hybrid Markdown editor."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Markdown file to open."),
                                 QStringLiteral("[file]"));
    const QCommandLineOption themeOption(QStringLiteral("theme"),
                                         QStringLiteral("Theme file to use instead of the default location."),
                                         QStringLiteral("path"));
    parser.addOption(themeOption);
    parser.process(app);

    static const core::ElementRegistry registry = core::ElementRegistry::withBuiltins();
    ui::MainWindow window(registry);

    ui::ThemeManager themes(
        app, parser.isSet(themeOption) ? parser.value(themeOption) : ui::ThemeManager::defaultThemePath());
    QObject::connect(&themes, &ui::ThemeManager::themeChanged, &window, &ui::MainWindow::applyTheme);
    QObject::connect(&themes, &ui::ThemeManager::problemsFound, &window,
                     [&window](const QString& path, const QStringList& problems) {
                         window.showMessage(QObject::tr("Theme not applied"),
                                            QObject::tr("%1 has errors; the built-in theme is used instead.\n\n%2")
                                                .arg(path, problems.join(QLatin1Char('\n'))));
                         for (const auto& p : problems) {
                             std::fprintf(stderr, "hitokoto: theme %s: %s\n", qPrintable(path), qPrintable(p));
                         }
                     });
    QObject::connect(&window, &ui::MainWindow::reloadThemeRequested, &themes, &ui::ThemeManager::reload);
    themes.reload();

    const QStringList files = parser.positionalArguments();
    if (qEnvironmentVariableIsSet("HITOKOTO_PROBE_FIRST_FRAME")) {
        installStartupProbe(window, sinceMain, !files.isEmpty());
    }
    window.show();
    if (!files.isEmpty()) {
        window.openFile(files.first());
    }
    return QApplication::exec();
}
