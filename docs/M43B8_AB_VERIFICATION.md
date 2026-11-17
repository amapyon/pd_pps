# M43B-8 A/B 実装・実機検証記録（2026-11-17）

- 対象ボード：WeActStudio CH32X035 CoreBoard V1.0（CH32X035F8U6）
- 参照ベース：M43B-7 P2、GitHubチェックポイント `4e8939fcca5004e28042c69e591cb76ddf248725`
- M43B-8 A：`src/pd_pps/firmware/experiments/43b8_power_plan_refactor`
- M43B-8 B：`src/pd_pps/firmware/experiments/43b8b_pps_control`
- 用途：正式マイルストーンM5（PC制御）の先行実機実証。**製品としてのM5/M7合格ではない**。
- 証拠：2026-11-17のPowerShell/PD17ログ、ユーザー提示のDMMトレンド画像。生ログはユーザーPC側で保全し、`diagnostics/`配下はGit除外。
- 本文は提示されたログの**結果要約**。元ログのSHA-256、実機書込BINのSHA-256、試験機材の計測器校正状態は未取得。後日照合可能と断定しない。

## 1. 実装の到達点

### M43B-8 A：選択ロジック分離

- 純粋なPDO/APDO選択・RDO生成を`pd_power.c/h`に分離。既定PPS目標は7,000mV / 1,000mA、固定5Vフォールバックを維持。
- PD PHY/GoodCRC/USBFS ISR、メインループの安全処理順序、既存USB CDC読み取りコマンドを維持。
- Pythonテスト：70件中62 PASS / 8 SKIP（既知の旧版テスト移行後）。RISC-V向けビルド：FLASH **24,928 B / 62 KB**、RAM **5,896 B / 20 KB**。
- 実機：USB非接続のCC2でREADY約29秒・更新5/5成功、別試行で約20秒・更新3回完了。USB接続時は初回Request失敗、PS_RDY待ちタイムアウトなどの**不安定例も観測**。

### M43B-8 B：PC制御API

- 新しいCDCコマンド：`PPS GET`、`PPS SET <mV> <mA>`、`PPS STATUS`。
- 受付`queued=1`と、Accept/PS_RDY後の`pending=0 applied=1`を区別。`active_*`は確定済み契約値を保持し、未確定の`target_*`と混同しない。
- 入力ガード：**5,000～8,000mV・20mV刻み、500～1,000mA・50mA刻み**、かつ実際に受信したAPDOの範囲内。これはソフトウェア上の上限であり、実ハードウェアの定格保証ではない。
- USB IRQからPD処理を呼ばず、メインループ側で安全・PD処理後にCDCコマンドを扱う。既存の7V/1A既定値と読み取りコマンドを保持。
- 導入：新規フォルダー作成方式。M43B-7 P2とM43B-8 Aを上書きしない。

## 2. M43B-8 A：PC USBとPD/PPS同時動作の検証

| 試験条件 | 結果 | 判定と限界 |
|---|---|---|
| PD充電器未接続、COM16 CDC Probe | `PING/STAT/COUNT`合計15/15、`ov=0` | PC USB単独通信PASS |
| PD接続操作を含む45回Monitor | CDC45/45成功、ただし`STAT a=0 pe=0`のまま | CDCのみPASS。PD接続認識は観測できず、同時動作の成功証拠にしない |
| CC1、PC USB関連の切り分け | Source Capabilities 0、PD IRQ 0、`STOP reason=5`、CC検出から約650msでRECOVERY | 既知の失敗例。COM操作が直接原因とは断定不可 |
| CC2、USBFS IRQあり | PPS READY約30.6秒、周期更新6/6、GoodCRC7/7、USBFS IRQ101 | PD PHY/USBFS割り込みの同時動作実績。CDCコマンド送受信の証拠ではない |
| CC2、PD接続後にPC USBを挿入 | PPS READY累積40.0秒、周期更新7回完了、USBFS IRQ101 | USB挿入後の厳密な時刻不明。切断直前のTX_FAILEDはPD充電器抜去と整合 |
| CC2 READY中、CDC Monitor 15回 | 15/15成功、`STAT a=1 pe=4 pps=1 mv=7000 ma=1000 cfg=1`、周期更新増分2/2 | **PPS契約中のCDC送受信PASS** |
| CC2 READY中、CDC Monitor 120回 | **120/120成功、FAIL 0**。全`STAT`がREADY/PPS、全`PDERR`が0 | **約120秒の同時動作PASS（限定条件）** |

120回Monitorの`PDSTAB`増分：`elapsed` **20,472→136,852ms**、`ready` **19,843→135,624ms**、`req/pps` **4→28**、`ref_start/ref_done` **3→27**。観測区間のPPS更新は**24/24完了**。途中の`ref_start=7 ref_done=6`は次のサンプルで完了した。`COUNT rst=4`は試験中不変、`ov=0`。

**注意：**成功した条件は、PD充電器CC2・既知の成功接続順序・外部負荷なし。USB接続やCC方向を変えた失敗事例があるため、接続条件全体の安定性や量産品質は実証していない。

## 3. M43B-8 B：テスト・ビルド・実機確認

### ホスト・クロスビルド

- Windows Pythonテスト：`Ran 74 tests`、**65 PASS / 9 SKIP / FAIL 0 / ERROR 0**。
- 旧テストが新TARGETや意図的に変更したヘッダーの旧ハッシュを期待したため、専用修正KITで更新。M43B-8 B変更ソースのSHA-256を保護するテスト3件を追加しPASS。
- SKIP 9件にはネイティブCランタイムテストが含まれ、Windows環境上では未実行として扱う。
- CH32X035F8U6向けGCCビルド**PASS**：FLASH **26,184 B / 62 KB（41.24%）**、RAM **5,904 B / 20 KB（28.83%）**、ELF/BIN/HEX生成成功。M43B-8 AからFLASH +1,256 B、RAM +8 B。
- 別ホストでのPPS設定ポリシーCランタイムテストについては会話内に`STAGE_B_PPS_POLICY_PASS`の報告があるが、ユーザーPCでのネイティブ実行ログは未取得。独立のWindows PASSとして計上しない。

### 実機CDCコマンド

| 検証 | 観測結果 | 判定 |
|---|---|---|
| `PPS GET` | `target_mv=7000 target_ma=1000 active_mv=7000 active_ma=1000 pe=4 pps=1` | 読み取りPASS |
| `PPS STATUS` | `pe=4 pending=0 applied=1`、目標値・契約値とも7,000/1,000 | 状態取得PASS |
| 同一値`PPS SET 7000 1000` | `unchanged=1`、`pending=0 applied=1` | 重複指定PASS。追加Request回数は独立照合なし |
| `PPS SET 7020 1000` | `queued=1`→`pe=3 pending=1 applied=0`→`pe=4 pending=0 applied=1 active_mv=7020` | 7,000→7,020mV契約変更PASS |
| `PPS SET 7000 1000` | 上記と対称に、確定済み契約値7,020→7,000mV | 電圧復帰PASS |
| 無効値3件 | `7001/1000`、`8020/1000`、`7000/1050`が全件`ERR PPS range` | 3/3拒否PASS、元の目標・契約値7,000/1,000を維持 |
| 実測電圧往復 | 指定7.00→7.20→7.00Vに対しDMMグラフ約6.95→7.15→6.95V | **約+0.20Vの実電圧追従PASS**。個々の絶対電圧精度は未評価 |
| `PPS SET 7000 500` | `queued=1`→`pe=3 pending=1 applied=0`→`pe=4 pending=0 applied=1 active_ma=500` | 要求電流1,000→500mA PASS |
| `PPS SET 7000 1000` | `queued=1`→`pe=3 pending=1 applied=0`→`pe=4 pending=0 applied=1 active_ma=1000` | 要求電流500→1,000mA PASS |

DMM画像の高電圧側は約30秒間の平坦な推移であり、実測の差分は約200mV。設定値とDMM実測値の約50mVのずれを校正値・抵抗降下・Source精度のいずれかに帰属する証拠はない。

**電流の意味：** `ma=500`または`1000`はPPS契約上の要求電流であり、電流実測値でも、500mA/1Aが常に流れることの証明でもない。**実負荷における電流制限特性は未検証**。

### テストIDの取り扱い

会話中の「B08（DMM 7.2V）」「B09（電流設定往復）」は、配布KITの`tests/TEST_PLAN.md`に記載されたB08（交渉中GET）やB09（競合時SET）とは**別内容**。過去の会話ラベルを改名せず、正式記録では上表の**操作内容**で識別する。KITの競合・切断・再接続等の未実施ケースをPASS扱いしない。

## 4. 現在の制約・未検証

- M43B-8 Bの基本的な**PPS設定値の取得、変更、契約確定の通知、およびDMMでの電圧変化**は実証した。正式M5は未完了。
- USBケーブルの接続順序、CC1/CC2、複数充電器での再現率・長時間連続運転・大電力負荷・PD Hard Reset復旧・異常系保護は未評価。
- 一部の実験で`VBUS ~0V`、`Source Capabilities 0`、Request TX_FAILED/PS_RDYタイムアウトを確認。COMポート/DTRのON/OFFが単独原因とは実証されていない。成功例と失敗例を両方保持する。
- PCからの通信断後にPPSが継続する場合の**安全仕様（継続／既定値へ戻す／出力停止）**は未確定。現実装に自動安全復帰があると誤認しない。
- 自動計測・履歴保存・制御競合・認証・製品ハードウェア保護は未実装または未検証。外部負荷を用いた新試験は独立の安全判定が必要。
- ソースコミットとGitHub Pushの状態は**実際のGitステータス／リモート確認で決定**する。ZIP作成や本書だけでコミット済みと記載しない。

## 5. 次の作業（M43B-8 C前の検証）

- [ ] C01：COMポートclose中もPD/PPSが継続するか。
- [ ] C02：PPS READY中にPC USBケーブルを物理的に外しても契約を維持するか。
- [ ] C03：USB再接続後にCDCで現在の契約を再取得できるか。
- [ ] C04：PC USB維持中にPD充電器を外し、非READY・安全停止を確認する。
- [ ] C05：PD再接続時の初期目標値と安全な復帰動作を確認する。
- [ ] 接続順序・CC1/CC2・充電器差の反復試験、通信断時の意図するフェイルセーフを決定する。
- [ ] CDC以外の計測・ログ機能との統合、負荷/製品電気安全評価。

## 6. ローカル参照と再現コマンド

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps\firmware\experiments\43b8b_pps_control'
python -m unittest discover -s tests -p 'test_*.py' -v
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\pj\pd_pps\src\pd_pps\dev-workspace\pd-pps.ps1' -Action Build -Project firmware/experiments/43b8b_pps_control
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\pj\pd_pps\fw_m43b8b_pps_control_v01_20261117\tests\Invoke-PpsCdc.ps1' -Mode Get -Port COM16
powershell.exe -NoProfile -ExecutionPolicy Bypass -File 'C:\pj\pd_pps\fw_m43b8b_pps_control_v01_20261117\tests\Invoke-PpsCdc.ps1' -Mode Status -Port COM16
```

COM16は今回の試験環境で確認されたポート番号であり、別のPCや再列挙後も固定とは限らない。`PONG M43A`は互換性のために残る旧版の識別応答で、ファームウェア実体を示す一意なビルドIDではない。
