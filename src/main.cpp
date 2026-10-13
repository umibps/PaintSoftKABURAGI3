#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>
#include "MainWindow.hpp"
#include "PreferencesDialog.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // 高DPI対応
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("PaintSoft KABURAGI");
    app.setOrganizationName("KABURAGI");

    // 言語設定の読み込み（システム言語またはユーザー設定言語）
    QString effectiveLang = Kaburagi::PreferencesDialog::effectiveLanguage();
    std::cout << "[PaintSoft KABURAGI] Effective Language: " << effectiveLang.toStdString() << std::endl;

    if (effectiveLang == "ja") {
        QString appDir = QApplication::applicationDirPath();
        // 1. アプリケーション本体の日本語翻訳
        auto* appTranslator = new QTranslator(&app);
        bool appLoaded = appTranslator->load("paintsoft_ja", appDir + "/translations") ||
                         appTranslator->load("paintsoft_ja", "translations") ||
                         appTranslator->load("paintsoft_ja", ":/translations");
        if (appLoaded) {
            app.installTranslator(appTranslator);
            std::cout << "[PaintSoft KABURAGI] Application Japanese translation loaded." << std::endl;
        }

        // 2. Qt標準ダイアログ用日本語翻訳
        auto* qtTranslator = new QTranslator(&app);
        QString transPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
        bool qtLoaded = qtTranslator->load("qtbase_ja", transPath) ||
                        qtTranslator->load("qtbase_ja", appDir + "/translations") ||
                        qtTranslator->load("qt_ja", appDir + "/translations") ||
                        qtTranslator->load("qtbase_ja", "translations") ||
                        qtTranslator->load("qt_ja", "translations");
        if (qtLoaded) {
            app.installTranslator(qtTranslator);
            std::cout << "[PaintSoft KABURAGI] Qtbase Japanese translation loaded." << std::endl;
        }
    } else {
        std::cout << "[PaintSoft KABURAGI] English mode active (no Japanese translation installed)." << std::endl;
    }

    // カラーピッカー (QColorDialog) の数値入力部 (QSpinBox) が狭く数値が見切れる問題の解消用スタイルシート
    QString qss = 
        "QColorDialog QSpinBox {"
        "    min-width: 68px;"
        "    min-height: 22px;"
        "    font-size: 11px;"
        "    padding-left: 3px;"
        "    padding-right: 18px;"
        "    border: 1px solid #7f9db9;"
        "    border-radius: 2px;"
        "    background-color: white;"
        "}"
        "QColorDialog QSpinBox::up-button {"
        "    subcontrol-origin: border;"
        "    subcontrol-position: top right;"
        "    width: 15px;"
        "    border-left: 1px solid #c0c0c0;"
        "}"
        "QColorDialog QSpinBox::down-button {"
        "    subcontrol-origin: border;"
        "    subcontrol-position: bottom right;"
        "    width: 15px;"
        "    border-left: 1px solid #c0c0c0;"
        "}"
        "QColorDialog QSpinBox QLineEdit {"
        "    min-width: 44px;"
        "    font-size: 11px;"
        "    padding: 0px 1px;"
        "    background: transparent;"
        "}"
        "QColorDialog QLabel {"
        "    font-size: 11px;"
        "}"
        "QColorDialog QLineEdit {"
        "    min-width: 65px;"
        "    min-height: 22px;"
        "    font-size: 11px;"
        "    padding: 1px 3px;"
        "}"
        "QColorDialog QPushButton {"
        "    min-width: 70px;"
        "    padding: 3px 10px;"
        "    font-size: 11px;"
        "}";
    app.setStyleSheet(qss);

    Kaburagi::MainWindow window;

    // コマンドライン引数でファイルパスが渡された場合、そのファイルをキャンバスとして開く
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        if (!arg.startsWith('-') && QFile::exists(arg)) {
            window.openFile(arg);
            break;
        }
    }

    window.show();

    std::cout << "[PaintSoft KABURAGI] Started successfully." << std::endl;

    return app.exec();
}
