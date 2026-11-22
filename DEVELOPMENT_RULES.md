# PD PPS 開発共通ルール

- 文書ID：`DEVELOPMENT_RULES`
- バージョン：1.7
- 改訂日：2026-11-22（v1.7：診断コンソールログの時刻記録を標準化）
- 正本：`C:\pj\pd_pps\DEVELOPMENT_RULES.md`
- 対象：ChatGPT中心で進める PD/PPS Explorer の開発・実験・成果物管理

## 1. 製品目的と優先順位

PD/PPS Explorer の中心価値は、USB PD/PPSの能力を**調べる・設定する・測定する**こと。将来はスタンドアロンUI、PC制御、安全な復旧・測定機能を備え、BOOTHで**組み立て済み完成品**と**ベアPCB**を販売する。製品目的の変更、重大な仕様拡張、正式な設計判断にはユーザーの判断を必要とする。

判断の優先順位は **人と機器の安全 → 実験証拠と正常動作実績の保全 → 製品目的への貢献 → 小さく検証可能な変更 → 開発の操作性**。チャット体験の改善を優先しても、未検証のファームウェアを「完成」と扱わない。

正式な製品マイルストーンは `ROADMAP.md` のM0～M10。PD17内部のM13bなどや、M43Aなどのファームウェア名とは番号体系を混同しない。

## 2. 情報の正本と矛盾時の扱い

1. 現在のチャットでユーザーが明示した指示（安全制約の範囲内）。
2. 現在アクセスできる正本：ルートの`DEVELOPMENT_RULES.md`、`ROADMAP.md`、`HANDOFF.md`、`README.md`、`pd_pps_zip_rules.md`と`docs/PRODUCT_SPEC.md`（製品仕様）。
3. 実コード、変更差分、実測の生ログ、テスト結果、配線資料、Git上の実際の記録。
4. 過去チャットと要約（探索の手掛かりであり、実測の代用にしない）。

**技術的な主張では原資料と実測を優先**する。文書同士に矛盾があれば、日付・証拠・矛盾箇所を示し、黙って一方を採用しない。チャットでZIPを作成したことは、Windows PCへの配置・Gitコミット・実機動作の証拠ではない。PC上のローカルファイルをChatGPTが常時読めるとは仮定しない。

## 3. チャットの標準的な進め方

- **開始時**：現行ファームウェア、正式マイルストーン、直近の検証済み試行、未解決事項、次の作業と必要な資料を把握する。
- **作業前**：今回の目的、変更するもの／変更しないもの、確認したい仮説、成功判定、停止条件を簡潔に示す。
- **開発時**：既存の正常動作版（現在はM13b／M43A）を先に活用し、原因を特定しないまま大幅に書き直さない。新ファームウェアは独立した実験フォルダーへ作る。
- **実機試験時**：原則として一度に1つの試験を案内する。実行場所、コピペできるコマンド、想定出力、保存先、提出してほしいログを明記する。
- **報告時**：`確定事項`／`仮説`／`未検証`／`判断`／`次の操作`を分ける。PASSは実施したテスト範囲だけを意味する。
- **終了・切替時**：最新状態は`HANDOFF.md`、全体の試験方針・マイルストーンやゲートの変更は`ROADMAP.md`へ反映する。製品仕様の変更は`docs/PRODUCT_SPEC.md`に反映する。過去の実験履歴やログは残す。

単純な質問にまで毎回この全手順を適用する必要はない。実装、実験、成果物の配布時には再現性を優先する。

### ソースコード一式の受け渡しと調査の分担（v1.4追補）

- **ソースコード一式を依頼するときの標準対象**は、`C:\pj\pd_pps\src\pd_pps\` **フォルダー全体（すべてのサブフォルダーを含む）**。原則としてユーザーはこのフォルダーをまとめてZIP化して渡す。ユーザーに個々のソースファイルの選別、`Select-String`等による検索結果の抽出、特定行の貼り付けを依頼しない。
- ZIPには`firmware/experiments/`全体（M13bの`17_pd_spr_sink_rebuild/`、M43Aの`43a_pd17_phy_baseline/`および他の実験を含む）、`ch32fun/`、`docs/`、`dev-workspace/`、`.vscode/`、ソース側`README.md`、Makefile・設定ファイルなどを含める。**Git Submoduleの`ch32fun/`は参照情報だけでなく、実際のライブラリファイルを含める。**
- 通常のソース調査では、ルート`.git/`、`tools/`のコンパイラやビルドツール本体、`diagnostics/`の実測ログは標準ZIPの対象外とする。特定の生ログやルート正本が必要な場合は、理由と必要なフォルダー・ファイルを明示して別途受け取る。
- **受領後のファイル探索、依存関係・実装比較、差分解析、原因候補の抽出はChatGPT側で行う。** ユーザーには、Windows環境への適用、機器操作、実機試験などChatGPT側で実施できない作業を中心に依頼する。
- ソース受領は、M13b／M43Aの変更、ファームウェア書き込み、基板配線変更、ファイル削除、Git操作を許可するものではない。改修が必要なら既存実績版を保護し、新しい実験フォルダーに変更内容と個別テスト計画を準備したうえで、影響のある操作の前に確認する。
- ファイルサイズや転送制約でフォルダー全体を一度に渡せない場合は、**サブフォルダー単位などの代替手順を合意してから**進める。個別コード断片の抽出を既定手順に戻さない。

### PowerShellの一括実行と異常時停止（v1.5追補）

- **複数行のコピペ用PowerShell手順**を提示するときは、原則として`& { ... }`で全体を囲み、**1つのスクリプトブロックとして実行**できる形にする。実行途中でエラーがあっても、その後の行を個別に実行してしまう構成にしない。単純な1行コマンドは例外とする。`.ps1`ファイルとして提供する場合も同等の異常時停止を実装する。
- ブロック冒頭で`$ErrorActionPreference = 'Stop'`を設定し、適用可能なら`Set-StrictMode -Version Latest`を用いる。前提パス、必要ファイル、既存データ、期待する配置先・ハッシュ等を**操作前に確認**する。期待と異なる場合は`throw`で中断し、`try/catch`では失敗を報告したうえで再スローして正常終了と誤認させない。
- `powershell.exe`、Python、Git、コンパイラ、ビルドツール等の**外部プロセスの実行直後**に`$LASTEXITCODE`を検査し、非0なら`throw`で停止する。ファイル操作の非終端エラーも停止扱いとし、各段階が成功した場合だけ後続処理へ進む。
- **最終成功メッセージは、すべての必須ステップと検証が成功した場合だけ表示**する。検証の失敗後に無条件の成功表示が実行されるような複数コマンドの分割提示をしない。未実施・未検証はPASSとして報告しない。
- **dry-runと実際の変更を区別**する。`-Apply`、既存ファイル上書き、Flash、Git変更など影響のある操作は、事前検証と対象・影響の明示を行い、ユーザーの明示的な実行操作・確認を経て実施する。検証から自動的に危険な操作へ進むことを既定動作にしない。
- `& { ... }`と`throw`による中断は**ロールバックや原子的実行を保証しない**。失敗前のファイル配置などは残り得るため、無断で削除・巻き戻しせず、部分適用の可能性を明示する。


### CDC + USB PD二重通信の標準条件と一括FLASH（v1.6追補）

- **USB CDCとUSB PD/PPSの同時動作が現在の重点検証対象**。特段の指示がない限り、試験計画、ログ取得スクリプト、次の試行提案では**PC側USB CDC接続あり（CDC ON）＋PD充電器接続あり（CC1/CC2を試行で指定）**を標準条件とする。CDC OFFは個別の切り分け試験を明示した場合に限り採用し、断りなく標準条件へ戻さない。
- CDC ONは**物理ケーブル接続の試験条件**。COMポートの列挙だけでCDC通信が実際に動作したとは判断しない。必要に応じてCDCの送受信カウンター・応答・ポート同定を別記録する。逆に、CDC ONが求められる試験で`COM16`の存在を理由に「ケーブルを外せ」と誤判定・中断するスクリプトを書かない。COM番号の固定が妥当かは試験環境に合わせて確認する。
- **安全上の前提は省略しない。** PC側D+/D-とPD充電器側D+/D-が短絡・共有する疑いは未解消。二重接続を試験の標準とすることと、未確認配線を安全とみなすことは別である。両ポートのデータ線が競合しない電気的構成を使い、VBUS逆給電、GND、CC、電流制限を考慮する。危険な配線のままの直接同時接続を当然の手順としない。異常時停止スクリプトは、配線・給電経路の安全確認の代わりにならない。
- **新規`fw`型ZIPの後続操作は、原則としてZIP検証→展開→`install.ps1`のdry-run→明示`APPLY`→配置・ハッシュ検証→ホストテスト→クロスビルド→BIN/ELF・サイズ・SHA-256確認→WCH-LinkEで対象MCU確認→明示`FLASH`→書き込み終了判定までを、異常時停止する**1本のPowerShell 5.1スクリプト**として提示・同梱する。BUILDで終わる複数スクリプトに分割しない。後続の診断ログ取得・SRAM読出しは、目的に応じて別の手順でよい。
- `fw`型KITには、単一エントリーポイント`Run-Firmware.ps1`を同梱する運用を標準とする。`install.ps1`は従来どおりdry-runと`-Apply`による**ファイル配置だけ**を行い、単体ではFLASHしない。`Run-Firmware.ps1`が順序を管理し、適用前の`APPLY`と書き込み前の`FLASH`を別々に明示確認する。規約更新など`rules`型ZIPにはFLASHを含めない。
- `& { ... }`、`$ErrorActionPreference = 'Stop'`、`Set-StrictMode -Version Latest`、`try/catch`＋再スロー、各外部プロセスの終了コード検査、必須ログと出力ファイルの検証を用いる。**途中で失敗したらFLASHを含む後続操作を一切実行しない。** ただし、停止は適用済みファイルや書き込み途中の内容をロールバックしない。個別ステップの部分完了を正しく報告する。
- **PowerShell 5.1と`minichlink`のstderrに注意。** `minichlink -i`等が正常メッセージをstderrへ出すことがあるため、`$ErrorActionPreference='Stop'`の下で`& $minichlink ... 2>&1`を捕捉する方式による`NativeCommandError`の誤停止を避け、`Start-Process -RedirectStandardOutput/-RedirectStandardError -Wait -PassThru`等でそれぞれを保管して終了コードと内容を検証する。`Detected CH32X035`と書き込み結果を検証し、`Image written.`だけで実機通信をPASSとはしない。
- ZIP、インストール、ホスト試験、クロスビルド、FLASH、実機PD/PPS通信とSRAM診断の**結果は段階別に記録する**。最終成功表示は選択した一括処理の全必須ステップが成功した場合だけにする。明示入力がなければCANCELEDとし、FLASH済みと誤認させない。

## 4. 正常動作版・変更の保護

- **M13b**はGoodCRC/PPS正常動作実績のあるPHY参照元、**M43A**は2026-11-15時点で書き込み済みの実機版。今回のルール配布はファームウェアを変更しない。
- 動作実績のある実験フォルダーを上書きしない。新しい版は別フォルダーに置き、差分と前提版を明記する。
- 書き込み、基板配線、USB給電経路、負荷条件、Gitリモート履歴に影響する操作を、単なる診断や配布の副作用として実施しない。
- ルートGitへの移行と`ch32fun`のSubmodule復旧は完了している。移行スクリプトを再実行せず、ソースファイルと実験ログを削除しない。
- **Git構造整理直後に旧コードがコンパイルできなくても、移行自体を失敗扱いしない。** 必要なマイルストーンで修正する。その場合もビルド成功・実機動作を偽って報告しない。

## 5. 実験・診断・証拠の基準

試行ID、ファームウェア名とハッシュ（分かれば）、充電器・ポート・CC向き・ケーブル、PC USBの物理接続状態、実際のCDC通信量（測れた場合）、負荷、接続時間、生ログへの参照、ログ品質、判定を記録する。比較実験は可能な限り一度に一因子だけ変更する。

現在のM43A GoodCRC調査では、**初回Request**と**PPS周期更新Request**を別々に集計する。各Requestは再送0/1/2回で成功、`TX_FAILED`、`REVIEW`等に区分する。`TX_END`はGoodCRC受信の証拠ではない。CDC ON/OFFのファイル名は実験条件ラベルであり、PC上でのUSB列挙や転送実績を証明しない。`TX_FAILED`だけではSourceがGoodCRCを送らなかったのかSinkが認識できなかったのか区別できない。

ログ欠落や解釈不能な記録を成功率の分母に無断算入しない。実測ログは**元のバイト列・文字コードを保持**し、再解析結果は別ファイルとして出力する。集計では分母、除外理由、比較条件を示す。

### 診断コンソールログのタイムスタンプ（v1.7追補、2026-11-22）

- **新規作成・改修するPowerShell診断スクリプトと試験用コンソール出力**では、操作、状態取得、測定値、エラー、判定結果など、検証の根拠となる出力行に日時を付ける。既存スクリプトと過去ログを一括改修するものではない。
- **日時の標準形式は `yyyy-MM-dd HH:mm:ss.fff zzz`** とする（試験PCのローカル時刻とUTCオフセットを併記。JSTでは `+09:00`）。PowerShell 5.1では `[DateTimeOffset]::Now.ToString('yyyy-MM-dd HH:mm:ss.fff zzz')` を使用できる。
- **DMM等の外部測定と照合する実機時系列試験**では、上記日時に加えて `Stopwatch` に基づく実行開始からの経過時間 `T+0.000s` を併記する。時系列の間隔計算は壁時計の時刻差だけに依存しない。
- **コンソール表示と保存ログは、同じ時刻を付けた同一の行を出力**する。日時の再生成で両者にずれを作らない。試行ごとに重複しないログ保存先を用い、元ログを上書きしない。
- **重要な物理操作の前後**には、操作指示、操作者が完了を通知した時刻、経過時間、DMM照合マーカーを記録する。Enter入力時刻は操作完了の申告時刻であり、実際の抜き差し・電圧遷移の瞬間と同一視しない。CDC応答を受信した時刻も、MCU内の状態遷移時刻と区別する。
- **ログ取得は試験目的に必要な最小頻度**で行い、過剰なCDCポーリングによってPD/PPS通信や測定対象の挙動を変えない。コマンド実行前後の時刻、DMMの記録範囲、時刻同期の誤差を判定時に説明する。
- 外部プログラムの**既存の標準出力・標準エラーやMCUが送る生データ**は、バイト列と意味を改変しない。行単位の時刻付与が困難なときは、取得開始・終了の時刻を別に記録し、元出力をそのまま保存する。
- 標準の例：`2026-11-22 00:14:05.133 +09:00 | T+17.568s | G01 fired=1 full=1 dropped=1 guard=1`。

## 6. 電気的安全と実機操作

PPSの「要求電流」は充電器に要求する値であり、負荷電流の実測値でも独立の定電流保護でもない。現在のM43A比較は**外部負荷なし**を基準とする。意図的短絡・無制限の過電流・逆給電・無治具の故障注入はしない。配線変更時はGND、VBUS分離、CC、INA226のシャント直列経路等を確認する。

Flash、配線変更、故障注入、ファイル・Git管理情報の削除、既存データの上書き、GitHubへの強制Pushなど、不可逆または影響の大きい操作は**事前に対象と影響を明示**する。ビルド成功と電気的安全・実機安定の証明は別である。

## 7. 配置・Git・ファイルの運用

**単一Gitルートは`C:\pj\pd_pps`に確立済み**。旧`src\pd_pps`はGit移行後、通常のソースディレクトリになる。ソース側READMEのGit境界に関する旧記述より、ルートREADMEと本規約を優先する。

- 正本：ルートの`README.md`、`ROADMAP.md`、`DEVELOPMENT_RULES.md`、`HANDOFF.md`、`pd_pps_zip_rules.md`。製品仕様の正本は`docs/PRODUCT_SPEC.md`であり、実際の設計決定や機能要件に応じて更新する。
- ソース：`src\pd_pps\firmware\experiments\`。外部`ch32fun`は`src\pd_pps\ch32fun`のGit Submodule（参照コミット`a800cbd1f1ca5e250c73da83c669f0965d2052e5`）。
- 診断：`diagnostics\`全体を`.gitignore`でGit管理対象から除外する。生ログ・解析結果・診断スナップショット・診断スクリプトはローカル保管。Git管理が必要な再利用スクリプトは`diagnostics\`外に置く。
- ツール：`tools\`。コンパイラ等のバイナリは通常Gitに含めない。
- 正本の変更はルートGitで差分確認してコミットする。インストーラーは**自動`git add -A`、commit、remote設定、push、force-pushを行わない**。
- **バックアップは自動作成しない。** 既存ファイルを更新する場合は、添付版に対する既知SHA-256の一致、または差分確認後の明示的`-ReplaceExisting`を必要とする。まだコミットしていない変更や未追跡ファイルはGitHubで保護されているとは限らない。

## 8. ZIP配布と検証の報告

新規の`fw`、`diag`、`rules`型ZIPは`pd_pps_zip_rules.md`（現行v2.5）に従う。**ZIPの外部ファイル名から`pd_pps_`接頭辞を外す**（既発行ZIPは改名しない）。ZIP内部のASCII名、単一親フォルダー、SHA-256 manifest、README/CHANGELOG、既定dry-run、明示的`-Apply`、改変したもの／していないもの、配布先・実行コマンド・未検証事項を必須とする。実験用KITの個別試験計画はZIP内`tests/TEST_PLAN.md`に同梱する。PowerShellコマンドはWindows PowerShell 5.1を前提に、カレントディレクトリ依存を避ける。

成果物の作成、ファイルの配布、Windowsへの導入、ホスト試験、クロスビルド、実機確認、GitHub反映は**それぞれ別の完了状態**として扱う。

## 9. 本ルールの改訂

変更理由・影響・旧版との差分を示し、ZIPのCHANGELOGと規約バージョンで追跡する。長期ルールは本書、現在の状態は`HANDOFF.md`、マイルストーンと履歴は`ROADMAP.md`に記録する。チャット内の提案だけでルールが自動変更されたとは扱わない。


## 10. 新規ファイルの改行コード・文字コード（2026-11-15追補）

- **新規・更新するテキストファイルはLF（`\n`）改行を標準とし、CRLFへ変換しない**。末尾は原則として改行1個を付ける。
- Gitルート`C:\pj\pd_pps\.gitattributes`を改行の正本とし、**`* text=auto eol=lf`** でテキストをLF、バイナリを非変換とする。エディター向けにはルート`.editorconfig`で`end_of_line = lf`を宣言する。既存の`core.autocrlf`設定を無断で変更しない。
- Markdown、JSON、C/C++、Python、PowerShell、Git設定、PowerShell補助スクリプトもLFを使用する。Windows PowerShell 5.1向け`.ps1`はASCIIのみならBOM不要、日本語等の非ASCIIを含む場合は**UTF-8 BOM付き**で保存する。
- ZIPに同梱するテキストもLFで作成し、インストーラーは**バイト列を変更せずコピー**する。既存の実測ログ（UTF-8/UTF-16 LE等）および受け取った原資料は改行コードを変換しない。
- 新しく生成するExcel向けCSVは、別途定めたUTF-8 BOMを守りつつ、改行をLFで出力してよい。CSVを受け付ける外部ツールに固有要件がある場合のみ記録を残して例外を認める。
- Gitの警告を抑える目的で`core.safecrlf=false`等へ変更しない。**原因を解消**する。従来のCRLFファイルは後から段階的に整備し、全ソースや過去ログを一括変換しない。
- `.gitattributes`の適用後、Gitで改行変換の警告が発生したときは、対象ファイルの実際の改行と属性（`git check-attr text eol -- <path>`）を確認する。

## 11. 試験計画・製品仕様・実験KITの管理（v1.2制定、v1.3更新）

- **全体のテストの正本はルート`ROADMAP.md`**とする。正式M0～M10の受入ゲート、共通試験方針、未実施・保留、実測根拠、試験優先度、マイルストーン間の繰越事項を記載する。実際に確認した範囲を超えてPASSにしない。
- **個別実験の`TEST_PLAN`は、その実験用KITのZIP内`tests/TEST_PLAN.md`に収録**する。目的、対象版、準備・接続・安全、テストID、操作、期待結果、判定条件、保存ログ、停止条件、未検証事項を書く。`rules`型ZIPには実験固有TEST_PLANを要求しない。
- ルートや`src/pd_pps`直下に`M1_TEST_PLAN.md`、`M4B_TEST_PLAN.md`等の**独立した個別TEST_PLANを新規作成・登録しない**。既存の個別TEST_PLANを整理・削除する際は、必要な手順が対象KITのZIPに含まれることを先に確認する。旧計画の実績・生ログ・Git履歴は削除せず、過去の参照記述には履歴であることを示す。
- 新版KIT作成時はZIP内部に個別TEST_PLANを同梱する。計画・実行結果の相違はROADMAPの進捗と実測証拠へ反映する。ZIPに計画があることと実機試験に合格したことは別。
- **`docs/PRODUCT_SPEC.md`は維持**し、機能要件・制約・製品判断の変更に応じて随時更新する。今回はユーザーPCに存在する本文を読み込めていないため、配布ZIPから上書きしない。Gitには対象ファイルを明示して登録する。
- ZIP名の新規命名は`<type>_<module>_vNN_YYYYMMDD.zip`（例：`fw_m43b_goodcrc_v01_20261115.zip`、`rules_workspace_v04_20261115.zip`）。**ルール文書は版番号なしの固定ファイル名**`pd_pps_zip_rules.md`に統一し、改訂版は同じパスで更新する。版は本文・CHANGELOG・Git履歴で管理する。旧`pd_pps_zip_rules_v2.md`等の整理は別途明示的な操作とし、インストーラーが削除しない。
- `diagnostics/`はGit管理対象外（2026-11-15の`1eb1add`で除外済み）。Submodule登録は`af7c51c`で済んだ。既存のローカルファイル・Submoduleはルール配布時に再初期化しない。
- ファイル削除・Gitからの除外・Gitコミット・PushはZIPの`install.ps1`に含めず、対象と影響を確認したうえでユーザーが別操作で実施する。

## 12. ルール文書の単一正本（v1.3追補）

- ルートに配置するルール文書名は固定し、ファイル名に`_v1`、`_v2`等の版番号を付けない。ZIP配布規約の正本は**`pd_pps_zip_rules.md`**。`DEVELOPMENT_RULES.md`、`README.md`、`ROADMAP.md`、`HANDOFF.md`も固定名で更新する。
- 規約の版数や変更理由は**本文のバージョン／改訂日、配布KITのCHANGELOG、Gitのコミット履歴**で追跡する。版ごとの規約文書をルートで並行管理しない。
- 旧名の規約ファイルが存在するときは、最新版を新しい固定名で設置・内容検証してから、対象を指定したユーザー操作でGit管理から退役させる。インストーラーは旧ファイルを自動削除・移動しない。
- ZIPアーカイブそのものの名前には引き続き`_vNN_YYYYMMDD`を使用し、既発行ZIPは改名しない。

<!-- DEV_ENV_STANDARD_V01_20261122 -->
## 13. Chat-independent environment and one-shot execution (2026-11-22 supplement)

- Use `config/dev_environment.json` and `scripts/project/Run-DevChecks.ps1` before proposing new tool-dependent actions. Tool paths in the README are examples until confirmed on the Windows PC. Do not assume native `gcc` is present; distinguish xPack RISC-V GCC from a Windows-native C compiler.
- **Single command is mandatory for a routine workflow**: ZIP validation/unpacking, dry-run validation, authorized apply, post-verify, compatible tests, build when applicable, evidence and reporting shall be automated within ONE entrypoint. Do not require the user to separately invoke dry-run, apply, tests, and build. This supplements section 3 and its prior one-shot requirement; it does not waive any safety gate.
- Safe file creation/update can be authorized by `-Apply` in that one invocation. Flash, new device power/rewiring, destructive Git operations, and fault injection require their own explicit consent; do not silently proceed. Missing mandatory prerequisites stop the workflow; optional native-C host tests are `SKIP`/`NOT_TESTED`, NEVER `PASS`.
- New and revised `.ps1` files must pass AST syntax inspection, executable/function-shadowing checks (avoid `function Git`), and protected-variable assignment checks (avoid `$Host`). Pass on the target Windows PowerShell 5.1 must be demonstrated separately; if not run, mark `NOT_TESTED`.
- One run must print or save the cause of a failure without requiring manual log extraction in another step. Preserve original raw logs. No automatic backups, Git commits/pushes, Flash, or global environment changes.
- The persistent standard and exact commands are documented in `docs/DEVELOPMENT_ENVIRONMENT_STANDARD.md`. A new chat must consult these artifacts instead of relying on chat memory.
