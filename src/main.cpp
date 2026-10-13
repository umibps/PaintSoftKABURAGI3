#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QDir>
#include <QFile>
#include "MainWindow.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // 高DPI対応
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("PaintSoft KABURAGI");
    app.setOrganizationName("KABURAGI");

    // 日本語翻訳の読み込み (Qt標準ダイアログの日本語化: QColorDialog, QFileDialog等)
    auto* qtTranslator = new QTranslator(&app);
    QString transPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    bool loaded = qtTranslator->load("qtbase_ja", transPath);
    if (!loaded) {
        QString appDir = QApplication::applicationDirPath();
        loaded = qtTranslator->load("qtbase_ja", appDir + "/translations") ||
                 qtTranslator->load("qt_ja", appDir + "/translations") ||
                 qtTranslator->load("qtbase_ja", "translations") ||
                 qtTranslator->load("qt_ja", "translations");
    }
    if (loaded) {
        app.installTranslator(qtTranslator);
        std::cout << "[PaintSoft KABURAGI] Japanese translation loaded successfully." << std::endl;
    } else {
        std::cerr << "[PaintSoft KABURAGI] Warning: Could not load Japanese translation file." << std::endl;
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
