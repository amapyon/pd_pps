# PD PPS 開発ワークスペース

CH32X035を使用したUSB Power Delivery（PD）／Programmable Power Supply（PPS）の実験用Visual Studio Codeワークスペースです。

**運用方針：** 2026年11月8日に旧ルート直下の実験25フォルダーを`firmware/experiments/`へ移行しました。過去の実験コードはスナップショットとして保持し、製品向け統合版は将来`firmware/product/`へ新規作成します（**現時点では存在しません**）。

このREADMEは **Gitリポジトリ `C:\pj\pd_pps\src\pd_pps` の直下** にあります。現在確認できるBuild/Flash補助スクリプトはリポジトリ内の`dev-workspace/pd-pps.ps1`です。旧運用で使っていた`C:\pj\pd_pps`直下のスクリプトやマルチルート`.code-workspace`は、このリポジトリのZIPには含まれていません。**標準手順はGitリポジトリをVS Codeで直接開く方法**です。

## 1. 開発環境

| 項目 | 内容 |
|---|---|
| OS | Windows 11 |
| エディター | Visual Studio Code（リポジトリ直下をフォルダーとして開く） |
| MCU | WeActStudio CH32X035 CoreBoard（CH32X035） |
| 書き込み・デバッグ | WCH-LinkE、ch32funの`minichlink` |
| ビルド | RISC-V GCC、GNU Make（xPack） |
| 操作 | Windows PowerShell 5.1、`pd-pps.ps1` |
| バージョン管理 | Git / GitHub（[`amapyon/pd_pps`](https://github.com/amapyon/pd_pps)） |

既存の`pd-pps.ps1`は、`tools/xpack-riscv-none-elf-gcc-14.2.0-3`と`tools/xpack-windows-build-tools-4.4.1-3`の配置を前提としています。ツールチェーンの変更時はスクリプト側のパスも確認してください。

## 2. フォルダー構成とGit管理の範囲

```text
C:\pj\pd_pps\                              # 開発環境のルート（Git管理外）
├── tools\                                # xPackコンパイラ・Make等（別途用意）
└── src\
    └── pd_pps\                          # Gitリポジトリ
        ├── .vscode\
        │   └── tasks.json               # 実験選択・PD17のVS Codeタスク
        ├── .gitignore
        ├── .gitmodules
        ├── ch32fun\                      # Gitサブモジュール
        ├── dev-workspace\               # 実行する開発補助スクリプト
        │   ├── pd-pps.ps1
        │   ├── organize-pdpps-workspace.ps1
        │   └── ...
        ├── docs\                        # 仕様・設計資料
        ├── firmware\
        │   └── experiments\             # 実験スナップショット25フォルダー
        │       ├── 01_led_blink\
        │       ├── ...
        │       ├── 07_pps_manual_voltage_echo\
        │       │   └── 07_pps_manual_voltage\  # 入れ子Makefile
        │       ├── 16b_source_caps_integrity\
        │       └── 17_pd_spr_sink_rebuild\
        └── README.md
```

- **現在の実行用：** `src\pd_pps\dev-workspace\pd-pps.ps1`。実験パスをリポジトリからの相対パスで指定します。
- **VS Code設定：** リポジトリ内の`.vscode/tasks.json`。固定PD17タスクと、25実験から選択する共通タスクがあります。
- **自動選択肢更新：** `dev-workspace/organize-pdpps-workspace.ps1`。通常はリポジトリ内の`.vscode/tasks.json`を対象とします。`-Mode Inspect`は読取専用、`-Mode Apply`はバックアップ後に変更します。
- **製品版：** `firmware/product/`は計画上の配置先で、まだ存在しません。実験17を製品版と誤認しないでください。

旧来の`C:\pj\pd_pps\pd_pps.code-workspace`を使用している場合のみ、整理スクリプトの`-WorkspaceFile`にそのパスを明示して従来のマルチルート形式を更新できます。**旧来の外側ファイルはこのGitリポジトリの管理対象ではありません。**

## 3. VS Codeで開く

次のように**Gitリポジトリのフォルダー自体**を開きます。

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
code .
```

VS Codeの「タスクの実行」で`.vscode/tasks.json`の設定を使用できます。タスクが参照する`${workspaceFolder}`は`src/pd_pps`を指し、`dev-workspace/pd-pps.ps1`を直接起動します。外側のマルチルートワークスペースやスクリプトのコピーは不要です。

実験の選択肢は現在25件登録済みです。新しいMakefileを追加した場合は後述の整理スクリプトで更新します。

## 4. ビルド・書き込み・ログ

### VS Codeのタスク

| 操作 | タスク名 | 動作 |
|---|---|---|
| 実験を選んでBuild | `PD PPS: Build (select project)` | 25実験から選択しBuild（書き込みなし） |
| 実験を選んでBuild・書き込み | `PD PPS: Build and flash (select project)` | 同一実験をBuildしてMCUへ書き込む |
| WCH-LinkE確認 | `PD PPS: Check WCH-LinkE` | 接続先の情報を表示（書き込みなし） |
| デバッグログ | `PD PPS: Debug log (minichlink -T)` | `debugprintf`ログを表示（`Ctrl+C`で終了） |
| 実験17固定 | `PD17: Build`／`PD17: Flash`／`PD17: Log` | 実験17の専用タスク |

Buildタスクは`Ctrl+Shift+B`（`Tasks: Run Build Task`）からも選択できます。プロジェクト選択は`${input:pdppsProject}`を利用し、`.vscode/tasks.json`に登録したMakefileのあるディレクトリを指定します。

**注意：** Flashは接続中MCUのファームウェアを上書きします。ログ端末`minichlink -T`を先に終了してください。実機へ書き込む前に対象実験と電源・負荷条件を必ず確認してください。

### PowerShellから直接実行

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'

# ビルドのみ
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\pd-pps.ps1 -Action Build `
  -Project firmware/experiments/11_pps_ina226_monitor

# ビルドして書き込み（接続するMCUを上書き）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\pd-pps.ps1 -Action Flash `
  -Project firmware/experiments/11_pps_ina226_monitor

# 接続確認／ログ（必要時のみ）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\pd-pps.ps1 -Action Probe
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\pd-pps.ps1 -Action Log
```

`-Project`はリポジトリ直下からの相対パスです。入れ子の実験の例は`firmware/experiments/07_pps_manual_voltage_echo/07_pps_manual_voltage`です。

## 5. 実験プロジェクトの管理

2026年11月1日の棚卸しでは、**Makefileを持つ実験プロジェクトが19個**検出されました（当時の記録）。2026年11月8日現在、実験スナップショット25フォルダーを`firmware/experiments`に配置しています。

| 系列 | 内容 |
|---|---|
| `01` | LED点滅 |
| `02`～`03` | USB PD固定電圧 |
| `04`～`08` | PPS制御、電流制限、手動指定、再接続 |
| `09`～`10` | ADCによる電圧測定・校正 |
| `11_pps_ina226_monitor` | INA226（R010）によるバス電圧・電流監視 |

`11_pps_ina226_monitor`ではINA226のI²Cアドレス`0x40`での認識、ID取得、設定書き込み、約7Vのバス電圧測定を確認しています。**既知負荷を使った電流測定精度の検証は未完了**です。

### 新しい実験を追加する手順

1. `src/pd_pps/firmware/experiments`に`NN_内容`の形式で番号付きフォルダーを作成する。
2. 近い実験のソース、ヘッダー、Makefileをコピーする。`TARGET`、依存ファイル、配線を確認し、ビルド生成物は引き継がない。
3. 実験フォルダーのREADMEに目的・配線・実行手順・測定条件と結果を記録する。
4. 実行用の`organize-pdpps-workspace.ps1`でVS Codeの選択肢を更新する。
5. ビルド確認後、必要な場合のみ書き込む。
6. `.vscode/tasks.json`の選択肢を整理スクリプトで更新し、差分を確認する。
7. 変更内容を確認して必要なファイルだけをGitにコミットする。

### プロジェクト選択リストの更新

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'

# 検出対象の確認のみ（書き込みなし）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\organize-pdpps-workspace.ps1 -Mode Inspect

# .vscode/tasks.jsonの実験選択肢を更新（変更前にバックアップ）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\organize-pdpps-workspace.ps1 -Mode Apply
```

探索対象は`firmware/experiments`内の番号付き実験のみです。入れ子のMakefileを含め、ch32funや無関係なMakefileは除外します。既存の選択肢と一致する場合、`Apply`は書き換えを行いません。変更があった場合だけ`C:\pj\pd_pps\workspace-backups`へバックアップします。**ソースコード・Makefile・MCUは変更しません。**

従来のマルチルートワークスペースを別途お持ちの場合は、`-WorkspaceFile 'C:\pj\pd_pps\pd_pps.code-workspace'`を指定すると、旧形式のプロジェクト一覧も更新できます。そのファイルはこのZIPには含まれていないため、存在を前提にしません。

## 6. 開発スクリプトとVS Code設定のGit管理

現在の標準運用では、`dev-workspace/*.ps1`と`.vscode/tasks.json`を**Gitリポジトリ内で直接管理**します。外側のスクリプトからコピーするための`capture-workspace.ps1`は、現行リポジトリに存在しません。旧READMEのコピー前提は使用しません。

整理スクリプトの`Apply`を実行して`.vscode/tasks.json`が変更された場合は、差分を確認してからコミットしてください。

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
git diff -- .vscode/tasks.json dev-workspace

# Git管理済みファイルの更新だけをステージング
git add -u -- .vscode/tasks.json dev-workspace

git diff --cached --check
git diff --cached --stat
# 内容を確認後にコミット・Pushする
```

`.gitignore`は`.vscode/`を除外しますが、**既に追跡されている**`.vscode/tasks.json`の更新には`git add -u`を使用できます。未追跡文書や開発途中の実験16bソースを巻き込む`git add -A`は避けてください。

## 7. 新しいPCでの開発環境の再構築

`C:\pj\pd_pps\src\pd_pps`にリポジトリを配置し、Git内のVS CodeタスクとPowerShellスクリプトを直接使用します。

### ① Gitリポジトリとサブモジュールを取得

```powershell
New-Item -ItemType Directory -Force 'C:\pj\pd_pps\src' | Out-Null
Set-Location 'C:\pj\pd_pps\src'
git clone --recurse-submodules https://github.com/amapyon/pd_pps.git
```

既存cloneのサブモジュールが空なら、リポジトリ直下で`git submodule update --init --recursive`を実行します。

### ② xPackツールチェーンを確認する

Build補助スクリプトは、`C:\pj\pd_pps\tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin`と`C:\pj\pd_pps\tools\xpack-windows-build-tools-4.4.1-3\bin`を前提とします。必要に応じて、リポジトリにある`dev-workspace/install-pd-pps-tools.ps1`を内容確認のうえ実行してください。USBドライバーおよびWCH-LinkEは別途確認が必要です。

### ③ リポジトリを直接開いてBuildする

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\pd-pps.ps1 -Action Build `
  -Project firmware/experiments/17_pd_spr_sink_rebuild
code .
```

Flashは実機の電源・負荷・安全条件を確認してから別途実行します。`firmware/product/`は現在存在せず、製品向け統合版のBuild対象ではありません。
## 8. Git運用（実験コードとREADME）

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
git status --short

# 例：新しい実験を追加した場合のみ、対象ディレクトリを明示
# git add -- firmware/experiments/NN_new_experiment

git diff --cached --check
git diff --cached --stat
# 内容を確認してコミット・Pushする
```

`NN_new_experiment`は例示名です。実験16bの未コミット変更や未追跡の引継ぎ・ROADMAP文書を、意図せずコミットしないでください。生成物・機密情報もステージングしないでください。
## 9. よくある確認事項

| 状況 | 確認箇所 |
|---|---|
| 新規実験が選択肢に出ない | `firmware/experiments/NN_.../Makefile`、整理スクリプトの`-Mode Inspect`と`Apply` |
| Buildできない | `dev-workspace/pd-pps.ps1`、xPackツール、Makefile、ch32funサブモジュール |
| `PD PPS`タスクが見当たらない | `src/pd_pps`自体をVS Codeで開いたか、`.vscode/tasks.json`を確認 |
| 書き込めない | WCH-LinkE接続、Probe、実行中のログ端末、対象ファームウェア |
| ログが表示されない | ファームウェアが`debugprintf`を使用しているか |
| `firmware/product/`がない | 製品版は未作成。過去の実験コードの移行とは別工程 |
