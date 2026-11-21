# PD PPS 開発環境

<!-- M43B8_CD_RESULT_20261122_START -->
## 2026-11-22：M43B-8 C/D 切断・再接続／G01故障注入の実機結果

- **M43B-8 C**：外部USB-PD/USB-CDC分離アダプターで7V/要求上限1A PPS・周期更新、COM Close／PC USB抜き差し、PD切断後の電源断／復帰が対象条件でPASS。1N5819×2の外部電源ORでMCUを維持したままCC DETACHによる契約失効（C04-B5）およびPD再接続・再契約（C04-B6）を確認。
- **M43B-8 D**：ベンチ専用故障注入版。FLASH 27,000B／RAM 5,924B、MCU UUID `c0-08-ab-cd-28-31-bc-5c`、BIN読み戻しSHA-256一致。通常PPS（P1）、FIFO満杯に伴うDETACH通知欠落とCCガード（P2：`fired/full/dropped/guard=1`、`lost_delta=1`）、故障注入後のPPS再契約（P3）を実機でPASS。
- **限定事項**：決定的な故障注入であり、自然発生する全競合・高負荷・他Source・3V3連続波形・製品安全性を網羅しない。要求1Aは実測電流ではない。M5/M7は引き続き進行中。
- **詳細とDMM証跡**：[M43B-8 C/D 実機検証記録](docs/M43B8_CD_VERIFICATION.md)。CDC生ログは`diagnostics/`でローカル保管（Git除外）。故障注入専用D版のFlashを製品運用の推奨とみなさない。
<!-- M43B8_CD_RESULT_20261122_END -->


CH32X035F8U6（WeActStudio CH32X035 CoreBoard V1.0）を使用した、USB Power Delivery（USB PD）／Programmable Power Supply（PPS）Sink開発の**ローカル開発環境ルート**です。

- **配置先:** `C:\pj\pd_pps\README.md`（このファイル）
- **Git管理ルート:** `C:\pj\pd_pps`（移行済み、ブランチ`main`）
- **更新基準日:** 2026-11-17
- **直近の実機検証版:** **M43B-8 B**（`src/pd_pps/firmware/experiments/43b8b_pps_control`。PPS CDC設定と実電圧変化を確認。詳細は`docs/M43B8_AB_VERIFICATION.md`）
- **現在地:** M43B-8 AでPDO/APDO選択を分離、M43B-8 BでPC USB CDCからのPPS電圧・要求電流変更を実機確認。M5（PC制御）・M7（安全性）は進行中。USB接続順序ごとの不安定性は未解決。
- **原則:** M13b、M43A、M43B-5/6/7、M43B-8 A/Bをそれぞれ独立した実験版として保全。次の安全性・切断再接続の実験も別版で実施する。

> このREADMEは、開発環境とチャット連携の入口です。**単一Gitルートは本ディレクトリに確立済み**です。`src/pd_pps/ch32fun`はSubmoduleとして登録済みです。今回のルールZIPの配布・展開はWindows上での適用やGitコミットを意味しません。詳細な実験・ビルド手順は [`src/pd_pps/README.md`](src/pd_pps/README.md) を参照します。

## 2026-11-17：M43B-8 A/Bの現行結果（過去のM43B-7記載に優先）

- **M43B-8 A：** `src/pd_pps/firmware/experiments/43b8_power_plan_refactor`。PDO/APDO選択・RDO構築を`pd_power.c/h`へ分離。7V/1A既定動作のまま、PC USB CDC/PPS同時動作を**120/120コマンド成功、PPS更新24/24完了**で検証。
- **M43B-8 B：** `src/pd_pps/firmware/experiments/43b8b_pps_control`。`PPS GET`/`PPS SET`/`PPS STATUS`を追加。実機で7.00→7.02→7.00V（契約値）、7.00→7.20→7.00V（DMM約6.95→7.15→6.95V）、要求電流1,000→500→1,000mA、無効値3件拒否を確認。
- **検証：** Python **74件実行・65 PASS・9 SKIP**、RISC-VビルドPASS（FLASH **26,184B**、RAM **5,904B**）。SKIPにはネイティブCテストが含まれる。
- **未検証：** 通信断時安全動作、接続順序、複数充電器、長時間/負荷試験、電流制限実測。`ma`は電流の要求値であり、実測電流ではない。
- **記録の正本：** [`docs/M43B8_AB_VERIFICATION.md`](docs/M43B8_AB_VERIFICATION.md)。本READMEのこれより下にあるM43B-7/M43A主体の節は**履歴**として保持する。

## 1. ディレクトリの役割

以下は**現在のGit構成とルール適用後の標準配置**を示した概略です。存在するファイルを網羅した実ディスク一覧ではありません。

```text
C:\pj\pd_pps\
├── .git\                              # Gitルート（確立済み）
├── .gitignore                         # 開発ツールと生ログを除外
├── README.md                          # 本書
├── ROADMAP.md                         # 製品のマイルストーン（正本）
├── DEVELOPMENT_RULES.md              # 開発とチャットの共通規約
├── HANDOFF.md                        # 次チャットへ引き継ぐ最新状態
├── pd_pps_zip_rules.md               # 成果物ZIP作成規約（固定名）
├── .gitmodules                        # ch32funのSubmodule設定
├── docs\                            # 製品仕様と資料（規格資料等はGit管理外）
│   └── PRODUCT_SPEC.md                # 製品仕様の正本（維持・随時更新）
├── tools\                           # xPackのRISC-V GCC / Makeなど（Git管理外）
├── src\
│   └── pd_pps\                     # 新リポジトリ内の通常のソースフォルダー
│       ├── README.md                # ソース側の詳細な開発・実験手順
│       ├── .vscode\tasks.json       # VS Codeタスク
│       ├── ch32fun\                 # Git Submodule（cnlohr/ch32fun）
│       ├── dev-workspace\           # Build / Flash / ログ取得の補助スクリプト
│       │   └── pd-pps.ps1
│       ├── docs\                    # 回路・配線・検証記録
│       └── firmware\
│           └── experiments\
│               ├── 17_pd_spr_sink_rebuild\   # M13bの実績あるPD PHYの参照元
│               ├── 43a_pd17_phy_baseline\   # M43A（過去の比較基準）
│               ├── 43b5_refresh_irq\        # M43B-5：USB物理接続の切り分け
│               ├── 43b6_p0p1_refactor\      # M43B-6：P0/P1主要回帰検証済み
│               └── 43b7_p2_session_refactor\ # M43B-7：P2主要回帰検証済み
└── diagnostics\                     # 実測・解析・記録（ディレクトリ全体をGit除外）
    └── m43a_goodcrc\
        ├── logs\                    # 試行ごとのWCH-LinkE生ログ
        ├── analysis\                # GoodCRC / Request集計結果
        ├── records\                 # 試行条件の記録表
        └── scripts\                 # ログ取得・解析用PowerShell（現行v3）
```

**管理の境界**

| 保存場所 | 役割 | 管理上の注意 |
|---|---|---|
| `src\pd_pps\` | ソースコード、Makefile、開発スクリプト | **ルートGit**で管理（旧内側Gitは移行済み） |
| `src\pd_pps\ch32fun\` | `cnlohr/ch32fun`外部依存 | **Git Submodule**として指定コミットを管理 |
| `docs\PRODUCT_SPEC.md` | 製品仕様の正本 | 削除せず、要件や設計判断の変更に合わせ随時更新 |
| `ROADMAP.md` | 全体試験計画の正本 | M0～M10の横断的なゲートと結果・未実施事項を記録 |
| 実験用KIT ZIP | 実験個別の試験計画 | ZIP内`tests/TEST_PLAN.md`に収録 |
| `tools\` | ローカルのコンパイラ等 | Gitリポジトリに含めない |
| `diagnostics\` | ログ、CSV、試行記録、解析スクリプト | **全体をGit除外**。元ログを保全 |
| ルートの`README.md`・`ROADMAP.md`・規約・`HANDOFF.md` | 開発方針の正本 | **ルートGit**で管理する。旧内側Gitから履歴移行は行わない |

以前使用していたルート直下の`pd_pps.code-workspace`や`pd-pps.ps1`は**旧運用**に由来します。現行の標準は`src\pd_pps`自体をVS Codeで開き、リポジトリ内の`dev-workspace\pd-pps.ps1`を使用する方法です。旧ファイルが存在しても、現行手順の前提にはしません。

## 1.1 チャット中心開発の正本・現在のGit構成

- `DEVELOPMENT_RULES.md`：実験・変更・検証・チャット進行の共通規約。
- `ROADMAP.md`：製品目的とM0～M10マイルストーンの正本。
- `HANDOFF.md`：現在のファームウェア、直近の実験結果、仮説、次の作業。
- `pd_pps_zip_rules.md`：ファームウェア・診断・ルールZIPの配布規約（バージョンをファイル名に含めない）。

新しいチャットでは**この4ファイルの最新内容を参照**します。ChatGPTはWindows上のローカルファイルを自動読取できるとは限らないため、必要な場合は添付してください。今回のルール配布ZIPは一時的に`C:\pj\pd_pps`へ保存し、`install.ps1`で配置します。配布物を作成しただけでPCへ反映済みとはみなしません。

**Git移行結果：** `C:\pj\pd_pps`が唯一の親Gitルートです。`src\pd_pps`の独立した旧Gitメタデータは移行済み、`ch32fun`はSubmodule（登録コミット`af7c51c`）です。`diagnostics/`の全体Git除外は`1eb1add`でコミット済みです。**移行スクリプトの再実行は不要**です。旧GitHub履歴の上書きやPushは未実施で、別途判断します。バックアップは自動作成しません。

既存のソース側`src\pd_pps\README.md`に残る「Gitリポジトリが`src\pd_pps`にある」という記述は旧運用です。ソース側READMEの内容を無条件に書き換えることはせず、必要な開発段階で整合させます。リポジトリ移行時点で旧コードがコンパイルできなくても、移行作業の失敗とは扱いません。

## 2. 開発環境

| 項目 | 現在の前提 |
|---|---|
| OS | Windows 11 |
| エディター | Visual Studio Code |
| マイコン・基板 | CH32X035F8U6 / WeActStudio CoreBoard V1.0 |
| ビルド | RISC-V GCC、GNU Make（xPack） |
| 書き込み・デバッグ | WCH-LinkE、ch32fun `minichlink` |
| コマンド実行 | Windows PowerShell 5.1 |
| Git管理ルート | `C:\pj\pd_pps`（移行済み）。旧GitHub: <https://github.com/amapyon/pd_pps>（新ルートGitのリモートは未設定、Push未実施） |

ソース側READMEによる既知のツール配置例：

```text
C:\pj\pd_pps\tools\xpack-riscv-none-elf-gcc-14.2.0-3\bin
C:\pj\pd_pps\tools\xpack-windows-build-tools-4.4.1-3\bin
```

実際の参照先は`src\pd_pps\dev-workspace\pd-pps.ps1`の内容で確認してください。新しいPCへのGit cloneやツール導入手順は[ソース側README](src/pd_pps/README.md)に記載されています。

## 3. 通常の開発操作

以下のPowerShellコマンドは、**すべて `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ...` 形式**で実行します。

### VS Codeで開く

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
code .
```

VS Codeのタスクは`src\pd_pps\.vscode\tasks.json`にあります。メニューに出る実験一覧は、ソース側READMEの記載時点と異なる可能性があります。

### WCH-LinkEの接続確認

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\src\pd_pps\dev-workspace\pd-pps.ps1" -Action Probe
```

### 旧M43Aのビルド（履歴用・現行M43B-7のビルド手順ではない）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\src\pd_pps\dev-workspace\pd-pps.ps1" -Action Build -Project firmware/experiments/43a_pd17_phy_baseline
```

M43Aでは過去にエントリファイルの`#include "src/main.c"`が欠け、`main`未定義でリンク失敗したことがあります。修正版はビルド成功と記録されていますが、**手元の作業ツリーが修正版と一致するかはビルド前に確認**してください。

### 旧M43Aの書き込みコマンド例（履歴用・今回実行しない）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\src\pd_pps\dev-workspace\pd-pps.ps1" -Action Flash -Project firmware/experiments/43a_pd17_phy_baseline
```

**注意:** FlashはMCU上の現行ファームウェアを上書きします。上記は過去のM43Aを参照するコマンド例であり、**この文書更新・Gitコミットのために実行しません**。M43B-7のソース/ビルド/書き込みは実験フォルダー付属の手順を使用し、PD電源・配線・負荷条件を確認してください。

## 4. ハードウェアと接続

M43A～M43B-7の実機比較で使用してきた接続構成は次のとおりです。

| 用途 | 接続・設定 |
|---|---|
| PD充電器とのCC通信 | 基板J1のCC1/CC2（PC14/PC15） |
| PCとのUSB CDC通信 | 別USB-Cブレークアウト経由、D+→TP1/PC17、D−→TP2/PC16 |
| WCH-LinkEデバッグ | `debugprintf`、実績のあるPC環境ではCOM5 |
| M43AのUSB CDC | VID `1209` / PID `D035`、シリアル`043`、実績のあるPC環境ではCOM16 |
| VBUS測定 | PA0 ADC_IN0（100kΩ / 10kΩ分圧） |
| 電圧・電流監視 | INA226、I²C PA2=SCL / PA3=SDA、7bitアドレス`0x40`、R010 |

COM番号は接続状況によって変わります。Windowsのデバイスマネージャー等で確認してください。

**配線上の注意:**

- PC通信用USBのVBUSは、確認済み構成どおりWeAct基板へ接続しない。共通GNDをとる構成を維持し、PCとPD充電器の給電競合を避ける。
- J1のUSBデータ線を1本切断した加工はあるが、切断がD+かD−かは**未確定**。確認前に切断・はんだ修正を追加しない。
- INA226の0.01ΩシャントをVBUSとGNDに直結しない。電流測定は適切な負荷のハイサイド直列に挿入する必要がある。
- 現在の比較実験は**外部負荷なし**で実施する。PPSの「7V・1A」は要求電圧と電流上限を示し、1Aを実際に消費したことを意味しない。

回路と配線の詳細は [`src/pd_pps/docs/HARDWARE_WIRING.md`](src/pd_pps/docs/HARDWARE_WIRING.md) と当該実験のREADMEを参照してください。

## 5. M43B-7までの実機結果と旧M43Aの記録

### 最新の基準：M43B-7 P2（2026-11-15）

- **M43B-5:** PC通信用USBが物理切断された05ではPPS READY約31.9秒・更新6/6成功。USB接続/CDCアイドルの06R1はGoodCRCタイムアウト3回で`TX_FAILED`、PnP監視120秒はCOM16/WCH-LinkEの認識変化なし。USB再切断の07では初回GoodCRC成功・READY約30ms後にVBUS異常停止。
- **M43B-6 P0/P1:** `43b6_p0p1_refactor`。異常状態の初期化とCDC数値解析の重複を削減し、ホスト比較・実機R01/R02・CDC R03（24/24）で既知挙動との回帰を確認。
- **M43B-7 P2:** `43b7_p2_session_refactor`。`main.c`の責務を`pd_session.c/h`と`pd_session_diag.c/h`へ分離。PD PHY・USBFS ISR・GoodCRC時間条件は変更せず、R01～R03の主要回帰ゲートPASS。

| M43B-7試験 | PC USB接続条件 | 結果 | 判定範囲 |
|---|---|---|---|
| R01 OFF CC1 | 物理切断 | 初回GoodCRC一致、Accept/PS_RDY/READY、READY 24ms、約590msで`VBUS_FAULT` | 旧版と同じ短時間遷移・保護停止の回帰PASS。長時間安定の合格ではない |
| R02 ON CC1 IDLE | 接続・CDCコマンド休止 | 初回RequestでCRCタイムアウト3回、再送2回、`TX_FAILED`、USBFS IRQ 0 | 旧版と同じ失敗経路の回帰PASS。PD通信そのものは失敗 |
| R03 CDC INDEX | 接続・PD充電器なし | PING/STAT/COUNT/VBLOG/PDTRACE 24/24 PASS | インデックス解析・応答互換性の確認。非空履歴の全経路は未検証 |

**未解決:** PC USB接続がGoodCRC失敗と相関するが電気的原因は未確定。READY直後の実VBUS低下はADC値とおおむね一致しており、ADC精度校正は製品PCB製作後に行う方針。根本原因の修正とP3の構造整理は混ぜない。

Git管理する検証の詳細は [`docs/M43B7_P2_VERIFICATION.md`](docs/M43B7_P2_VERIFICATION.md)。実測生ログ・SRAMは`diagnostics/`内で別途保全する。**現PCへ適用・コミット・Pushしたという意味ではない。**

### M43A以前の履歴（以下の既存記録は当時の状態）

### ソフトウェアの位置付け

- **M13b:** `firmware/experiments/17_pd_spr_sink_rebuild`。過去にCC1、初回Request再送なしで7V・1A PPS契約に成功した、実績あるPD PHYの参照元。
- **M43A:** `firmware/experiments/43a_pd17_phy_baseline`。M13b由来のPD PHYを、M42のUSB CDC・センサー監視機能と統合した**過去の比較版**。
- **M43B系列:** M43B-5～7の診断・リファクタリング実験は実装と短時間実機試験を実施済み。詳細は本節冒頭の最新結果を参照。
- **製品向け統合版:** `firmware/product/`は将来の計画上の配置先であり、作成済みの製品版として扱わない。

### 観測された事実（2026-11-15時点）

| 試験 | PC CDC条件 | 初回Request | PPS周期更新 | 確認内容 |
|---|---|---|---|---|
| 旧M13b | PC通信用USBなし | 再送0で成功 | 3回成功 | 7V・1AのPPS契約 |
| M43A（先行試験） | CDC通信中 | 再送1で成功 | 接続が短く未実施 | 7V・1A契約、CDC応答を確認 |
| M43A 試行01 | OFF / CC1 | 再送0で成功 | 2回成功 | READY 11.386秒、TRACE欠落0 |
| M43A 試行02 | ON / CC1 | 再送0で成功 | 最初の更新で再送2回後TX_FAILED | READY 5.000秒、契約中のVBUS低下検出0、TRACE欠落0 |

「ON/OFF」は実験者が付けたPC CDCケーブルの**接続条件ラベル**です。特に試行02のログ自体は、WindowsのUSB列挙状態やCDC転送回数を証明しません。初回Requestが成功しても、その後のPPS更新が成功するとは限りません。

**今回の課題として確定した現象:** 初回Requestだけでなく、PPS契約成立後の更新Requestでも、GoodCRC未受信→再送→`TX_FAILED`が起こり得ます。一方で、CDC通信中に成功した事例もあります。

**原因は未確定:** USBFS割り込み負荷、PD送信完了後のRX移行タイミング、GND/CC信号品質、配線・接触条件などは、いずれも**仮説**です。`TX_FAILED`だけでは、充電器がGoodCRCを送信しなかったのか、MCUが受信・認識できなかったのかを区別できません。物理層の確定にはCC波形観測やPDアナライザー等が必要です。

## 6. M43AのGoodCRC比較実験（過去の診断v3手順）

以下は当時のログ形式・試行例の記録です。**現在のM43B-7のR01～R03を再実施する指示ではありません。**

**目的:** M43Aのソースと配線を変更せず、同じ充電器・ケーブル・CC向き・無負荷条件で、PC CDC接続の有無とRequestごとの再送回数を比較します。初回Requestと5秒周期のPPS更新Requestは**別々に集計**します。

### 6.1 M43A当時の診断パッケージ（旧v3・履歴）

現在案内済みの診断v3では、以下を使用します。

```text
C:\pj\pd_pps\diagnostics\m43a_goodcrc\
├── scripts\
│   ├── Start-M43A-Log.ps1
│   └── Summarize-M43A-Trials-v3.ps1
├── logs\
│   ├── M43A_01_OFF_CC1.log
│   └── M43A_02_ON_CC1.log
├── analysis\
│   ├── M43A_requests_v3.csv
│   ├── M43A_trial_details_v3.csv
│   └── M43A_summary_v3.csv
└── records\
    └── M43A_trial_sheet.csv
```

これは**v3のファイル名・出力方式**の説明です。インストール済みの有無は環境によります。まず存在確認してください。

```powershell
Test-Path 'C:\pj\pd_pps\diagnostics\m43a_goodcrc\scripts\Start-M43A-Log.ps1'
Test-Path 'C:\pj\pd_pps\diagnostics\m43a_goodcrc\scripts\Summarize-M43A-Trials-v3.ps1'
```

### 6.2 ログ取得

以下は試行03（CDC ON / CC1）の例です。**既に試行03を取得済みなら、その番号は再利用せず、次の未使用番号に変更**してください。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\diagnostics\m43a_goodcrc\scripts\Start-M43A-Log.ps1" -Trial 3 -Mode ON -CC CC1
```

試行ごとにPD充電器を1回接続し、可能ならPPS更新を2回観測できる約12秒以上を目安に接続してから切断します。ログ端末は`Ctrl+C`で終了します。同じ試行番号のログがあれば、v3のログ取得スクリプトは上書きせずエラーにします。

次の試行04（CDC OFF / CC1）の例：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\diagnostics\m43a_goodcrc\scripts\Start-M43A-Log.ps1" -Trial 4 -Mode OFF -CC CC1
```

- `ON`: PC通信用USBケーブルを接続した条件。周期的なCDCコマンド送信を実施したことは意味しません。
- `OFF`: PC通信用USBケーブルを外した条件。WCH-LinkE側のデバッグログは継続して取得します。
- 両条件で充電器・PDケーブル・CC向き・無負荷・測定時間を可能な限り固定します。
- まず接続の有無を比較し、CDC通信量を変える実験は**別条件**として記録します。

ログ名の形式は`M43A_<番号2桁>_<ON|OFF>_<CC1|CC2>.log`です。実測ログを修正・上書きしないでください。

### 6.3 v3の解析

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\pj\pd_pps\diagnostics\m43a_goodcrc\scripts\Summarize-M43A-Trials-v3.ps1"
```

v3は`logs\`の対象ファイルを読み取り、**`analysis\`直下の固定名CSVに出力**します。

| 出力 | 内容 |
|---|---|
| `M43A_requests_v3.csv` | INITIAL / REFRESHの各Request、再送数、GoodCRC / TX_FAILED |
| `M43A_trial_details_v3.csv` | 試行別の結果、停止要因、ログ品質 |
| `M43A_summary_v3.csv` | ON/OFF・CC別の集計 |

**v3の制約:** 解析結果の固定名CSVは再解析で上書きされる可能性があります。後から比較する場合は、解析前に既存の`analysis\`を別名で保管してください。ログ品質が`REVIEW`の試行は、TRACE欠落・不完全ログなどを個別確認し、成功率へ無条件に算入しません。

### 6.4 今後のZIP配布規約（新規KIT）

新規配布KITには**[`pd_pps_zip_rules.md`](pd_pps_zip_rules.md)（文書版v2.3）**を適用します。ルール文書は固定名で更新し、配布ZIPの名前だけに版番号を付けます。**診断v3は新規約に準拠済みという意味ではありません**。

- 新しいZIP名は`<fw|diag|rules>_<module>_vNN_YYYYMMDD.zip`。**先頭に`pd_pps_`を付けない**。既発行ZIPは改名しない。
- ZIP内部はASCIIパス・単一親フォルダーとし、`README.md`、`CHANGELOG.md`、`manifest.json`、`install.ps1`を必須にする。**実験用KIT（fw/diag）では`tests/TEST_PLAN.md`も必須**。
- インストーラーは既定でdry-run、`-Apply`を明示して配置。既存ファイルの未知の差分はCONFLICTとして停止し、バックアップ・自動削除はしない。
- 診断スクリプトは`diagnostics\m43a_goodcrc\scripts\vNN\`へ版別に配置し、生ログは保護する。解析結果は`analysis\YYYYMMDD_HHMMSS\`へ新規保存。
- **全体のテスト計画とマイルストーンのゲートはルート`ROADMAP.md`へ集約**し、独立した`M*_TEST_PLAN.md`は新規作成しない。既存の計画書は、必要内容を対応するKITへ収録したことを確認してからユーザーが別操作で整理する。
- 製品仕様`docs/PRODUCT_SPEC.md`は保持して随時更新する。`diagnostics/`は全体をGit管理外とする。
- ビルド、PowerShell実行、実機動作、Gitコミット、Pushは**別々の検証項目**として報告。

旧v3のインストーラーは同名スクリプトを強制コピーする処理があり、v04以降の安全なインストール方式とは異なります。**既にv3を導入済みなら、通常のログ取得や解析のために旧インストーラーを再実行する必要はありません。**

## 7. 次の開発・検証

1. **P0/P1/P2までのコミット確定:** M43B-6・M43B-7ソースと、更新したルートのREADME/ROADMAP/HANDOFF、`docs/M43B7_P2_VERIFICATION.md`だけを明示的にステージし差分確認する。生ログ、旧未追跡ファイル、規約の別変更を巻き込まない。
2. **P3計画:** 検証済みM43B-7を固定し、診断イベント定義の一元化・数値コードの名称化・PC側時系列レポートを独立した新実験で実施する。PD PHY・USBFS ISR・時間条件・SRAM既存形式の互換性を保つ。
3. **残課題:** PC USB物理接続とGoodCRC失敗の関係、PPS READY直後の実VBUS低下の原因は未解決。P3の挙動整理と原因対策を別の変更セットとして扱う。ADC精度校正は製品PCB後工程へ回す。

### M43A時点の旧試験案（当時の予定。今回の次操作ではない）

1. **M43Aを保全**したまま、CDC ON/OFFを交互に比較する。現在、試行01・02のログが解析対象。
2. 試行03（ON / CC1）・04（OFF / CC1）を同じ測定条件で取得する。接続時間はPPS更新を観測できる程度に固定する。
3. 初回Requestの「再送0／1／2で成功、TX_FAILED」と、周期更新Requestの同じ分類を分けて集計する。
4. 差の傾向が見えたら、CDCの接続だけとCDC転送負荷の影響を分離する。必要なら、USBFS ISR発生とPD TX_END→RX受信窓のタイミングを**軽量な記録方式**で調べる。
5. 新しいファームウェア（例：M43B）を作る場合は、M13bの成功実績のあるPD PHYを優先して再利用し、**新規の実験フォルダー**に追加する。

1～2試行の結果からUSB CDCを原因と断定しないでください。

## 8. 参照資料と記録

| 参照先 | 内容 |
|---|---|
| [`src/pd_pps/README.md`](src/pd_pps/README.md) | Gitリポジトリ、VS Code、ビルド、実験の運用 |
| [`src/pd_pps/docs/HARDWARE_WIRING.md`](src/pd_pps/docs/HARDWARE_WIRING.md) | 基板と追加回路の配線・注意事項 |
| `src\pd_pps\firmware\experiments\17_pd_spr_sink_rebuild\` | M13bのPD PHY参照元 |
| `src\pd_pps\firmware\experiments\43a_pd17_phy_baseline\` | 過去のM43A比較ソース |
| `diagnostics\m43a_goodcrc\logs\` | 各試行の生ログ |
| `diagnostics\m43a_goodcrc\records\` | 実験条件の記録CSV |
| `PD_PPS_引継ぎ_2026-11-15.md` | 2026-11-15の引き継ぎ資料。**保存場所は未確認** |
| [`pd_pps_zip_rules.md`](pd_pps_zip_rules.md) | **現行ZIP規約の固定ファイル名**（文書版v2.3） |
| `docs/PRODUCT_SPEC.md` | 継続管理する製品仕様の正本。今回のKITでは変更しない |
| [`ROADMAP.md`](ROADMAP.md) | 全体テスト計画とM0～M10の受入ゲートの正本 |
| [`DEVELOPMENT_RULES.md`](DEVELOPMENT_RULES.md) | チャット・検証・変更管理ルール |
| [`HANDOFF.md`](HANDOFF.md) | 新しいチャットに渡す現在の状態 |

ルートREADMEの記述とリポジトリの実ファイルが食い違う場合は、実際のファイル・現在のログ・Git差分を確認し、確定した内容で双方の文書を更新してください。

---

**文書の位置付け:** このREADMEはWindows上の`C:\pj\pd_pps\`全体の運用案内です。直近の試験対象はM43B-7 P2ですが、この文書の作成だけでPCのファイル・実機FLASH・Git状態を変更したものではありません。
