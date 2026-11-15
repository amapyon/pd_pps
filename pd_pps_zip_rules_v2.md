# PD PPS ZIPファイル作成規約 v2.1

- 改訂日：2026-11-15
- 正本：`C:\pj\pd_pps\pd_pps_zip_rules_v2.md`
- 旧版`pd_pps_zip_rules_v1.md`を**今後の新規配布について改訂**。旧ZIPは遡って改変しない。
- v2.1追補：Gitルートの`.gitattributes`／`.editorconfig`でLF改行を統一。既存ログを変換しない。
- v2.0変更点：`rules`型追加、Gitルート統一、バックアップ不要、既知ハッシュでの更新、Git移行操作の分離。

## 1. 配布単位

1 ZIP = 1目的。種別は`fw`（ファームウェア）、`diag`（診断）、`rules`（開発・文書管理）。ルール配布の中でファームウェアを変更しない。複数種別の変更は原則別々のZIPとする。実績版M13bと現行M43Aを規約配布の副作用で変えない。

## 2. ZIP名と内部階層

命名：`pd_pps_<type>_<module>_vNN_YYYYMMDD.zip`。

例：`pd_pps_rules_workspace_v01_20261115.zip`、`pd_pps_diag_m43a_goodcrc_v04_20261115.zip`。

ZIP内のファイル名・フォルダー名にはASCII英数字、`-`、`_`、`.`だけを使用する。ディレクトリ階層には`/`を使う。日本語・絶対パス・`..`・重複パス・NTFS代替データストリーム・シンボリックリンクを含めない。ZIP直下は**ZIPの拡張子を除いた名前の単一親フォルダー**とする。版は不変で、変更時はvNNを増やす。

必須ファイル：`README.md`、`CHANGELOG.md`、`manifest.json`、`install.ps1`。ファイル本文はUTF-8でよい。

ルール配布の例：

```text
pd_pps_rules_workspace_v01_20261115/
  README.md
  CHANGELOG.md
  manifest.json
  install.ps1
  rules/
    README.md
    ROADMAP.md
    DEVELOPMENT_RULES.md
    HANDOFF.md
    pd_pps_zip_rules_v2.md
    .gitignore
  scripts/
    Initialize-Root-Git.ps1
    Show-PD-PPS-Status.ps1
```

`fw`は実装ソースを`firmware/experiments/<新規固有名>`に収める。実際にビルドした場合のみ`build/`を追加する。`diag`は`scripts/`、必要に応じ`records/`テンプレート、`tests/`を持つ。存在しないバイナリ・実施していないテスト結果を含めない。

## 3. 正本の配置先

プロジェクトルートは**`C:\pj\pd_pps`（単一のGitルート）**とする。

- ルート：`README.md`、`ROADMAP.md`、`DEVELOPMENT_RULES.md`、`HANDOFF.md`、本規約、`.gitignore`。
- 開発管理用スクリプト：`scripts\project\`。
- ファームウェアソース：`src\pd_pps\firmware\experiments\`。
- 診断：`diagnostics\<対象>\logs\`、`analysis\`、`records\`、`scripts\`。
- ローカルツール群：`tools\`（通常Gitへ含めない）。

**ZIPの一時保存先は`C:\pj\pd_pps`であり、展開先と最終配置先は別**。展開したルール群を正本として二重管理しない。ルートへのGit移行前は、ソース側READMEに旧Git境界が残っていても実験の履歴と区別する。

## 4. インストールと既存ファイル保護

- `install.ps1`は引数なしで**dry-run**。変更対象と配置先を表示するだけ。
- `-Apply`指定時のみ、manifestの配置宣言に従ってファイルをコピーし、コピー前後にSHA-256を検証する。
- **バックアップは作成しない**（ユーザーの明示的な方針）。GitHub履歴が未追跡・未コミットのローカルファイルを保護しているとは限らない。
- 既存ファイルが同じハッシュならスキップ。既知の旧版SHA-256と一致する場合は`-Apply`で更新可能。それ以外は**変更前に中止**し、実際の差分を確認したうえで`-ReplaceExisting`を明示する場合だけ更新する。
- 無関係なファイル削除、ログの自動移動、ファームウェアの書き込み、Gitの自動commit/Pushを行わない。
- 旧`src\pd_pps\.git`などGit管理情報の削除は**別の移行スクリプトで明示的`-Apply`**を必要とする。ソースファイルは削除しない。GitHubへの強制Pushは行わない。
- インストール先はプロジェクトルートからの安全な相対パスだけを許可し、`..`などでルート外へ書き込まない。

## 5. 診断ログと解析結果の保全

ログ形式：`diagnostics\m43a_goodcrc\logs\M43A_<試行番号2桁>_<ON|OFF>_<CC1|CC2>.log`。試行番号は原則一意で再利用しない。ON/OFFは実験者の物理接続条件ラベルであり、実際のUSB列挙・CDC転送量の証明ではない。

新規約準拠の解析出力は`analysis\YYYYMMDD_HHMMSS\`に毎回**新規ディレクトリ**を作り、`requests.csv`、`trial_details.csv`、`summary.csv`、`run_manifest.json`を保存する。元ログ・過去の解析を上書きしない。初回とPPS更新Requestを別集計し、GoodCRC受信とTX_ENDを混同しない。欠落・不完全な記録は`REVIEW`。元ログのUTF-8/UTF-16 LE等を勝手に変換しない。

## 6. README・CHANGELOG・manifest

`README.md`には目的、対象ボード/MCUと版、前提、変更対象と非変更対象、既定パス、ZIP展開から実行までの**コピー可能なPowerShellコマンド**、安全上の注意、未検証事項を記す。

`CHANGELOG.md`には今回の変更、旧版との差、互換性と注意を記す。

`manifest.json`にはパッケージID、種別、版、作成日、対象ルート、ZIP内の**manifest自体を除く全ファイルのSHA-256**、配置対象の相対パス、必要なら既知の旧版ハッシュ、検証結果を記す。manifestにないファイルを勝手に配置しない。

## 7. 文字コードと検証

- ZIP内ファイル名はASCII。**ZIPに含めるテキストの改行はLFに統一**する（CRLF禁止）。Markdown本文はUTF-8、PowerShell 5.1用`.ps1`はASCIIのみならBOM不要・日本語など非ASCIIを含むならUTF-8 BOM、Excel向けCSVはUTF-8 BOMを基本とする。
- PowerShellコマンドは`powershell.exe -NoProfile -ExecutionPolicy Bypass -File "<フルパス>"`形式で案内する。カレントディレクトリ依存を避ける。
- 配布前にZIP CRC、単一親フォルダー、全パス安全性・重複、manifestハッシュ、コマンドと実在ファイル、dry-run／`-Apply`／再実行の仕様を検査する。
- Windows PowerShell 5.1で実行できない場合は**`NOT_TESTED`**と明示する。ホスト試験、クロスビルド、実機確認は別判定とする。Git整理のために既存コードのビルド検証を先送りしても構わないが、PASSと偽らない。
- 配布時にZIP名、変更点、内部構成、配置先、コマンド、検証結果・制約を説明する。

## 8. Git管理の方針

新しいGitの正本は`C:\pj\pd_pps`のみ。旧`src\pd_pps\.git`は**明示的な移行操作**で削除できる。旧履歴を保管する必要はないが、ソース・ログを削除してはならない。`ch32fun`の古い`.git`ポインターも対象になり得る。旧`src\pd_pps\.gitmodules`の修正・削除はルール導入では行わず、必要時に整理する。

インストーラーは`git add -A`、自動コミット、remote設定、`git reset --hard`、`git clean`、Push／強制Pushを行わない。旧GitHubリポジトリと新ルートの履歴は別系列になり得るため、空のGitHubリポジトリへ接続するか、既存remoteの履歴変更について明示的に判断する。


## 9. Gitの改行属性と作成時検査（v2.1）

- Gitルート`.gitattributes`で**`* text=auto eol=lf`**を定義する。既知のバイナリ拡張子は`-text`を明示し、Gitがバイト列を変換しないようにする。
- `.editorconfig`で`end_of_line = lf`、`insert_final_newline = true`を宣言する。PowerShell 5.1向け非ASCIIスクリプトはUTF-8 BOM。
- ZIP生成時、Markdown、PowerShell、JSON、設定ファイル等のテキストに**CR（`\r`）が混入しないこと**を検査する（元の実測ログなどは例外）。
- `git add`時に`LF will be replaced by CRLF`警告が出ない状態を目標とし、Gitの警告機能そのものは無効化しない。以前作成されたCRLFファイルの警告は、必要な時にそのファイルだけ変換して解消する。
- 本規則追加に伴う既存ソース一括正規化、実測ログの変換、`git add -A`、自動commit/Pushは行わない。
