# PD PPS 開発ワークスペース

CH32X035を使用したUSB Power Delivery（PD）／Programmable Power Supply（PPS）の実験用VS Codeワークスペースです。

このワークスペースでは、各実験を独立したフォルダーとして残し、過去の実験を参照しながら次の実験を追加します。ソースコードを一つのプロジェクトに統合することは当面行いません。

## 1. 開発環境

| 項目 | 内容 |
|---|---|
| OS | Windows 11 |
| エディター | Visual Studio Code |
| MCU | CH32X035（WeActStudio CH32X035 CoreBoard） |
| 書き込み・デバッグ | WCH-LinkE、ch32funの`minichlink` |
| ビルド | RISC-V GCC、GNU Make（xPack） |
| 操作 | Windows PowerShell 5.1／`pd-pps.ps1` |
| バージョン管理 | Git（`src/pd_pps`がリポジトリ） |

ビルドツールのパスは `pd-pps.ps1` に定義されています。既存の設定では、`tools/xpack-riscv-none-elf-gcc-14.2.0-3` と `tools/xpack-windows-build-tools-4.4.1-3` を使用します。ツールのバージョンを変える場合は、同スクリプトのパスを確認してください。

## 2. フォルダー構成

    C:\pj\pd_pps\
    ├── README.md                         ← 本書
    ├── pd_pps.code-workspace             ← VS Codeワークスペース設定
    ├── pd-pps.ps1                        ← Build / Flash / Probe / Log
    ├── organize-pdpps-workspace.ps1     ← 実験一覧の検出・設定更新
    ├── workspace-backups\               ← ワークスペース設定のバックアップ
    ├── tools\                           ← 開発ツール
    └── src\
        └── pd_pps\                      ← Gitリポジトリ
            ├── ch32fun\                 ← ライブラリ・書き込みツール
            ├── 01_led_blink\
            ├── ...
            ├── 10_pps_adc_calibrated\
            └── 11_pps_ina226_monitor\

**注意：** `README.md`、`pd-pps.ps1`、`pd_pps.code-workspace` など、ワークスペース直下のファイルは `src/pd_pps` のGitリポジトリの外側です。Gitで共有・バックアップする場合は別途管理が必要です。

## 3. VS Codeで開く

PowerShellから次を実行します。

    cd C:\pj\pd_pps
    code .\pd_pps.code-workspace

VS Codeでは `pd_pps` フォルダーがエクスプローラーに表示されます。ビルド生成物（`.bin`、`.elf`、`.o` など）はワークスペース設定により非表示になり、通常の検索では生成物および `ch32fun` 配下が除外されます。ファイル自体は削除されません。

## 4. ビルド・書き込み・ログ

### VS Codeのタスク

| 操作 | 起動方法 | 動作 |
|---|---|---|
| ビルド | `Ctrl+Shift+B` → プロジェクトを選択 | 選択した実験をビルド（書き込みなし） |
| ビルド＋書き込み | `Ctrl+Shift+P` → `Tasks: Run Task` → `PD PPS: Build and flash (select project)` | ビルド後、接続中のMCUへ書き込み |
| 接続確認 | `Tasks: Run Task` → `PD PPS: Check WCH-LinkE` | デバッガーとMCUを確認（書き込みなし） |
| デバッグログ | `Tasks: Run Task` → `PD PPS: Debug log (minichlink -T)` | `debugprintf`端末を開く。終了は`Ctrl+C` |

**書き込みは接続中のMCUのファームウェアを上書きします。** デバッグログと書き込みがWCH-LinkEを取り合う場合は、先にログを終了してください。ログはCOMポートのシリアルモニターではなく、`minichlink -T` を利用します。

### PowerShellから直接実行

ワークスペース直下で実行します。

    cd C:\pj\pd_pps

    # ビルドのみ
    .\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor

    # ビルド後に書き込み
    .\pd-pps.ps1 -Action Flash -Project 11_pps_ina226_monitor

    # WCH-LinkE / MCUの確認
    .\pd-pps.ps1 -Action Probe

    # debugprintfログ表示（Ctrl+Cで終了）
    .\pd-pps.ps1 -Action Log

PowerShellでスクリプト実行が制限されている場合は、次の形式で実行できます。

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor

ビルド・書き込み時の `-Project` には、`src/pd_pps` からの相対パスを指定します。例えば、入れ子の実験は `07_pps_manual_voltage_echo/07_pps_manual_voltage` と指定します。

## 5. 実験プロジェクトの管理

2026年11月1日の棚卸しでは、**Makefileを持つ実験プロジェクトが19個**検出されました。主な系列は以下のとおりです。

| 系列 | 内容 |
|---|---|
| `01` | LED点滅 |
| `02`～`03` | USB PD固定電圧 |
| `04`～`08` | PPS制御、電流制限、手動指定、再接続 |
| `09`～`10` | ADCによる電圧測定・校正 |
| `11_pps_ina226_monitor` | INA226（R010）によるバス電圧・電流監視 |

実験フォルダーは履歴として維持します。過去のフォルダーを上書きせず、新しい試みには `12_内容`、`13_内容` のような名前で新しいフォルダーを作成します。`11_pps_ina226_monitor` ではINA226のI²Cアドレス`0x40`で認識し、バス電圧の取得を確認済みです。**負荷電流の精度検証は別途必要です。**

### 新しい実験を追加する手順

1. `src/pd_pps` 内に、新しい番号付きフォルダーを作成する。
2. 目的に近い実験から必要なソースコード、ヘッダー、Makefileをコピーする。`TARGET`や依存ファイルを確認し、古いビルド生成物を引き継がない。
3. 新しい実験フォルダーのREADMEに、目的・配線・実行方法・結果を記録する。
4. `organize-pdpps-workspace.ps1` でVS Codeのプロジェクト選択リストを更新する。
5. 新しい実験をビルドし、必要な場合のみ書き込む。
6. 変更が妥当な段階でGitにコミットする。

### プロジェクト選択リストの更新

まず変更なしの調査を実施します。

    cd C:\pj\pd_pps
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\organize-pdpps-workspace.ps1 -Mode Inspect

検出内容を確認後、更新します。

    powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\organize-pdpps-workspace.ps1 -Mode Apply

`Apply` は既存のワークスペース設定を `workspace-backups` にバックアップし、Makefileを持つ実験の選択肢を更新します。ソースコード・Makefileの変更やMCUへの書き込みは行いません。完了後はVS Codeでワークスペースを開き直してください。

## 6. Git運用

Gitリポジトリは `C:\pj\pd_pps\src\pd_pps` です。

    cd C:\pj\pd_pps\src\pd_pps
    git status
    git add 12_new_experiment
    git diff --cached --stat
    git commit -m "Add new PPS experiment"

`12_new_experiment` は例示名です。実際の実験名に置き換えてください。コミット前に生成物が含まれていないか確認します。GitHubへの反映には別途 `git push` が必要です。

## 7. よくある確認事項

| 状況 | 確認箇所 |
|---|---|
| 新規実験が選択肢に出ない | フォルダー名が`NN_`形式か、Makefileがあるか、整理スクリプトを`Apply`したか |
| ビルドできない | `pd-pps.ps1`のツールパス、Makefile、`TARGET`、エラーログ |
| 書き込めない | WCH-LinkE接続、`Probe`の結果、ログ端末が稼働中でないか |
| ログが表示されない | 対象ファームウェアが`debugprintf`を使用しているか |
| INA226が認識されない | VCC/GND、PA2=SCL、PA3=SDA、I²Cアドレス`0x40`、GPIO解放処理 |
| INA226のバス電圧が0V | VBS端子が測定対象の電圧ラインへ接続されているか |

## 8. このワークスペースの運用原則

- **実験を消さない：** 動作確認済みの実験は原則保存し、新しい実験は別フォルダーで進める。
- **ビルドと書き込みを分ける：** まずビルドのみで確認し、書き込みは意図したときだけ行う。
- **変更前に戻れる状態を確保：** ソースはGit、ワークスペース設定はバックアップを利用する。
- **測定値と条件を記録する：** 使用した電源・接続・設定値・測定結果を各実験のREADMEに残す。
- **整理は設定中心に：** 当面は既存フォルダーの移動やコードの大規模な統合を行わない。
