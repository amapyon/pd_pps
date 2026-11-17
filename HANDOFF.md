# PD PPS 開発引き継ぎ（チャット切替用）

- 更新日：2026-11-17
- 文書状態：**M43B-8 A/Bまでの実機結果を反映。Gitコミット・Pushは実際の実行結果で判定し、本書の作成だけでは完了としない。**
- 正本：`C:\pj\pd_pps\HANDOFF.md`

## 1. 新しいチャットで最初に読む資料

1. `DEVELOPMENT_RULES.md`：開発・チャット・検証の共通規約。
2. `ROADMAP.md`：製品目標と正式M0～M10の進捗。
3. `HANDOFF.md`：今回の状態、仮説、次の実験。
4. 必要に応じ`README.md`、`pd_pps_zip_rules.md`、実コードと生ログ。

ChatGPTがユーザーPCの最新ファイルを直接参照できるとは限らない。必要な資料は添付する。**この文書はスナップショットであり、現PCのファイルやGit状態が同一とは保証しない。**

## 2. 現在の目的と開発状態

PD/PPS Explorer：USB PD/PPSの能力取得、PPS設定、INA226測定、スタンドアロン操作、PC通信を実現し、最終的にBOOTHで完成品とベアPCBを販売する。

- 正式マイルストーン：M0～M3は限定された範囲で**完了**、M4・M5・M7は**進行中**、M6・M8～M10は**未着手**。
- ハードウェア：WeActStudio CH32X035 CoreBoard、WCH-LinkE、PD Sink用USB-C、PC USB CDC用の別接続。
- **直近の実機検証版：M43B-8 B**（`src\pd_pps\firmware\experiments\43b8b_pps_control`）。PPS CDC設定、DMMでの実電圧変化、要求電流設定の往復操作を確認。M43B-7は比較基準。
- 正常動作実績のあるPHY参照元：**PD17 M13b**、`src\pd_pps\firmware\experiments\17_pd_spr_sink_rebuild`。
- 直近の開発：**M43B-8 Aの電力選択分離**、**M43B-8 BのPC設定API**を実機確認。次はM43B-8 Cに進む前にPC USB/COM/PDの切断・再接続の安全性試験（C01～C05）。接続条件によるPD失敗例は残存。
- **M13b・M43A・M43B-5/6/7・M43B-8 A/Bを保全**する。PHY/USBFS ISR/時間定数の変更を伴う原因対策は独立した実験で行う。

## 3. 実機で確認したこと

### M43B-8 A/B（2026-11-17、最優先の現行記録）

- **M43B-8 A：** PDO/APDO選択・RDO生成を`pd_power.c/h`へ分離。PPS 7V/1AとUSB CDC監視を同時に実機確認。CC2 READY下の120秒Monitorは**120/120成功**、`PDERR`全件0、PPS周期更新の増分**24/24完了**。
- **M43B-8 B：** `PPS GET`/`SET`/`STATUS`追加。要求受付時は`pending=1 applied=0`、Accept/PS_RDY後は`pending=0 applied=1`。7.02Vへの往復契約変更、DMMで確認した7.20Vへの実電圧往復変更、要求電流500/1,000mAの往復、無効値3件拒否がPASS。
- **検証：** Python74件中65 PASS/9 SKIP、クロスビルドFLASH 26,184B/RAM 5,904B。ネイティブC試験のSKIPは未実施扱い。
- **制約：** 一部接続順序/CC向きでSource Caps未受信やTX_FAILEDを観測。安全動作・負荷・長時間・他Sourceは未検証。要求電流は実負荷の電流制限の確認ではない。
- **詳細：** [`docs/M43B8_AB_VERIFICATION.md`](docs/M43B8_AB_VERIFICATION.md)。この下にあるM43B-7/M43A節は過去の比較記録。

### 過去の比較基準：M43B-7 P2まで（2026-11-15）

| 版／試験 | PC USB | 主な結果 | 判定 |
|---|---|---|---|
| M43B5_05 | OFF | GoodCRC一致7回、PPS READY約31.9秒、周期更新6/6 | 通信成功の単独実績 |
| M43B5_06R1 | ON／CDCアイドル | GoodCRC一致0、CRC_TIMEOUT3回、TX_FAILED、PnP120秒COM16安定 | USB再認識を伴わない既知失敗 |
| M43B5_07 | OFF | GoodCRC一致・READY約30ms、VBUS_FAULT | PPS長時間安定は未合格 |
| M43B6 R01/R02/R03 | OFF／ON／CDC | 既知成功・失敗・安全停止の回帰、CDC24/24 PASS | P0/P1主要回帰PASS |
| **M43B7 R01** | OFF | GoodCRC一致、Accept/PS_RDY/READY24ms、約590msでVBUS_FAULT | P2主要回帰PASS |
| **M43B7 R02** | ON／CDCアイドル | Request再送2、CRC_TIMEOUT3、TX_FAILED、USBFS IRQ0、READYなし | P2主要回帰PASS（PDは失敗） |
| **M43B7 R03** | ON／PD切断 | CDCのPING/STAT/COUNT/VBLOG/PDTRACE 24/24 PASS | P2 CDC回帰PASS |

ホストテスト：M43B-6は59件実行・失敗0・スキップ1、M43B-7は67 PASS/1 SKIP。**これは限定条件でのリファクタリング回帰PASSであり、PC USB接続中のPD安定や製品要件の完了ではない**。検証結果と元データSHA-256は`docs/M43B7_P2_VERIFICATION.md`。

### M43A時点の過去の実機記録

| 実験 | CDC接続ラベル | 初回Request | PPS周期更新 | 備考 |
|---|---|---|---|---|
| 旧M13b実績 | PC通信用USBなし | 再送0でGoodCRC受信実績 | 更新成功実績あり | M43Aと同条件の統計比較ではない |
| M43A先行実験 | CDC通信中 | 再送1回後にGoodCRC | この短時間試験では未確認 | PPS 7V・要求1A契約に成功 |
| **M43A試行01** | OFF / CC1 | 再送0回で成功 | **2/2成功** | 接続12,098ms、TRACE欠落0 |
| **M43A試行02** | ON / CC1 | 再送0回で成功 | **初回更新で2回再送後`TX_FAILED`** | `refresh_done=0`、VBUS低下0、Source HR 0、TRACE欠落0 |

**判定の限界：** 同条件の反復はOFF/ON各1回のみ。CDC接続が原因と断定できない。ON/OFFはユーザーが記録したケーブル条件であり、実際のCDC列挙・転送量を示す計測値ではない。`TX_FAILED`だけでSourceがGoodCRCを送らなかったと判断できない。

実験ログの標準保存先は`C:\pj\pd_pps\diagnostics\m43a_goodcrc\logs\`。ユーザーPC上への配置完了は未確認。診断v3の出力は固定名CSVで、今後のv04規約と異なる。**診断v04はまだ作成・実機導入していない。**

## 4. 事実・未確定の要因

- **観測事実：** PC通信用USBを物理接続したM43B5_06R1/M43B7 R02では初回GoodCRCを認識できず`TX_FAILED`。PC USB物理切断のM43B5_05/07・M43B7 R01では初回GoodCRC一致。ただし05以外は長時間READYを維持していない。
- **負の証拠：** 06R1のPnP監視ではCOM16/WCH-LinkEは安定。CDCアイドルでUSBFS IRQが0でもGoodCRC失敗を再現。単純なUSBFS ISR競合は十分な説明にならない。
- **未確定：** PC USBの追加GND/D+/D−/接触条件、CC信号品質、送信後RX復帰、Source側のGoodCRC送出有無、充電器・ケーブル差。
- **VBUS：** 07/M43B-6/7のREADY直後の低電圧停止はADC実測値と実VBUSがおおむね一致している前提で扱い、精度改善は製品PCBの完成後に回す。実際の低電圧原因は未確定。

原因対策はP3リファクタリングとは分け、必要ならPDアナライザー／CC波形を利用する。

## 5. 次に実施する作業

**2026-11-17更新：** 直近はM43B-8 Bの検証記録とA/Bソースを選択的にGitへコミット・Pushする。その次にファームウェアを変更せずC01 COM Close、C02 PC USB切断、C03再接続、C04 PD充電器切断、C05再接続を行う。安全上の期待動作を決めずに「継続しているから安全」と判断しない。

以下の2026-11-15作業項目は履歴として残す。


1. **今回のコミット：** 現行3文書を基準にした本更新KITを既知SHA-256照合で適用する。M43B-6・M43B-7のソースと、ルートREADME/ROADMAP/HANDOFF、`docs/M43B7_P2_VERIFICATION.md`だけを選択的にGitへステージし、差分確認後にローカルコミットする。
2. **P3：** M43B-7を基準に診断イベント名・状態遷移の説明を一元管理し、PC側でSRAMとPDログの時系列表示を作る。既存バイナリ診断フォーマットとCDC応答を維持する。P3は**未実装**。
3. **別系統の検証：** PC USB接続によるGoodCRC失敗、契約後のVBUS低電圧を引き続き切り分ける。ただし当面はADC精度の追加検証をしない。

**履歴注記：** 従前のM43A試行03/04・各10試行の指示は、M43A時点の計画であり現在の直近操作ではない。実際に後続M43B系列の試験が実施済み。

## 6. 保留事項・変更禁止範囲

- M13b/M43A/M43B-5/M43B-6/M43B-7の既存ソースを上書きせず、根拠なくPD PHYを改修しない。
- Source発Hard Resetの実機試験、長時間連続運転、危険を伴う故障注入は**ユーザーが再開を決めるまで保留**。
- M43B-5、M43B-6、M43B-7は**作成済み／短時間実機検証済み**。P3は未作成・未検証。
- **新Gitルートは`C:\pj\pd_pps`として確立済み**。`ch32fun`をSubmoduleとして固定（`af7c51c`）、`diagnostics/`を全体Git除外（`1eb1add`）した。ソース・測定ログは保全する。
- GitHubのリモート設定・コミット・Pushは自動では行わない。**現PCのリモート設定は未確認**。ZIPの作成だけではPCへの適用・コミットを意味しない。

## 7. この文書の更新方法

有意な作業終了時に、①実機FW名／コミット（分かれば）、②最新の確定試験、③未確認の仮説、④次の1操作、⑤元データの所在、⑥未検証の範囲、⑦保留事項を更新する。長い履歴は`ROADMAP.md`と実験ログへ残し、チャット全文は転載しない。

## 8. 最新の開発・配布ルール（2026-11-15改訂）

- 全体のテスト計画・正式マイルストーンのゲート・進捗・未実施事項は**ルート`ROADMAP.md`**にまとめる。
- 個別の`TEST_PLAN`は、今後作成する**各実験用KIT ZIPの`tests/TEST_PLAN.md`**に入れる。既存の`src/pd_pps/M*_TEST_PLAN.md`等は自動削除せず、必要事項を対応KITへ収録したことを確認してからユーザーが削除する。
- **`docs/PRODUCT_SPEC.md`は残し、製品仕様に応じて適宜更新**する。現ファイル本文は今回の変更KITから置換しない。
- **ルール文書名は版番号なしの固定名**とし、ZIP規約は`pd_pps_zip_rules.md`を唯一の現行正本とする。旧版名の文書は新正本の検証後にユーザーが個別にGit管理から退役させる（インストーラーでは削除しない）。
- **新規ZIP名から`pd_pps_`接頭辞を削除**する。例：`rules_workspace_v05_20261115.zip`。既存ZIPは改名・再梱包しない。
- `diagnostics/`はGit管理全体から除外、`ch32fun`はSubmodule。実測ログ・ファームウェア・配線には今回のルール変更で触れない。
- 新規ルールZIPは既定dry-run、明示的`-Apply`。旧版ハッシュが一致しない場合は停止し差分確認する。自動バックアップ、Gitコミット、Pushはしない。
