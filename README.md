# PD PPS 開発ワークスペース

CH32X035を使用したUSB Power Delivery（PD）／Programmable Power Supply（PPS）の実験用Visual Studio Codeワークスペースです。

**運用方針（方針A）：** 各実験を独立した番号付きフォルダーとして残し、過去の実験を参照しながら次の実験を追加します。当面、既存プロジェクトの大規模な移動・統合は行いません。

このREADMEは **Gitリポジトリ `src/pd_pps` の直下** にあります。日常的に使うワークスペース設定やPowerShellスクリプトは、従来の場所で実行しつつ、`dev-workspace/` にコピーして同じGitリポジトリで履歴管理します。

## 1. 開発環境

| 項目 | 内容 |
|---|---|
| OS | Windows 11 |
| エディター | Visual Studio Code（マルチルートワークスペース） |
| MCU | WeActStudio CH32X035 CoreBoard（CH32X035） |
| 書き込み・デバッグ | WCH-LinkE、ch32funの`minichlink` |
| ビルド | RISC-V GCC、GNU Make（xPack） |
| 操作 | Windows PowerShell 5.1、`pd-pps.ps1` |
| バージョン管理 | Git / GitHub（[`amapyon/pd_pps`](https://github.com/amapyon/pd_pps)） |

既存の`pd-pps.ps1`は、`tools/xpack-riscv-none-elf-gcc-14.2.0-3`と`tools/xpack-windows-build-tools-4.4.1-3`の配置を前提としています。ツールチェーンの変更時はスクリプト側のパスも確認してください。

## 2. フォルダー構成とGit管理の範囲

```text
C:\pj\pd_pps\                              # ワークスペース全体（Git管理外）
├── pd_pps.code-workspace                  # 日常使用するVS Code設定
├── pd-pps.ps1                            # Build / Flash / Probe / Log
├── organize-pdpps-workspace.ps1          # 実験選択リストを更新
├── setup-workspace-view.ps1              # マルチルート表示の初期設定
├── install-pd-pps-tools.ps1              # 開発ツール導入用
├── install-zadig.ps1                     # USBドライバー関連の補助
├── env.ps1                               # ローカル環境用（通常Git管理しない）
├── .vscode\
│   └── settings.json                     # Workspace Tools側の表示設定
├── workspace-backups\                    # 設定変更前のバックアップ
├── tools\                                # コンパイラ等の実行ファイル
└── src\
    └── pd_pps\                          # Gitリポジトリ
        ├── README.md                     # 本書（Git管理対象）
        ├── .gitignore
        ├── .gitmodules
        ├── ch32fun\                      # Gitサブモジュール
        ├── 01_led_blink\
        ├── ...
        ├── 10_pps_adc_calibrated\
        ├── 11_pps_ina226_monitor\
        └── dev-workspace\               # 開発環境設定のGit管理用コピー
            ├── capture-workspace.ps1
            ├── pd_pps.code-workspace
            ├── pd-pps.ps1
            ├── organize-pdpps-workspace.ps1
            ├── setup-workspace-view.ps1
            ├── install-pd-pps-tools.ps1
            ├── install-zadig.ps1
            └── .vscode\
                └── settings.json
```

**二つの場所の役割に注意してください。**

- **実行用（`C:\pj\pd_pps\`）：** VS Codeが開く`.code-workspace`と、実際に実行する`*.ps1`。通常はこちらを編集します。
- **Git管理用（`src\pd_pps\dev-workspace\`）：** 実行用ファイルのスナップショット。`capture-workspace.ps1`で実行用ファイルをコピーして更新します。
- **Gitリポジトリ本体（`src\pd_pps\`）：** 実験ソース、README、`dev-workspace`。`git status`・`git commit`・`git push`はここで実行します。

実行用とGit管理用は**自動同期されません**。設定を変更したら、コピーしてからコミットしてください。`tools/`、`workspace-backups/`、通常の`env.ps1`は管理用コピーに含めません。

## 3. VS Codeで開く

PowerShellから次を実行します。

```powershell
cd C:\pj\pd_pps
code .\pd_pps.code-workspace
```

エクスプローラーには、以下の2ルートが表示されます。

| ルート | 内容 |
|---|---|
| `pd_pps` | Git管理下の実験プロジェクト、README、`dev-workspace` |
| `Workspace Tools` | 実行用PowerShellスクリプト、ワークスペース設定など |

`Workspace Tools`では`.vscode/settings.json`により`src`、`tools`、`workspace-backups`を非表示にし、二重表示や不要なファイルを減らしています。ワークスペース全体の設定では、`.bin`、`.elf`、`.o`などのビルド生成物を非表示にし、通常検索から生成物や`ch32fun`を除外します。**非表示は削除ではありません。**

## 4. ビルド・書き込み・ログ

### VS Codeのタスク

| 操作 | 起動方法 | 動作 |
|---|---|---|
| ビルド | `Ctrl+Shift+B` → 実験を選択 | ビルドのみ（書き込みなし） |
| ビルド＋書き込み | `Ctrl+Shift+P` → `Tasks: Run Task` → `PD PPS: Build and flash (select project)` | ビルド後にMCUへ書き込み |
| 接続確認 | `Tasks: Run Task` → `PD PPS: Check WCH-LinkE` | WCH-LinkE / MCU情報を表示 |
| デバッグログ | `Tasks: Run Task` → `PD PPS: Debug log (minichlink -T)` | `debugprintf`端末を開く（終了は`Ctrl+C`） |

既存のワークスペースは`pd_pps`と`Workspace Tools`のマルチルート構成です。タスクからのソース参照には`${workspaceFolder:pd_pps}`を使用します。プロジェクト選択は番号付き実験のMakefileを検出して更新します。

**注意：** Flashは接続中のMCUのファームウェアを上書きします。`minichlink -T`を開いている場合は、書き込みの前に終了してください。ログは通常のCOMポートシリアルモニターではありません。

### PowerShellから直接実行

```powershell
cd C:\pj\pd_pps

# ビルドのみ
.\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor

# ビルドして書き込み
.\pd-pps.ps1 -Action Flash -Project 11_pps_ina226_monitor

# WCH-LinkE / MCUの確認
.\pd-pps.ps1 -Action Probe

# debugprintfログ表示（Ctrl+Cで終了）
.\pd-pps.ps1 -Action Log
```

PowerShellの実行ポリシーにより直接実行できない場合：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor
```

`-Project`には`src/pd_pps`からの相対パスを指定します。入れ子の例は`07_pps_manual_voltage_echo/07_pps_manual_voltage`です。

## 5. 実験プロジェクトの管理

2026年11月1日の棚卸しでは、**Makefileを持つ実験プロジェクトが19個**検出されました。

| 系列 | 内容 |
|---|---|
| `01` | LED点滅 |
| `02`～`03` | USB PD固定電圧 |
| `04`～`08` | PPS制御、電流制限、手動指定、再接続 |
| `09`～`10` | ADCによる電圧測定・校正 |
| `11_pps_ina226_monitor` | INA226（R010）によるバス電圧・電流監視 |

`11_pps_ina226_monitor`ではINA226のI²Cアドレス`0x40`での認識、ID取得、設定書き込み、約7Vのバス電圧測定を確認しています。**既知負荷を使った電流測定精度の検証は未完了**です。

### 新しい実験を追加する手順

1. `src/pd_pps`に`12_内容`、`13_内容`のような番号付きフォルダーを作成する。
2. 近い実験のソース、ヘッダー、Makefileをコピーする。`TARGET`、依存ファイル、配線を確認し、ビルド生成物は引き継がない。
3. 実験フォルダーのREADMEに目的・配線・実行手順・測定条件と結果を記録する。
4. 実行用の`organize-pdpps-workspace.ps1`でVS Codeの選択肢を更新する。
5. ビルド確認後、必要な場合のみ書き込む。
6. ワークスペース設定が変わった場合は`capture-workspace.ps1`でGit管理用コピーを更新する。
7. 変更内容を確認してGitにコミットする。

### プロジェクト選択リストの更新

```powershell
cd C:\pj\pd_pps

# 調査のみ（ファイル変更なし）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\organize-pdpps-workspace.ps1 -Mode Inspect

# 実験の選択肢を更新（変更前に設定をバックアップ）
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\organize-pdpps-workspace.ps1 -Mode Apply
```

`Apply`は`pd_pps.code-workspace`を更新します。**ソースコード・Makefileの移動やMCUへの書き込みは行いません。** 完了後はVS Codeでワークスペースを開き直してください。

## 6. ワークスペース設定・スクリプトのGit管理

### 実行用ファイルをGit管理用コピーへ反映

実行用のPowerShellスクリプト、`.code-workspace`、`.vscode/settings.json`を変更した後に実行します。

```powershell
cd C:\pj\pd_pps\src\pd_pps

powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\dev-workspace\capture-workspace.ps1

git status --short
git diff -- dev-workspace
```

`capture-workspace.ps1`は**ワークスペース全体のルートから`dev-workspace/`への一方向コピー**です。ソースコードやMakefileには触れず、元の実行用ファイルも変更しません。`env.ps1`は通常コピーしません（環境固有の情報や秘密情報が含まれていないかを確認する必要があります）。

### 変更をコミットし、GitHubへ反映

```powershell
cd C:\pj\pd_pps\src\pd_pps

git add dev-workspace
git diff --cached --check
git diff --cached --stat
git diff --cached             # 内容・機密情報を確認

git commit -m "Update development workspace settings"
git push
```

`.gitignore`には`.vscode/`の除外規則があります。**初回だけ**`dev-workspace/.vscode/settings.json`を追加するには`git add -f dev-workspace/.vscode/settings.json`が必要です。一度追跡された後の更新は通常の`git add dev-workspace`で取り込めます。

> `capture-workspace.ps1`によるコピーだけではコミットされません。また、`git commit`だけではGitHubに反映されず、`git push`が必要です。

## 7. 新しいPCでの開発環境の再構築

Gitリポジトリを`C:\pj\pd_pps\src\pd_pps`に配置し、`dev-workspace/`のファイルを実行用の場所に戻す方式です。**既存環境を再構築する場合は上書き前にバックアップしてください。**

### ① Gitリポジトリとサブモジュールを取得

```powershell
New-Item -ItemType Directory -Force C:\pj\pd_pps\src | Out-Null
cd C:\pj\pd_pps\src

git clone --recurse-submodules https://github.com/amapyon/pd_pps.git
```

既にclone済みで`ch32fun`が空の場合は、リポジトリ内で次を実行します。

```powershell
git submodule update --init --recursive
```

### ② Git管理用コピーを実行用の場所に配置

```powershell
$root = 'C:\pj\pd_pps'
$repo = Join-Path $root 'src\pd_pps'

$files = @(
    'pd_pps.code-workspace',
    'pd-pps.ps1',
    'organize-pdpps-workspace.ps1',
    'setup-workspace-view.ps1',
    'install-pd-pps-tools.ps1',
    'install-zadig.ps1'
)

foreach ($file in $files) {
    Copy-Item (Join-Path $repo "dev-workspace\$file") `
      (Join-Path $root $file)
}

New-Item -ItemType Directory -Force (Join-Path $root '.vscode') |
    Out-Null
Copy-Item (Join-Path $repo 'dev-workspace\.vscode\settings.json') `
  (Join-Path $root '.vscode\settings.json')
```

### ③ 開発ツールと接続を確認

`tools/`内のコンパイラ・MakeなどはGitに含まれません。使用するスクリプトの内容を確認して必要なツールを導入し、WCH-LinkE接続を確認します。ローカル環境で必要な`env.ps1`は別途準備してください。

```powershell
cd C:\pj\pd_pps
.\pd-pps.ps1 -Action Probe
.\pd-pps.ps1 -Action Build -Project 11_pps_ina226_monitor
code .\pd_pps.code-workspace
```

上記は再構築の基本手順です。開発ツールのインストール、ドライバー、環境依存設定は別途確認が必要です。

## 8. Git運用（実験コードとREADME）

```powershell
cd C:\pj\pd_pps\src\pd_pps

git status --short
# 例：新しい実験フォルダーを追加
git add 12_new_experiment
git diff --cached --check
git diff --cached --stat
git commit -m "Add new PPS experiment"
git push
```

`12_new_experiment`は例示名です。実験に応じて変更してください。生成物や機密情報がステージングされていないか確認してからコミットします。

## 9. よくある確認事項

| 状況 | 確認箇所 |
|---|---|
| 新規実験が選択肢に出ない | `NN_`形式のフォルダー名、Makefile、整理スクリプトの`Apply` |
| ビルドできない | `pd-pps.ps1`のツールパス、Makefile、`TARGET`、`ch32fun`サブモジュール |
| 書き込めない | WCH-LinkE接続、`Probe`、実行中のログ端末 |
| ログが表示されない | ファームウェアが`debugprintf`を使用しているか |
| INA226が認識されない | VCC/GND、PA2=SCL、PA3=SDA、アドレス`0x40`、GPIO解放方式 |
| INA226のバス電圧が0V | VBSが測定対象ラインに接続されているか |
| READMEやスクリプトの変更がGitに出ない | 編集場所がリポジトリ外ではないか、`capture-workspace.ps1`を実行したか |
| `dev-workspace/.vscode/settings.json`が追加できない | `.gitignore`の`.vscode/`規則。初回のみ`git add -f`を使用 |
| 設定を戻したい | `workspace-backups/`を確認し、必要ならGitの`dev-workspace/`と比較する |

## 10. 運用原則

- **実験を消さない：** 動作確認済みフォルダーを維持し、新しい実験は別フォルダーで進める。
- **ビルドと書き込みを分ける：** 通常はビルドで確認し、書き込みは意図したときだけ実施する。
- **変更前に戻れる状態を確保する：** 実験コードはGit、VS Code設定はバックアップとGit管理用コピーを利用する。
- **測定条件を記録する：** 電源・配線・負荷・PPS設定値・測定結果を各実験のREADMEに残す。
- **実行用ファイルを正本とする：** 開発環境の設定変更後は`capture-workspace.ps1`で保存用コピーを更新する。
- **Git管理用コピーを確認してコミットする：** 設定内容と機密情報を点検し、必要に応じて`git push`する。
- **整理は設定中心にする：** 当面は既存実験の移動やコードの大規模な統合を行わない。
