# PaintSoft KABURAGI 3.0

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)]()
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C.svg?logo=cplusplus)]()
[![Qt 6.8](https://img.shields.io/badge/Framework-Qt%206.8-41CD52.svg?logo=qt)]()

[English](#english) | [日本語](#japanese)

---

<a name="japanese"></a>
## 概要 (Japanese)

**PaintSoft KABURAGI** は、C++20 と Qt 6 を用いて開発された、高速・高機能なオープンソースのイラスト・漫画・グラフィック制作ソフトウェアです。

広大なキャンバスでもメモリ消費を抑え、なめらかで低遅延な描画体験を提供する「タイル型メモリプール（タイルグラフィックスエンジン）」を核に、SAI風ベクター制御点編集、3Dモデル（MMD/VRM/VRMA/物理演算）連携、漫画コマ割り・フキダシ・集中線・トーン生成、CMYKネイティブ/プルーフ対応、各種調整レイヤーなど、プロユースに耐えうる充実した機能を備えています。

---

### 主な機能・特徴

#### 🚀 描画エンジン・パフォーマンス
- **タイル型グラフィックスエンジン (64x64 Tile Pool)**:
  - ゼロアロケーション・メモリ再利用アーキテクチャにより、大判イラスト描画時もメモリ断片化やGCスパイクを防止。
  - レイヤー合成・ダーティ矩形レンダリングの最適化による低遅延な描画レスポンス。
- **多彩な描画ツール**:
  - Gペン、丸ペン、水彩筆、平筆、エアブラシ、鉛筆、マーカー、消しゴム、ぼかし、指先ツール。
  - 手ブレ補正（スムージング）、筆圧感知対応。

#### ✒️ ベクター線画レイヤー (SAI風制御点)
- **ベクターパス描画 & 制御点編集**:
  - 描いた線を後から制御点単位で移動・変形可能。
  - 線の太さ修正（太く/細く）、線の引き直し、角の丸め・尖らせ。

#### 🧊 3Dモデル・ポージング連携
- **MMD (.pmx) & VRM (.vrm) ネイティブ対応**:
  - 3Dモデルの配置、ボーンポージング、IK操作、マニピュレータによる直感的な姿勢制御。
  - VRMA (.vrma) モーション再生および物理演算シミュレーション。
  - 3Dレイヤーのキャンバス直接描画 & 2Dラスタライズ。

#### 📖 漫画・同人誌制作支援機能
- **コマ割りツール**: ドラッグでコマ枠を分割、枠線の太さ・余白設定。
- **フキダシツール**: 楕円・矩形・しっぽ形状の自動生成。
- **効果線・集中線フィルター**: 放射状・流線集中線の自動パラメトリック生成。
- **スクリーントーン化**: ドット、線、万線、網点トーンのリアルタイムプレビューと線数・角度制御。

#### 🎨 カラーマネジメント & カラーモード
- **CMYKネイティブ / CMYKプルーフ / RGB**:
  - 用途に応じたカラーモード切替（RGB / CMYKプルーフ / CMYKネイティブ）。
  - ICCプロファイル適用による印刷向け色再現。
- **HUDカラーピッカー & 各種パレット**:
  - キャンバス上で素早く色を選択できるHUDピッカー。
  - スウォッチパレット、履歴、スポイト。

#### ⚙️ レイヤーシステム & 調整レイヤー
- **豊富なブレンドモード**: 乗算、加算（発光）、スクリーン、オーバーレイ、比較（明/暗）、覆い焼き、焼き込みなど。
- **非破壊調整レイヤー**: 色相・彩度・明度、トーンカーブ、グラデーションマップ、2値化など。
- **クリッピングマスク / アルファロック / レイヤーマスク / 0〜255グラデーション選択範囲マスク**。

#### 📐 キャンバス変形 & 補助機能
- **キャンバス変形**: サイズ変更（解像度・アンカー指定）、拡大縮小、90°回転（時計回り/反時計回り）、180°回転、左右反転、上下反転。
- **多言語対応**: 日本語・英語に対応（システム言語を自動検出し、環境設定でいつでも切替可能）。
- **参照画像ウィンドウ**: 作業用サブディスプレイや資料表示に最適な独立フロートウィンドウ。
- **コマンドライン連携**: 起動時引数で画像ファイルを渡すことで自動オープン。

---

### 動作環境・必要要件

- **OS**: Windows 10 / 11 (64-bit)
- **コンパイラ**: GCC / MinGW-w64 13.1 以上 (C++20 サポート必須) または MSVC 2022
- **フレームワーク**: Qt 6.8.0 以上 (Qt Widgets, Qt OpenGL, Qt Multimedia, etc.)
- **ビルドツール**: CMake 3.20 以上, Ninja (推奨)

---

### ビルド手順

#### 1. リポジトリのクローン
```bash
git clone https://github.com/umibps/PaintSoftKABURAGI3.git
cd PaintSoftKABURAGI3
```

#### 2. 環境変数の設定 (MinGW / Qt 6 の場合)
```powershell
$env:PATH = "C:\Qt\6.8.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
```

#### 3. ビルドと実行
```powershell
# 設定
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# ビルド
cmake --build build

# 実行
.\build\PaintSoftKABURAGI.exe
```

---

<a name="english"></a>
## Overview (English)

**PaintSoft KABURAGI** is a high-performance, open-source illustration and manga creation software built with modern C++20 and Qt 6.

Designed with a low-latency "Tile-Based Graphics Engine" that eliminates memory fragmentation and avoids runtime allocations, KABURAGI provides smooth, responsive digital painting even on large-scale canvases. It features SAI-style vector stroke editing, 3D character integration (MMD/VRM/VRMA with physics), comic framing tools, CMYK native/proof color spaces, adjustment layers, and robust transform operations.

---

### Key Features

- **Tile-Based Memory Engine**: 64x64 tile pool architecture ensuring zero memory fragmentation and fluid brush strokes.
- **Expressive Brushes**: G-pen, Maru-pen, Water color, Flat brush, Airbrush, Pencil, Marker, Smudge, and Blur with stabilization.
- **Vector Control Points (SAI-style)**: Post-editing line geometry, line weight (thick/thin), and reshape points.
- **3D Layer Integration**: Native support for MMD (`.pmx`) and VRM (`.vrm`) character models with posing gizmos, VRMA animation playback, and bone physics.
- **Manga & Comic Workflow**: Frame divider, Speech balloons, Speed lines (focus/radiation), and Screentone half-toning.
- **Color Management**: Seamless switching between RGB, CMYK Soft-Proofing, and CMYK Native painting with ICC support.
- **Adjustment Layers**: Non-destructive adjustments including Tone Curves, Gradient Maps, Hue/Saturation, and Thresholding.
- **Canvas Transformations**: Canvas resizing (anchor-aware), scaling, 90°/180° rotations, and horizontal/vertical flips.
- **Internationalization (i18n)**: English and Japanese UI support with automatic OS language detection.

---

### Building from Source

#### Prerequisites
- Windows 10 / 11 (64-bit)
- C++20 compliant compiler (GCC/MinGW 13.1+ or MSVC 2022+)
- Qt 6.8.0 or newer
- CMake 3.20+ and Ninja

#### Build Steps
```powershell
# Clone the repository
git clone https://github.com/umibps/PaintSoftKABURAGI3.git
cd PaintSoftKABURAGI3

# Configure & Build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Launch
.\build\PaintSoftKABURAGI.exe
```

---

## ライセンス / License

本ソフトウェアのソースコードは **MIT License** の下で公開されています。詳細は [LICENSE](LICENSE) ファイルをご覧ください。

This software is released under the **MIT License**. See the [LICENSE](LICENSE) file for details.

### Third-Party Licenses (サードパーティライセンス)

- **Qt Toolkit**: 
  - 本ソフトウェアは The Qt Company の Qt 6 (LGPLv3) を動的リンク（Dynamic Linking）で使用しています。
  - Qt is licensed under the GNU Lesser General Public License version 3 (LGPLv3).
  - Qt のソースコードおよび詳細については [https://www.qt.io/](https://www.qt.io/) を参照してください。
  - LGPLv3 の条項に基づき、利用者は本ソフトウェアに付属する Qt の共有ライブラリ（DLL）を同一または互換性のある別のバージョンの Qt ライブラリと差し替えて動作させることが可能です。
