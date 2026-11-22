# PD PPS ZIPファイル作成規約 v2.5

- 改訂日：2026-11-22
- 正本：`C:\pj\pd_pps\pd_pps_zip_rules.md`
- 旧規約の内容は改訂履歴に保持する。正本ファイル名は版なしの`pd_pps_zip_rules.md`に固定し、今後の規約改訂も同じファイルで行う。旧ZIPは遡って改変しない。
- v2.5追補：新規・改修診断スクリプトのコンソール時刻記録を必須化し、DMM照合と生ログ保護を標準化。
- v2.4追補：二重USB接続（CDC ON+PD/PPS）を主要試験条件とし、`fw`型ZIPはインストールから明示FLASHまでの一括実行スクリプトを必須とする。
- v2.3追補：規約文書のファイル名から版番号を削除し、`pd_pps_zip_rules.md`を単一正本とする。
- v2.2追補：ZIPファイル名から`pd_pps_`を除去。全体試験計画は`ROADMAP.md`、個別試験計画は実験KIT内`tests/TEST_PLAN.md`に統一。製品仕様`docs/PRODUCT_SPEC.md`を維持。
- v2.1追補：Gitルートの`.gitattributes`／`.editorconfig`でLF改行を統一。既存ログを変換しない。
- v2.0変更点：`rules`型追加、Gitルート統一、バックアップ不要、既知ハッシュでの更新、Git移行操作の分離。

## 1. 配布単位

1 ZIP = 1目的。種別は`fw`（ファームウェア）、`diag`（診断）、`rules`（開発・文書管理）。ルール配布の中でファームウェアを変更しない。複数種別の変更は原則別々のZIPとする。実績版M13bと現行M43Aを規約配布の副作用で変えない。

## 2. ZIP名と内部階層

命名：`<type>_<module>_vNN_YYYYMMDD.zip`。**先頭に`pd_pps_`を付けない。**

例：`rules_workspace_v04_20261115.zip`、`diag_m43a_goodcrc_v04_20261115.zip`、`fw_m43b_goodcrc_v01_20261115.zip`（M43Bはあくまで命名例であり、実機版が存在するという意味ではない）。既発行の`pd_pps_...zip`や日本語名の旧ZIPは、履歴保護のため自動改名・内部変更しない。

ZIP内のファイル名・フォルダー名にはASCII英数字、`-`、`_`、`.`だけを使用する。ディレクトリ階層には`/`を使う。日本語・絶対パス・`..`・重複パス・NTFS代替データストリーム・シンボリックリンクを含めない。ZIP直下は**ZIPの拡張子を除いた名前の単一親フォルダー**とする。版は不変で、変更時はvNNを増やす。

必須ファイル：`README.md`、`CHANGELOG.md`、`manifest.json`、`install.ps1`。**新規`fw`型ZIPは`Run-Firmware.ps1`も必須**とする。ファイル本文はUTF-8でよい。

ルール配布の例：

```text
rules_workspace_v04_20261115/
  README.md
  CHANGELOG.md
  manifest.json
  install.ps1
  rules/
    README.md
    ROADMAP.md
    DEVELOPMENT_RULES.md
    HANDOFF.md
    pd_pps_zip_rules.md
    .gitignore
  scripts/
    Initialize-Root-Git.ps1
    Show-PD-PPS-Status.ps1
```

`fw`は実装ソースを`firmware/experiments/<新規固有名>`に収める。実際にビルドした場合のみ`build/`を追加する。`diag`は`scripts/`、必要に応じ`records/`テンプレートを持つ。**実験用KITとして配布する`fw`・`diag`には`tests/TEST_PLAN.md`を必須**とする。`rules`型ZIPは実験用KITではないため不要。存在しないバイナリ・実施していないテスト結果を含めない。

実験用KITの例（命名例であり、当該版の作成・実装を意味しない）：

```text
fw_m43b_goodcrc_v01_20261115/
  README.md
  CHANGELOG.md
  manifest.json
  install.ps1
  Run-Firmware.ps1   # ZIP検証後からFLASHまでを一括管理
  tests/
    TEST_PLAN.md        # 個別の試験手順と判定条件
  firmware/
    experiments/
      43b_example/
        ...
```

`tests/TEST_PLAN.md`には、対象版・目的・前提配線・安全条件・試験ID・手順・期待結果・PASS/FAIL/REVIEW基準・必要ログ・停止条件・未実施を明記する。**全体のテスト進捗・マイルストーン通過条件はルート`ROADMAP.md`に記録**する。単体の`M*_TEST_PLAN.md`はルートや`src/pd_pps`直下へ新規配置しない。

## 3. 正本の配置先

プロジェクトルートは**`C:\pj\pd_pps`（単一のGitルート）**とする。

- ルート：`README.md`、`ROADMAP.md`（全体テスト方針・ゲートを含む）、`DEVELOPMENT_RULES.md`、`HANDOFF.md`、本規約、`.gitignore`。
- 開発管理用スクリプト：`scripts\project\`。
- ファームウェアソース：`src\pd_pps\firmware\experiments\`。
- 製品仕様：`docs\PRODUCT_SPEC.md`（Git管理する独立の正本。製品判断・要求の変更に応じて更新し、KIT配布の副作用で上書きしない）。
- 診断：`diagnostics\<対象>\logs\`、`analysis\`、`records\`、`scripts\`。**`diagnostics/`全体がGit除外対象**。
- ローカルツール群：`tools\`（通常Gitへ含めない）。

**ZIPの一時保存先は`C:\pj\pd_pps`であり、展開先と最終配置先は別**。展開したルール群を正本として二重管理しない。単一Gitルートへの移行は完了済みであり、ソース側READMEの旧Git境界に関する記述は現行運用として使用しない。

## 4. インストールと既存ファイル保護

- `install.ps1`は引数なしで**dry-run**。変更対象と配置先を表示するだけ。
- `-Apply`指定時のみ、manifestの配置宣言に従ってファイルをコピーし、コピー前後にSHA-256を検証する。
- **バックアップは作成しない**（ユーザーの明示的な方針）。GitHub履歴が未追跡・未コミットのローカルファイルを保護しているとは限らない。
- 既存ファイルが同じハッシュならスキップ。既知の旧版SHA-256と一致する場合は`-Apply`で更新可能。それ以外は**変更前に中止**し、実際の差分を確認したうえで`-ReplaceExisting`を明示する場合だけ更新する。
- **`install.ps1`単体**はファイル配置専用とし、無関係なファイル削除、旧`M*_TEST_PLAN.md`の自動削除、ログの自動移動、ファームウェアの書き込み、Gitの自動commit/Pushを行わない。`fw`型の上位`Run-Firmware.ps1`からは、個別の`FLASH`確認を経て書き込みまで進む。個別TEST_PLANの移行・削除はユーザーによる別操作。
- Gitルートと`ch32fun` Submoduleは既に復旧済み。現行`.git`とSubmoduleの管理情報をルール適用の副作用で削除・変更しない。GitHubへの強制Pushは行わない。
- インストール先はプロジェクトルートからの安全な相対パスだけを許可し、`..`などでルート外へ書き込まない。

## 5. 診断ログと解析結果の保全

ログ形式：`diagnostics\m43a_goodcrc\logs\M43A_<試行番号2桁>_<ON|OFF>_<CC1|CC2>.log`。試行番号は原則一意で再利用しない。ON/OFFは実験者の物理接続条件ラベルであり、実際のUSB列挙・CDC転送量の証明ではない。

新規約準拠の解析出力は`analysis\YYYYMMDD_HHMMSS\`に毎回**新規ディレクトリ**を作り、`requests.csv`、`trial_details.csv`、`summary.csv`、`run_manifest.json`を保存する。元ログ・過去の解析を上書きしない。初回とPPS更新Requestを別集計し、GoodCRC受信とTX_ENDを混同しない。欠落・不完全な記録は`REVIEW`。元ログのUTF-8/UTF-16 LE等を勝手に変換しない。

### 診断コンソールの時刻記録（v2.5）

新規作成・改修する`fw`／`diag`／`rules`型KITのPowerShell診断・実機試験スクリプトは、ルート`DEVELOPMENT_RULES.md`の「診断コンソールログのタイムスタンプ」に従う。

- 操作、測定、状態、エラー、判定など**証跡となるコンソール出力には** `yyyy-MM-dd HH:mm:ss.fff zzz` 形式の日時（オフセット付き）を付与する。
- 外部測定器と時系列照合する試験では `Stopwatch` による `T+秒` と、操作・DMM照合マーカーを併記する。
- コンソールと保存ログに**同一のタイムスタンプ付き行**を出力し、実行ごとに固有のログ先へ保存する。取得頻度は必要最小限とする。
- 手操作のEnter入力時刻、CDC応答受信時刻、実際の物理イベント時刻は区別して記述する。
- `minichlink`、MCUなどが出す**生ログ・標準出力・標準エラーは改変しない**。必要なら取得開始・終了の日時を別に記録し、元のバイト列・文字コードを保全する。
- 実験KITの`tests/TEST_PLAN.md`には、時刻記録の方法、照合する測定器・イベント、ログ保存先、照合可能な時間分解能・誤差を記載する。時刻情報がない既存ログを後から補正・上書きしない。

## 6. README・CHANGELOG・manifest

`README.md`には目的、対象ボード/MCUと版、前提、変更対象と非変更対象、既定パス、ZIP展開から実行までの**コピー可能なPowerShellコマンド**、安全上の注意、未検証事項を記す。実験KITの`tests/TEST_PLAN.md`は、計画であって実績を意味しないことを明記する。

`CHANGELOG.md`には今回の変更、旧版との差、互換性と注意を記す。

`manifest.json`にはパッケージID、種別、版、作成日、対象ルート、ZIP内の**manifest自体を除く全ファイルのSHA-256**、配置対象の相対パス、必要なら既知の旧版ハッシュ、検証結果を記す。manifestにないファイルを勝手に配置しない。

## 7. 文字コードと検証

- ZIP内ファイル名はASCII。**ZIPに含めるテキストの改行はLFに統一**する（CRLF禁止）。Markdown本文はUTF-8、PowerShell 5.1用`.ps1`はASCIIのみならBOM不要・日本語など非ASCIIを含むならUTF-8 BOM、Excel向けCSVはUTF-8 BOMを基本とする。
- PowerShellコマンドは`powershell.exe -NoProfile -ExecutionPolicy Bypass -File "<フルパス>"`形式で案内する。カレントディレクトリ依存を避ける。
- 配布前にZIP CRC、単一親フォルダー、全パス安全性・重複、manifestハッシュ、コマンドと実在ファイル、dry-run／`-Apply`／再実行の仕様を検査する。
- Windows PowerShell 5.1で実行できない場合は**`NOT_TESTED`**と明示する。ホスト試験、クロスビルド、実機確認は別判定とする。Git整理のために既存コードのビルド検証を先送りしても構わないが、PASSと偽らない。
- 配布時にZIP名、変更点、内部構成、配置先、コマンド、検証結果・制約を説明する。

## 8. Git管理の方針

新しいGitの正本は`C:\pj\pd_pps`のみ。ルートGitは初期化済み。`src\pd_pps\ch32fun`は`cnlohr/ch32fun`へのGit Submoduleとして登録済み（`af7c51c`）。旧`src\pd_pps\.gitmodules`等の修正・削除はルール導入では行わず、必要時に明示的に整理する。ソース・ログ・Submoduleの内容は保持する。

インストーラーは`git add -A`、自動コミット、remote設定、`git reset --hard`、`git clean`、Push／強制Pushを行わない。旧GitHubリポジトリと新ルートの履歴は別系列になり得るため、空のGitHubリポジトリへ接続するか、既存remoteの履歴変更について明示的に判断する。


## 9. Gitの改行属性と作成時検査（v2.1）

- Gitルート`.gitattributes`で**`* text=auto eol=lf`**を定義する。既知のバイナリ拡張子は`-text`を明示し、Gitがバイト列を変換しないようにする。
- `.editorconfig`で`end_of_line = lf`、`insert_final_newline = true`を宣言する。PowerShell 5.1向け非ASCIIスクリプトはUTF-8 BOM。
- ZIP生成時、Markdown、PowerShell、JSON、設定ファイル等のテキストに**CR（`\r`）が混入しないこと**を検査する（元の実測ログなどは例外）。
- `git add`時に`LF will be replaced by CRLF`警告が出ない状態を目標とし、Gitの警告機能そのものは無効化しない。以前作成されたCRLFファイルの警告は、必要な時にそのファイルだけ変換して解消する。
- 本規則追加に伴う既存ソース一括正規化、実測ログの変換、`git add -A`、自動commit/Pushは行わない。

## 10. v2.3の運用上の決定事項

1. **本規約の固定ファイル名は`pd_pps_zip_rules.md`**とする。規約の改訂時はこの同じパスで更新する。ルートGitでは現行ルールのみを管理し、旧名（`pd_pps_zip_rules_v1.md`・`pd_pps_zip_rules_v2.md`）は新版を検証後、明示的にGit管理から退役させる。既存ファイルを自動削除しない。
2. 実験のテスト計画はZIP内部の`tests/TEST_PLAN.md`を正本とする。単独TEST_PLANを開発ルートで配らない。全体の試験方針・マイルストーン受入条件は`ROADMAP.md`に集約する。
3. 旧`M*_TEST_PLAN.md`の削除・Git管理変更は**手動で対象を指定**する。インストーラーには削除機能を設けない。削除前に必要な手順が対応する実験KITへ含まれていることを確認する。
4. `docs/PRODUCT_SPEC.md`は製品仕様の正本であり保持する。未提供の現行内容を配布ZIPから推測して置換しない。
5. `diagnostics/`は全体がGit管理対象外。既存ZIPおよび生データは自動移動・改名しない。

## 11. 規約文書とZIPアーカイブの版の区別

- **規約Markdownファイル名は固定**（`pd_pps_zip_rules.md`）。改訂版は同じ正本を更新し、版数と差分は本文、CHANGELOG、Git履歴で管理する。複数版の規約をGit管理のルートに並べない。
- **配布ZIPアーカイブのファイル名は別ルール**であり、再現性のため`<type>_<module>_vNN_YYYYMMDD.zip`の版番号を保持する。先頭に`pd_pps_`は付けない。過去のZIPは自動改名しない。
- 旧名の規約を整理するときは、固定名への新規導入・内容確認を完了してから、Git管理の変更と必要ならローカル削除をユーザーが個別実施する。


## 12. v2.4：CDC ON二重通信とFLASHまでの一括実行

### 標準の試験条件

- 現在のGoodCRC診断は、**PC USB CDCとUSB PD/PPSを同時に接続して動作させること**を主要目的とする。試行で別条件が明示されない限り、`CDC ON`（PC通信用USB物理接続）＋PD充電器（CC1/CC2を指定）＋無負荷を標準とする。単なる旧版退行確認など、`CDC OFF`を実施する場合はその理由と試行IDを明記する。標準試験を勝手にOFFへ置き換えない。
- `COM16`等のCOMポート名は接続の手掛かりであり、PD/USB両通信の成立を証明しない。CDC ON試験でCOMポートが存在することを「ケーブル接続異常」と判定しない。列挙・送受信・電気的接続を区別して記録する。
- **配線の安全性は別の必須前提**：PCとPD充電器のD+/D-に共有疑いがある。二重接続要求はそのリスクを免除しない。D+/D-が電気的に競合しない構成、CC、VBUS/GND、逆給電の条件を守り、未検証の同時直結を安全と主張しない。ソフトウェアのfail-fastでは配線衝突は予防できない。

### 新規`fw`型ZIPの必須エントリーポイント

- ZIP直下の単一親フォルダーに**`Run-Firmware.ps1`**を同梱する。READMEにも、これを使う**一つの貼付可能な`& { ... }`PowerShell 5.1ブロック**を記載する。`Run-Firmware.ps1`自体をワンクリックで実行しても、`APPLY`と`FLASH`の確認を省略しない。
- このランナーは、前提ファイル・ZIP/manifest SHA-256の確認から、展開、`install.ps1`のdry-run、ユーザーの`APPLY`、インストール後の確認、ホスト試験、CH32X035クロスビルド、BIN/ELFサイズ・ハッシュ確認、WCH-LinkEの`-i`プローブ、`FLASH`確認、書き込み、終了ログ確認まで**順序どおり1回の実行**で進める。BUILDで正常終了してFLASHを別途コピペする方式は標準にしない。
- ファームウェア書き込み対象がCH32X035であること、BINが対象容量に収まること、前提の接続・給電状態を、書き込み前に表示・照合する。現在書き込み済みの版が置換されることを明記する。`FLASH`が入力されなければCANCELEDと表示し、書き込みしない。
- どのステップでも前提・終了コード・ハッシュ・必須生成物・期待出力が異常なら即時`throw`し、**後続のビルドやFLASHを実行しない**。`& { ... }`、`$ErrorActionPreference='Stop'`、`Set-StrictMode`、`try/catch`再スロー、`$LASTEXITCODE`又は`Start-Process`の`ExitCode`検査を用いる。1本のスクリプトによるfail-fastは**ロールバックを保証しない**。
- PowerShell 5.1で`minichlink`のstderrを`2>&1`に合成して`NativeCommandError`として誤停止させないよう、`Start-Process -RedirectStandardOutput/-RedirectStandardError -Wait -PassThru`等を使用する。プローブ結果の`Detected CH32X035`とプログラム結果`Image written.`、終了コードを検査し、ログは既存を上書きしない固有フォルダーに保存する。
- `FLASH COMMAND PASSED`は書き込みコマンドの正常終了のみを意味し、PD/PPSやCDCの実機試験PASSを意味しない。実機ログ採取、SRAMダンプ取得、Git操作は目的に応じて独立した手順とし、暗黙に実行しない。
- `rules`型・`diag`型などファームウェアを書き込まないZIPには、`Run-Firmware.ps1`もFLASHも不要。`rules`型インストーラーは従来どおり文書のみを更新する。

### 配布と検証の記録

- READMEと`tests/TEST_PLAN.md`に、同時接続条件、電気的前提、実行シーケンス、`APPLY`・`FLASH`の2段階確認、失敗時停止、ログ保存先、試験結果の判定単位を示す。
- 受入報告は`ZIP検証`／`適用`／`ホストテスト`／`ビルド`／`FLASH`／`実機通信`を区別し、未実施をPASSとしない。Windows実機で試せていないコード・ツール連携は`NOT_TESTED`と明記する。
