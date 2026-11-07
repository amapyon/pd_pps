# M13c-1：長時間PPS試験用統計とfail-closed強化

**対象**：`17_pd_spr_sink_rebuild` / CH32X035 / M13b通常運用版への差分
**日付**：2026-11-07
**状態**：ホストテストPASS。Windows/xPack Build・実機テスト未実施。

## 目的

M13bのUGREEN 90551（USB-C1、CC2）およびダイソー GaN 20W（CC1）の両方で、初回PPS 7V/1Aと5秒周期の更新、VBUS早期契約失効を実機確認した。M13c-1は**通常PPS接続を長時間維持しても診断できるようにする**ことと、**通常PEタイムアウトやVBUS ADC監視故障の際にPHYを安全停止させる**ことだけを対象とする。意図的なHard Reset送信や追加Get_Source_Capは実装しない。

## M13c-1の変更

- `pd_stability.[ch]`：セッション単位・固定容量の累積統計。32bitミリ秒の折り返しに対応。カウンタは飽和する。IRQでは使用しない。**127件で満杯になるRAM TRACEと独立**。
- `PD17 STABILITY ...`：確認済みCC切断後500msを経て、一度だけ出力する。`session_ms`はCC attach→確認済みCC detach、`ready_ms`は契約を有効としたReady状態の累計時間、`loop_gap_max_ms`は接続中メインループの最長間隔。
- `caps`はPolicy Engineへ渡されたSourceCaps件数、`requests`はPHYが送信を開始したRequest件数（CRCリトライは含めない）、`pps_ready`/`fixed_ready`は**PS_RDYによるReady到達回数**。`refresh_started`は5秒タイマーから開始したPPS更新回数、`refresh_done`はその更新に対応するReady到達回数。
- `PD17 STABILITY ERR ...`：Source発Hard Reset受信数、PE_RECOVERY遷移回数、早期VBUS消失/ADC故障件数、契約失効直前のPE fault累積値、セッション中のTRACE超過数とPHYイベントキュー欠落数。
- `PDTR_ADC_FAIL_STOP`（code=27）と`PE_EV_SENSOR_FAULT`（`code=15 detail=13`）：ADC監視が起動・変換・完了のいずれかで失敗したとき、CC接続中かつPEがアクティブなら、PD TX/GoodCRCと保留イベントを停止し、契約を直ちに無効化する。**ADC故障はVBUSの0Vを示すものではない**。
- `PDTR_PE_FAIL_STOP`（code=28）：通常PEの交渉タイムアウト、TX_FAILED、不正/競合SourceCaps等でPE_RECOVERYへ入った場合、PHYはGoodCRCやPD TXを継続しない。物理CC切断・再接続以外で復帰しない。

**重要**：ファームウェアはCC/PD通信を止めるものであり、VBUSを遮断するハードウェアスイッチではない。過電圧・短絡・過電流のハードウェア保護に代わるものではない。ADC低電圧閾値やHard Reset復旧の規格適合性を保証しない。Source発Hard Resetの実機受信試験も未実施のまま。

## 長時間観測の注意

- 127件のRAM TRACEは、5秒ごとの更新を続けると通常は満杯になる。`trace_dropped > 0`はその時点では異常を証明せず、**詳細イベントを追跡できないことを意味する**。成功/失敗は`STABILITY`累積回数とVBUSの全期間最小/最大値、ADCタイムアウト、契約状態から判断する。
- `ready_ms`はスプリント同様の連続Readyを保証する指標ではなく、PPS更新のためWAIT_ACCEPT/WAIT_PS_RDYに移った時間を除いた合計。`pps_ready`が増えることは契約再成立の証拠だが、電気的な規格適合の証拠ではない。
- 診断`printf`はCC切断確定から500ms後のみ、PD通信中には一切出力しない。最後の物理ケーブル抜去で契約失効が発生すると`pe_recovery`と`policy_faults`にはその**期待される停止**も含まれる。`pe_recovery=0`を合格基準にしない。
- 10分試験などの長時間測定は負荷なし、USB-C単独、WCH-LinkE保持、USB計測器なし。各充電器について1回ずつ。まずは短い試験で新しい診断の実機動作を確認し、その後に延長する。

## テスト

ホスト（Linux/macOS等のC11コンパイラ）で：

```sh
sh tests/run_m13c_host_test.sh
```

新テスト：`test_m13c_stability.c`は時刻折り返し・計数・飽和・物理CCセッション初期化、`test_m13c_fail_closed.c`は実PHY/PEモックでADC監視故障と通常PEタイムアウト、GoodCRC抑止、CC再接続時のみ再許可を確認。M12b～M13b旧テストも回帰実施する。

## Windows実機ゲート（各段階でログ確認）

**ゲート1（まず行う）**：充電器なし、Build→Flash→heartbeat。使用ファイル`PD17_M13c1_stability_guard.zip`、`17_pd_spr_sink_rebuild`に上書きする差分。M13bを保存してから更新する。

**ゲート2（ゲート1後）**：UGREEN 90551 USB-C1を無負荷で直接接続し、約12～15秒維持した後、USB-Cのみ切断。`PD17 STABILITY`2行、既存`PD17 MODE`、`PD17 VBUS`、`PD17 HR VBUS`を確認。PPS Readyと5秒更新2回以上、`adc_fault=0`、`event_lost_session=0`、通常の切断に伴う`vbus_loss=1`・`pe_recovery=1`（予期しない途中の復旧遷移は別途TRACEと突合）が主要指標。`trace_drop_session`はゼロを目標とするが、測定時間によって溢れ得る。

**ゲート3**：ダイソー GaN 20Wで同様の試験（別CCセッション）。さらに1～10分と段階を上げる。**Hard Resetの実機注入や短絡・過負荷はこのゲートに含めない**。

## 未解決・次のM13c

- Source起因Hard Resetを安全に注入できる治具と試験条件。Sink発HRのみの実機成功をSource発HRの実機成功と混同しない。
- ADC故障を実機で安全に注入するための方法。現状はホスト模擬のみ。
- 規格全般、PPS経過時間と送信間隔、Power/Current制限、製品基板保護回路、PC/OLEDとの統合。
