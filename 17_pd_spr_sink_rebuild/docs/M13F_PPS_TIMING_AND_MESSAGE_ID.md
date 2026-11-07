# PD17 M13f — PPS更新・MessageIDの時刻境界と32bit折り返し

更新日：2026-11-08

## 目的と範囲

M13e（GitHub `b8391ea`）を基準に、PPS 7V/1Aの5秒周期更新、WAIT応答後の120ms再試行、PEタイムアウトとMessageIDの折り返しを**製品のPolicy Engine/PHYを用いたホスト模擬**で検証する。

- Source発Hard Resetの実機試験、長時間連続運転、ADC実機故障注入は実施しない（従来どおり保留）。
- `src/main.c`、`src/pd_phy_ch32x035.c`、`Makefile`、ベンチ専用刺激は変更しない。
- **製品コード変更：`src/pd_policy.c`、`src/pd_policy.h`の2ファイル**。当初はホスト検証のみ。その後、WindowsクロスBuild/Flashと2機種の短時間通常PPS実機回帰を別途実施してPASSした。以前のM13c-2結果の流用ではない。
- 5秒、120ms、15/45/500/650msは**現行コードに採用済みの設定値**の境界を検証するものであり、USB PD規格適合性の試験ではない。

## 発見した不具合と修正

以前のコードには、WAIT応答を受けていない場合でも`defer_until_ms`の初期値`0`を常に`reached(now, 0)`で判定する処理があった。

`reached()`は`(int32_t)(now - deadline) >= 0`の符号付き差で近距離の期限を判定する。この実装では、**未設定を表す0を長時間後の時刻として比較してしまう**。たとえば、`last_pps_ms=0xffffe000`でReadyを得たPPS契約は、5,000ms後の`0xfffff388`でも更新Requestを開始できないことを実際の旧`pd_policy.c`を用いた最小再現テストで確認した。

修正：

1. `pe_t`に`defer_active`（WAITによる待機期限が有効な間のみtrue）を追加する。
2. `PE_EV_WAIT`時に`defer_until_ms=now+120`と`defer_active=true`をセットする。
3. `pe_tick()`で120ms期限の到達時にフラグを解除する。PPS更新条件は`!defer_active`とする。
4. 契約失効、CC切断、Hard Resetでフラグを破棄する（`invalidate()`/`pe_init()`）。

**効果**：未設定期限を0と比較しない。WAIT待機が一度満了した後も古い期限が後のPPS更新を阻害しない。32bitのミリ秒カウンタが折り返す前後においても120msという短い差分を判定できる。

`reached()`は期限までの距離が2^31msより小さく、メインループが通常周期で動作していることを前提とする。長時間ハング、スリープ、全規格タイミングを保証するものではない。

## ホストテスト一覧（計20ケース）

### Policy Engine：15ケース

| ID | ケース | 主な判定 |
| --- | --- | --- |
| PT-01 | PPS 4,999/5,000ms | 直前はReady、時刻到達でRequest。次回起点はPS_RDY |
| PT-02 | 32bit時刻の高位領域、WAITなし | `0xffffe000`付近で5秒後にRequestを開始（旧コードの再現不具合） |
| PT-03 | ミリ秒時刻の`0xffffffff→0`越え | 5秒後の更新RequestとReady復帰 |
| PT-04 | WAIT応答後119/120ms | 119msでは待機、120msで再試行 |
| PT-05 | 高位領域のWAIT待機 | WAIT期限を有効フラグで判定 |
| PT-06 | WAIT期限自体の32bit折り返し | 境界で再試行 |
| PT-07 | 以前のWAIT期限の失効 | 次の5秒更新に古いWAIT期限が残らない |
| PT-08 | 11回の連続PPS更新 | `0x7fffffff`を越えても更新継続 |
| PT-09 | SourceCaps待機649/650ms | 期限までは待機、期限でRecovery |
| PT-10 | Request送信開始14/15ms | 未送信なら15msでRecovery |
| PT-11 | Accept待機44/45ms | 45msでRecovery |
| PT-12 | PS_RDY待機499/500ms | 500msでRecovery |
| PT-13 | 初回契約前のWAIT | SourceCaps待機期限を回避できずRecovery |
| PT-14 | 固定5V契約 | 長時間経過してもPPS更新要求なし |
| PT-15 | CC切断 | WAIT期限を破棄し新セッションへ |

### PHY：5ケース

| ID | ケース | 主な判定 |
| --- | --- | --- |
| MT-01 | Request連続17送信 | GoodCRCでMessageID 0～7循環、2回の折り返し |
| MT-02 | MessageID 7→0前後の誤GoodCRC | 不一致IDではカウンタを進めない |
| MT-03 | MessageID 7でTXリトライ上限 | 送信失敗後はID=0、`PE_EV_TX_FAILED`を通知 |
| MT-04 | 受信MID 7→0 | MID=7の重複を除外、MID=0の新メッセージを受理 |
| MT-05 | TX待機中のHard Reset | CRC待機を停止しIDを初期化、通常送信は禁止 |

`MT`は実際の`src/pd_phy_ch32x035.c`をモックレジスタとIRQで駆動する。PD線の電気信号、DMA競合、電源の実挙動は模擬しない。

## 実行手順

POSIXシェルとC11コンパイラーがあるPC上で、リポジトリルートから：

```sh
sh 17_pd_spr_sink_rebuild/tests/run_m13f_host_test.sh
```

このシェルスクリプトが20ケースを実行する。Windows PowerShellだけの環境ではホスト用GCC/ClangまたはWSLなどが必要。従来のWindows `pd-pps.ps1 -Action Build`はMCU用クロスビルドであり、ホスト模擬試験とは別物。

## 実施結果（ホスト、2026-11-08）

- 旧`pd_policy.c`を用いた再現：**32bit高位領域で更新Requestが開始されないことを確認**。
- 修正後：Policy Engine **15/15 PASS**、PHY **5/5 PASS**、合計 **20/20 PASS**。
- 既存M9～M13eホスト試験を含めたシェル **17系統すべてPASS**（M13f追加分を含む）。
- M13dログ判定Python **12/12 PASS**。
- Clang UndefinedBehaviorSanitizer（M13f Policy/PHY）**PASS**。
- 製品版への故障注入フック・Sink発Hard Resetの自動送信は追加していない。
- **当初のホストテスト時点では**Windows/xPack Build、Flash、充電器2機種でのM13f実機試験は未実施。その後の短時間実機結果は下記に追記した。

## 追加実施結果：Windows Build/Flash・2機種短時間実機（2026-11-08）

- Windows/xPack `Build`・`Flash`成功。`WCH-LinkE`でCH32X035を認識し、未接続時の`PD17: alive, charger detached, state=0, lost=0`を2回確認。FLASH **38,372B（60.44%）**、RAM **3,388B（16.54%）**。
- **UGREEN 90551 USB-C1、CC2**：セッション26,856ms、Ready 26,213ms（97.6%）、PPS更新 **5/5**、`requests=6`・`pps_ready=6`、最大ループ間隔3ms。Request `0x6302BC14`（PPS 7V/1A）をTRACEで確認。ADC校正値7.042V、INA226 BUS 6.998V。切断時`PD17 STOP reason=1`、`detach_after_stop_ms=124`、ADC監視タイムアウト0、TRACE/イベント欠落0。
- **ダイソー GaN 20W**：セッション37,893ms、Ready 37,048ms（97.8%）、PPS更新 **7/7**、`requests=8`・`pps_ready=8`、最大ループ間隔1ms。ADC校正値7.006V、INA226 BUS 6.961V。切断時`PD17 STOP reason=1`、`detach_after_stop_ms=113`、ADC監視タイムアウト0、TRACE/イベント欠落0。今回提示されたログは診断概要で、個々のRequest RDOは未提示だが、PPS Ready回数と7V付近の電圧が一致する。
- 両機種とも`source_hr=0`、`adc_fault=0`、`policy_faults=1`（通常のケーブル抜去によるVBUS契約失効）、`trace_drop_session=0`、`event_lost_session=0`。
- **判定：M13fの通常PPS通信・周期更新・正常切断診断の短時間実機回帰は2機種PASS**。約24.9日のミリ秒時刻境界そのものを実機で再現した試験ではない。Source発Hard Reset実機・ADC故障注入実機・長時間連続運転は保留。

## 実機手順（実施済みの手順を記録）

1. 充電器を外しWCH-LinkEで給電。M13fをWindowsでBuild→Flash→heartbeatを確認する。
2. UGREEN 90551 USB-C1、外部負荷なし、約15～20秒間PPS 7V/1Aを確認して切断。`PD17 STABILITY`の`refresh_started=refresh_done`、`PD17 STOP reason=1`、エラー0を確認する。
3. UGREEN PASS後にダイソー GaN 20Wでも同じ短時間試験をする。
4. 2機種PASS後、結果をROADMAPへ追記する（本更新で実施）。Gitコミットは別途行う。

この修正が必要となる**24日以上の稼働時間そのものを実機で再現した試験ではない**。長時間連続運転はユーザーの判断で別途実施。

## 残課題

- 接続中にSourceが`WAIT`や`REJECT`を繰り返した場合のリトライ回数・最大許容時間の方針は別途決める。今回の修正はWAIT期限の比較と境界条件の正常動作だけを対象とし、無期限のWAIT/REJECT耐性を証明しない。
- Source発Hard Resetの実機受信は試験環境未整備のため保留。
- 実機ADC故障注入と長時間連続運転は別途実施。
