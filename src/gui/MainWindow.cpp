#include "MainWindow.hpp"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QStackedWidget>
#include <QApplication>
#include <QSettings>
#include <QShortcut>
#include <QActionGroup>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QKeySequenceEdit>
#include <QKeyEvent>
#include <QToolButton>
#include <QPainter>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QMimeData>
#include <QClipboard>
#include <QStandardPaths>
#include <QDir>
#include "CanvasContainer.hpp"
#include "DocumentSerializer.hpp"
#include "NewCanvasDialog.hpp"
#include "PreferencesDialog.hpp"
#include "UndoCommands.hpp"
#include "TextSettingsWidget.hpp"
#include "AiAssistantDockWidget.hpp"
#include "ThreeDSettingsWidget.hpp"
#include "FilterDialogs.hpp"
#include "DisplayFilterManager.hpp"
#include "PreviewWindow.hpp"
#include "ReferenceImageWindow.hpp"

namespace Kaburagi {

static QIcon createFlipHorizontalIcon() {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 中央の軸線（破線）
    p.setPen(QPen(QColor(110, 110, 110), 1, Qt::DashLine));
    p.drawLine(8, 1, 8, 15);

    // 左側の三角形
    QPolygonF polyL;
    polyL << QPointF(2, 8) << QPointF(6, 3) << QPointF(6, 13);
    p.setPen(QPen(QColor(40, 80, 160), 1));
    p.setBrush(QColor(70, 130, 220));
    p.drawPolygon(polyL);

    // 右側の三角形 (反転鏡像)
    QPolygonF polyR;
    polyR << QPointF(14, 8) << QPointF(10, 3) << QPointF(10, 13);
    p.setPen(QPen(QColor(40, 80, 160), 1));
    p.setBrush(QColor(140, 180, 240));
    p.drawPolygon(polyR);

    return QIcon(pm);
}

static QIcon createSwitchBgColorIcon() {
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // 奥の四角形 (第2背景色)
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.setBrush(QColor(220, 220, 220));
    p.drawRect(5, 1, 9, 9);

    // 手前の四角形 (第1背景色)
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.setBrush(QColor(255, 255, 255));
    p.drawRect(1, 5, 9, 9);

    // 矢印（切り替えを表す⇄）
    p.setPen(QPen(QColor(30, 80, 180), 1.5));
    p.drawLine(10, 14, 15, 14);
    p.drawLine(13, 12, 15, 14);
    p.drawLine(13, 16, 15, 14);

    return QIcon(pm);
}

static QString toolTypeName(ToolType tool) {
    switch (tool) {
    case ToolType::Pen: return "Pen";
    case ToolType::Airbrush: return "Airbrush";
    case ToolType::Watercolor: return "Watercolor";
    case ToolType::Marker: return "Marker";
    case ToolType::Eraser: return "Eraser";
    case ToolType::Bucket: return "Bucket";
    case ToolType::Blur: return "Blur";
    case ToolType::Blend: return "Blend";
    case ToolType::VectorControlPoint: return "VectorControlPoint";
    case ToolType::VectorPressure: return "VectorPressure";
    case ToolType::VectorLineEdit: return "VectorLineEdit";
    case ToolType::VectorColorChange: return "VectorColorChange";
    case ToolType::VectorEllipse: return "VectorEllipse";
    case ToolType::VectorCircle: return "VectorCircle";
    case ToolType::VectorPolygon: return "VectorPolygon";
    case ToolType::TextBalloon: return "TextBalloon";
    case ToolType::Pipette: return "Pipette";
    case ToolType::RectSelect: return "RectSelect";
    case ToolType::EllipseSelect: return "EllipseSelect";
    case ToolType::LassoSelect: return "LassoSelect";
    case ToolType::ColorSelect: return "ColorSelect";
    case ToolType::MagicWand: return "MagicWand";
    case ToolType::Gradient: return "Gradient";
    case ToolType::CompositeBrush: return "CompositeBrush";
    default: return "Unknown";
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("PaintSoft KABURAGI 3.0 (SAI Style)");
    resize(1280, 850);

    // デフォルトドキュメント作成 (1600x1200, 350dpi)
    m_document = new Document(1600, 1200, 350.0, this);

    setupLayout();
    createMenuBar();
    createQuickToolBar();
    createStatusBar();
    setupDocument(m_document);
    loadSettings();

    // ユーザーリクエスト: 矩形選択ツールを選択状態にする
    if (m_toolBox) {
        m_toolBox->setCurrentTool(ToolType::RectSelect);
    }
    onToolChanged(ToolType::RectSelect);

    // グローバルイベントフィルタ登録 (ショートカットキー用)
    qApp->installEventFilter(this);

    // ドラッグ＆ドロップの有効化
    setAcceptDrops(true);

    // 自動バックアップタイマー初期化
    connect(&m_autoBackupTimer, &QTimer::timeout, this, &MainWindow::onAutoBackupTimer);
    restartAutoBackupTimer();

    // 最近使ったファイル履歴の読み込み
    QSettings histSettings;
    m_recentFiles = histSettings.value("RecentFiles/list").toStringList();
    updateRecentFilesMenu();

    // SAI風のクラシックUIスタイルシート (Windows 2000 / XP風のすっきりした配色)
    setStyleSheet(
        "QMainWindow { background: #ece9d8; }"
        "QMenuBar { background: #ece9d8; font-size: 11px; }"
        "QMenuBar::item:selected { background: #316ac5; color: white; }"
        "QToolBar { background: #ece9d8; border-bottom: 1px solid #999999; spacing: 4px; padding: 2px; }"
        "QStatusBar { background: #ece9d8; font-size: 11px; border-top: 1px solid #b0b0b0; }"
        "QLabel { font-size: 11px; color: #000000; }"
        "QComboBox { font-size: 11px; background: white; border: 1px solid #7f9db9; padding: 1px 4px; }"
        "QSlider::groove:horizontal { height: 4px; background: #c0c0c0; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #5080c0; width: 12px; margin: -4px 0; border-radius: 6px; }"
        "QToolBar QToolButton { background: #f0f0f0; border: 1px solid #a0a0a0; border-radius: 3px; padding: 2px 8px; font-size: 11px; margin: 0 2px; }"
        "QToolBar QToolButton:hover { background: #e0e8ff; border-color: #3060b0; }"
        "QToolBar QToolButton:pressed { background: #c0d0f0; }"
        "QToolBar QToolButton:disabled { background: #ece9d8; color: #999999; border-color: #c0c0c0; }"
        "QDockWidget { font-size: 11px; font-weight: bold; color: #000000; }"
        "QDockWidget::title { background: #dcd7ca; text-align: left; padding: 3px 6px; border: 1px solid #b0b0b0; }"
        "QDockWidget::close-button, QDockWidget::float-button { border: 1px solid transparent; background: transparent; padding: 1px; }"
        "QDockWidget::close-button:hover, QDockWidget::float-button:hover { background: #e0e8ff; border: 1px solid #3060b0; }"
        "QColorDialog QSpinBox { min-width: 68px; min-height: 22px; font-size: 11px; padding-left: 3px; padding-right: 18px; border: 1px solid #7f9db9; border-radius: 2px; background-color: white; }"
        "QColorDialog QSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 15px; border-left: 1px solid #c0c0c0; }"
        "QColorDialog QSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 15px; border-left: 1px solid #c0c0c0; }"
        "QColorDialog QSpinBox QLineEdit { min-width: 44px; font-size: 11px; padding: 0px 1px; background: transparent; }"
        "QColorDialog QLabel { font-size: 11px; }"
        "QColorDialog QLineEdit { min-width: 65px; min-height: 22px; font-size: 11px; padding: 1px 3px; }"
        "QColorDialog QPushButton { min-width: 70px; padding: 3px 10px; font-size: 11px; }"
    );
}

MainWindow::~MainWindow() {
    m_isRestoringSettings = true;
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void MainWindow::createMenuBar() {
    auto menu = menuBar();

    auto fileMenu = menu->addMenu("ファイル(&F)");
    fileMenu->addAction("新規キャンバス(&N)...", QKeySequence::New, this, &MainWindow::onNewCanvas);
    fileMenu->addAction("開く(&O)...", QKeySequence::Open, this, &MainWindow::onOpenCanvas);
    m_recentFilesMenu = fileMenu->addMenu("最近使ったファイル(&R)");
    fileMenu->addSeparator();
    fileMenu->addAction("保存(&S)", QKeySequence::Save, this, &MainWindow::onSaveCanvas);
    fileMenu->addAction("名前を付けて保存(&A)...", QKeySequence::SaveAs, this, &MainWindow::onSaveAsCanvas);
    fileMenu->addAction("閉じる(&C)", QKeySequence::Close, this, &MainWindow::onCloseCanvas);
    fileMenu->addSeparator();
    fileMenu->addAction("終了(&X)", QKeySequence::Quit, this, &QWidget::close);

    auto editMenu = menu->addMenu("編集(&E)");
    m_undoAction = editMenu->addAction("元に戻す(&U)");
    m_undoAction->setShortcut(QKeySequence::Undo);
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);

    m_redoAction = editMenu->addAction("やり直し(&R)");
    m_redoAction->setShortcuts({ QKeySequence::Redo, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z) });
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);

    editMenu->addSeparator();
    m_cutAction = editMenu->addAction("切り取り(&T)");
    m_cutAction->setShortcut(QKeySequence::Cut);
    connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);

    m_copyAction = editMenu->addAction("コピー(&C)");
    m_copyAction->setShortcut(QKeySequence::Copy);
    connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);

    m_copyVisibleAction = editMenu->addAction("可視部をコピー(&V)");
    m_copyVisibleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_copyVisibleAction, &QAction::triggered, this, &MainWindow::onCopyVisible);

    m_pasteAction = editMenu->addAction("貼り付け(&P)");
    m_pasteAction->setShortcut(QKeySequence::Paste);
    connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPaste);

    editMenu->addSeparator();
    m_deletePixelsAction = editMenu->addAction("ピクセルデータを削除(&D)", this, &MainWindow::onDeletePixels);
    m_deletePixelsAction->setShortcut(QKeySequence(Qt::Key_Delete));
    m_deletePixelsAction->setEnabled(false);

    editMenu->addSeparator();
    m_transformAction = editMenu->addAction("変形(&T)");
    m_transformAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_transformAction, &QAction::triggered, this, [this]() {
        if (m_toolBox) {
            m_toolBox->setCurrentTool(ToolType::Transform);
        }
        if (m_canvasWidget) {
            if (!m_canvasWidget->isTransforming()) {
                m_canvasWidget->startTransform();
            }
        }
    });

    connect(editMenu, &QMenu::aboutToShow, this, &MainWindow::updateEditActions);
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateEditActions);

    editMenu->addSeparator();
    auto actPref = editMenu->addAction("環境設定(&P)...", this, &MainWindow::onPreferences);
    actPref->setMenuRole(QAction::PreferencesRole);

    menu->addMenu("キャンバス(&C)");
    auto layerMenu = menu->addMenu("レイヤー(&L)");
    m_newRasterLayerAction = layerMenu->addAction("新規通常レイヤー(&N)", this, &MainWindow::onNewRasterLayer);
    m_newRasterLayerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));

    m_newVectorLayerAction = layerMenu->addAction("新規ベクトルレイヤー(&V)", this, &MainWindow::onNewVectorLayer);
    m_newTextLayerAction = layerMenu->addAction("新規テキストレイヤー(&T)", this, &MainWindow::onNewTextLayer);
    m_newGroupLayerAction = layerMenu->addAction("新規レイヤーグループ(&G)", this, &MainWindow::onNewGroupLayer);
    m_newPanelFolderAction = layerMenu->addAction("新規コマフォルダ(&F)", this, &MainWindow::onNewPanelFolder);
    m_newThreeDLayerAction = layerMenu->addAction("新規3Dレイヤー(&3)", this, &MainWindow::onNewThreeDLayer);
    m_newAdjustmentLayerAction = layerMenu->addAction("新規色調補正レイヤー(&J)", this, [this]() {
        if (m_document) m_document->addAdjustmentLayer(AdjustmentType::BrightnessContrast);
    });
    m_newLayerFromVisibleAction = layerMenu->addAction("可視部をレイヤーに(&E)", this, &MainWindow::onNewLayerFromVisible);

    layerMenu->addSeparator();

    m_duplicateLayerAction = layerMenu->addAction("レイヤーを複製(&D)", this, &MainWindow::onDuplicateLayer);
    m_mergeDownAction = layerMenu->addAction("下のレイヤーと結合(&M)", this, &MainWindow::onMergeDown);
    m_deleteLayerAction = layerMenu->addAction("レイヤーを削除(&R)", this, &MainWindow::onDeleteLayer);

    layerMenu->addSeparator();

    m_selectOpaqueAction = layerMenu->addAction("不透明部分を選択範囲にする(&O)", this, &MainWindow::onSelectOpaque);
    m_addOpaqueToSelectionAction = layerMenu->addAction("不透明部分を選択範囲に加える(&A)", this, &MainWindow::onAddOpaqueToSelection);

    layerMenu->addSeparator();

    m_rasterizeLayerAction = layerMenu->addAction("レイヤーをラスタライズ(&Z)", this, &MainWindow::onRasterizeLayer);
    m_fillLayerAction = layerMenu->addAction("レイヤーを描画色で塗りつぶす(&F)", this, &MainWindow::onFillLayer);
    m_fillLayerAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));

    connect(layerMenu, &QMenu::aboutToShow, this, &MainWindow::updateLayerMenuActions);
    updateLayerMenuActions();

    // 選択(&S) メニュー (GIMP互換: すべて選択 / 選択解除 / 反転)
    auto selectMenu = menu->addMenu("選択(&S)");
    auto actSelectAll = selectMenu->addAction("すべて選択(&A)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->selectAll();
    });
    actSelectAll->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));

    auto actClearSel = selectMenu->addAction("選択を解除(&D)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->clearSelection();
    });
    actClearSel->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_D), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A) });

    auto actInvertSel = selectMenu->addAction("選択範囲の反転(&I)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->invertSelection();
    });
    actInvertSel->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));

    auto filterMenu = menu->addMenu("フィルター(&T)");
    filterMenu->addAction("焦点ぼかし(&F)...", this, &MainWindow::onFilterFocusBlur);
    filterMenu->addAction("ガウスぼかし(&G)...", this, &MainWindow::onFilterGaussianBlur);
    filterMenu->addAction("モーションぼかし(&M)...", this, &MainWindow::onFilterMotionBlur);
    filterMenu->addSeparator();
    filterMenu->addAction("明るさ- コントラスト(&B)...", this, &MainWindow::onFilterBrightnessContrast);
    filterMenu->addAction("色相/彩度/明度(&H)...", this, &MainWindow::onFilterHueSaturation);
    filterMenu->addAction("トーンカーブ(&C)...", this, &MainWindow::onFilterToneCurve);
    filterMenu->addAction("グラデーションマップ(&P)...", this, &MainWindow::onFilterGradientMap);


    auto viewMenu = menu->addMenu("ビュー(&V)");

    // 表示フィルター
    auto displayFilterMenu = viewMenu->addMenu("表示フィルター(&F)");
    m_displayFilterActionGroup = new QActionGroup(this);
    m_displayFilterActionGroup->setExclusive(true);

    m_filterNoneAction = displayFilterMenu->addAction("フィルター無し(&N)", this, []() {
        DisplayFilterManager::instance().setFilterType(DisplayFilterType::None);
    });
    m_filterNoneAction->setCheckable(true);
    m_displayFilterActionGroup->addAction(m_filterNoneAction);

    m_filterGrayscaleAction = displayFilterMenu->addAction("グレースケール(&G)", this, []() {
        DisplayFilterManager::instance().setFilterType(DisplayFilterType::Grayscale);
    });
    m_filterGrayscaleAction->setCheckable(true);
    m_displayFilterActionGroup->addAction(m_filterGrayscaleAction);

    m_filterGrayscaleYiqAction = displayFilterMenu->addAction("グレースケール (YIQ)(&Y)", this, []() {
        DisplayFilterManager::instance().setFilterType(DisplayFilterType::GrayscaleYIQ);
    });
    m_filterGrayscaleYiqAction->setCheckable(true);
    m_displayFilterActionGroup->addAction(m_filterGrayscaleYiqAction);

    m_filterIccAction = displayFilterMenu->addAction("ICCプロファイル(&I)", this, []() {
        DisplayFilterManager::instance().setFilterType(DisplayFilterType::IccProfile);
    });
    m_filterIccAction->setCheckable(true);
    m_displayFilterActionGroup->addAction(m_filterIccAction);

    connect(&DisplayFilterManager::instance(), &DisplayFilterManager::filterChanged, this, &MainWindow::updateDisplayFilterActions);
    updateDisplayFilterActions();

    viewMenu->addSeparator();

    // GIMPスタイル: 選択範囲の境界線表示・非表示トグル (Ctrl+T / Ctrl+H)
    auto actShowSelection = viewMenu->addAction("選択範囲の境界線を表示(&S)", this, [this](bool checked) {
        if (m_canvasWidget) m_canvasWidget->setShowSelection(checked);
    });
    actShowSelection->setCheckable(true);
    actShowSelection->setChecked(true);
    actShowSelection->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_T), QKeySequence(Qt::CTRL | Qt::Key_H) });

    viewMenu->addSeparator();

    viewMenu->addAction("時計回りに90°回転", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setRotationAngle(m_canvasWidget->rotationAngle() + 90.0);
    });
    viewMenu->addAction("反時計回りに90°回転", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setRotationAngle(m_canvasWidget->rotationAngle() - 90.0);
    });
    auto actRotCW15 = viewMenu->addAction("時計回りに15°回転", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setRotationAngle(m_canvasWidget->rotationAngle() + 15.0);
    });
    actRotCW15->setShortcut(QKeySequence(Qt::Key_End));

    auto actRotCCW15 = viewMenu->addAction("反時計回りに15°回転", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setRotationAngle(m_canvasWidget->rotationAngle() - 15.0);
    });
    actRotCCW15->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_End));

    auto actResetRot = viewMenu->addAction("回転をリセット (0°)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setRotationAngle(0.0);
    });
    actResetRot->setShortcut(QKeySequence(Qt::Key_Home));

    viewMenu->addSeparator();

    m_flipHAction = viewMenu->addAction("左右反転表示", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->toggleFlipHorizontally();
    });
    m_flipHAction->setCheckable(true);
    m_flipHAction->setShortcut(QKeySequence(Qt::Key_H));

    viewMenu->addAction("表示位置・回転をリセット (100%)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->resetTransform();
    });
    viewMenu->addAction("画面サイズに合わせる (全体表示)", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->fitToView();
    });

    viewMenu->addSeparator();

    auto actZoomIn = viewMenu->addAction("ズームイン", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setZoom(m_canvasWidget->zoom() * 1.2);
    });
    actZoomIn->setShortcuts({ QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal) });

    auto actZoomOut = viewMenu->addAction("ズームアウト", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setZoom(m_canvasWidget->zoom() / 1.2);
    });
    actZoomOut->setShortcuts({ QKeySequence::ZoomOut, QKeySequence(Qt::CTRL | Qt::Key_Minus) });

    auto actZoom100 = viewMenu->addAction("100%表示", this, [this]() {
        if (m_canvasWidget) m_canvasWidget->setZoom(1.0);
    });
    actZoom100->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu->addSeparator();
    m_previewWindowAction = viewMenu->addAction(tr("プレビューウィンドウ(&P)..."), this, &MainWindow::onTogglePreviewWindow);
    m_previewWindowAction->setCheckable(true);
    m_referenceImageWindowAction = viewMenu->addAction(tr("参考画像ウィンドウ(&R)..."), this, &MainWindow::onToggleReferenceImageWindow);
    m_referenceImageWindowAction->setCheckable(true);

    // ウィンドウ(&W) メニュー (ドッキング・フローティング切替)
    auto windowMenu = menu->addMenu("ウィンドウ(&W)");
    windowMenu->addAction(m_previewWindowAction);
    windowMenu->addAction(m_referenceImageWindowAction);
    windowMenu->addSeparator();

    auto navMenu = windowMenu->addMenu("ナビゲーション＆レイヤー");
    navMenu->addAction(m_navLayerDock->toggleViewAction());
    navMenu->addSeparator();
    navMenu->addAction("左側にドッキング", this, [this]() {
        m_navLayerDock->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, m_navLayerDock);
        m_navLayerDock->show();
        saveSettings();
    });
    navMenu->addAction("右側にドッキング", this, [this]() {
        m_navLayerDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_navLayerDock);
        m_navLayerDock->show();
        saveSettings();
    });
    navMenu->addAction("フローティングウィンドウにする", this, [this]() {
        m_navLayerDock->setFloating(true);
        m_navLayerDock->show();
        saveSettings();
    });

    auto colorMenu = windowMenu->addMenu("色選択＆ブラシ設定");
    colorMenu->addAction(m_colorBrushDock->toggleViewAction());
    colorMenu->addSeparator();
    colorMenu->addAction("左側にドッキング", this, [this]() {
        m_colorBrushDock->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, m_colorBrushDock);
        m_colorBrushDock->show();
        saveSettings();
    });
    colorMenu->addAction("右側にドッキング", this, [this]() {
        m_colorBrushDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_colorBrushDock);
        m_colorBrushDock->show();
        saveSettings();
    });
    colorMenu->addAction("フローティングウィンドウにする", this, [this]() {
        m_colorBrushDock->setFloating(true);
        m_colorBrushDock->show();
        saveSettings();
    });

    auto aiMenu = windowMenu->addMenu("AI アシスタント");
    aiMenu->addAction(m_aiAssistantDock->toggleViewAction());
    aiMenu->addSeparator();
    aiMenu->addAction("右側にドッキング", this, [this]() {
        m_aiAssistantDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_aiAssistantDock);
        m_aiAssistantDock->show();
        saveSettings();
    });
    aiMenu->addAction("左側にドッキング", this, [this]() {
        m_aiAssistantDock->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, m_aiAssistantDock);
        m_aiAssistantDock->show();
        saveSettings();
    });
    aiMenu->addAction("フローティングウィンドウにする", this, [this]() {
        m_aiAssistantDock->setFloating(true);
        m_aiAssistantDock->show();
        saveSettings();
    });

    windowMenu->addSeparator();
    windowMenu->addAction("レイアウトを初期状態にリセット", this, &MainWindow::resetDockLayout);

    menu->addMenu("その他(&O)");

    // ウィンドウ右上端に "選択範囲編集" トグルボタンを追加
    m_btnToggleSelectionEdit = new QToolButton(this);
    m_btnToggleSelectionEdit->setText(tr("選択範囲編集"));
    m_btnToggleSelectionEdit->setCheckable(true);
    m_btnToggleSelectionEdit->setToolTip(tr("選択範囲編集モードのON/OFF\nON時は通常描画ツールで選択範囲を編集できます（クイックマスク）"));
    m_btnToggleSelectionEdit->setStyleSheet(
        "QToolButton {"
        "  padding: 3px 8px;"
        "  font-weight: bold;"
        "  border: 1px solid #777;"
        "  border-radius: 3px;"
        "  background-color: #e0e0e0;"
        "}"
        "QToolButton:checked {"
        "  background-color: #2a75d3;"
        "  color: white;"
        "  border: 1px solid #1a5ab3;"
        "}"
        "QToolButton:hover {"
        "  background-color: #d0d0d0;"
        "}"
        "QToolButton:checked:hover {"
        "  background-color: #3882e0;"
        "}"
    );
    connect(m_btnToggleSelectionEdit, &QToolButton::toggled, this, &MainWindow::onToggleSelectionEdit);
    menu->setCornerWidget(m_btnToggleSelectionEdit, Qt::TopRightCorner);
}

void MainWindow::createQuickToolBar() {
    auto tb = addToolBar("クイックツールバー");
    tb->setObjectName("QuickToolBar");
    tb->setMovable(false);

    // 手ブレ補正セレクタ (SAI風: 0, 1, 2, ..., S-1, ..., S-7)
    tb->addWidget(new QLabel(" 手ブレ補正: ", this));
    m_smoothCombo = new QComboBox(this);
    m_smoothCombo->addItem("OFF (0)", 0);
    for (int i = 1; i <= 8; ++i) {
        m_smoothCombo->addItem(QString::number(i), i);
    }
    m_smoothCombo->addItem("S-1", 9);
    m_smoothCombo->addItem("S-2", 10);
    m_smoothCombo->addItem("S-3", 11);
    m_smoothCombo->addItem("S-4", 12);
    m_smoothCombo->addItem("S-5", 13);
    m_smoothCombo->addItem("S-6", 14);
    m_smoothCombo->addItem("S-7", 15);
    m_smoothCombo->setCurrentIndex(3); // デフォルト3
    tb->addWidget(m_smoothCombo);

    tb->addSeparator();

    // 左右反転表示トグルボタン (手ブレ補正と元に戻すの間に配置)
    m_btnFlipH = new QToolButton(this);
    m_btnFlipH->setCheckable(true);
    m_btnFlipH->setText("反転");
    m_btnFlipH->setToolTip("左右反転表示 (H)");
    m_btnFlipH->setIcon(createFlipHorizontalIcon());
    m_btnFlipH->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnFlipH->setStyleSheet(
        "QToolButton { font-size: 11px; padding: 2px 6px; border: 1px solid #c0c0c0; border-radius: 2px; background: #f0f0f0; }"
        "QToolButton:hover { background: #e0e8f5; border-color: #7090c0; }"
        "QToolButton:checked { background: #cce0ff; border-color: #3377dd; font-weight: bold; }"
    );
    connect(m_btnFlipH, &QToolButton::clicked, this, [this]() {
        if (m_canvasWidget) {
            m_canvasWidget->toggleFlipHorizontally();
        }
    });
    tb->addWidget(m_btnFlipH);

    // 第1背景色⇔第2背景色 切り替えボタン (左右反転表示ボタンの右隣り)
    m_btnSwitchBgColor = new QToolButton(this);
    m_btnSwitchBgColor->setText("背景切替");
    m_btnSwitchBgColor->setToolTip("第1背景色⇔第2背景色 切り替え");
    m_btnSwitchBgColor->setIcon(createSwitchBgColorIcon());
    m_btnSwitchBgColor->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_btnSwitchBgColor->setStyleSheet(
        "QToolButton { font-size: 11px; padding: 2px 6px; border: 1px solid #c0c0c0; border-radius: 2px; background: #f0f0f0; }"
        "QToolButton:hover { background: #e0e8f5; border-color: #7090c0; }"
        "QToolButton:pressed { background: #cce0ff; }"
    );
    connect(m_btnSwitchBgColor, &QToolButton::clicked, this, [this]() {
        if (m_document) {
            m_document->switchBackgroundColor();
            QString activeName = m_document->isSecondaryBgColorActive() ? "第2背景色" : "第1背景色";
            QColor curColor = m_document->isSecondaryBgColorActive() ? m_document->secondaryBackgroundColor() : m_document->primaryBackgroundColor();
            statusBar()->showMessage(QString("背景色を切り替えました: %1 (%2)").arg(activeName).arg(curColor.name()), 3000);
        }
    });
    tb->addWidget(m_btnSwitchBgColor);

    tb->addSeparator();

    // 元に戻す / やり直し ボタン (手振れ補正の右側・キャンバス直上付近に配置)
    tb->addAction(m_undoAction);
    tb->addAction(m_redoAction);

    connect(m_smoothCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSmoothLevelChanged);
}

void MainWindow::createStatusBar() {
    auto sb = statusBar();
    m_statusMemoryLabel = new QLabel("メモリ負荷: 0.1% (タイルプール待機中 / ゼロアロケーション)", this);
    sb->addPermanentWidget(m_statusMemoryLabel);
}

void MainWindow::setupLayout() {
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks);
    setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // --- ドック 1: ナビゲーター & レイヤーパネル ---
    m_navLayerDock = new QDockWidget("ナビゲーション / レイヤー", this);
    m_navLayerDock->setObjectName("NavLayerDock");
    m_navLayerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto leftCol1 = new QWidget(m_navLayerDock);
    leftCol1->setMinimumWidth(185);
    auto leftLayout1 = new QVBoxLayout(leftCol1);
    leftLayout1->setContentsMargins(2, 2, 2, 2);
    leftLayout1->setSpacing(2);

    m_navSplitter = new QSplitter(Qt::Vertical, leftCol1);
    m_navSplitter->setChildrenCollapsible(false);
    m_navSplitter->setHandleWidth(5);
    m_navSplitter->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background: #d4d4d4;"
        "  border-top: 1px solid #b0b0b0;"
        "  border-bottom: 1px solid #f0f0f0;"
        "  height: 5px;"
        "}"
        "QSplitter::handle:vertical:hover {"
        "  background: #5080df;"
        "}"
    );

    m_navigator = new NavigatorWidget(this);
    m_navigator->setDocument(m_document);
    m_navSplitter->addWidget(m_navigator);

    m_layerPanel = new LayerPanelWidget(this);
    m_layerPanel->setDocument(m_document);
    m_navSplitter->addWidget(m_layerPanel);

    m_navSplitter->setStretchFactor(0, 0); // ナビゲーター
    m_navSplitter->setStretchFactor(1, 1); // レイヤーパネル
    m_navSplitter->setSizes({ 205, 300 });

    leftLayout1->addWidget(m_navSplitter, 1);

    m_navLayerDock->setWidget(leftCol1);
    addDockWidget(Qt::LeftDockWidgetArea, m_navLayerDock);

    // --- ドック 2: カラーサークル & ツールボックス & ブラシ設定 ---
    m_colorBrushDock = new QDockWidget("色選択 / ツール / ブラシ設定", this);
    m_colorBrushDock->setObjectName("ColorBrushDock");
    m_colorBrushDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto leftCol2 = new QWidget(m_colorBrushDock);
    leftCol2->setMinimumWidth(205);
    auto leftLayout2 = new QVBoxLayout(leftCol2);
    leftLayout2->setContentsMargins(2, 2, 2, 2);
    leftLayout2->setSpacing(2);

    // カラーホイールと右側ボタン（パレットトグル、色選択）を行配置
    auto wheelRowWidget = new QWidget(leftCol2);
    auto wheelRowLayout = new QHBoxLayout(wheelRowWidget);
    wheelRowLayout->setContentsMargins(0, 0, 0, 0);
    wheelRowLayout->setSpacing(3);

    m_colorWheel = new ColorWheelWidget(wheelRowWidget);
    wheelRowLayout->addWidget(m_colorWheel, 1);

    auto wheelBtnCol = new QWidget(wheelRowWidget);
    auto wheelBtnLayout = new QVBoxLayout(wheelBtnCol);
    wheelBtnLayout->setContentsMargins(0, 2, 0, 2);
    wheelBtnLayout->setSpacing(4);

    m_btnTogglePalette = new QPushButton("パレット", wheelBtnCol);
    m_btnTogglePalette->setCheckable(true);
    m_btnTogglePalette->setFixedHeight(24);
    m_btnTogglePalette->setToolTip("カラーパレットの表示/非表示を切り替え");
    m_btnTogglePalette->setStyleSheet(
        "QPushButton { font-size: 11px; padding: 2px 4px; background: #f0f0f0; border: 1px solid #a0a0a0; border-radius: 2px; }"
        "QPushButton:hover { background: #e0e8ff; border-color: #3060b0; }"
        "QPushButton:checked { background: #316ac5; color: white; border-color: #104090; font-weight: bold; }"
    );

    m_btnPickColorDialog = new QPushButton("色選択", wheelBtnCol);
    m_btnPickColorDialog->setFixedHeight(24);
    m_btnPickColorDialog->setToolTip("色選択ダイアログを開く");
    m_btnPickColorDialog->setStyleSheet(
        "QPushButton { font-size: 11px; padding: 2px 4px; background: #f0f0f0; border: 1px solid #a0a0a0; border-radius: 2px; }"
        "QPushButton:hover { background: #e0e8ff; border-color: #3060b0; }"
        "QPushButton:pressed { background: #c0d0f0; }"
    );

    wheelBtnLayout->addWidget(m_btnTogglePalette);
    wheelBtnLayout->addWidget(m_btnPickColorDialog);
    wheelBtnLayout->addStretch();

    wheelRowLayout->addWidget(wheelBtnCol, 0);
    leftLayout2->addWidget(wheelRowWidget);

    // パレットウィジェット（色選択円の下に横12 x 縦12のパレットを表示、初期はトグルOFFで非表示）
    m_paletteWidget = new ColorPaletteWidget(leftCol2);
    m_paletteWidget->setVisible(false);
    leftLayout2->addWidget(m_paletteWidget);

    // スプリッター: ツールのテーブル (m_toolBox) と 各ツールの設定ウィジェット (m_toolSettingsStack) の間をユーザーが調整可能に
    m_toolSplitter = new QSplitter(Qt::Vertical, leftCol2);
    m_toolSplitter->setChildrenCollapsible(false);
    m_toolSplitter->setHandleWidth(5);
    m_toolSplitter->setStyleSheet(
        "QSplitter::handle:vertical {"
        "  background: #d4d4d4;"
        "  border-top: 1px solid #b0b0b0;"
        "  border-bottom: 1px solid #f0f0f0;"
        "  height: 5px;"
        "}"
        "QSplitter::handle:vertical:hover {"
        "  background: #5080df;"
        "}"
    );

    m_toolBox = new ToolBoxWidget(this);
    m_toolSplitter->addWidget(m_toolBox);

    m_brushSettings = new BrushSettingsWidget(this);
    m_rectSelectSettings = new RectSelectSettingsWidget(this);
    m_ellipseSelectSettings = new EllipseSelectSettingsWidget(this);
    m_lassoSelectSettings = new LassoSelectSettingsWidget(this);
    m_colorSelectSettings = new ColorSelectSettingsWidget(this);
    m_bucketSettings = new BucketSettingsWidget(this);
    m_autoSelectSettings = new AutoSelectSettingsWidget(this);
    m_blurSettings = new BlurSettingsWidget(this);
    m_smudgeSettings = new SmudgeSettingsWidget(this);
    m_watercolorSettings = new WatercolorSettingsWidget(this);
    m_compositeBrushSettings = new CompositeBrushSettingsWidget(this);
    m_imageBrushSettings = new ImageBrushSettingsWidget(this);
    m_gradientSettings = new GradientSettingsWidget(this);
    m_vectorEraserSettings = new VectorEraserSettingsWidget(this);
    m_vectorControlPointSettings = new VectorControlPointSettingsWidget(this);
    m_vectorLineSettings = new VectorLineSettingsWidget(this);
    m_vectorChangeLineSettings = new VectorChangeLineSettingsWidget(this);
    m_vectorColorChangeSettings = new VectorColorChangeSettingsWidget(this);
    m_vectorEllipseSettings = new VectorEllipseSettingsWidget(this);
    m_vectorCircleSettings = new VectorCircleSettingsWidget(this);
    m_vectorPolygonSettings = new VectorPolygonSettingsWidget(this);
    m_vectorPressureSettings = new VectorPressureSettingsWidget(this);
    m_transformSettings = new TransformSettingsWidget(this);
    m_adjustmentSettings = new AdjustmentSettingsWidget(this);
    m_textSettings = new TextSettingsWidget(this);
    m_panelSettings = new PanelSettingsWidget(this);
    m_threeDSettings = new ThreeDSettingsWidget(this);

    class DynamicStackedWidget : public QStackedWidget {
    public:
        using QStackedWidget::QStackedWidget;
        QSize sizeHint() const override {
            if (currentWidget()) return currentWidget()->sizeHint();
            return QSize(160, 200);
        }
        QSize minimumSizeHint() const override {
            if (currentWidget()) return currentWidget()->minimumSizeHint();
            return QSize(80, 80);
        }
    };

    m_toolSettingsStack = new DynamicStackedWidget(this);
    connect(m_toolSettingsStack, &QStackedWidget::currentChanged, m_toolSettingsStack, &QWidget::updateGeometry);
    m_toolSettingsStack->addWidget(m_brushSettings);                  // index 0: ブラシ設定
    m_toolSettingsStack->addWidget(m_rectSelectSettings);             // index 1: 矩形選択設定
    m_toolSettingsStack->addWidget(m_ellipseSelectSettings);          // 楕円選択設定
    m_toolSettingsStack->addWidget(m_lassoSelectSettings);            // 投げ縄選択設定
    m_toolSettingsStack->addWidget(m_colorSelectSettings);            // 色域選択設定
    m_toolSettingsStack->addWidget(m_bucketSettings);                 // index 2: バケツ塗りつぶし設定
    m_toolSettingsStack->addWidget(m_autoSelectSettings);             // index 3: 自動選択設定
    m_toolSettingsStack->addWidget(m_blurSettings);                   // index 4: ぼかしツール設定
    m_toolSettingsStack->addWidget(m_smudgeSettings);                 // index 5: 指先ツール設定
    m_toolSettingsStack->addWidget(m_watercolorSettings);             // index 6: 水彩筆ツール設定
    m_toolSettingsStack->addWidget(m_compositeBrushSettings);         // index 7: 合成ブラシツール設定
    m_toolSettingsStack->addWidget(m_imageBrushSettings);             // index 8: 画像ブラシ設定
    m_toolSettingsStack->addWidget(m_gradientSettings);               // index 9: グラデーション設定
    m_toolSettingsStack->addWidget(m_vectorEraserSettings);           // index 10: ベクター消しゴム設定
    m_toolSettingsStack->addWidget(m_vectorControlPointSettings);      // index 11: ベクター制御点ツール設定
    m_toolSettingsStack->addWidget(m_vectorLineSettings);              // index 12: ベクター折れ線・曲線設定
    m_toolSettingsStack->addWidget(m_vectorChangeLineSettings);        // index 13: ベクター線変更設定
    m_toolSettingsStack->addWidget(m_vectorColorChangeSettings);       // index 14: ベクター線色変更設定
    m_toolSettingsStack->addWidget(m_vectorEllipseSettings);           // index 15: ベクター楕円設定
    m_toolSettingsStack->addWidget(m_vectorCircleSettings);            // index 16: ベクター円設定
    m_toolSettingsStack->addWidget(m_vectorPolygonSettings);           // index 17: ベクター多角形設定
    m_toolSettingsStack->addWidget(m_vectorPressureSettings);          // index 18: ベクター筆圧設定
    m_toolSettingsStack->addWidget(m_transformSettings);               // index 19: 変形ツール設定
    m_toolSettingsStack->addWidget(m_adjustmentSettings);              // index 20: 調整レイヤー設定
    m_toolSettingsStack->addWidget(m_textSettings);                    // index 21: テキスト設定
    m_toolSettingsStack->addWidget(m_panelSettings);                   // index 22: コマ設定
    m_toolSettingsStack->addWidget(m_threeDSettings);                  // index 23: 3Dモデル設定

    m_toolSettingsStack->setMinimumWidth(0);
    auto settingsScrollArea = new QScrollArea(m_toolSplitter);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setFrameShape(QFrame::NoFrame);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    settingsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    settingsScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    settingsScrollArea->setWidget(m_toolSettingsStack);
    m_toolSplitter->addWidget(settingsScrollArea);

    m_toolSplitter->setStretchFactor(0, 0); // ツールボックス側
    m_toolSplitter->setStretchFactor(1, 1); // 設定ウィジェット側
    m_toolSplitter->setSizes({ 160, 240 });

    leftLayout2->addWidget(m_toolSplitter, 1);

    m_colorBrushDock->setWidget(leftCol2);
    addDockWidget(Qt::LeftDockWidgetArea, m_colorBrushDock);

    // --- 中央: キャンバスタブ & キャンバスウィジェット ---
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::South); // キャンバスのタブは下側に配置
    m_tabWidget->setTabsClosable(true);             // 各タブの右側に閉じるボタンを追加
    m_tabWidget->setStyleSheet(
        "QTabBar::tab { font-size: 11px; height: 22px; padding: 0 8px; }"
        "QTabBar::close-button { subcontrol-position: right; margin-left: 4px; }"
    );

    // タブバー右端にも閉じるボタンを追加
    m_tabCloseBtn = new QToolButton(m_tabWidget);
    m_tabCloseBtn->setText("×");
    m_tabCloseBtn->setToolTip(tr("キャンバスを閉じる (Ctrl+W)"));
    m_tabCloseBtn->setStyleSheet(
        "QToolButton { font-size: 13px; font-weight: bold; padding: 1px 7px; border: 1px solid #aaa; border-radius: 2px; background: #e0e0e0; }"
        "QToolButton:hover { background: #e81123; color: white; border-color: #e81123; }"
    );
    m_tabWidget->setCornerWidget(m_tabCloseBtn, Qt::BottomRightCorner);

    connect(m_tabCloseBtn, &QToolButton::clicked, this, &MainWindow::onCloseCanvas);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int /*index*/) {
        onCloseCanvas();
    });

    m_canvasWidget = new CanvasWidget(this);
    m_canvasWidget->setDocument(m_document);
    m_canvasWidget->setTextSettingsWidget(m_textSettings);
    m_canvasWidget->setPanelSettingsWidget(m_panelSettings);
    m_canvasWidget->setThreeDSettingsWidget(m_threeDSettings);

    m_threeDSettings->setDocument(m_document);
    m_threeDSettings->setCanvasWidget(m_canvasWidget);
    connect(m_threeDSettings, &ThreeDSettingsWidget::settingsChanged, this, [this]() {
        if (m_document) m_document->composite();
        if (m_canvasWidget) m_canvasWidget->update();
    });

    m_canvasContainer = new CanvasContainer(m_canvasWidget, this);
    m_tabWidget->addTab(m_canvasContainer, "新規キャンバス.sai (1600x1200)");

    setCentralWidget(m_tabWidget);

    // ナビゲーターとキャンバスの相互連動
    connect(m_canvasWidget, &CanvasWidget::transformChanged, m_navigator, &NavigatorWidget::updateViewTransform);
    connect(m_navigator, &NavigatorWidget::zoomChanged, m_canvasWidget, &CanvasWidget::setZoom);
    connect(m_navigator, &NavigatorWidget::rotationChanged, m_canvasWidget, &CanvasWidget::setRotationAngle);
    connect(m_navigator, &NavigatorWidget::flipHorizontallyTriggered, m_canvasWidget, &CanvasWidget::toggleFlipHorizontally);
    connect(m_navigator, &NavigatorWidget::resetTransformTriggered, m_canvasWidget, &CanvasWidget::resetTransform);
    connect(m_navigator, &NavigatorWidget::viewCenterChanged, m_canvasWidget, &CanvasWidget::setViewCenter);

    // 左右反転トグルボタン・アクションの同期
    connect(m_canvasWidget, &CanvasWidget::transformChanged, this, [this](double, double, bool flipped, const QPointF&, const QSizeF&) {
        if (m_btnFlipH) {
            m_btnFlipH->blockSignals(true);
            m_btnFlipH->setChecked(flipped);
            m_btnFlipH->blockSignals(false);
        }
        if (m_flipHAction) {
            m_flipHAction->blockSignals(true);
            m_flipHAction->setChecked(flipped);
            m_flipHAction->blockSignals(false);
        }
    });

    // カラーホイール、ColorBoxWidget、キャンバスの相互連動
    if (m_toolBox && m_toolBox->colorBox()) {
        auto colorBox = m_toolBox->colorBox();

        // カラーホイールで色が変わったとき -> ColorBoxの現在アクティブな色を更新
        connect(m_colorWheel, &ColorWheelWidget::colorChanged, this, [this, colorBox](const QColor& color) {
            colorBox->setCurrentActiveColor(color);
            if (m_canvasWidget) {
                m_canvasWidget->setCurrentColor(colorBox->primaryColor());
                m_canvasWidget->setSecondaryColor(colorBox->secondaryColor());
            }
            if (m_paletteWidget) {
                m_paletteWidget->setCurrentColor(colorBox->primaryColor());
            }
            saveSettings();
        });

        // ColorBoxの描画色が変更されたとき -> キャンバスに反映
        connect(colorBox, &ColorBoxWidget::primaryColorChanged, this, [this](const QColor& color) {
            if (m_canvasWidget) {
                m_canvasWidget->setCurrentColor(color);
            }
            if (m_paletteWidget) {
                m_paletteWidget->setCurrentColor(color);
            }
            saveSettings();
        });

        // ColorBoxの背景色が変更されたとき -> キャンバスに反映
        connect(colorBox, &ColorBoxWidget::secondaryColorChanged, this, [this](const QColor& color) {
            if (m_canvasWidget) {
                m_canvasWidget->setSecondaryColor(color);
            }
            saveSettings();
        });

        // ColorBoxの編集対象色が変わったとき -> カラーホイールに反映
        connect(colorBox, &ColorBoxWidget::activeColorChanged, this, [this](const QColor& color) {
            if (m_colorWheel) {
                m_colorWheel->setColor(color);
            }
        });

        // キャンバスでスポイト（Pipette）されたとき -> ColorBoxとカラーホイールに反映
        connect(m_canvasWidget, &CanvasWidget::colorPicked, this, [this, colorBox](const QColor& color) {
            colorBox->setCurrentActiveColor(color);
            if (m_colorWheel) {
                m_colorWheel->setColor(color);
            }
            if (m_canvasWidget) {
                m_canvasWidget->setCurrentColor(colorBox->primaryColor());
            }
            if (m_paletteWidget) {
                m_paletteWidget->setCurrentColor(colorBox->primaryColor());
            }
            saveSettings();
        });

        // [X] キーで描画色・背景色入れ替えショートカット
        auto swapKey = new QShortcut(QKeySequence(Qt::Key_X), this);
        connect(swapKey, &QShortcut::activated, colorBox, &ColorBoxWidget::swapColors);

        // 「パレット」トグルボタンの連動（ONでパレット表示、OFFで非表示）
        connect(m_btnTogglePalette, &QPushButton::toggled, this, [this](bool checked) {
            if (m_paletteWidget) {
                m_paletteWidget->setVisible(checked);
            }
            saveSettings();
        });

        // 「色選択」ボタンの連動（色選択ダイアログを表示し、描画色を変更）
        connect(m_btnPickColorDialog, &QPushButton::clicked, this, [this, colorBox]() {
            QColor cur = m_colorWheel ? m_colorWheel->currentColor() : Qt::black;
            QColor c = QColorDialog::getColor(cur, this, tr("色選択"), QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
            if (c.isValid()) {
                if (m_colorWheel) {
                    m_colorWheel->setColor(c);
                }
                if (colorBox) {
                    colorBox->setPrimaryColor(c);
                }
                if (m_canvasWidget) {
                    m_canvasWidget->setCurrentColor(c);
                }
                if (m_paletteWidget) {
                    m_paletteWidget->setCurrentColor(c);
                }
                saveSettings();
            }
        });

        // パレットウィジェットで色クリック時（描画色を変更）
        connect(m_paletteWidget, &ColorPaletteWidget::colorSelected, this, [this, colorBox](const QColor& color) {
            if (m_colorWheel) {
                m_colorWheel->setColor(color);
            }
            if (colorBox) {
                colorBox->setPrimaryColor(color);
            }
            if (m_canvasWidget) {
                m_canvasWidget->setCurrentColor(color);
            }
            if (m_paletteWidget) {
                m_paletteWidget->setCurrentColor(color);
            }
            saveSettings();
        });
    }

    // レイヤー切り替えとツールボックスの連動 (SAI風レイヤー別ツール切り替え)
    connect(m_document, &Document::activeLayerChanged, this, &MainWindow::updateLayerTools);
    connect(m_document, &Document::layerListChanged, this, &MainWindow::updateLayerTools);
    updateLayerTools();

    // ツールボックスとキャンバスの連動
    connect(m_toolBox, &ToolBoxWidget::toolChanged, this, &MainWindow::onToolChanged);

    // 矩形選択設定とキャンバスの連動
    connect(m_rectSelectSettings, &RectSelectSettingsWidget::modeChanged, this, [this](SelectionMode mode) {
        if (m_canvasWidget) m_canvasWidget->setSelectionMode(mode);
    });
    connect(m_rectSelectSettings, &RectSelectSettingsWidget::alignToWindowChanged, this, [this](bool align) {
        if (m_canvasWidget) m_canvasWidget->setSelectionAlignToWindow(align);
    });

    // 楕円選択設定とキャンバスの連動
    connect(m_ellipseSelectSettings, &EllipseSelectSettingsWidget::modeChanged, this, [this](SelectionMode mode) {
        if (m_canvasWidget) m_canvasWidget->setEllipseSelectMode(mode);
    });
    connect(m_ellipseSelectSettings, &EllipseSelectSettingsWidget::selectMethodChanged, this, [this](EllipseSelectMethod method) {
        if (m_canvasWidget) m_canvasWidget->setEllipseSelectMethod(method);
    });
    connect(m_ellipseSelectSettings, &EllipseSelectSettingsWidget::alignToWindowChanged, this, [this](bool align) {
        if (m_canvasWidget) m_canvasWidget->setSelectionAlignToWindow(align);
    });

    // 投げ縄選択設定とキャンバスの連動
    connect(m_lassoSelectSettings, &LassoSelectSettingsWidget::modeChanged, this, [this](SelectionMode mode) {
        if (m_canvasWidget) m_canvasWidget->setLassoSelectMode(mode);
    });

    // 色域選択設定とキャンバスの連動
    connect(m_colorSelectSettings, &ColorSelectSettingsWidget::modeChanged, this, [this](SelectionMode mode) {
        if (m_canvasWidget) m_canvasWidget->setColorSelectMode(mode);
    });
    connect(m_colorSelectSettings, &ColorSelectSettingsWidget::referenceModeChanged, this, [this](ColorSelectReference ref) {
        if (m_canvasWidget) m_canvasWidget->setColorSelectReference(ref);
    });
    connect(m_colorSelectSettings, &ColorSelectSettingsWidget::toleranceChanged, this, [this](int tol) {
        if (m_canvasWidget) m_canvasWidget->setColorSelectTolerance(tol);
    });
    connect(m_colorSelectSettings, &ColorSelectSettingsWidget::selectTransparentChanged, this, [this](bool trans) {
        if (m_canvasWidget) m_canvasWidget->setColorSelectTransparent(trans);
    });

    // バケツ設定とキャンバスの連動
    connect(m_bucketSettings, &BucketSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setBucketBlendMode);
    connect(m_bucketSettings, &BucketSettingsWidget::referenceModeChanged, m_canvasWidget, &CanvasWidget::setBucketReference);
    connect(m_bucketSettings, &BucketSettingsWidget::expandPixelsChanged, m_canvasWidget, &CanvasWidget::setBucketExpandPixels);
    connect(m_bucketSettings, &BucketSettingsWidget::narrowAreaFillChanged, m_canvasWidget, &CanvasWidget::setBucketNarrowAreaFill);
    connect(m_bucketSettings, &BucketSettingsWidget::toleranceChanged, m_canvasWidget, &CanvasWidget::setBucketTolerance);

    // バケツ設定変更時の設定永続化
    connect(m_bucketSettings, &BucketSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_bucketSettings, &BucketSettingsWidget::referenceModeChanged, this, [this](BucketReference) { saveSettings(); });
    connect(m_bucketSettings, &BucketSettingsWidget::expandPixelsChanged, this, [this](int) { saveSettings(); });
    connect(m_bucketSettings, &BucketSettingsWidget::narrowAreaFillChanged, this, [this](bool) { saveSettings(); });
    connect(m_bucketSettings, &BucketSettingsWidget::toleranceChanged, this, [this](int) { saveSettings(); });

    // 自動選択設定とキャンバスの連動
    connect(m_autoSelectSettings, &AutoSelectSettingsWidget::modeChanged, this, [this](SelectionMode mode) {
        if (m_canvasWidget) m_canvasWidget->setAutoSelectMode(mode);
    });
    connect(m_autoSelectSettings, &AutoSelectSettingsWidget::referenceModeChanged, m_canvasWidget, &CanvasWidget::setAutoSelectReference);
    connect(m_autoSelectSettings, &AutoSelectSettingsWidget::expandPixelsChanged, m_canvasWidget, &CanvasWidget::setAutoSelectExpandPixels);
    connect(m_autoSelectSettings, &AutoSelectSettingsWidget::narrowAreaFillChanged, m_canvasWidget, &CanvasWidget::setAutoSelectNarrowAreaFill);
    connect(m_autoSelectSettings, &AutoSelectSettingsWidget::toleranceChanged, m_canvasWidget, &CanvasWidget::setAutoSelectTolerance);

    // ブラシ設定とキャンバスの連動
    connect(m_brushSettings, &BrushSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setBrushBlendMode);
    connect(m_brushSettings, &BrushSettingsWidget::brushSizeChanged, m_canvasWidget, &CanvasWidget::setBrushSize);
    connect(m_brushSettings, &BrushSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setMinSizeRatio);
    connect(m_brushSettings, &BrushSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setBrushDensity);
    connect(m_brushSettings, &BrushSettingsWidget::antialiasChanged, m_canvasWidget, &CanvasWidget::setAntialias);
    connect(m_brushSettings, &BrushSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setPressureSizeEnabled);
    connect(m_brushSettings, &BrushSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setPressureDensityEnabled);
    connect(m_brushSettings, &BrushSettingsWidget::hardnessChanged, m_canvasWidget, &CanvasWidget::setBrushHardness);
    connect(m_brushSettings, &BrushSettingsWidget::falloffChanged, m_canvasWidget, &CanvasWidget::setBrushFalloff);
    connect(m_brushSettings, &BrushSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setBrushMinPressure);
    connect(m_brushSettings, &BrushSettingsWidget::airbrushIntervalChanged, m_canvasWidget, &CanvasWidget::setAirbrushInterval);
    connect(m_brushSettings, &BrushSettingsWidget::scatterChanged, m_canvasWidget, &CanvasWidget::setBrushScatter);
    connect(m_brushSettings, &BrushSettingsWidget::scatterAmountChanged, m_canvasWidget, &CanvasWidget::setBrushScatterAmount);
    connect(m_brushSettings, &BrushSettingsWidget::scatterRangeChanged, m_canvasWidget, &CanvasWidget::setBrushScatterRange);
    connect(m_brushSettings, &BrushSettingsWidget::scatterSizeRandomChanged, m_canvasWidget, &CanvasWidget::setBrushScatterSizeRandom);
    connect(m_brushSettings, &BrushSettingsWidget::scatterScaleUpChanged, m_canvasWidget, &CanvasWidget::setBrushScatterScaleUp);

    // ブラシ設定変更時のツール別設定即時記憶・永続化
    connect(m_brushSettings, &BrushSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::brushSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::antialiasChanged, this, [this](bool) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::hardnessChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::falloffChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::airbrushIntervalChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::scatterChanged, this, [this](bool) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::scatterAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::scatterRangeChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::scatterSizeRandomChanged, this, [this](int) { saveSettings(); });
    connect(m_brushSettings, &BrushSettingsWidget::scatterScaleUpChanged, this, [this](bool) { saveSettings(); });

    // ぼかし設定とキャンバスの連動
    connect(m_blurSettings, &BlurSettingsWidget::blurSizeChanged, m_canvasWidget, &CanvasWidget::setBlurSize);
    connect(m_blurSettings, &BlurSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setBlurMinSizeRatio);
    connect(m_blurSettings, &BlurSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setBlurDensity);
    connect(m_blurSettings, &BlurSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setBlurPressureSizeEnabled);
    connect(m_blurSettings, &BlurSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setBlurPressureDensityEnabled);
    connect(m_blurSettings, &BlurSettingsWidget::colorMixChanged, m_canvasWidget, &CanvasWidget::setBlurColorMix);
    connect(m_blurSettings, &BlurSettingsWidget::colorStretchChanged, m_canvasWidget, &CanvasWidget::setBlurColorStretch);
    connect(m_blurSettings, &BlurSettingsWidget::hardnessChanged, m_canvasWidget, &CanvasWidget::setBlurHardness);
    connect(m_blurSettings, &BlurSettingsWidget::falloffChanged, m_canvasWidget, &CanvasWidget::setBlurFalloff);
    connect(m_blurSettings, &BlurSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setBlurMinPressure);

    // ぼかし設定変更時の設定永続化
    connect(m_blurSettings, &BlurSettingsWidget::blurSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::colorMixChanged, this, [this](int) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::colorStretchChanged, this, [this](int) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::hardnessChanged, this, [this](int) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::falloffChanged, this, [this](int) { saveSettings(); });
    connect(m_blurSettings, &BlurSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });

    // 指先設定とキャンバスの連動
    connect(m_smudgeSettings, &SmudgeSettingsWidget::smudgeSizeChanged, m_canvasWidget, &CanvasWidget::setSmudgeSize);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setSmudgeMinSizeRatio);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setSmudgeDensity);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::rateChanged, m_canvasWidget, &CanvasWidget::setSmudgeRate);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setSmudgePressureSizeEnabled);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setSmudgePressureDensityEnabled);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::hardnessChanged, m_canvasWidget, &CanvasWidget::setSmudgeHardness);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::falloffChanged, m_canvasWidget, &CanvasWidget::setSmudgeFalloff);
    connect(m_smudgeSettings, &SmudgeSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setSmudgeMinPressure);

    // 指先設定変更時の設定永続化
    connect(m_smudgeSettings, &SmudgeSettingsWidget::smudgeSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::rateChanged, this, [this](int) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::hardnessChanged, this, [this](int) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::falloffChanged, this, [this](int) { saveSettings(); });
    connect(m_smudgeSettings, &SmudgeSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });

    // 水彩筆設定とキャンバスの連動
    connect(m_watercolorSettings, &WatercolorSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setWatercolorBlendMode);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::watercolorSizeChanged, m_canvasWidget, &CanvasWidget::setWatercolorSize);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setWatercolorMinSizeRatio);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setWatercolorDensity);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::hardnessChanged, m_canvasWidget, &CanvasWidget::setWatercolorHardness);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::falloffChanged, m_canvasWidget, &CanvasWidget::setWatercolorFalloff);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setWatercolorMinPressure);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::strengthChanged, m_canvasWidget, &CanvasWidget::setWatercolorStrength);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::waterChanged, m_canvasWidget, &CanvasWidget::setWatercolorWater);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::colorStretchChanged, m_canvasWidget, &CanvasWidget::setWatercolorColorStretch);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::blurPressureChanged, m_canvasWidget, &CanvasWidget::setWatercolorBlurPressure);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setWatercolorPressureSizeEnabled);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setWatercolorPressureDensityEnabled);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::antialiasChanged, m_canvasWidget, &CanvasWidget::setWatercolorAntialias);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterChanged, m_canvasWidget, &CanvasWidget::setWatercolorScatter);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterAmountChanged, m_canvasWidget, &CanvasWidget::setWatercolorScatterAmount);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterRangeChanged, m_canvasWidget, &CanvasWidget::setWatercolorScatterRange);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterSizeRandomChanged, m_canvasWidget, &CanvasWidget::setWatercolorScatterSizeRandom);
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterScaleUpChanged, m_canvasWidget, &CanvasWidget::setWatercolorScatterScaleUp);

    // 水彩筆設定変更時の設定永続化
    connect(m_watercolorSettings, &WatercolorSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::watercolorSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::hardnessChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::falloffChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::strengthChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::waterChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::colorStretchChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::blurPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::antialiasChanged, this, [this](bool) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterChanged, this, [this](bool) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterRangeChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterSizeRandomChanged, this, [this](int) { saveSettings(); });
    connect(m_watercolorSettings, &WatercolorSettingsWidget::scatterScaleUpChanged, this, [this](bool) { saveSettings(); });

    // 合成ブラシ設定とキャンバスの連動
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushBlendMode);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::referenceModeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushReference);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::compositeBrushSizeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushSize);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushMinSizeRatio);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushDensity);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::hardnessChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushHardness);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::falloffChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushFalloff);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushMinPressure);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::strengthChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushStrength);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::waterChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushWater);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::colorStretchChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushColorStretch);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::blurPressureChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushBlurPressure);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushPressureSizeEnabled);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushPressureDensityEnabled);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::antialiasChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushAntialias);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushScatter);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterAmountChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushScatterAmount);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterRangeChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushScatterRange);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterSizeRandomChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushScatterSizeRandom);
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterScaleUpChanged, m_canvasWidget, &CanvasWidget::setCompositeBrushScatterScaleUp);

    // 合成ブラシ設定変更時の設定永続化
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::referenceModeChanged, this, [this](CompositeBrushReference) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::compositeBrushSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::hardnessChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::falloffChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::strengthChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::waterChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::colorStretchChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::blurPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::antialiasChanged, this, [this](bool) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterChanged, this, [this](bool) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterRangeChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterSizeRandomChanged, this, [this](int) { saveSettings(); });
    connect(m_compositeBrushSettings, &CompositeBrushSettingsWidget::scatterScaleUpChanged, this, [this](bool) { saveSettings(); });

    // 画像ブラシ設定とキャンバスの連動
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::imageSelected, this, [this](const QString&, const QImage& img) {
        if (m_canvasWidget) m_canvasWidget->setImageBrushImage(img);
        saveSettings();
    });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setImageBrushBlendMode);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::brushSizeChanged, m_canvasWidget, &CanvasWidget::setImageBrushSize);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::minSizeChanged, m_canvasWidget, &CanvasWidget::setImageBrushMinSizeRatio);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::spacingChanged, m_canvasWidget, &CanvasWidget::setImageBrushSpacing);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::densityChanged, m_canvasWidget, &CanvasWidget::setImageBrushDensity);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::minPressureChanged, m_canvasWidget, &CanvasWidget::setImageBrushMinPressure);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::pressureSizeChanged, m_canvasWidget, &CanvasWidget::setImageBrushPressureSizeEnabled);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::pressureDensityChanged, m_canvasWidget, &CanvasWidget::setImageBrushPressureDensityEnabled);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterChanged, m_canvasWidget, &CanvasWidget::setImageBrushScatter);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterAmountChanged, m_canvasWidget, &CanvasWidget::setImageBrushScatterAmount);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterRangeChanged, m_canvasWidget, &CanvasWidget::setImageBrushScatterRange);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterSizeRandomChanged, m_canvasWidget, &CanvasWidget::setImageBrushScatterSizeRandom);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterScaleUpChanged, m_canvasWidget, &CanvasWidget::setImageBrushScatterScaleUp);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::flipHChanged, m_canvasWidget, &CanvasWidget::setImageBrushFlipH);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::flipVChanged, m_canvasWidget, &CanvasWidget::setImageBrushFlipV);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::angleChanged, m_canvasWidget, &CanvasWidget::setImageBrushAngle);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::followCursorChanged, m_canvasWidget, &CanvasWidget::setImageBrushFollowCursor);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomRotateChanged, m_canvasWidget, &CanvasWidget::setImageBrushRandomRotate);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomRotateAmountChanged, m_canvasWidget, &CanvasWidget::setImageBrushRandomRotateAmount);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeChanged, m_canvasWidget, &CanvasWidget::setImageBrushRandomSize);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeAmountChanged, m_canvasWidget, &CanvasWidget::setImageBrushRandomSizeAmount);
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeScaleUpChanged, m_canvasWidget, &CanvasWidget::setImageBrushRandomSizeScaleUp);

    // 画像ブラシ設定変更時の設定永続化
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::brushSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::minSizeChanged, this, [this](float) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::spacingChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::densityChanged, this, [this](float) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::minPressureChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::pressureSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::pressureDensityChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterRangeChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterSizeRandomChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::scatterScaleUpChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::flipHChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::flipVChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::angleChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::followCursorChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomRotateChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomRotateAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeChanged, this, [this](bool) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeAmountChanged, this, [this](int) { saveSettings(); });
    connect(m_imageBrushSettings, &ImageBrushSettingsWidget::randomSizeScaleUpChanged, this, [this](bool) { saveSettings(); });

    // グラデーション設定とキャンバスの連動
    connect(m_gradientSettings, &GradientSettingsWidget::blendModeChanged, m_canvasWidget, &CanvasWidget::setGradientBlendMode);
    connect(m_gradientSettings, &GradientSettingsWidget::shapeChanged, m_canvasWidget, &CanvasWidget::setGradientShape);
    connect(m_gradientSettings, &GradientSettingsWidget::colorModeChanged, m_canvasWidget, &CanvasWidget::setGradientColorMode);
    connect(m_gradientSettings, &GradientSettingsWidget::repeatModeChanged, m_canvasWidget, &CanvasWidget::setGradientRepeatMode);
    connect(m_gradientSettings, &GradientSettingsWidget::opacityChanged, m_canvasWidget, &CanvasWidget::setGradientOpacity);
    connect(m_gradientSettings, &GradientSettingsWidget::reverseChanged, m_canvasWidget, &CanvasWidget::setGradientReverse);
    connect(m_gradientSettings, &GradientSettingsWidget::ditherChanged, m_canvasWidget, &CanvasWidget::setGradientDither);

    // グラデーション設定変更時の設定永続化
    connect(m_gradientSettings, &GradientSettingsWidget::blendModeChanged, this, [this](BlendMode) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::shapeChanged, this, [this](GradientShape) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::colorModeChanged, this, [this](GradientColorMode) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::repeatModeChanged, this, [this](GradientRepeatMode) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::opacityChanged, this, [this](float) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::reverseChanged, this, [this](bool) { saveSettings(); });
    connect(m_gradientSettings, &GradientSettingsWidget::ditherChanged, this, [this](bool) { saveSettings(); });

    // ベクター消しゴム設定とキャンバスの連動
    connect(m_vectorEraserSettings, &VectorEraserSettingsWidget::eraserModeChanged, m_canvasWidget, &CanvasWidget::setVectorEraserMode);
    connect(m_vectorEraserSettings, &VectorEraserSettingsWidget::eraserSizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setVectorEraserSize(static_cast<float>(sz));
    });
    connect(m_vectorEraserSettings, &VectorEraserSettingsWidget::eraserModeChanged, this, [this](VectorEraserMode) { saveSettings(); });
    connect(m_vectorEraserSettings, &VectorEraserSettingsWidget::eraserSizeChanged, this, [this](double) { saveSettings(); });

    // ベクター制御点ツール設定とキャンバスの連動
    connect(m_vectorControlPointSettings, &VectorControlPointSettingsWidget::modeChanged, m_canvasWidget, &CanvasWidget::setVectorControlPointMode);
    connect(m_vectorControlPointSettings, &VectorControlPointSettingsWidget::modeChanged, this, [this](VectorControlPointMode) { saveSettings(); });

    // ベクター折れ線・曲線ツール設定とキャンバスの連動
    connect(m_vectorLineSettings, &VectorLineSettingsWidget::sizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setVectorLineSize(static_cast<float>(sz));
        saveSettings();
    });
    connect(m_vectorLineSettings, &VectorLineSettingsWidget::densityChanged, this, [this](double dens) {
        if (m_canvasWidget) m_canvasWidget->setVectorLineDensity(static_cast<float>(dens));
        saveSettings();
    });
    connect(m_vectorLineSettings, &VectorLineSettingsWidget::closePathChanged, this, [this](bool closed) {
        if (m_canvasWidget) m_canvasWidget->setVectorClosePath(closed);
        saveSettings();
    });

    // ベクター線変更ツール設定とキャンバスの連動
    connect(m_vectorChangeLineSettings, &VectorChangeLineSettingsWidget::sizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setChangeLineSize(static_cast<float>(sz));
        saveSettings();
    });
    connect(m_vectorChangeLineSettings, &VectorChangeLineSettingsWidget::densityChanged, this, [this](double dens) {
        if (m_canvasWidget) m_canvasWidget->setChangeLineDensity(static_cast<float>(dens));
        saveSettings();
    });
    connect(m_vectorChangeLineSettings, &VectorChangeLineSettingsWidget::hardnessChanged, this, [this](int h) {
        if (m_canvasWidget) m_canvasWidget->setChangeLineHardness(h);
        saveSettings();
    });
    connect(m_vectorChangeLineSettings, &VectorChangeLineSettingsWidget::falloffChanged, this, [this](int f) {
        if (m_canvasWidget) m_canvasWidget->setChangeLineFalloff(f);
        saveSettings();
    });

    // ベクター楕円ツール設定とキャンバスの連携
    connect(m_vectorEllipseSettings, &VectorEllipseSettingsWidget::sizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setVectorEllipseSize(static_cast<float>(sz));
        saveSettings();
    });
    connect(m_vectorEllipseSettings, &VectorEllipseSettingsWidget::densityChanged, this, [this](double dens) {
        if (m_canvasWidget) m_canvasWidget->setVectorEllipseDensity(static_cast<float>(dens));
        saveSettings();
    });
    connect(m_vectorEllipseSettings, &VectorEllipseSettingsWidget::hardnessChanged, this, [this](int h) {
        if (m_canvasWidget) m_canvasWidget->setVectorEllipseHardness(h);
        saveSettings();
    });
    connect(m_vectorEllipseSettings, &VectorEllipseSettingsWidget::falloffChanged, this, [this](int f) {
        if (m_canvasWidget) m_canvasWidget->setVectorEllipseFalloff(f);
        saveSettings();
    });
    connect(m_vectorEllipseSettings, &VectorEllipseSettingsWidget::filledChanged, this, [this](bool filled) {
        if (m_canvasWidget) m_canvasWidget->setVectorEllipseFilled(filled);
        saveSettings();
    });

    // ベクター円ツール設定とキャンバスの連携
    connect(m_vectorCircleSettings, &VectorCircleSettingsWidget::sizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setVectorCircleSize(static_cast<float>(sz));
        saveSettings();
    });
    connect(m_vectorCircleSettings, &VectorCircleSettingsWidget::densityChanged, this, [this](double dens) {
        if (m_canvasWidget) m_canvasWidget->setVectorCircleDensity(static_cast<float>(dens));
        saveSettings();
    });
    connect(m_vectorCircleSettings, &VectorCircleSettingsWidget::hardnessChanged, this, [this](int h) {
        if (m_canvasWidget) m_canvasWidget->setVectorCircleHardness(h);
        saveSettings();
    });
    connect(m_vectorCircleSettings, &VectorCircleSettingsWidget::falloffChanged, this, [this](int f) {
        if (m_canvasWidget) m_canvasWidget->setVectorCircleFalloff(f);
        saveSettings();
    });
    connect(m_vectorCircleSettings, &VectorCircleSettingsWidget::filledChanged, this, [this](bool filled) {
        if (m_canvasWidget) m_canvasWidget->setVectorCircleFilled(filled);
        saveSettings();
    });

    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::sidesChanged, this, [this](int s) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonSides(s);
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::regularChanged, this, [this](bool reg) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonRegular(reg);
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::sizeChanged, this, [this](double sz) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonSize(static_cast<float>(sz));
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::densityChanged, this, [this](double dens) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonDensity(static_cast<float>(dens));
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::hardnessChanged, this, [this](int h) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonHardness(h);
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::falloffChanged, this, [this](int f) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonFalloff(f);
        saveSettings();
    });
    connect(m_vectorPolygonSettings, &VectorPolygonSettingsWidget::filledChanged, this, [this](bool filled) {
        if (m_canvasWidget) m_canvasWidget->setVectorPolygonFilled(filled);
        saveSettings();
    });

    // 変形ツール設定とキャンバスの連動
    connect(m_transformSettings, &TransformSettingsWidget::applyRequested, this, [this]() {
        if (m_canvasWidget) m_canvasWidget->applyTransform();
    });
    connect(m_transformSettings, &TransformSettingsWidget::cancelRequested, this, [this]() {
        if (m_canvasWidget) m_canvasWidget->cancelTransform();
    });
    connect(m_transformSettings, &TransformSettingsWidget::transformModeChanged, this, [this](TransformMode mode) {
        if (m_canvasWidget) m_canvasWidget->setTransformMode(mode);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::flipHorizontalRequested, this, [this]() {
        if (m_canvasWidget) m_canvasWidget->flipTransformHorizontal();
    });
    connect(m_transformSettings, &TransformSettingsWidget::flipVerticalRequested, this, [this]() {
        if (m_canvasWidget) m_canvasWidget->flipTransformVertical();
    });
    connect(m_transformSettings, &TransformSettingsWidget::warpSubModeChanged, this, [this](WarpSubMode mode) {
        if (m_canvasWidget) m_canvasWidget->setWarpSubMode(mode);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::warpBrushSizeChanged, this, [this](float sz) {
        if (m_canvasWidget) m_canvasWidget->setWarpBrushSize(sz);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::warpBrushStrengthChanged, this, [this](float st) {
        if (m_canvasWidget) m_canvasWidget->setWarpBrushStrength(st);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::warpMeshDivisionsChanged, this, [this](int div) {
        if (m_canvasWidget) m_canvasWidget->setWarpMeshDivisions(div);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::cageSubModeChanged, this, [this](CageSubMode mode) {
        if (m_canvasWidget) m_canvasWidget->setCageSubMode(mode);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::resetCageRequested, this, [this]() {
        if (m_canvasWidget) m_canvasWidget->resetCage();
    });
    connect(m_transformSettings, &TransformSettingsWidget::interpolationChanged, this, [this](InterpolationMode mode) {
        if (m_canvasWidget) m_canvasWidget->setTransformInterpolation(mode);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::keepAspectChanged, this, [this](bool keep) {
        if (m_canvasWidget) m_canvasWidget->setTransformKeepAspect(keep);
        saveSettings();
    });
    connect(m_transformSettings, &TransformSettingsWidget::fromCenterChanged, this, [this](bool center) {
        if (m_canvasWidget) m_canvasWidget->setTransformFromCenter(center);
        saveSettings();
    });

    // 調整レイヤー設定の変更連動
    connect(m_adjustmentSettings, &AdjustmentSettingsWidget::parametersChanged, this, [this]() {
        if (m_document) {
            m_document->composite();
        }
        if (m_canvasWidget) {
            m_canvasWidget->update();
        }
    });

    // テキスト設定とキャンバスの連動
    connect(m_canvasWidget, &CanvasWidget::textItemSelected, m_textSettings, &TextSettingsWidget::setFromItem);
    connect(m_textSettings, &TextSettingsWidget::settingsChanged, m_canvasWidget, &CanvasWidget::applyTextSettingsToSelected);
    connect(m_textSettings, &TextSettingsWidget::textChanged, m_canvasWidget, &CanvasWidget::applyTextSettingsToSelected);

    // コマ設定とキャンバスの連動
    connect(m_panelSettings, &PanelSettingsWidget::settingsChanged, m_canvasWidget, &CanvasWidget::applyPanelSettingsToSelected);

    // AI アシスタント ドック
    m_aiAssistantDock = new AiAssistantDockWidget(this);
    m_aiAssistantDock->setObjectName("AiAssistantDock");
    m_aiAssistantDock->setDocument(m_document);
    m_aiAssistantDock->setCanvasWidget(m_canvasWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_aiAssistantDock);
    connect(m_aiAssistantDock, &AiAssistantDockWidget::applyAsNewLayer, this, &MainWindow::onAiApplyAsNewLayer);
    connect(m_aiAssistantDock, &AiAssistantDockWidget::applyOverwriteSelection, this, &MainWindow::onAiApplyOverwriteSelection);

    // ドックウィジェットの配置変更（移動・フローティング切替・表示切替）時の設定永続化
    auto onDockLayoutChanged = [this]() {
        if (!m_isRestoringSettings) {
            saveSettings();
        }
    };
    connect(m_navLayerDock, &QDockWidget::dockLocationChanged, this, onDockLayoutChanged);
    connect(m_navLayerDock, &QDockWidget::topLevelChanged, this, onDockLayoutChanged);
    connect(m_navLayerDock, &QDockWidget::visibilityChanged, this, onDockLayoutChanged);

    connect(m_colorBrushDock, &QDockWidget::dockLocationChanged, this, onDockLayoutChanged);
    connect(m_colorBrushDock, &QDockWidget::topLevelChanged, this, onDockLayoutChanged);
    connect(m_colorBrushDock, &QDockWidget::visibilityChanged, this, onDockLayoutChanged);

    connect(m_aiAssistantDock, &QDockWidget::dockLocationChanged, this, onDockLayoutChanged);
    connect(m_aiAssistantDock, &QDockWidget::topLevelChanged, this, onDockLayoutChanged);
    connect(m_aiAssistantDock, &QDockWidget::visibilityChanged, this, onDockLayoutChanged);

    if (m_toolSplitter) {
        connect(m_toolSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
            if (!m_isRestoringSettings) {
                saveSettings();
            }
        });
    }

    if (m_navSplitter) {
        connect(m_navSplitter, &QSplitter::splitterMoved, this, [this](int, int) {
            if (!m_isRestoringSettings) {
                saveSettings();
            }
        });
    }

    // アンドゥスタックの連動
    if (m_document && m_document->undoStack()) {
        connect(m_document->undoStack(), &QUndoStack::canUndoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::canRedoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::undoTextChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::redoTextChanged, this, &MainWindow::updateUndoRedoActions);
    }

    if (m_document) {
        connect(m_document, &Document::activeLayerChanged, this, &MainWindow::updateDeletePixelsAction);
        connect(m_document, &Document::layerListChanged, this, &MainWindow::updateDeletePixelsAction);
        connect(m_document, &Document::selectionChanged, this, &MainWindow::updateDeletePixelsAction);
        updateDeletePixelsAction();
    }
}

void MainWindow::initDefaultToolConfigs() {
    m_toolConfigs[ToolType::Pen]                = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  true,  false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Airbrush]           = { 40.0f, 0.5f,  0.5f, BlendMode::Normal, true,  true,  true,  0, 100, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Watercolor]         = { 20.0f, 0.2f,  0.7f, BlendMode::Normal, true,  true,  true,  50, 100, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Marker]             = { 14.0f, 1.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Eraser]             = { 20.0f, 0.0f,  1.0f, BlendMode::Normal, true,  true,  false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Bucket]             = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Blur]               = { 30.0f, 0.3f,  0.8f, BlendMode::Normal, true,  true,  true,  0, 100, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Blend]              = { 30.0f, 0.3f,  0.8f, BlendMode::Normal, true,  true,  true,  0, 100, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorControlPoint] = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorPressure]     = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorLineEdit]     = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorColorChange]  = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorEllipse]      = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorCircle]       = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::VectorPolygon]      = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::TextBalloon]        = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::Pipette]            = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, true,  false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::RectSelect]         = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, false, false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::EllipseSelect]      = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, false, false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::LassoSelect]        = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, false, false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::ColorSelect]        = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, false, false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
    m_toolConfigs[ToolType::MagicWand]          = { 10.0f, 0.0f,  1.0f, BlendMode::Normal, false, false, false, 100, 0, 0, 30, false, 5, 100, 0, false };
}

void MainWindow::saveCurrentToolConfig() {
    if (m_brushSettings) {
        m_toolConfigs[m_currentTool] = m_brushSettings->config();
    }
}

void MainWindow::saveSettings() {
    if (m_isRestoringSettings) return;

    saveCurrentToolConfig();

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("General");
    settings.setValue("currentTool", static_cast<int>(m_currentTool));
    if (m_colorWheel) {
        settings.setValue("currentColor", m_colorWheel->currentColor().name(QColor::HexArgb));
    }
    if (m_toolBox && m_toolBox->colorBox()) {
        settings.setValue("primaryColor", m_toolBox->colorBox()->primaryColor().name(QColor::HexArgb));
        settings.setValue("secondaryColor", m_toolBox->colorBox()->secondaryColor().name(QColor::HexArgb));
    }
    if (m_smoothCombo) {
        settings.setValue("smoothIndex", m_smoothCombo->currentIndex());
    }
    if (m_btnTogglePalette) {
        settings.setValue("paletteVisible", m_btnTogglePalette->isChecked());
    }
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.endGroup();

    // ドックウィジェットの明示的な配置設定 (フェイルセーフ＆確実な復元用)
    settings.beginGroup("Docks");
    if (m_navLayerDock) {
        settings.setValue("NavLayer_floating", m_navLayerDock->isFloating());
        settings.setValue("NavLayer_visible", m_navLayerDock->isVisible());
        settings.setValue("NavLayer_area", static_cast<int>(dockWidgetArea(m_navLayerDock)));
        settings.setValue("NavLayer_width", m_navLayerDock->width());
        if (m_navLayerDock->isFloating()) {
            settings.setValue("NavLayer_geometry", m_navLayerDock->geometry());
        }
    }
    if (m_colorBrushDock) {
        settings.setValue("ColorBrush_floating", m_colorBrushDock->isFloating());
        settings.setValue("ColorBrush_visible", m_colorBrushDock->isVisible());
        settings.setValue("ColorBrush_area", static_cast<int>(dockWidgetArea(m_colorBrushDock)));
        settings.setValue("ColorBrush_width", m_colorBrushDock->width());
        if (m_colorBrushDock->isFloating()) {
            settings.setValue("ColorBrush_geometry", m_colorBrushDock->geometry());
        }
    }
    if (m_toolSplitter) {
        settings.setValue("ToolSplitter_state", m_toolSplitter->saveState());
    }
    if (m_navSplitter) {
        settings.setValue("NavSplitter_state", m_navSplitter->saveState());
    }
    settings.endGroup();

    settings.beginGroup("Tools");
    for (const auto& [tool, cfg] : m_toolConfigs) {
        QString prefix = toolTypeName(tool) + "/";
        settings.setValue(prefix + "size", cfg.size);
        settings.setValue(prefix + "minSizeRatio", cfg.minSizeRatio);
        settings.setValue(prefix + "density", cfg.density);
        settings.setValue(prefix + "blendMode", static_cast<int>(cfg.blendMode));
        settings.setValue(prefix + "antialias", cfg.antialias);
        settings.setValue(prefix + "pressureSize", cfg.pressureSize);
        settings.setValue(prefix + "pressureDensity", cfg.pressureDensity);
        settings.setValue(prefix + "hardness", cfg.hardness);
        settings.setValue(prefix + "falloff", cfg.falloff);
        settings.setValue(prefix + "minPressure", cfg.minPressure);
        settings.setValue(prefix + "airbrushInterval", cfg.airbrushInterval);
        settings.setValue(prefix + "scatter", cfg.scatter);
        settings.setValue(prefix + "scatterAmount", cfg.scatterAmount);
        settings.setValue(prefix + "scatterRange", cfg.scatterRange);
        settings.setValue(prefix + "scatterSizeRandom", cfg.scatterSizeRandom);
        settings.setValue(prefix + "scatterScaleUp", cfg.scatterScaleUp);
    }
    settings.endGroup();

    settings.beginGroup("BlurTool");
    if (m_blurSettings) {
        settings.setValue("size", m_blurSettings->blurSize());
        settings.setValue("minSizeRatio", m_blurSettings->minSizeRatio());
        settings.setValue("density", m_blurSettings->blurDensity());
        settings.setValue("pressureSize", m_blurSettings->isPressureSizeEnabled());
        settings.setValue("pressureDensity", m_blurSettings->isPressureDensityEnabled());
        settings.setValue("colorMix", m_blurSettings->colorMix());
        settings.setValue("colorStretch", m_blurSettings->colorStretch());
        settings.setValue("hardness", m_blurSettings->hardness());
        settings.setValue("falloff", m_blurSettings->falloff());
        settings.setValue("minPressure", m_blurSettings->minPressure());
    }
    settings.endGroup();

    settings.beginGroup("SmudgeTool");
    if (m_smudgeSettings) {
        settings.setValue("size", m_smudgeSettings->smudgeSize());
        settings.setValue("minSizeRatio", m_smudgeSettings->minSizeRatio());
        settings.setValue("density", m_smudgeSettings->smudgeDensity());
        settings.setValue("rate", m_smudgeSettings->rate());
        settings.setValue("pressureSize", m_smudgeSettings->isPressureSizeEnabled());
        settings.setValue("pressureDensity", m_smudgeSettings->isPressureDensityEnabled());
        settings.setValue("hardness", m_smudgeSettings->hardness());
        settings.setValue("falloff", m_smudgeSettings->falloff());
        settings.setValue("minPressure", m_smudgeSettings->minPressure());
    }
    settings.endGroup();

    settings.beginGroup("WatercolorTool");
    if (m_watercolorSettings) {
        settings.setValue("size", m_watercolorSettings->watercolorSize());
        settings.setValue("minSizeRatio", m_watercolorSettings->minSizeRatio());
        settings.setValue("density", m_watercolorSettings->watercolorDensity());
        settings.setValue("blendMode", static_cast<int>(m_watercolorSettings->blendMode()));
        settings.setValue("hardness", m_watercolorSettings->hardness());
        settings.setValue("falloff", m_watercolorSettings->falloff());
        settings.setValue("minPressure", m_watercolorSettings->minPressure());
        settings.setValue("strength", m_watercolorSettings->strength());
        settings.setValue("water", m_watercolorSettings->water());
        settings.setValue("colorStretch", m_watercolorSettings->colorStretch());
        settings.setValue("blurPressure", m_watercolorSettings->blurPressure());
        settings.setValue("pressureSize", m_watercolorSettings->isPressureSizeEnabled());
        settings.setValue("pressureDensity", m_watercolorSettings->isPressureDensityEnabled());
        settings.setValue("antialias", m_watercolorSettings->antialias());
        settings.setValue("scatter", m_watercolorSettings->isScatter());
        settings.setValue("scatterAmount", m_watercolorSettings->scatterAmount());
        settings.setValue("scatterRange", m_watercolorSettings->scatterRange());
        settings.setValue("scatterSizeRandom", m_watercolorSettings->scatterSizeRandom());
        settings.setValue("scatterScaleUp", m_watercolorSettings->isScatterScaleUp());
    }
    settings.endGroup();

    settings.beginGroup("CompositeBrushTool");
    if (m_compositeBrushSettings) {
        settings.setValue("size", m_compositeBrushSettings->compositeBrushSize());
        settings.setValue("minSizeRatio", m_compositeBrushSettings->minSizeRatio());
        settings.setValue("density", m_compositeBrushSettings->compositeBrushDensity());
        settings.setValue("blendMode", static_cast<int>(m_compositeBrushSettings->blendMode()));
        settings.setValue("reference", static_cast<int>(m_compositeBrushSettings->referenceMode()));
        settings.setValue("hardness", m_compositeBrushSettings->hardness());
        settings.setValue("falloff", m_compositeBrushSettings->falloff());
        settings.setValue("minPressure", m_compositeBrushSettings->minPressure());
        settings.setValue("strength", m_compositeBrushSettings->strength());
        settings.setValue("water", m_compositeBrushSettings->water());
        settings.setValue("colorStretch", m_compositeBrushSettings->colorStretch());
        settings.setValue("blurPressure", m_compositeBrushSettings->blurPressure());
        settings.setValue("pressureSize", m_compositeBrushSettings->isPressureSizeEnabled());
        settings.setValue("pressureDensity", m_compositeBrushSettings->isPressureDensityEnabled());
        settings.setValue("antialias", m_compositeBrushSettings->antialias());
        settings.setValue("scatter", m_compositeBrushSettings->isScatter());
        settings.setValue("scatterAmount", m_compositeBrushSettings->scatterAmount());
        settings.setValue("scatterRange", m_compositeBrushSettings->scatterRange());
        settings.setValue("scatterSizeRandom", m_compositeBrushSettings->scatterSizeRandom());
        settings.setValue("scatterScaleUp", m_compositeBrushSettings->isScatterScaleUp());
    }
    settings.endGroup();

    settings.beginGroup("ImageBrushTool");
    if (m_imageBrushSettings) {
        settings.setValue("size", m_imageBrushSettings->brushSize());
        settings.setValue("minSizeRatio", m_imageBrushSettings->minSizeRatio());
        settings.setValue("spacing", m_imageBrushSettings->spacing());
        settings.setValue("density", m_imageBrushSettings->density());
        settings.setValue("minPressure", m_imageBrushSettings->minPressure());
        settings.setValue("blendMode", static_cast<int>(m_imageBrushSettings->blendMode()));
        settings.setValue("pressureSize", m_imageBrushSettings->isPressureSizeEnabled());
        settings.setValue("pressureDensity", m_imageBrushSettings->isPressureDensityEnabled());
        settings.setValue("scatter", m_imageBrushSettings->isScatter());
        settings.setValue("scatterAmount", m_imageBrushSettings->scatterAmount());
        settings.setValue("scatterRange", m_imageBrushSettings->scatterRange());
        settings.setValue("scatterSizeRandom", m_imageBrushSettings->scatterSizeRandom());
        settings.setValue("scatterScaleUp", m_imageBrushSettings->isScatterScaleUp());
        settings.setValue("flipH", m_imageBrushSettings->isFlipH());
        settings.setValue("flipV", m_imageBrushSettings->isFlipV());
        settings.setValue("angle", m_imageBrushSettings->angle());
        settings.setValue("followCursor", m_imageBrushSettings->isFollowCursor());
        settings.setValue("randomRotate", m_imageBrushSettings->isRandomRotate());
        settings.setValue("randomRotateAmount", m_imageBrushSettings->randomRotateAmount());
        settings.setValue("randomSize", m_imageBrushSettings->isRandomSize());
        settings.setValue("randomSizeAmount", m_imageBrushSettings->randomSizeAmount());
        settings.setValue("randomSizeScaleUp", m_imageBrushSettings->isRandomSizeScaleUp());
        settings.setValue("selectedImageId", m_imageBrushSettings->selectedImageId());
    }
    settings.endGroup();

    settings.beginGroup("BucketTool");
    if (m_bucketSettings) {
        settings.setValue("blendMode", static_cast<int>(m_bucketSettings->blendMode()));
        settings.setValue("reference", static_cast<int>(m_bucketSettings->referenceMode()));
        settings.setValue("expandPixels", m_bucketSettings->expandPixels());
        settings.setValue("narrowAreaFill", m_bucketSettings->isNarrowAreaFill());
        settings.setValue("tolerance", m_bucketSettings->tolerance());
    }
    settings.endGroup();

    settings.beginGroup("GradientTool");
    if (m_gradientSettings) {
        settings.setValue("shape", static_cast<int>(m_gradientSettings->shape()));
        settings.setValue("colorMode", static_cast<int>(m_gradientSettings->colorMode()));
        settings.setValue("repeatMode", static_cast<int>(m_gradientSettings->repeatMode()));
        settings.setValue("blendMode", static_cast<int>(m_gradientSettings->blendMode()));
        settings.setValue("opacity", m_gradientSettings->opacity());
        settings.setValue("reverse", m_gradientSettings->isReverse());
        settings.setValue("dither", m_gradientSettings->isDither());
    }
    settings.endGroup();

    settings.beginGroup("VectorEraserTool");
    if (m_vectorEraserSettings) {
        settings.setValue("mode", static_cast<int>(m_vectorEraserSettings->eraserMode()));
        settings.setValue("size", m_vectorEraserSettings->eraserSize());
    }
    settings.endGroup();

    settings.beginGroup("VectorControlPointTool");
    if (m_vectorControlPointSettings) {
        settings.setValue("mode", static_cast<int>(m_vectorControlPointSettings->mode()));
    }
    settings.endGroup();

    settings.beginGroup("VectorLineTool");
    if (m_vectorLineSettings) {
        settings.setValue("size", m_vectorLineSettings->lineSize());
        settings.setValue("density", m_vectorLineSettings->lineDensity());
        settings.setValue("closePath", m_vectorLineSettings->isClosePath());
    }
    settings.endGroup();

    settings.beginGroup("VectorChangeLineTool");
    if (m_vectorChangeLineSettings) {
        settings.setValue("size", m_vectorChangeLineSettings->lineSize());
        settings.setValue("density", m_vectorChangeLineSettings->lineDensity());
        settings.setValue("hardness", m_vectorChangeLineSettings->lineHardness());
        settings.setValue("falloff", m_vectorChangeLineSettings->lineFalloff());
    }
    settings.endGroup();

    settings.beginGroup("VectorEllipseTool");
    if (m_vectorEllipseSettings) {
        settings.setValue("size", m_vectorEllipseSettings->lineSize());
        settings.setValue("density", m_vectorEllipseSettings->lineDensity());
        settings.setValue("hardness", m_vectorEllipseSettings->lineHardness());
        settings.setValue("falloff", m_vectorEllipseSettings->lineFalloff());
        settings.setValue("filled", m_vectorEllipseSettings->isFilled());
    }
    settings.endGroup();

    settings.beginGroup("VectorCircleTool");
    if (m_vectorCircleSettings) {
        settings.setValue("size", m_vectorCircleSettings->lineSize());
        settings.setValue("density", m_vectorCircleSettings->lineDensity());
        settings.setValue("hardness", m_vectorCircleSettings->lineHardness());
        settings.setValue("falloff", m_vectorCircleSettings->lineFalloff());
        settings.setValue("filled", m_vectorCircleSettings->isFilled());
    }
    settings.endGroup();

    settings.beginGroup("VectorPolygonTool");
    if (m_vectorPolygonSettings) {
        settings.setValue("sides", m_vectorPolygonSettings->sides());
        settings.setValue("regular", m_vectorPolygonSettings->isRegular());
        settings.setValue("size", m_vectorPolygonSettings->lineSize());
        settings.setValue("density", m_vectorPolygonSettings->lineDensity());
        settings.setValue("hardness", m_vectorPolygonSettings->lineHardness());
        settings.setValue("falloff", m_vectorPolygonSettings->lineFalloff());
        settings.setValue("filled", m_vectorPolygonSettings->isFilled());
    }
    settings.endGroup();

    settings.beginGroup("TransformTool");
    if (m_transformSettings) {
        settings.setValue("interpolation", static_cast<int>(m_transformSettings->interpolationMode()));
        settings.setValue("keepAspect", m_transformSettings->isKeepAspect());
        settings.setValue("fromCenter", m_transformSettings->isFromCenter());
        settings.setValue("transformMode", static_cast<int>(m_transformSettings->transformMode()));
        settings.setValue("warpSubMode", static_cast<int>(m_transformSettings->warpSubMode()));
        settings.setValue("warpBrushSize", m_transformSettings->warpBrushSize());
        settings.setValue("warpBrushStrength", m_transformSettings->warpBrushStrength());
        settings.setValue("warpMeshDivisions", m_transformSettings->warpMeshDivisions());
        settings.setValue("cageSubMode", static_cast<int>(m_transformSettings->cageSubMode()));
    }
    settings.endGroup();

    if (m_toolBox) {
        m_toolBox->saveSlots(settings);
    }

    settings.sync();
}

void MainWindow::loadSettings() {
    m_isRestoringSettings = true;

    initDefaultToolConfigs();

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "KaburagiPaint", "PaintSoftKABURAGI");
    settings.beginGroup("Tools");
    for (auto& [tool, cfg] : m_toolConfigs) {
        QString prefix = toolTypeName(tool) + "/";
        if (settings.contains(prefix + "size")) {
            cfg.size = settings.value(prefix + "size", cfg.size).toFloat();
            cfg.minSizeRatio = settings.value(prefix + "minSizeRatio", cfg.minSizeRatio).toFloat();
            cfg.density = settings.value(prefix + "density", cfg.density).toFloat();
            cfg.blendMode = static_cast<BlendMode>(settings.value(prefix + "blendMode", static_cast<int>(cfg.blendMode)).toInt());
            cfg.antialias = settings.value(prefix + "antialias", cfg.antialias).toBool();
            cfg.pressureSize = settings.value(prefix + "pressureSize", cfg.pressureSize).toBool();
            cfg.pressureDensity = settings.value(prefix + "pressureDensity", cfg.pressureDensity).toBool();
            cfg.hardness = settings.value(prefix + "hardness", cfg.hardness).toInt();
            cfg.falloff = settings.value(prefix + "falloff", cfg.falloff).toInt();
            cfg.minPressure = settings.value(prefix + "minPressure", cfg.minPressure).toInt();
            cfg.airbrushInterval = settings.value(prefix + "airbrushInterval", cfg.airbrushInterval).toInt();
            cfg.scatter = settings.value(prefix + "scatter", cfg.scatter).toBool();
            cfg.scatterAmount = settings.value(prefix + "scatterAmount", cfg.scatterAmount).toInt();
            cfg.scatterRange = settings.value(prefix + "scatterRange", cfg.scatterRange).toInt();
            cfg.scatterSizeRandom = settings.value(prefix + "scatterSizeRandom", cfg.scatterSizeRandom).toInt();
            cfg.scatterScaleUp = settings.value(prefix + "scatterScaleUp", cfg.scatterScaleUp).toBool();
        }
    }
    settings.endGroup();

    settings.beginGroup("General");
    int toolInt = settings.value("currentTool", static_cast<int>(ToolType::Pen)).toInt();
    m_currentTool = static_cast<ToolType>(toolInt);
    if (m_toolBox) {
        m_toolBox->setCurrentTool(m_currentTool);
    }
    if (m_canvasWidget) {
        m_canvasWidget->setCurrentTool(m_currentTool);
    }

    if (settings.contains("currentColor") && m_colorWheel) {
        QColor col(settings.value("currentColor").toString());
        if (col.isValid()) {
            m_colorWheel->setColor(col);
            if (m_canvasWidget) {
                m_canvasWidget->setCurrentColor(col);
            }
        }
    }

    if (m_toolBox && m_toolBox->colorBox()) {
        if (settings.contains("primaryColor")) {
            QColor pCol(settings.value("primaryColor").toString());
            if (pCol.isValid()) {
                m_toolBox->colorBox()->setPrimaryColor(pCol);
                if (m_canvasWidget) {
                    m_canvasWidget->setCurrentColor(pCol);
                }
            }
        }
        if (settings.contains("secondaryColor")) {
            QColor sCol(settings.value("secondaryColor").toString());
            if (sCol.isValid()) {
                m_toolBox->colorBox()->setSecondaryColor(sCol);
                if (m_canvasWidget) {
                    m_canvasWidget->setSecondaryColor(sCol);
                }
            }
        }
    }

    if (settings.contains("smoothIndex") && m_smoothCombo) {
        int sIdx = settings.value("smoothIndex", 3).toInt();
        if (sIdx >= 0 && sIdx < m_smoothCombo->count()) {
            m_smoothCombo->setCurrentIndex(sIdx);
        }
    }

    if (m_btnTogglePalette) {
        bool palVis = settings.value("paletteVisible", false).toBool();
        m_btnTogglePalette->setChecked(palVis);
        if (m_paletteWidget) {
            m_paletteWidget->setVisible(palVis);
        }
    }

    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }
    bool stateRestored = false;
    if (settings.contains("windowState")) {
        stateRestored = restoreState(settings.value("windowState").toByteArray());
    }
    settings.endGroup();

    // 明示的なドック配置情報の復元 (restoreState に失敗した場合、またはフェイルセーフ用)
    settings.beginGroup("Docks");
    if (m_navLayerDock) {
        bool navFloating = settings.value("NavLayer_floating", false).toBool();
        int navAreaInt = settings.value("NavLayer_area", static_cast<int>(Qt::LeftDockWidgetArea)).toInt();
        Qt::DockWidgetArea navArea = static_cast<Qt::DockWidgetArea>(navAreaInt);
        if (navArea != Qt::LeftDockWidgetArea && navArea != Qt::RightDockWidgetArea) {
            navArea = Qt::LeftDockWidgetArea;
        }

        if (!stateRestored) {
            m_navLayerDock->setFloating(navFloating);
            if (!navFloating) {
                addDockWidget(navArea, m_navLayerDock);
            }
        } else {
            if (m_navLayerDock->isFloating() != navFloating) {
                m_navLayerDock->setFloating(navFloating);
            }
            if (!navFloating && dockWidgetArea(m_navLayerDock) != navArea) {
                addDockWidget(navArea, m_navLayerDock);
            }
        }

        if (m_navLayerDock->isFloating() && settings.contains("NavLayer_geometry")) {
            QRect geom = settings.value("NavLayer_geometry").toRect();
            if (geom.isValid()) {
                m_navLayerDock->setGeometry(geom);
            }
        }
        m_navLayerDock->show();
    }

    if (m_colorBrushDock) {
        bool colorFloating = settings.value("ColorBrush_floating", false).toBool();
        int colorAreaInt = settings.value("ColorBrush_area", static_cast<int>(Qt::LeftDockWidgetArea)).toInt();
        Qt::DockWidgetArea colorArea = static_cast<Qt::DockWidgetArea>(colorAreaInt);
        if (colorArea != Qt::LeftDockWidgetArea && colorArea != Qt::RightDockWidgetArea) {
            colorArea = Qt::LeftDockWidgetArea;
        }

        if (!stateRestored) {
            m_colorBrushDock->setFloating(colorFloating);
            if (!colorFloating) {
                addDockWidget(colorArea, m_colorBrushDock);
            }
        } else {
            if (m_colorBrushDock->isFloating() != colorFloating) {
                m_colorBrushDock->setFloating(colorFloating);
            }
            if (!colorFloating && dockWidgetArea(m_colorBrushDock) != colorArea) {
                addDockWidget(colorArea, m_colorBrushDock);
            }
        }

        if (m_colorBrushDock->isFloating() && settings.contains("ColorBrush_geometry")) {
            QRect geom = settings.value("ColorBrush_geometry").toRect();
            if (geom.isValid()) {
                m_colorBrushDock->setGeometry(geom);
            }
        }
        m_colorBrushDock->show();
    }
    if (m_toolSplitter && settings.contains("ToolSplitter_state")) {
        m_toolSplitter->restoreState(settings.value("ToolSplitter_state").toByteArray());
    }
    if (m_navSplitter && settings.contains("NavSplitter_state")) {
        m_navSplitter->restoreState(settings.value("NavSplitter_state").toByteArray());
    }
    settings.endGroup();

    // 現在のツールの設定を BrushSettingsWidget に反映
    if (m_brushSettings) {
        auto it = m_toolConfigs.find(m_currentTool);
        if (it != m_toolConfigs.end()) {
            m_brushSettings->setAirbrushMode(m_currentTool == ToolType::Airbrush);
            m_brushSettings->setEraserMode(m_currentTool == ToolType::Eraser);
            m_brushSettings->setConfig(it->second);
            if (m_canvasWidget) {
                m_canvasWidget->setBrushBlendMode(it->second.blendMode);
                m_canvasWidget->setBrushSize(it->second.size);
                m_canvasWidget->setMinSizeRatio(it->second.minSizeRatio);
                m_canvasWidget->setBrushDensity(it->second.density);
                m_canvasWidget->setAntialias(it->second.antialias);
                m_canvasWidget->setPressureSizeEnabled(it->second.pressureSize);
                m_canvasWidget->setPressureDensityEnabled(it->second.pressureDensity);
                m_canvasWidget->setBrushHardness(it->second.hardness);
                m_canvasWidget->setBrushFalloff(it->second.falloff);
                m_canvasWidget->setBrushMinPressure(it->second.minPressure);
                m_canvasWidget->setAirbrushInterval(it->second.airbrushInterval);
            }
        }
    }

    settings.beginGroup("BlurTool");
    if (m_blurSettings) {
        BlurConfig bcfg;
        bcfg.size = settings.value("size", 30.0f).toFloat();
        bcfg.minSizeRatio = settings.value("minSizeRatio", 0.0f).toFloat();
        bcfg.density = settings.value("density", 1.0f).toFloat();
        bcfg.pressureSize = settings.value("pressureSize", true).toBool();
        bcfg.pressureDensity = settings.value("pressureDensity", false).toBool();
        bcfg.colorMix = settings.value("colorMix", 50).toInt();
        bcfg.colorStretch = settings.value("colorStretch", 50).toInt();
        bcfg.hardness = settings.value("hardness", 0).toInt();
        bcfg.falloff = settings.value("falloff", 100).toInt();
        bcfg.minPressure = settings.value("minPressure", 0).toInt();
        m_blurSettings->setConfig(bcfg);

        if (m_canvasWidget) {
            m_canvasWidget->setBlurSize(m_blurSettings->blurSize());
            m_canvasWidget->setBlurMinSizeRatio(m_blurSettings->minSizeRatio());
            m_canvasWidget->setBlurDensity(m_blurSettings->blurDensity());
            m_canvasWidget->setBlurPressureSizeEnabled(m_blurSettings->isPressureSizeEnabled());
            m_canvasWidget->setBlurPressureDensityEnabled(m_blurSettings->isPressureDensityEnabled());
            m_canvasWidget->setBlurColorMix(m_blurSettings->colorMix());
            m_canvasWidget->setBlurColorStretch(m_blurSettings->colorStretch());
            m_canvasWidget->setBlurHardness(m_blurSettings->hardness());
            m_canvasWidget->setBlurFalloff(m_blurSettings->falloff());
            m_canvasWidget->setBlurMinPressure(m_blurSettings->minPressure());
        }
    }
    settings.endGroup();

    settings.beginGroup("SmudgeTool");
    if (m_smudgeSettings) {
        SmudgeConfig scfg;
        scfg.size = settings.value("size", 30.0f).toFloat();
        scfg.minSizeRatio = settings.value("minSizeRatio", 0.0f).toFloat();
        scfg.density = settings.value("density", 1.0f).toFloat();
        scfg.rate = settings.value("rate", 50).toInt();
        scfg.pressureSize = settings.value("pressureSize", true).toBool();
        scfg.pressureDensity = settings.value("pressureDensity", false).toBool();
        scfg.hardness = settings.value("hardness", 0).toInt();
        scfg.falloff = settings.value("falloff", 100).toInt();
        scfg.minPressure = settings.value("minPressure", 0).toInt();
        m_smudgeSettings->setConfig(scfg);

        if (m_canvasWidget) {
            m_canvasWidget->setSmudgeSize(m_smudgeSettings->smudgeSize());
            m_canvasWidget->setSmudgeMinSizeRatio(m_smudgeSettings->minSizeRatio());
            m_canvasWidget->setSmudgeDensity(m_smudgeSettings->smudgeDensity());
            m_canvasWidget->setSmudgeRate(m_smudgeSettings->rate());
            m_canvasWidget->setSmudgePressureSizeEnabled(m_smudgeSettings->isPressureSizeEnabled());
            m_canvasWidget->setSmudgePressureDensityEnabled(m_smudgeSettings->isPressureDensityEnabled());
            m_canvasWidget->setSmudgeHardness(m_smudgeSettings->hardness());
            m_canvasWidget->setSmudgeFalloff(m_smudgeSettings->falloff());
            m_canvasWidget->setSmudgeMinPressure(m_smudgeSettings->minPressure());
        }
    }
    settings.endGroup();

    settings.beginGroup("WatercolorTool");
    if (m_watercolorSettings) {
        WatercolorConfig wcfg;
        wcfg.size = settings.value("size", 20.0f).toFloat();
        wcfg.minSizeRatio = settings.value("minSizeRatio", 0.2f).toFloat();
        wcfg.density = settings.value("density", 0.7f).toFloat();
        wcfg.blendMode = static_cast<BlendMode>(settings.value("blendMode", static_cast<int>(BlendMode::Normal)).toInt());
        wcfg.hardness = settings.value("hardness", 50).toInt();
        wcfg.falloff = settings.value("falloff", 100).toInt();
        wcfg.minPressure = settings.value("minPressure", 0).toInt();
        wcfg.strength = settings.value("strength", 50).toInt();
        wcfg.water = settings.value("water", 50).toInt();
        wcfg.colorStretch = settings.value("colorStretch", 50).toInt();
        wcfg.blurPressure = settings.value("blurPressure", 0).toInt();
        wcfg.pressureSize = settings.value("pressureSize", true).toBool();
        wcfg.pressureDensity = settings.value("pressureDensity", true).toBool();
        wcfg.antialias = settings.value("antialias", true).toBool();
        wcfg.scatter = settings.value("scatter", false).toBool();
        wcfg.scatterAmount = settings.value("scatterAmount", 5).toInt();
        wcfg.scatterRange = settings.value("scatterRange", 100).toInt();
        wcfg.scatterSizeRandom = settings.value("scatterSizeRandom", 0).toInt();
        wcfg.scatterScaleUp = settings.value("scatterScaleUp", false).toBool();
        m_watercolorSettings->setConfig(wcfg);

        if (m_canvasWidget) {
            m_canvasWidget->setWatercolorBlendMode(m_watercolorSettings->blendMode());
            m_canvasWidget->setWatercolorSize(m_watercolorSettings->watercolorSize());
            m_canvasWidget->setWatercolorMinSizeRatio(m_watercolorSettings->minSizeRatio());
            m_canvasWidget->setWatercolorDensity(m_watercolorSettings->watercolorDensity());
            m_canvasWidget->setWatercolorHardness(m_watercolorSettings->hardness());
            m_canvasWidget->setWatercolorFalloff(m_watercolorSettings->falloff());
            m_canvasWidget->setWatercolorMinPressure(m_watercolorSettings->minPressure());
            m_canvasWidget->setWatercolorStrength(m_watercolorSettings->strength());
            m_canvasWidget->setWatercolorWater(m_watercolorSettings->water());
            m_canvasWidget->setWatercolorColorStretch(m_watercolorSettings->colorStretch());
            m_canvasWidget->setWatercolorBlurPressure(m_watercolorSettings->blurPressure());
            m_canvasWidget->setWatercolorPressureSizeEnabled(m_watercolorSettings->isPressureSizeEnabled());
            m_canvasWidget->setWatercolorPressureDensityEnabled(m_watercolorSettings->isPressureDensityEnabled());
            m_canvasWidget->setWatercolorAntialias(m_watercolorSettings->antialias());
            m_canvasWidget->setWatercolorScatter(m_watercolorSettings->isScatter());
            m_canvasWidget->setWatercolorScatterAmount(m_watercolorSettings->scatterAmount());
            m_canvasWidget->setWatercolorScatterRange(m_watercolorSettings->scatterRange());
            m_canvasWidget->setWatercolorScatterSizeRandom(m_watercolorSettings->scatterSizeRandom());
            m_canvasWidget->setWatercolorScatterScaleUp(m_watercolorSettings->isScatterScaleUp());
        }
    }
    settings.endGroup();

    settings.beginGroup("CompositeBrushTool");
    if (m_compositeBrushSettings) {
        CompositeBrushConfig ccfg;
        ccfg.size = settings.value("size", 20.0f).toFloat();
        ccfg.minSizeRatio = settings.value("minSizeRatio", 0.2f).toFloat();
        ccfg.density = settings.value("density", 0.7f).toFloat();
        ccfg.blendMode = static_cast<BlendMode>(settings.value("blendMode", static_cast<int>(BlendMode::Normal)).toInt());
        ccfg.reference = static_cast<CompositeBrushReference>(settings.value("reference", static_cast<int>(CompositeBrushReference::Canvas)).toInt());
        ccfg.hardness = settings.value("hardness", 50).toInt();
        ccfg.falloff = settings.value("falloff", 100).toInt();
        ccfg.minPressure = settings.value("minPressure", 0).toInt();
        ccfg.strength = settings.value("strength", 50).toInt();
        ccfg.water = settings.value("water", 50).toInt();
        ccfg.colorStretch = settings.value("colorStretch", 50).toInt();
        ccfg.blurPressure = settings.value("blurPressure", 0).toInt();
        ccfg.pressureSize = settings.value("pressureSize", true).toBool();
        ccfg.pressureDensity = settings.value("pressureDensity", true).toBool();
        ccfg.antialias = settings.value("antialias", true).toBool();
        ccfg.scatter = settings.value("scatter", false).toBool();
        ccfg.scatterAmount = settings.value("scatterAmount", 5).toInt();
        ccfg.scatterRange = settings.value("scatterRange", 100).toInt();
        ccfg.scatterSizeRandom = settings.value("scatterSizeRandom", 0).toInt();
        ccfg.scatterScaleUp = settings.value("scatterScaleUp", false).toBool();
        m_compositeBrushSettings->setConfig(ccfg);

        if (m_canvasWidget) {
            m_canvasWidget->setCompositeBrushBlendMode(m_compositeBrushSettings->blendMode());
            m_canvasWidget->setCompositeBrushReference(m_compositeBrushSettings->referenceMode());
            m_canvasWidget->setCompositeBrushSize(m_compositeBrushSettings->compositeBrushSize());
            m_canvasWidget->setCompositeBrushMinSizeRatio(m_compositeBrushSettings->minSizeRatio());
            m_canvasWidget->setCompositeBrushDensity(m_compositeBrushSettings->compositeBrushDensity());
            m_canvasWidget->setCompositeBrushHardness(m_compositeBrushSettings->hardness());
            m_canvasWidget->setCompositeBrushFalloff(m_compositeBrushSettings->falloff());
            m_canvasWidget->setCompositeBrushMinPressure(m_compositeBrushSettings->minPressure());
            m_canvasWidget->setCompositeBrushStrength(m_compositeBrushSettings->strength());
            m_canvasWidget->setCompositeBrushWater(m_compositeBrushSettings->water());
            m_canvasWidget->setCompositeBrushColorStretch(m_compositeBrushSettings->colorStretch());
            m_canvasWidget->setCompositeBrushBlurPressure(m_compositeBrushSettings->blurPressure());
            m_canvasWidget->setCompositeBrushPressureSizeEnabled(m_compositeBrushSettings->isPressureSizeEnabled());
            m_canvasWidget->setCompositeBrushPressureDensityEnabled(m_compositeBrushSettings->isPressureDensityEnabled());
            m_canvasWidget->setCompositeBrushAntialias(m_compositeBrushSettings->antialias());
            m_canvasWidget->setCompositeBrushScatter(m_compositeBrushSettings->isScatter());
            m_canvasWidget->setCompositeBrushScatterAmount(m_compositeBrushSettings->scatterAmount());
            m_canvasWidget->setCompositeBrushScatterRange(m_compositeBrushSettings->scatterRange());
            m_canvasWidget->setCompositeBrushScatterSizeRandom(m_compositeBrushSettings->scatterSizeRandom());
            m_canvasWidget->setCompositeBrushScatterScaleUp(m_compositeBrushSettings->isScatterScaleUp());
        }
    }
    settings.endGroup();

    settings.beginGroup("ImageBrushTool");
    if (m_imageBrushSettings) {
        ImageBrushConfig ibcfg;
        ibcfg.size = settings.value("size", 40.0f).toFloat();
        ibcfg.minSizeRatio = settings.value("minSizeRatio", 0.0f).toFloat();
        ibcfg.spacing = settings.value("spacing", 20).toInt();
        ibcfg.density = settings.value("density", 1.0f).toFloat();
        ibcfg.minPressure = settings.value("minPressure", 0).toInt();
        ibcfg.blendMode = static_cast<BlendMode>(settings.value("blendMode", static_cast<int>(BlendMode::Normal)).toInt());
        ibcfg.pressureSize = settings.value("pressureSize", true).toBool();
        ibcfg.pressureDensity = settings.value("pressureDensity", false).toBool();
        ibcfg.scatter = settings.value("scatter", false).toBool();
        ibcfg.scatterAmount = settings.value("scatterAmount", 5).toInt();
        ibcfg.scatterRange = settings.value("scatterRange", 100).toInt();
        ibcfg.scatterSizeRandom = settings.value("scatterSizeRandom", 0).toInt();
        ibcfg.scatterScaleUp = settings.value("scatterScaleUp", false).toBool();
        ibcfg.flipH = settings.value("flipH", false).toBool();
        ibcfg.flipV = settings.value("flipV", false).toBool();
        ibcfg.angle = settings.value("angle", 0).toInt();
        ibcfg.followCursor = settings.value("followCursor", false).toBool();
        ibcfg.randomRotate = settings.value("randomRotate", false).toBool();
        ibcfg.randomRotateAmount = settings.value("randomRotateAmount", 360).toInt();
        ibcfg.randomSize = settings.value("randomSize", false).toBool();
        ibcfg.randomSizeAmount = settings.value("randomSizeAmount", 0).toInt();
        ibcfg.randomSizeScaleUp = settings.value("randomSizeScaleUp", false).toBool();
        ibcfg.selectedImageId = settings.value("selectedImageId", "").toString();
        m_imageBrushSettings->setConfig(ibcfg);

        if (m_canvasWidget) {
            m_canvasWidget->setImageBrushImage(m_imageBrushSettings->currentImage());
            m_canvasWidget->setImageBrushBlendMode(m_imageBrushSettings->blendMode());
            m_canvasWidget->setImageBrushSize(m_imageBrushSettings->brushSize());
            m_canvasWidget->setImageBrushMinSizeRatio(m_imageBrushSettings->minSizeRatio());
            m_canvasWidget->setImageBrushSpacing(m_imageBrushSettings->spacing());
            m_canvasWidget->setImageBrushDensity(m_imageBrushSettings->density());
            m_canvasWidget->setImageBrushMinPressure(m_imageBrushSettings->minPressure());
            m_canvasWidget->setImageBrushPressureSizeEnabled(m_imageBrushSettings->isPressureSizeEnabled());
            m_canvasWidget->setImageBrushPressureDensityEnabled(m_imageBrushSettings->isPressureDensityEnabled());
            m_canvasWidget->setImageBrushScatter(m_imageBrushSettings->isScatter());
            m_canvasWidget->setImageBrushScatterAmount(m_imageBrushSettings->scatterAmount());
            m_canvasWidget->setImageBrushScatterRange(m_imageBrushSettings->scatterRange());
            m_canvasWidget->setImageBrushScatterSizeRandom(m_imageBrushSettings->scatterSizeRandom());
            m_canvasWidget->setImageBrushScatterScaleUp(m_imageBrushSettings->isScatterScaleUp());
            m_canvasWidget->setImageBrushFlipH(m_imageBrushSettings->isFlipH());
            m_canvasWidget->setImageBrushFlipV(m_imageBrushSettings->isFlipV());
            m_canvasWidget->setImageBrushAngle(m_imageBrushSettings->angle());
            m_canvasWidget->setImageBrushFollowCursor(m_imageBrushSettings->isFollowCursor());
            m_canvasWidget->setImageBrushRandomRotate(m_imageBrushSettings->isRandomRotate());
            m_canvasWidget->setImageBrushRandomRotateAmount(m_imageBrushSettings->randomRotateAmount());
            m_canvasWidget->setImageBrushRandomSize(m_imageBrushSettings->isRandomSize());
            m_canvasWidget->setImageBrushRandomSizeAmount(m_imageBrushSettings->randomSizeAmount());
            m_canvasWidget->setImageBrushRandomSizeScaleUp(m_imageBrushSettings->isRandomSizeScaleUp());
        }
    }
    settings.endGroup();

    settings.beginGroup("BucketTool");
    if (m_bucketSettings) {
        BlendMode bmode = static_cast<BlendMode>(settings.value("blendMode", static_cast<int>(BlendMode::Normal)).toInt());
        m_bucketSettings->setBlendMode(bmode);
        if (settings.contains("reference")) {
            m_bucketSettings->setReferenceMode(static_cast<BucketReference>(settings.value("reference").toInt()));
        }
        if (settings.contains("expandPixels")) {
            m_bucketSettings->setExpandPixels(settings.value("expandPixels").toInt());
        }
        if (settings.contains("narrowAreaFill")) {
            m_bucketSettings->setNarrowAreaFill(settings.value("narrowAreaFill").toBool());
        }
        if (settings.contains("tolerance")) {
            m_bucketSettings->setTolerance(settings.value("tolerance").toInt());
        }
        if (m_canvasWidget) {
            m_canvasWidget->setBucketBlendMode(m_bucketSettings->blendMode());
            m_canvasWidget->setBucketReference(m_bucketSettings->referenceMode());
            m_canvasWidget->setBucketExpandPixels(m_bucketSettings->expandPixels());
            m_canvasWidget->setBucketNarrowAreaFill(m_bucketSettings->isNarrowAreaFill());
            m_canvasWidget->setBucketTolerance(m_bucketSettings->tolerance());
        }
    }
    settings.endGroup();

    settings.beginGroup("GradientTool");
    if (m_gradientSettings) {
        GradientConfig gcfg;
        gcfg.shape = static_cast<GradientShape>(settings.value("shape", static_cast<int>(GradientShape::Linear)).toInt());
        gcfg.colorMode = static_cast<GradientColorMode>(settings.value("colorMode", static_cast<int>(GradientColorMode::FgToBg)).toInt());
        gcfg.repeatMode = static_cast<GradientRepeatMode>(settings.value("repeatMode", static_cast<int>(GradientRepeatMode::None)).toInt());
        gcfg.blendMode = static_cast<BlendMode>(settings.value("blendMode", static_cast<int>(BlendMode::Normal)).toInt());
        gcfg.opacity = settings.value("opacity", 1.0f).toFloat();
        gcfg.reverse = settings.value("reverse", false).toBool();
        gcfg.dither = settings.value("dither", false).toBool();
        m_gradientSettings->setConfig(gcfg);

        if (m_canvasWidget) {
            m_canvasWidget->setGradientBlendMode(m_gradientSettings->blendMode());
            m_canvasWidget->setGradientShape(m_gradientSettings->shape());
            m_canvasWidget->setGradientColorMode(m_gradientSettings->colorMode());
            m_canvasWidget->setGradientRepeatMode(m_gradientSettings->repeatMode());
            m_canvasWidget->setGradientOpacity(m_gradientSettings->opacity());
            m_canvasWidget->setGradientReverse(m_gradientSettings->isReverse());
            m_canvasWidget->setGradientDither(m_gradientSettings->isDither());
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorEraserTool");
    if (m_vectorEraserSettings) {
        m_vectorEraserSettings->setEraserMode(static_cast<VectorEraserMode>(
            settings.value("mode", static_cast<int>(VectorEraserMode::Intersection)).toInt()));
        m_vectorEraserSettings->setEraserSize(settings.value("size", 20.0).toDouble());
        if (m_canvasWidget) {
            m_canvasWidget->setVectorEraserMode(m_vectorEraserSettings->eraserMode());
            m_canvasWidget->setVectorEraserSize(static_cast<float>(m_vectorEraserSettings->eraserSize()));
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorControlPointTool");
    if (m_vectorControlPointSettings) {
        m_vectorControlPointSettings->setMode(static_cast<VectorControlPointMode>(
            settings.value("mode", static_cast<int>(VectorControlPointMode::MovePoint)).toInt()));
        if (m_canvasWidget) {
            m_canvasWidget->setVectorControlPointMode(m_vectorControlPointSettings->mode());
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorLineTool");
    if (m_vectorLineSettings) {
        double sz = settings.value("size", 4.0).toDouble();
        double dens = settings.value("density", 1.0).toDouble();
        bool closed = settings.value("closePath", false).toBool();
        m_vectorLineSettings->setLineSize(sz);
        m_vectorLineSettings->setLineDensity(dens);
        m_vectorLineSettings->setClosePath(closed);
        if (m_canvasWidget) {
            m_canvasWidget->setVectorLineSize(static_cast<float>(sz));
            m_canvasWidget->setVectorLineDensity(static_cast<float>(dens));
            m_canvasWidget->setVectorClosePath(closed);
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorChangeLineTool");
    if (m_vectorChangeLineSettings) {
        double sz = settings.value("size", 4.0).toDouble();
        double dens = settings.value("density", 1.0).toDouble();
        int hardness = settings.value("hardness", 100).toInt();
        int falloff = settings.value("falloff", 0).toInt();
        m_vectorChangeLineSettings->setLineSize(sz);
        m_vectorChangeLineSettings->setLineDensity(dens);
        m_vectorChangeLineSettings->setLineHardness(hardness);
        m_vectorChangeLineSettings->setLineFalloff(falloff);
        if (m_canvasWidget) {
            m_canvasWidget->setChangeLineSize(static_cast<float>(sz));
            m_canvasWidget->setChangeLineDensity(static_cast<float>(dens));
            m_canvasWidget->setChangeLineHardness(hardness);
            m_canvasWidget->setChangeLineFalloff(falloff);
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorEllipseTool");
    if (m_vectorEllipseSettings) {
        double sz = settings.value("size", 4.0).toDouble();
        double dens = settings.value("density", 1.0).toDouble();
        int hardness = settings.value("hardness", 100).toInt();
        int falloff = settings.value("falloff", 0).toInt();
        bool filled = settings.value("filled", false).toBool();
        m_vectorEllipseSettings->setLineSize(sz);
        m_vectorEllipseSettings->setLineDensity(dens);
        m_vectorEllipseSettings->setLineHardness(hardness);
        m_vectorEllipseSettings->setLineFalloff(falloff);
        m_vectorEllipseSettings->setFilled(filled);
        if (m_canvasWidget) {
            m_canvasWidget->setVectorEllipseSize(static_cast<float>(sz));
            m_canvasWidget->setVectorEllipseDensity(static_cast<float>(dens));
            m_canvasWidget->setVectorEllipseHardness(hardness);
            m_canvasWidget->setVectorEllipseFalloff(falloff);
            m_canvasWidget->setVectorEllipseFilled(filled);
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorCircleTool");
    if (m_vectorCircleSettings) {
        double sz = settings.value("size", 4.0).toDouble();
        double dens = settings.value("density", 1.0).toDouble();
        int hardness = settings.value("hardness", 100).toInt();
        int falloff = settings.value("falloff", 0).toInt();
        bool filled = settings.value("filled", false).toBool();
        m_vectorCircleSettings->setLineSize(sz);
        m_vectorCircleSettings->setLineDensity(dens);
        m_vectorCircleSettings->setLineHardness(hardness);
        m_vectorCircleSettings->setLineFalloff(falloff);
        m_vectorCircleSettings->setFilled(filled);
        if (m_canvasWidget) {
            m_canvasWidget->setVectorCircleSize(static_cast<float>(sz));
            m_canvasWidget->setVectorCircleDensity(static_cast<float>(dens));
            m_canvasWidget->setVectorCircleHardness(hardness);
            m_canvasWidget->setVectorCircleFalloff(falloff);
            m_canvasWidget->setVectorCircleFilled(filled);
        }
    }
    settings.endGroup();

    settings.beginGroup("VectorPolygonTool");
    if (m_vectorPolygonSettings) {
        int sides = settings.value("sides", 5).toInt();
        bool regular = settings.value("regular", false).toBool();
        double sz = settings.value("size", 4.0).toDouble();
        double dens = settings.value("density", 1.0).toDouble();
        int hardness = settings.value("hardness", 100).toInt();
        int falloff = settings.value("falloff", 0).toInt();
        bool filled = settings.value("filled", false).toBool();
        m_vectorPolygonSettings->setSides(sides);
        m_vectorPolygonSettings->setRegular(regular);
        m_vectorPolygonSettings->setLineSize(sz);
        m_vectorPolygonSettings->setLineDensity(dens);
        m_vectorPolygonSettings->setLineHardness(hardness);
        m_vectorPolygonSettings->setLineFalloff(falloff);
        m_vectorPolygonSettings->setFilled(filled);
        if (m_canvasWidget) {
            m_canvasWidget->setVectorPolygonSides(sides);
            m_canvasWidget->setVectorPolygonRegular(regular);
            m_canvasWidget->setVectorPolygonSize(static_cast<float>(sz));
            m_canvasWidget->setVectorPolygonDensity(static_cast<float>(dens));
            m_canvasWidget->setVectorPolygonHardness(hardness);
            m_canvasWidget->setVectorPolygonFalloff(falloff);
            m_canvasWidget->setVectorPolygonFilled(filled);
        }
    }
    settings.endGroup();

    settings.beginGroup("TransformTool");
    if (m_transformSettings) {
        int interpInt = settings.value("interpolation", static_cast<int>(InterpolationMode::Bilinear)).toInt();
        InterpolationMode interp = static_cast<InterpolationMode>(interpInt);
        bool keep = settings.value("keepAspect", false).toBool();
        bool center = settings.value("fromCenter", false).toBool();
        int tModeInt = settings.value("transformMode", static_cast<int>(TransformMode::Unified)).toInt();
        TransformMode tMode = static_cast<TransformMode>(tModeInt);
        int wSubInt = settings.value("warpSubMode", static_cast<int>(WarpSubMode::Brush)).toInt();
        WarpSubMode wSub = static_cast<WarpSubMode>(wSubInt);
        float wSize = settings.value("warpBrushSize", 60.0f).toFloat();
        float wStr = settings.value("warpBrushStrength", 50.0f).toFloat();
        int wDiv = settings.value("warpMeshDivisions", 4).toInt();
        int cSubInt = settings.value("cageSubMode", static_cast<int>(CageSubMode::AutoBox)).toInt();
        CageSubMode cSub = static_cast<CageSubMode>(cSubInt);

        m_transformSettings->setInterpolationMode(interp);
        m_transformSettings->setKeepAspect(keep);
        m_transformSettings->setFromCenter(center);
        m_transformSettings->setTransformMode(tMode);
        m_transformSettings->setWarpSubMode(wSub);
        m_transformSettings->setWarpBrushSize(wSize);
        m_transformSettings->setWarpBrushStrength(wStr);
        m_transformSettings->setWarpMeshDivisions(wDiv);
        m_transformSettings->setCageSubMode(cSub);

        if (m_canvasWidget) {
            m_canvasWidget->setTransformInterpolation(interp);
            m_canvasWidget->setTransformKeepAspect(keep);
            m_canvasWidget->setTransformFromCenter(center);
            m_canvasWidget->setTransformMode(tMode);
            m_canvasWidget->setWarpSubMode(wSub);
            m_canvasWidget->setWarpBrushSize(wSize);
            m_canvasWidget->setWarpBrushStrength(wStr);
            m_canvasWidget->setWarpMeshDivisions(wDiv);
            m_canvasWidget->setCageSubMode(cSub);
        }
    }
    settings.endGroup();

    if (m_toolBox) {
        m_toolBox->loadSlots(settings);
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (m_firstShow) {
        m_firstShow = false;

        // ドックウィジェットの表示を確実に保証
        if (m_navLayerDock && !m_navLayerDock->isVisible()) {
            m_navLayerDock->show();
        }
        if (m_colorBrushDock && !m_colorBrushDock->isVisible()) {
            m_colorBrushDock->show();
        }

        // 初期表示が完全に完了したため、設定保存ガードを解除
        m_isRestoringSettings = false;

        // クリーンな状態で現在の正常な配置を保存
        saveSettings();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) {
        saveSettings();
        QMainWindow::closeEvent(event);
    } else {
        event->ignore();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString localPath = urls.first().toLocalFile();
        if (!localPath.isEmpty()) {
            openFile(localPath);
            event->acceptProposedAction();
        }
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent*>(event);
        if (!keyEvent->isAutoRepeat()) {
            QWidget* focusW = QApplication::focusWidget();
            bool isTextInput = focusW && (qobject_cast<QLineEdit*>(focusW) ||
                                          qobject_cast<QTextEdit*>(focusW) ||
                                          qobject_cast<QPlainTextEdit*>(focusW) ||
                                          qobject_cast<QKeySequenceEdit*>(focusW));
            if (!isTextInput) {
                int key = keyEvent->key();
                if (key != Qt::Key_Shift && key != Qt::Key_Control && key != Qt::Key_Alt && key != Qt::Key_Meta && key != 0) {
                    QKeySequence seq(key | static_cast<int>(keyEvent->modifiers()));
                    QKeySequence plainSeq(key);
                    if (m_toolBox) {
                        if (m_toolBox->handleShortcutKey(seq) || m_toolBox->handleShortcutKey(plainSeq)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::maybeSave() {
    if (!m_document || !m_document->undoStack() || m_document->undoStack()->isClean()) {
        return true;
    }
    const auto ret = QMessageBox::warning(
        this, "PaintSoft KABURAGI",
        "キャンバスへの変更が保存されていません。\n保存しますか？",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );
    if (ret == QMessageBox::Save) {
        return onSaveCanvas();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    return true;
}

void MainWindow::setupDocument(Document* doc) {
    if (!doc) {
        if (m_document) {
            delete m_document;
            m_document = nullptr;
        }
        if (m_canvasWidget) m_canvasWidget->setDocument(nullptr);
        if (m_layerPanel) m_layerPanel->setDocument(nullptr);
        if (m_navigator) m_navigator->setDocument(nullptr);
        if (m_aiAssistantDock) m_aiAssistantDock->setDocument(nullptr);
        if (m_threeDSettings) m_threeDSettings->setDocument(nullptr);
        if (m_previewWindow) m_previewWindow->setDocument(nullptr);
        if (m_btnToggleSelectionEdit) {
            m_btnToggleSelectionEdit->blockSignals(true);
            m_btnToggleSelectionEdit->setChecked(false);
            m_btnToggleSelectionEdit->blockSignals(false);
        }
        if (m_tabWidget) {
            m_tabWidget->clear();
        }
        if (m_tabCloseBtn) {
            m_tabCloseBtn->setEnabled(false);
        }
        updateEditActions();
        updateLayerMenuActions();
        updateUndoRedoActions();
        updateWindowTitle();
        return;
    }

    if (m_document && m_document != doc) {
        if (m_canvasWidget) m_canvasWidget->setDocument(nullptr);
        if (m_layerPanel) m_layerPanel->setDocument(nullptr);
        if (m_navigator) m_navigator->setDocument(nullptr);
        if (m_aiAssistantDock) m_aiAssistantDock->setDocument(nullptr);
        if (m_previewWindow) m_previewWindow->setDocument(nullptr);
        delete m_document;
    }
    m_document = doc;
    if (m_canvasWidget) m_canvasWidget->setDocument(m_document);
    if (m_layerPanel) m_layerPanel->setDocument(m_document);
    if (m_navigator) m_navigator->setDocument(m_document);
    if (m_aiAssistantDock) m_aiAssistantDock->setDocument(m_document);
    if (m_threeDSettings) m_threeDSettings->setDocument(m_document);
    if (m_previewWindow) m_previewWindow->setDocument(m_document);

    if (m_document->undoStack()) {
        connect(m_document->undoStack(), &QUndoStack::canUndoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::canRedoChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::undoTextChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::redoTextChanged, this, &MainWindow::updateUndoRedoActions);
        connect(m_document->undoStack(), &QUndoStack::cleanChanged, this, &MainWindow::updateWindowTitle);
    }

    connect(m_document, &Document::activeLayerChanged, this, &MainWindow::updateLayerTools);
    connect(m_document, &Document::layerListChanged, this, &MainWindow::updateLayerTools);
    if (m_btnToggleSelectionEdit) {
        m_btnToggleSelectionEdit->blockSignals(true);
        m_btnToggleSelectionEdit->setChecked(m_document->isSelectionEditMode());
        m_btnToggleSelectionEdit->blockSignals(false);
    }
    connect(m_document, &Document::selectionEditModeChanged, this, [this](bool enabled) {
        if (m_btnToggleSelectionEdit && m_btnToggleSelectionEdit->isChecked() != enabled) {
            m_btnToggleSelectionEdit->blockSignals(true);
            m_btnToggleSelectionEdit->setChecked(enabled);
            m_btnToggleSelectionEdit->blockSignals(false);
        }
        updateLayerTools();
        if (m_canvasWidget) m_canvasWidget->update();
    });
    updateLayerTools();

    connect(m_document, &Document::activeLayerChanged, this, &MainWindow::updateEditActions);
    connect(m_document, &Document::layerListChanged, this, &MainWindow::updateEditActions);
    connect(m_document, &Document::selectionChanged, this, &MainWindow::updateEditActions);
    updateEditActions();

    connect(m_document, &Document::activeLayerChanged, this, &MainWindow::updateLayerMenuActions);
    connect(m_document, &Document::layerListChanged, this, &MainWindow::updateLayerMenuActions);
    connect(m_document, &Document::selectionChanged, this, &MainWindow::updateLayerMenuActions);
    updateLayerMenuActions();

    updateUndoRedoActions();
    updateWindowTitle();

    // 背景色切り替えボタンのツールチップ同期
    auto updateBgColorTip = [this]() {
        if (!m_btnSwitchBgColor || !m_document) return;
        QString activeName = m_document->isSecondaryBgColorActive() ? "第2背景色" : "第1背景色";
        QColor c1 = m_document->primaryBackgroundColor();
        QColor c2 = m_document->secondaryBackgroundColor();
        m_btnSwitchBgColor->setToolTip(QString("第1背景色⇔第2背景色 切り替え\n適用中: %1\n第1: %2, 第2: %3")
            .arg(activeName).arg(c1.name()).arg(c2.name()));
    };
    connect(m_document, &Document::backgroundColorChanged, this, updateBgColorTip);
    updateBgColorTip();

    // 中央タブのタイトル更新
    if (m_tabWidget) {
        QString docName = m_currentFilePath.isEmpty() ? "新規キャンバス.kab" : QFileInfo(m_currentFilePath).fileName();
        QString tabTitle = QString("%1 (%2x%3)").arg(docName).arg(m_document->width()).arg(m_document->height());
        if (m_tabWidget->count() == 0 && m_canvasContainer) {
            m_tabWidget->addTab(m_canvasContainer, tabTitle);
        } else {
            m_tabWidget->setTabText(0, tabTitle);
        }
    }
    if (m_tabCloseBtn) {
        m_tabCloseBtn->setEnabled(true);
    }
}

void MainWindow::updateWindowTitle() {
    if (!m_document) {
        setWindowTitle("PaintSoft KABURAGI 3.0");
        setWindowModified(false);
        return;
    }
    QString docName = m_currentFilePath.isEmpty() ? "無題" : QFileInfo(m_currentFilePath).fileName();
    setWindowTitle(QString("%1[*] - PaintSoft KABURAGI 3.0").arg(docName));
    if (m_document && m_document->undoStack()) {
        setWindowModified(!m_document->undoStack()->isClean());
    } else {
        setWindowModified(false);
    }
}

void MainWindow::addRecentFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }
    QSettings settings;
    settings.setValue("RecentFiles/list", m_recentFiles);
    updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu() {
    if (!m_recentFilesMenu) return;
    m_recentFilesMenu->clear();

    if (m_recentFiles.isEmpty()) {
        auto act = m_recentFilesMenu->addAction("履歴なし");
        act->setEnabled(false);
        return;
    }

    int count = 0;
    for (const QString& file : m_recentFiles) {
        count++;
        QString text = QString("&%1 %2").arg(count).arg(QFileInfo(file).fileName());
        auto act = m_recentFilesMenu->addAction(text, this, [this, file]() {
            openFile(file);
        });
        act->setToolTip(file);
    }

    m_recentFilesMenu->addSeparator();
    m_recentFilesMenu->addAction("履歴を消去(&C)", this, [this]() {
        m_recentFiles.clear();
        QSettings settings;
        settings.setValue("RecentFiles/list", m_recentFiles);
        updateRecentFilesMenu();
    });
}

void MainWindow::onPreferences() {
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        restartAutoBackupTimer();
        if (m_layerPanel) {
            m_layerPanel->updateScrollBarPosition();
        }
        if (m_aiAssistantDock) {
            m_aiAssistantDock->refreshLocalModels();
        }
    }
}

void MainWindow::restartAutoBackupTimer() {
    m_autoBackupTimer.stop();
    if (PreferencesDialog::isAutoBackupEnabled()) {
        int minutes = PreferencesDialog::autoBackupIntervalMinutes();
        m_autoBackupTimer.start(minutes * 60 * 1000);
    }
}

void MainWindow::onAutoBackupTimer() {
    if (!m_document || !m_document->undoStack() || m_document->undoStack()->isClean()) {
        return;
    }

    QString backupPath;
    if (!m_currentFilePath.isEmpty()) {
        QFileInfo fi(m_currentFilePath);
        backupPath = fi.absolutePath() + "/." + fi.completeBaseName() + "_backup.kab";
    } else {
        QString backupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
        QDir().mkpath(backupDir);
        backupPath = backupDir + "/untitled_backup.kab";
    }

    QString errorMsg;
    if (DocumentSerializer::saveToFile(m_document, backupPath, &errorMsg)) {
        statusBar()->showMessage("自動バックアップを保存しました", 3000);
    }
}

void MainWindow::onNewCanvas() {
    if (!maybeSave()) return;

    NewCanvasDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    m_currentFilePath.clear();
    auto doc = new Document(dlg.canvasWidth(), dlg.canvasHeight(), dlg.canvasDpi(), true, dlg.backgroundColor(), this);
    doc->setSecondaryBackgroundColor(dlg.secondaryBackgroundColor());
    setupDocument(doc);
    if (m_document->undoStack()) {
        m_document->undoStack()->setClean();
    }
    updateWindowTitle();
}

void MainWindow::onCloseCanvas() {
    if (!m_document) return;
    if (!maybeSave()) return;

    m_currentFilePath.clear();
    setupDocument(nullptr);
    statusBar()->showMessage(tr("キャンバスを閉じました。"), 3000);
}

bool MainWindow::onSaveCanvas() {
    if (!m_document) return false;
    if (m_currentFilePath.isEmpty()) {
        return onSaveAsCanvas();
    }
    QString errorMsg;
    if (DocumentSerializer::saveToFile(m_document, m_currentFilePath, &errorMsg)) {
        if (m_document->undoStack()) {
            m_document->undoStack()->setClean();
        }
        addRecentFile(m_currentFilePath);
        updateWindowTitle();
        if (m_tabWidget && m_tabWidget->count() > 0) {
            m_tabWidget->setTabText(0, QString("%1 (%2x%3)").arg(QFileInfo(m_currentFilePath).fileName()).arg(m_document->width()).arg(m_document->height()));
        }
        statusBar()->showMessage(QString("キャンバスを保存しました: %1").arg(m_currentFilePath), 4000);
        return true;
    } else {
        QMessageBox::critical(this, "保存エラー", QString("保存に失敗しました:\n%1").arg(errorMsg));
        return false;
    }
}

bool MainWindow::onSaveAsCanvas() {
    if (!m_document) return false;

    QString defaultPath = m_currentFilePath.isEmpty() ? "無題.kab" : m_currentFilePath;
    QString filter = "PaintSoft KABURAGI キャンバス (*.kab);;"
                     "PNG 画像 (*.png);;"
                     "JPEG 画像 (*.jpg *.jpeg);;"
                     "BMP 画像 (*.bmp);;"
                     "TIFF 画像 (*.tif *.tiff);;"
                     "WebP 画像 (*.webp);;"
                     "Photoshop 画像 (*.psd);;"
                     "すべてのファイル (*.*)";

    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this, "名前を付けて保存 / 画像エクスポート", defaultPath, filter, &selectedFilter
    );
    if (filePath.isEmpty()) {
        return false;
    }

    // 選択されたフィルタから拡張子を補正
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    if (selectedFilter.contains("*.kab")) {
        if (ext != "kab") filePath += ".kab";
    } else if (selectedFilter.contains("*.png")) {
        if (ext != "png") filePath += ".png";
    } else if (selectedFilter.contains("*.jpg")) {
        if (ext != "jpg" && ext != "jpeg") filePath += ".jpg";
    } else if (selectedFilter.contains("*.bmp")) {
        if (ext != "bmp") filePath += ".bmp";
    } else if (selectedFilter.contains("*.tif")) {
        if (ext != "tif" && ext != "tiff") filePath += ".tif";
    } else if (selectedFilter.contains("*.webp")) {
        if (ext != "webp") filePath += ".webp";
    } else if (selectedFilter.contains("*.psd")) {
        if (ext != "psd") filePath += ".psd";
    }

    fi = QFileInfo(filePath);
    ext = fi.suffix().toLower();

    if (ext == "kab") {
        m_currentFilePath = filePath;
        return onSaveCanvas();
    } else {
        // 画像エクスポート (PNG/JPEG/BMP/TIFF/WebP/PSD)
        QString errorMsg;
        if (DocumentSerializer::exportImage(m_document, filePath, &errorMsg)) {
            addRecentFile(filePath);
            statusBar()->showMessage(QString("画像をエクスポートしました: %1").arg(filePath), 4000);
            return true;
        } else {
            QMessageBox::critical(this, "エクスポートエラー", QString("画像の出力に失敗しました:\n%1").arg(errorMsg));
            return false;
        }
    }
}

void MainWindow::onOpenCanvas() {
    QString filter = "対応ファイル (*.kab *.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.psd);;"
                     "PaintSoft KABURAGI キャンバス (*.kab);;"
                     "すべての画像ファイル (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp *.psd);;"
                     "すべてのファイル (*.*)";
    QString filePath = QFileDialog::getOpenFileName(this, "キャンバスまたは画像を開く", QString(), filter);
    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void MainWindow::openFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    if (!maybeSave()) return;

    if (filePath.endsWith(".kab", Qt::CaseInsensitive)) {
        QString errorMsg;
        Document* doc = DocumentSerializer::loadFromFile(filePath, this, &errorMsg);
        if (!doc) {
            QMessageBox::critical(this, "読み込みエラー", QString("ファイルの読み込みに失敗しました:\n%1").arg(errorMsg));
            return;
        }

        m_currentFilePath = filePath;
        setupDocument(doc);
        if (m_document->undoStack()) {
            m_document->undoStack()->setClean();
        }
        addRecentFile(filePath);
        updateWindowTitle();
        statusBar()->showMessage(QString("キャンバスを開きました: %1").arg(filePath), 4000);
    } else {
        // 画像ファイルとしての読み込み (PNG/JPG/BMP/TIFF/WebP/PSD等)
        QImage img(filePath);
        if (img.isNull()) {
            QMessageBox::critical(this, "読み込みエラー", QString("画像ファイルを読み込めませんでした:\n%1").arg(filePath));
            return;
        }

        Document* doc = Document::fromQImage(img, 350.0, this);
        if (!doc) {
            QMessageBox::critical(this, "読み込みエラー", "画像のドキュメント化に失敗しました。");
            return;
        }

        m_currentFilePath.clear();
        setupDocument(doc);
        if (m_document->undoStack()) {
            m_document->undoStack()->setClean();
        }
        addRecentFile(filePath);
        updateWindowTitle();

        if (m_tabWidget && m_tabWidget->count() > 0) {
            m_tabWidget->setTabText(0, QString("%1 (%2x%3)").arg(QFileInfo(filePath).fileName()).arg(doc->width()).arg(doc->height()));
        }
        statusBar()->showMessage(QString("画像を開きました: %1").arg(filePath), 4000);
    }
}

void MainWindow::onUndo() {
    if (m_canvasWidget && m_canvasWidget->isDrawing()) return;
    if (m_document && m_document->undoStack()) {
        m_document->undoStack()->undo();
    }
}

void MainWindow::onRedo() {
    if (m_canvasWidget && m_canvasWidget->isDrawing()) return;
    if (m_document && m_document->undoStack()) {
        m_document->undoStack()->redo();
    }
}

void MainWindow::updateUndoRedoActions() {
    if (!m_undoAction || !m_redoAction) {
        return;
    }
    if (m_document && m_document->undoStack()) {
        m_undoAction->setEnabled(m_document->undoStack()->canUndo());
        m_redoAction->setEnabled(m_document->undoStack()->canRedo());
        QString uText = m_document->undoStack()->undoText();
        QString rText = m_document->undoStack()->redoText();
        m_undoAction->setToolTip(uText.isEmpty() ? "元に戻す (Ctrl+Z)" : QString("元に戻す: %1 (Ctrl+Z)").arg(uText));
        m_redoAction->setToolTip(rText.isEmpty() ? "やり直し (Ctrl+Y / Ctrl+Shift+Z)" : QString("やり直し: %1 (Ctrl+Y)").arg(rText));
    } else {
        m_undoAction->setEnabled(false);
        m_redoAction->setEnabled(false);
        m_undoAction->setToolTip("元に戻す (Ctrl+Z)");
        m_redoAction->setToolTip("やり直し (Ctrl+Y / Ctrl+Shift+Z)");
    }
}

void MainWindow::onCut() {
    if (!m_document || !m_document->hasSelection()) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->isLocked()) return;

    onCopy();
    if (m_canvasWidget) {
        m_canvasWidget->deleteSelectedPixels();
    }
    statusBar()->showMessage("選択範囲を切り取りました", 3000);
}

void MainWindow::onCopy() {
    QImage copyImg = copySelectionToImage();
    if (copyImg.isNull()) return;

    QGuiApplication::clipboard()->setImage(copyImg);
    updateEditActions();
    statusBar()->showMessage("選択範囲をコピーしました", 3000);
}

void MainWindow::onCopyVisible() {
    QImage copyImg = copyVisibleToImage();
    if (copyImg.isNull()) return;

    QGuiApplication::clipboard()->setImage(copyImg);
    updateEditActions();
    statusBar()->showMessage("可視部をコピーしました", 3000);
}

void MainWindow::onPaste() {
    QImage clipImg = QGuiApplication::clipboard()->image();
    if (clipImg.isNull()) return;

    if (!m_document) {
        // キャンバスがない場合：貼り付けた画像サイズのキャンバスを作成する
        Document* doc = Document::fromQImage(clipImg, 350.0, this);
        if (!doc) return;
        m_currentFilePath.clear();
        setupDocument(doc);
        if (m_document->undoStack()) {
            m_document->undoStack()->setClean();
        }
        updateWindowTitle();
        if (m_tabWidget && m_tabWidget->count() > 0) {
            m_tabWidget->setTabText(0, QString("新規キャンバス (%1x%2)").arg(doc->width()).arg(doc->height()));
        }
        statusBar()->showMessage("クリップボードから新規キャンバスを作成しました", 3000);
    } else {
        // キャンバスがある場合：新規通常レイヤーに貼り付ける
        int posX = 0;
        int posY = 0;
        if (m_document->hasSelection()) {
            QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
            posX = selBounds.left() + (selBounds.width() - clipImg.width()) / 2;
            posY = selBounds.top() + (selBounds.height() - clipImg.height()) / 2;
        } else {
            posX = std::max(0, (m_document->width() - clipImg.width()) / 2);
            posY = std::max(0, (m_document->height() - clipImg.height()) / 2);
        }

        QString layerName = QString("貼り付け %1").arg(m_document->layers().size() + 1);
        m_document->addRasterLayerWithImage(clipImg, layerName, QPoint(posX, posY));
        if (m_canvasWidget) {
            m_canvasWidget->update();
        }
        statusBar()->showMessage("新規通常レイヤーに貼り付けました", 3000);
    }
}

void MainWindow::updateEditActions() {
    bool hasSelection = m_document && m_document->hasSelection();
    auto activeL = m_document ? m_document->activeLayer() : nullptr;
    bool hasActiveLayer = (activeL != nullptr);
    bool isLayerUnlocked = activeL && !activeL->isLocked();

    if (m_copyAction) {
        m_copyAction->setEnabled(hasSelection && hasActiveLayer);
    }
    if (m_copyVisibleAction) {
        m_copyVisibleAction->setEnabled(hasSelection && hasActiveLayer);
    }
    if (m_cutAction) {
        m_cutAction->setEnabled(hasSelection && hasActiveLayer && isLayerUnlocked);
    }
    if (m_pasteAction) {
        bool hasClipImage = false;
        if (const QMimeData* mime = QGuiApplication::clipboard()->mimeData()) {
            hasClipImage = mime->hasImage();
        }
        m_pasteAction->setEnabled(hasClipImage);
    }
    updateDeletePixelsAction();
}

QImage MainWindow::copySelectionToImage() const {
    if (!m_document || !m_document->hasSelection()) return QImage();
    auto activeL = m_document->activeLayer();
    if (!activeL) return QImage();

    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return QImage();

    QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
    QRect targetRect = QRect(0, 0, docW, docH).intersected(selBounds);
    if (targetRect.isEmpty()) return QImage();

    QImage copyImg(targetRect.size(), QImage::Format_ARGB32);
    copyImg.fill(Qt::transparent);

    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            const Tile* tile = activeL->tileManager().getTile(tx, ty);
            if (!tile || !tile->isValid() || !tile->readBuffer()) continue;
            const uint32_t* srcBuf = reinterpret_cast<const uint32_t*>(tile->readBuffer());

            int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
            int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
            int px0 = std::max(targetRect.left(), tileX0);
            int px1 = std::min(targetRect.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
            int py0 = std::max(targetRect.top(), tileY0);
            int py1 = std::min(targetRect.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                int imgY = py - targetRect.top();
                uint32_t* dstScan = reinterpret_cast<uint32_t*>(copyImg.scanLine(imgY));

                for (int px = px0; px <= px1; ++px) {
                    if (!m_document->isPixelSelected(px, py)) continue;

                    int lx = px - tileX0;
                    int imgX = px - targetRect.left();
                    dstScan[imgX] = srcBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                }
            }
        }
    }
    return copyImg;
}

QImage MainWindow::copyVisibleToImage() const {
    if (!m_document || !m_document->hasSelection()) return QImage();

    int docW = m_document->width();
    int docH = m_document->height();
    if (docW <= 0 || docH <= 0) return QImage();

    QRect selBounds = m_document->selection().boundingRect().toAlignedRect();
    QRect targetRect = QRect(0, 0, docW, docH).intersected(selBounds);
    if (targetRect.isEmpty()) return QImage();

    QImage copyImg(targetRect.size(), QImage::Format_ARGB32);
    copyImg.fill(Qt::transparent);

    int minTx = targetRect.left() / TileMemoryPool::TILE_WIDTH;
    int maxTx = targetRect.right() / TileMemoryPool::TILE_WIDTH;
    int minTy = targetRect.top() / TileMemoryPool::TILE_HEIGHT;
    int maxTy = targetRect.bottom() / TileMemoryPool::TILE_HEIGHT;

    const auto& compBuffer = m_document->compositeBuffer();

    for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
            const Tile* tile = compBuffer.getTile(tx, ty);
            if (!tile || !tile->isValid() || !tile->readBuffer()) continue;
            const uint32_t* srcBuf = reinterpret_cast<const uint32_t*>(tile->readBuffer());

            int tileX0 = tx * TileMemoryPool::TILE_WIDTH;
            int tileY0 = ty * TileMemoryPool::TILE_HEIGHT;
            int px0 = std::max(targetRect.left(), tileX0);
            int px1 = std::min(targetRect.right(), tileX0 + static_cast<int>(TileMemoryPool::TILE_WIDTH) - 1);
            int py0 = std::max(targetRect.top(), tileY0);
            int py1 = std::min(targetRect.bottom(), tileY0 + static_cast<int>(TileMemoryPool::TILE_HEIGHT) - 1);

            for (int py = py0; py <= py1; ++py) {
                int ly = py - tileY0;
                int imgY = py - targetRect.top();
                uint32_t* dstScan = reinterpret_cast<uint32_t*>(copyImg.scanLine(imgY));

                for (int px = px0; px <= px1; ++px) {
                    if (!m_document->isPixelSelected(px, py)) continue;

                    int lx = px - tileX0;
                    int imgX = px - targetRect.left();
                    dstScan[imgX] = srcBuf[ly * TileMemoryPool::TILE_WIDTH + lx];
                }
            }
        }
    }
    return copyImg;
}

void MainWindow::onDeletePixels() {
    if (!m_document) return;
    if (!m_document->hasSelection()) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() != LayerType::Raster) return;

    if (m_canvasWidget) {
        m_canvasWidget->deleteSelectedPixels();
    }
}

void MainWindow::updateDeletePixelsAction() {
    if (!m_deletePixelsAction) return;
    bool canDelete = false;
    if (m_document && m_document->hasSelection()) {
        auto activeL = m_document->activeLayer();
        if (activeL && activeL->type() == LayerType::Raster && !activeL->isLocked()) {
            canDelete = true;
        }
    }
    m_deletePixelsAction->setEnabled(canDelete);
}

void MainWindow::onColorChanged(const QColor& color) {
    if (m_toolBox && m_toolBox->colorBox()) {
        m_toolBox->colorBox()->setCurrentActiveColor(color);
    }
    if (m_canvasWidget) {
        m_canvasWidget->setCurrentColor(color);
    }
    saveSettings();
}

void MainWindow::updateLayerTools() {
    if (m_document && m_toolBox) {
        if (m_document->isSelectionEditMode()) {
            m_toolBox->setLayerType(LayerType::Raster);
            if (m_currentTool == ToolType::VectorControlPoint ||
                m_currentTool == ToolType::VectorPolyline ||
                m_currentTool == ToolType::VectorCurve ||
                m_currentTool == ToolType::VectorLineEdit ||
                m_currentTool == ToolType::VectorColorChange ||
                m_currentTool == ToolType::VectorEllipse ||
                m_currentTool == ToolType::VectorCircle ||
                m_currentTool == ToolType::VectorPolygon ||
                m_currentTool == ToolType::VectorPressure ||
                m_currentTool == ToolType::Text ||
                m_currentTool == ToolType::TextBalloon ||
                m_currentTool == ToolType::PanelFrame ||
                m_currentTool == ToolType::PanelSplit ||
                m_currentTool == ToolType::ThreeD ||
                m_currentTool == ToolType::AdjustmentBrightnessContrast ||
                m_currentTool == ToolType::AdjustmentHueSaturation ||
                m_currentTool == ToolType::AdjustmentToneCurve ||
                m_currentTool == ToolType::AdjustmentLevels ||
                m_currentTool == ToolType::AdjustmentInvert ||
                m_currentTool == ToolType::AdjustmentGrayscale) {
                m_toolBox->setCurrentTool(ToolType::Pen);
            }
            onToolChanged(m_currentTool);
            return;
        }

        auto activeL = m_document->activeLayer();
        if (activeL) {
            m_toolBox->setLayerType(activeL->type());
            if (m_toolSettingsStack) {
                if (activeL->type() == LayerType::Adjustment) {
                    auto adjLayer = dynamic_cast<AdjustmentLayer*>(activeL.get());
                    if (adjLayer) {
                        m_adjustmentSettings->setAdjustmentLayer(adjLayer);
                        m_adjustmentSettings->updateHistogramFromSource(
                            m_document->compositeBuffer(),
                            QRect(0, 0, m_document->width(), m_document->height()));

                        ToolType targetTool = ToolType::AdjustmentBrightnessContrast;
                        switch (adjLayer->adjustmentType()) {
                        case AdjustmentType::BrightnessContrast: targetTool = ToolType::AdjustmentBrightnessContrast; break;
                        case AdjustmentType::HueSaturation: targetTool = ToolType::AdjustmentHueSaturation; break;
                        case AdjustmentType::ToneCurve: targetTool = ToolType::AdjustmentToneCurve; break;
                        case AdjustmentType::Levels: targetTool = ToolType::AdjustmentLevels; break;
                        case AdjustmentType::Invert: targetTool = ToolType::AdjustmentInvert; break;
                        case AdjustmentType::Grayscale: targetTool = ToolType::AdjustmentGrayscale; break;
                        }
                        m_toolBox->setCurrentTool(targetTool);
                    }
                    m_toolSettingsStack->setCurrentWidget(m_adjustmentSettings);
                } else if (m_currentTool == ToolType::Eraser) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorEraserSettings);
                    } else {
                        m_toolSettingsStack->setCurrentWidget(m_brushSettings);
                    }
                } else if (m_currentTool == ToolType::VectorControlPoint) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorControlPointSettings);
                    }
                } else if (m_currentTool == ToolType::VectorPolyline || m_currentTool == ToolType::VectorCurve) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorLineSettings);
                    }
                } else if (m_currentTool == ToolType::VectorLineEdit) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorChangeLineSettings);
                    }
                } else if (m_currentTool == ToolType::VectorColorChange) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorColorChangeSettings);
                    }
                } else if (m_currentTool == ToolType::VectorEllipse) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorEllipseSettings);
                    }
                } else if (m_currentTool == ToolType::VectorCircle) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorCircleSettings);
                    }
                } else if (m_currentTool == ToolType::VectorPolygon) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorPolygonSettings);
                    }
                } else if (m_currentTool == ToolType::VectorPressure) {
                    if (activeL->type() == LayerType::Vector) {
                        m_toolSettingsStack->setCurrentWidget(m_vectorPressureSettings);
                    }
                } else if (m_currentTool == ToolType::Text || m_currentTool == ToolType::TextBalloon) {
                    m_toolSettingsStack->setCurrentWidget(m_textSettings);
                } else if (activeL->type() == LayerType::ThreeD) {
                    auto threeD = dynamic_cast<ThreeDLayer*>(activeL.get());
                    if (threeD && m_threeDSettings) {
                        m_threeDSettings->setFromLayer(threeD);
                    }
                    if (m_toolBox) {
                        m_toolBox->setCurrentTool(ToolType::ThreeD);
                    }
                    m_toolSettingsStack->setCurrentWidget(m_threeDSettings);
                }
            }
        }
    }
}

void MainWindow::onToolChanged(ToolType tool) {
    // ツール設定UIの切り替え
    if (m_toolSettingsStack) {
        auto activeL = m_document ? m_document->activeLayer() : nullptr;
        bool isVector = (activeL && activeL->type() == LayerType::Vector);
        bool isAdjustment = (activeL && activeL->type() == LayerType::Adjustment);
        if (isAdjustment && (tool == ToolType::AdjustmentBrightnessContrast ||
                             tool == ToolType::AdjustmentHueSaturation ||
                             tool == ToolType::AdjustmentToneCurve ||
                             tool == ToolType::AdjustmentLevels ||
                             tool == ToolType::AdjustmentInvert ||
                             tool == ToolType::AdjustmentGrayscale)) {
            auto adjLayer = dynamic_cast<AdjustmentLayer*>(activeL.get());
            if (adjLayer) {
                m_adjustmentSettings->setAdjustmentLayer(adjLayer);
                AdjustmentType adjType = AdjustmentType::BrightnessContrast;
                switch (tool) {
                case ToolType::AdjustmentBrightnessContrast: adjType = AdjustmentType::BrightnessContrast; break;
                case ToolType::AdjustmentHueSaturation: adjType = AdjustmentType::HueSaturation; break;
                case ToolType::AdjustmentToneCurve: adjType = AdjustmentType::ToneCurve; break;
                case ToolType::AdjustmentLevels: adjType = AdjustmentType::Levels; break;
                case ToolType::AdjustmentInvert: adjType = AdjustmentType::Invert; break;
                case ToolType::AdjustmentGrayscale: adjType = AdjustmentType::Grayscale; break;
                default: break;
                }
                m_adjustmentSettings->setAdjustmentType(adjType);
                m_adjustmentSettings->updateHistogramFromSource(
                    m_document->compositeBuffer(),
                    QRect(0, 0, m_document->width(), m_document->height()));
            }
            m_toolSettingsStack->setCurrentWidget(m_adjustmentSettings);
        } else if (tool == ToolType::Eraser && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorEraserSettings);
        } else if (tool == ToolType::VectorControlPoint && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorControlPointSettings);
        } else if ((tool == ToolType::VectorPolyline || tool == ToolType::VectorCurve) && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorLineSettings);
        } else if (tool == ToolType::VectorLineEdit && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorChangeLineSettings);
        } else if (tool == ToolType::VectorColorChange && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorColorChangeSettings);
        } else if (tool == ToolType::VectorEllipse && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorEllipseSettings);
        } else if (tool == ToolType::VectorCircle && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorCircleSettings);
        } else if (tool == ToolType::VectorPolygon && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorPolygonSettings);
        } else if (tool == ToolType::VectorPressure && isVector) {
            m_toolSettingsStack->setCurrentWidget(m_vectorPressureSettings);
        } else if (tool == ToolType::RectSelect) {
            m_toolSettingsStack->setCurrentWidget(m_rectSelectSettings);
        } else if (tool == ToolType::EllipseSelect) {
            m_toolSettingsStack->setCurrentWidget(m_ellipseSelectSettings);
        } else if (tool == ToolType::LassoSelect) {
            m_toolSettingsStack->setCurrentWidget(m_lassoSelectSettings);
        } else if (tool == ToolType::ColorSelect) {
            m_toolSettingsStack->setCurrentWidget(m_colorSelectSettings);
        } else if (tool == ToolType::MagicWand) {
            m_toolSettingsStack->setCurrentWidget(m_autoSelectSettings);
        } else if (tool == ToolType::Bucket) {
            m_toolSettingsStack->setCurrentWidget(m_bucketSettings);
        } else if (tool == ToolType::Blur) {
            m_toolSettingsStack->setCurrentWidget(m_blurSettings);
        } else if (tool == ToolType::Blend) {
            m_toolSettingsStack->setCurrentWidget(m_smudgeSettings);
        } else if (tool == ToolType::Watercolor) {
            m_toolSettingsStack->setCurrentWidget(m_watercolorSettings);
        } else if (tool == ToolType::CompositeBrush) {
            m_toolSettingsStack->setCurrentWidget(m_compositeBrushSettings);
        } else if (tool == ToolType::Marker) {
            m_toolSettingsStack->setCurrentWidget(m_imageBrushSettings);
            if (m_imageBrushSettings) {
                m_imageBrushSettings->importFromClipboard(false);
            }
        } else if (tool == ToolType::Gradient) {
            m_toolSettingsStack->setCurrentWidget(m_gradientSettings);
        } else if (tool == ToolType::Transform) {
            m_toolSettingsStack->setCurrentWidget(m_transformSettings);
        } else if (tool == ToolType::Text || tool == ToolType::TextBalloon) {
            m_toolSettingsStack->setCurrentWidget(m_textSettings);
        } else if (tool == ToolType::PanelFrame || tool == ToolType::PanelSplit) {
            m_toolSettingsStack->setCurrentWidget(m_panelSettings);
        } else if (tool == ToolType::ThreeD) {
            m_toolSettingsStack->setCurrentWidget(m_threeDSettings);
        } else {
            m_toolSettingsStack->setCurrentWidget(m_brushSettings);
        }
    }

    if (tool == m_currentTool) return;

    // 1. 直前のツールのブラシ設定を記憶
    saveCurrentToolConfig();

    // 2. ツール切り替え
    m_currentTool = tool;
    if (m_canvasWidget) {
        m_canvasWidget->setCurrentTool(tool);
    }

    // 3. 切り替え先ツールの記憶済みブラシ設定を復元して適用
    auto it = m_toolConfigs.find(tool);
    if (it != m_toolConfigs.end()) {
        m_brushSettings->setAirbrushMode(tool == ToolType::Airbrush);
        m_brushSettings->setEraserMode(tool == ToolType::Eraser);
        m_brushSettings->setConfig(it->second);
        if (m_canvasWidget) {
            m_canvasWidget->setBrushBlendMode(it->second.blendMode);
            m_canvasWidget->setBrushSize(it->second.size);
            m_canvasWidget->setMinSizeRatio(it->second.minSizeRatio);
            m_canvasWidget->setBrushDensity(it->second.density);
            m_canvasWidget->setAntialias(it->second.antialias);
            m_canvasWidget->setPressureSizeEnabled(it->second.pressureSize);
            m_canvasWidget->setPressureDensityEnabled(it->second.pressureDensity);
            m_canvasWidget->setBrushHardness(it->second.hardness);
            m_canvasWidget->setBrushFalloff(it->second.falloff);
            m_canvasWidget->setBrushMinPressure(it->second.minPressure);
            m_canvasWidget->setAirbrushInterval(it->second.airbrushInterval);
            m_canvasWidget->setBrushScatter(it->second.scatter);
            m_canvasWidget->setBrushScatterAmount(it->second.scatterAmount);
            m_canvasWidget->setBrushScatterRange(it->second.scatterRange);
            m_canvasWidget->setBrushScatterSizeRandom(it->second.scatterSizeRandom);
            m_canvasWidget->setBrushScatterScaleUp(it->second.scatterScaleUp);
        }
    }

    if (tool == ToolType::Bucket && m_bucketSettings && m_canvasWidget) {
        m_canvasWidget->setBucketBlendMode(m_bucketSettings->blendMode());
    } else if (tool == ToolType::Watercolor && m_watercolorSettings && m_canvasWidget) {
        m_canvasWidget->setWatercolorBlendMode(m_watercolorSettings->blendMode());
        m_canvasWidget->setWatercolorScatter(m_watercolorSettings->isScatter());
        m_canvasWidget->setWatercolorScatterAmount(m_watercolorSettings->scatterAmount());
        m_canvasWidget->setWatercolorScatterRange(m_watercolorSettings->scatterRange());
        m_canvasWidget->setWatercolorScatterSizeRandom(m_watercolorSettings->scatterSizeRandom());
        m_canvasWidget->setWatercolorScatterScaleUp(m_watercolorSettings->isScatterScaleUp());
    } else if (tool == ToolType::CompositeBrush && m_compositeBrushSettings && m_canvasWidget) {
        m_canvasWidget->setCompositeBrushBlendMode(m_compositeBrushSettings->blendMode());
        m_canvasWidget->setCompositeBrushReference(m_compositeBrushSettings->referenceMode());
        m_canvasWidget->setCompositeBrushScatter(m_compositeBrushSettings->isScatter());
        m_canvasWidget->setCompositeBrushScatterAmount(m_compositeBrushSettings->scatterAmount());
        m_canvasWidget->setCompositeBrushScatterRange(m_compositeBrushSettings->scatterRange());
        m_canvasWidget->setCompositeBrushScatterSizeRandom(m_compositeBrushSettings->scatterSizeRandom());
        m_canvasWidget->setCompositeBrushScatterScaleUp(m_compositeBrushSettings->isScatterScaleUp());
    } else if (tool == ToolType::Marker && m_imageBrushSettings && m_canvasWidget) {
        m_canvasWidget->setImageBrushImage(m_imageBrushSettings->currentImage());
        m_canvasWidget->setImageBrushBlendMode(m_imageBrushSettings->blendMode());
        m_canvasWidget->setImageBrushSize(m_imageBrushSettings->brushSize());
        m_canvasWidget->setImageBrushMinSizeRatio(m_imageBrushSettings->minSizeRatio());
        m_canvasWidget->setImageBrushSpacing(m_imageBrushSettings->spacing());
        m_canvasWidget->setImageBrushDensity(m_imageBrushSettings->density());
        m_canvasWidget->setImageBrushMinPressure(m_imageBrushSettings->minPressure());
        m_canvasWidget->setImageBrushPressureSizeEnabled(m_imageBrushSettings->isPressureSizeEnabled());
        m_canvasWidget->setImageBrushPressureDensityEnabled(m_imageBrushSettings->isPressureDensityEnabled());
        m_canvasWidget->setImageBrushScatter(m_imageBrushSettings->isScatter());
        m_canvasWidget->setImageBrushScatterAmount(m_imageBrushSettings->scatterAmount());
        m_canvasWidget->setImageBrushScatterRange(m_imageBrushSettings->scatterRange());
        m_canvasWidget->setImageBrushScatterSizeRandom(m_imageBrushSettings->scatterSizeRandom());
        m_canvasWidget->setImageBrushScatterScaleUp(m_imageBrushSettings->isScatterScaleUp());
        m_canvasWidget->setImageBrushFlipH(m_imageBrushSettings->isFlipH());
        m_canvasWidget->setImageBrushFlipV(m_imageBrushSettings->isFlipV());
        m_canvasWidget->setImageBrushAngle(m_imageBrushSettings->angle());
        m_canvasWidget->setImageBrushFollowCursor(m_imageBrushSettings->isFollowCursor());
        m_canvasWidget->setImageBrushRandomRotate(m_imageBrushSettings->isRandomRotate());
        m_canvasWidget->setImageBrushRandomRotateAmount(m_imageBrushSettings->randomRotateAmount());
        m_canvasWidget->setImageBrushRandomSize(m_imageBrushSettings->isRandomSize());
        m_canvasWidget->setImageBrushRandomSizeAmount(m_imageBrushSettings->randomSizeAmount());
        m_canvasWidget->setImageBrushRandomSizeScaleUp(m_imageBrushSettings->isRandomSizeScaleUp());
    } else if (tool == ToolType::Gradient && m_gradientSettings && m_canvasWidget) {
        m_canvasWidget->setGradientBlendMode(m_gradientSettings->blendMode());
    }

    // 4. 設定の保存
    saveSettings();
}

void MainWindow::onSmoothLevelChanged(int index) {
    int level = m_smoothCombo->itemData(index).toInt();
    if (m_canvasWidget) {
        m_canvasWidget->setSmoothLevel(level);
    }
    saveSettings();
}

void MainWindow::resetDockLayout() {
    m_navLayerDock->setFloating(false);
    m_colorBrushDock->setFloating(false);
    addDockWidget(Qt::LeftDockWidgetArea, m_navLayerDock);
    addDockWidget(Qt::LeftDockWidgetArea, m_colorBrushDock);
    if (m_toolSplitter) {
        m_toolSplitter->setSizes({ 160, 240 });
    }
    if (m_navSplitter) {
        m_navSplitter->setSizes({ 205, 300 });
    }
    m_navLayerDock->show();
    m_colorBrushDock->show();
    saveSettings();
}

void MainWindow::updateLayerMenuActions() {
    bool hasDoc = (m_document != nullptr);
    if (m_newRasterLayerAction) m_newRasterLayerAction->setEnabled(hasDoc);
    if (m_newVectorLayerAction) m_newVectorLayerAction->setEnabled(hasDoc);
    if (m_newTextLayerAction) m_newTextLayerAction->setEnabled(hasDoc);
    if (m_newGroupLayerAction) m_newGroupLayerAction->setEnabled(hasDoc);
    if (m_newPanelFolderAction) m_newPanelFolderAction->setEnabled(hasDoc);
    if (m_newThreeDLayerAction) m_newThreeDLayerAction->setEnabled(hasDoc);
    if (m_newAdjustmentLayerAction) m_newAdjustmentLayerAction->setEnabled(hasDoc);
    if (m_newLayerFromVisibleAction) m_newLayerFromVisibleAction->setEnabled(hasDoc);

    auto activeL = hasDoc ? m_document->activeLayer() : nullptr;
    int activeIdx = hasDoc ? m_document->activeLayerIndex() : -1;
    int layerCount = hasDoc ? static_cast<int>(m_document->layers().size()) : 0;

    if (m_duplicateLayerAction) {
        m_duplicateLayerAction->setEnabled(hasDoc && activeL != nullptr);
    }
    if (m_mergeDownAction) {
        m_mergeDownAction->setEnabled(hasDoc && activeIdx > 0);
    }
    if (m_deleteLayerAction) {
        m_deleteLayerAction->setEnabled(hasDoc && layerCount > 1 && activeL != nullptr && activeL->name() != "背景");
    }
    if (m_selectOpaqueAction) {
        m_selectOpaqueAction->setEnabled(hasDoc && activeL != nullptr);
    }
    if (m_addOpaqueToSelectionAction) {
        m_addOpaqueToSelectionAction->setEnabled(hasDoc && activeL != nullptr);
    }
    if (m_rasterizeLayerAction) {
        bool canRasterize = activeL && (activeL->type() == LayerType::Vector || activeL->type() == LayerType::Text);
        m_rasterizeLayerAction->setEnabled(canRasterize);
    }
    if (m_fillLayerAction) {
        bool canFill = activeL && (activeL->type() == LayerType::Raster) && !activeL->isLocked();
        m_fillLayerAction->setEnabled(canFill);
    }
}

void MainWindow::onNewRasterLayer() {
    if (m_document) {
        m_document->addRasterLayer();
    }
}

void MainWindow::onNewVectorLayer() {
    if (m_document) {
        m_document->addVectorLayer();
    }
}

void MainWindow::onNewTextLayer() {
    if (m_document) {
        m_document->addTextLayer();
    }
}

void MainWindow::onNewGroupLayer() {
    if (m_document) {
        m_document->addGroupLayer();
    }
}

void MainWindow::onNewPanelFolder() {
    if (!m_document) return;
    int w = m_document->width();
    int h = m_document->height();
    int padX = std::max(20, w / 16);
    int padY = std::max(20, h / 16);
    QRectF r(padX, padY, w - padX * 2, h - padY * 2);
    QPolygonF poly;
    poly << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();

    float bWidth = m_panelSettings ? m_panelSettings->borderWidth() : 3.0f;
    QColor bColor = m_panelSettings ? m_panelSettings->borderColor() : Qt::black;
    auto panel = m_document->addPanelFolder(poly, bWidth, bColor);
    if (panel && m_panelSettings) {
        panel->setDrawPanelBorder(m_panelSettings->drawBorder());
        m_panelSettings->setFromPanel(panel.get());
    }
    if (m_toolBox) {
        m_toolBox->setCurrentTool(ToolType::PanelFrame);
    }
    m_document->composite();
    if (m_canvasWidget) m_canvasWidget->update();
}

void MainWindow::onNewThreeDLayer() {
    if (!m_document) return;
    auto l = m_document->addThreeDLayer(tr("3Dレイヤー"));
    if (l && m_threeDSettings) {
        m_threeDSettings->setFromLayer(l.get());
    }
    if (m_toolBox) {
        m_toolBox->setCurrentTool(ToolType::ThreeD);
    }
    m_document->composite();
    if (m_canvasWidget) m_canvasWidget->update();
}

void MainWindow::onNewLayerFromVisible() {
    if (m_document) {
        m_document->newLayerFromVisible();
    }
}

void MainWindow::onDuplicateLayer() {
    if (m_document && m_document->activeLayer()) {
        m_document->duplicateLayer(m_document->activeLayerIndex());
    }
}

void MainWindow::onMergeDown() {
    if (m_document && m_document->activeLayerIndex() > 0) {
        m_document->mergeDown(m_document->activeLayerIndex());
    }
}

void MainWindow::onDeleteLayer() {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->name() == "背景") return;
    if (m_document->layers().size() <= 1) return;
    m_document->removeLayer(m_document->activeLayerIndex());
}

void MainWindow::onSelectOpaque() {
    if (!m_document || !m_document->activeLayer()) return;
    QPainterPath opaquePath = m_document->createOpaquePath(m_document->activeLayerIndex());
    QPainterPath oldSel = m_document->selection();
    if (m_document->undoStack()) {
        auto cmd = new SelectCommand(m_document, oldSel, opaquePath);
        cmd->setText("不透明部分を選択範囲にする");
        m_document->undoStack()->push(cmd);
    } else {
        m_document->setSelection(opaquePath);
    }
    if (m_canvasWidget) {
        m_canvasWidget->update();
    }
}

void MainWindow::onAddOpaqueToSelection() {
    if (!m_document || !m_document->activeLayer()) return;
    QPainterPath opaquePath = m_document->createOpaquePath(m_document->activeLayerIndex());
    if (opaquePath.isEmpty()) return;
    QPainterPath oldSel = m_document->selection();
    QPainterPath newSel = oldSel.united(opaquePath);
    if (m_document->undoStack()) {
        auto cmd = new SelectCommand(m_document, oldSel, newSel);
        cmd->setText("不透明部分を選択範囲に加える");
        m_document->undoStack()->push(cmd);
    } else {
        m_document->setSelection(newSel);
    }
    if (m_canvasWidget) {
        m_canvasWidget->update();
    }
}

void MainWindow::onRasterizeLayer() {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || (activeL->type() != LayerType::Vector && activeL->type() != LayerType::Text)) return;
    m_document->rasterizeLayer(m_document->activeLayerIndex());
}

void MainWindow::onFillLayer() {
    if (!m_document) return;
    auto activeL = m_document->activeLayer();
    if (!activeL || activeL->type() != LayerType::Raster || activeL->isLocked()) return;

    QColor fgColor = Qt::black;
    if (m_canvasWidget) {
        fgColor = m_canvasWidget->currentColor();
    } else if (m_colorWheel) {
        fgColor = m_colorWheel->currentColor();
    }

    m_document->fillLayer(m_document->activeLayerIndex(), fgColor);
    if (m_canvasWidget) {
        m_canvasWidget->update();
    }
}

void MainWindow::onAiApplyAsNewLayer(const QImage& image) {
    if (!m_document || image.isNull()) return;

    QImage layerImg = image;
    QPoint offset(0, 0);

    if (m_document->hasSelection()) {
        QRect bounds = m_document->selectionBounds().toRect();
        if (bounds.isValid() && bounds.width() > 0 && bounds.height() > 0) {
            layerImg = image.scaled(bounds.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            offset = bounds.topLeft();
        }
    } else if (layerImg.size() != QSize(m_document->width(), m_document->height())) {
        layerImg = image.scaled(m_document->width(), m_document->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    auto newLayer = m_document->addRasterLayerWithImage(layerImg, tr("AI生成レイヤー"), offset);
    if (newLayer) {
        statusBar()->showMessage(tr("AI生成画像を新規レイヤーとして挿入しました。"), 4000);
    }
}

void MainWindow::onAiApplyOverwriteSelection(const QImage& image) {
    if (!m_document || image.isNull()) return;

    auto activeLayer = m_document->activeLayer();
    if (!activeLayer || activeLayer->type() != LayerType::Raster) {
        QMessageBox::warning(this, tr("AI画像適用"), tr("選択範囲への上書きは通常（ラスター）レイヤーでのみ利用可能です。"));
        return;
    }

    TileManager preTiles = activeLayer->tileManager().clone();

    QRect targetRect(0, 0, m_document->width(), m_document->height());
    QImage scaledImg = image;
    if (m_document->hasSelection()) {
        QRect bounds = m_document->selectionBounds().toRect();
        if (bounds.isValid() && bounds.width() > 0 && bounds.height() > 0) {
            targetRect = bounds;
            scaledImg = image.scaled(targetRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    } else {
        if (scaledImg.size() != QSize(m_document->width(), m_document->height())) {
            scaledImg = image.scaled(m_document->width(), m_document->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
    }

    int left = targetRect.left();
    int top = targetRect.top();
    int w = scaledImg.width();
    int h = scaledImg.height();

    for (int y = 0; y < h; ++y) {
        int docY = top + y;
        if (docY < 0 || docY >= m_document->height()) continue;
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(scaledImg.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            int docX = left + x;
            if (docX < 0 || docX >= m_document->width()) continue;

            if (m_document->hasSelection() && !m_document->isPixelSelected(docX, docY)) {
                continue;
            }

            activeLayer->tileManager().setPixel(docX, docY, static_cast<uint32_t>(srcLine[x]));
        }
    }

    activeLayer->invalidateContentBounds();
    activeLayer->updateContentBounds(targetRect);
    activeLayer->tileManager().markDirtyRect(targetRect);

    TileManager postTiles = activeLayer->tileManager().clone();

    if (m_document->undoStack()) {
        auto cmd = new DrawStrokeCommand(m_document, m_document->activeLayerIndex(), preTiles, postTiles, targetRect);
        cmd->setText(tr("AI生成結果の上書き"));
        m_document->undoStack()->push(cmd);
    }

    m_document->composite(targetRect);
    statusBar()->showMessage(tr("AI生成画像を選択範囲内に上書きしました。"), 4000);
}

void MainWindow::onFilterFocusBlur() {
    if (!m_document) return;
    FocusBlurDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("焦点ぼかし"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterGaussianBlur() {
    if (!m_document) return;
    GaussianBlurDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("ガウスぼかし"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterBrightnessContrast() {
    if (!m_document) return;
    BrightnessContrastDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("明るさ・コントラスト"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterHueSaturation() {
    if (!m_document) return;
    HueSaturationDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("色相・彩度・明度"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterToneCurve() {
    if (!m_document) return;
    ToneCurveDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("トーンカーブ"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterMotionBlur() {

    if (!m_document) return;
    MotionBlurDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("モーションぼかし"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::onFilterGradientMap() {
    if (!m_document) return;
    GradientMapDialog dlg(m_document, m_canvasWidget, this);
    if (!dlg.hasTargets()) {
        QMessageBox::information(this, tr("グラデーションマップ"), tr("フィルターを適用できる通常レイヤーが選択されていません。\n（通常レイヤーを選択するか、レイヤーをピン留めしてください）"));
        return;
    }
    dlg.exec();
}

void MainWindow::updateDisplayFilterActions() {
    auto type = DisplayFilterManager::instance().filterType();
    if (m_filterNoneAction) m_filterNoneAction->setChecked(type == DisplayFilterType::None);
    if (m_filterGrayscaleAction) m_filterGrayscaleAction->setChecked(type == DisplayFilterType::Grayscale);
    if (m_filterGrayscaleYiqAction) m_filterGrayscaleYiqAction->setChecked(type == DisplayFilterType::GrayscaleYIQ);
    if (m_filterIccAction) m_filterIccAction->setChecked(type == DisplayFilterType::IccProfile);
}

void MainWindow::onTogglePreviewWindow(bool checked) {
    if (!m_previewWindow) {
        m_previewWindow = new PreviewWindow(this);
        m_previewWindow->setDocument(m_document);
        connect(m_previewWindow, &PreviewWindow::windowClosed, this, [this]() {
            if (m_previewWindowAction) {
                m_previewWindowAction->setChecked(false);
            }
        });
    }

    if (checked) {
        m_previewWindow->setDocument(m_document);
        m_previewWindow->show();
        m_previewWindow->raise();
        m_previewWindow->activateWindow();
    } else {
        m_previewWindow->hide();
    }
}

void MainWindow::onToggleReferenceImageWindow(bool checked) {
    if (!m_referenceImageWindow) {
        m_referenceImageWindow = new ReferenceImageWindow(this);
        connect(m_referenceImageWindow, &ReferenceImageWindow::windowClosed, this, [this]() {
            if (m_referenceImageWindowAction) {
                m_referenceImageWindowAction->setChecked(false);
            }
        });
        connect(m_referenceImageWindow, &ReferenceImageWindow::colorPicked, this, &MainWindow::setDrawingColor);
        connect(m_referenceImageWindow, &ReferenceImageWindow::addToPalette, this, [this](const QColor& color) {
            if (m_paletteWidget) {
                m_paletteWidget->addColor(color);
            }
        });
    }

    if (checked) {
        m_referenceImageWindow->show();
        m_referenceImageWindow->raise();
        m_referenceImageWindow->activateWindow();
    } else {
        m_referenceImageWindow->hide();
    }
}

void MainWindow::setDrawingColor(const QColor& color) {
    if (!color.isValid()) return;
    if (m_toolBox && m_toolBox->colorBox()) {
        m_toolBox->colorBox()->setCurrentActiveColor(color);
    }
    if (m_colorWheel) {
        m_colorWheel->setColor(color);
    }
    if (m_canvasWidget) {
        m_canvasWidget->setCurrentColor(color);
    }
    if (m_paletteWidget) {
        m_paletteWidget->setCurrentColor(color);
    }
    saveSettings();
}

void MainWindow::onToggleSelectionEdit(bool checked) {
    if (!m_document) return;
    m_document->setSelectionEditMode(checked);
    updateLayerTools();
    if (m_canvasWidget) {
        m_canvasWidget->update();
    }
}

} // namespace Kaburagi

