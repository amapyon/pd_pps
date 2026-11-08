# M13a — USB PD Hard Reset受信時の契約失効と安全停止

- 対象: `17_pd_spr_sink_rebuild`、M12dの差分として適用
- 基準Source: UGREEN 90551、USB-C1、他ポート未使用
- Sink: WeActStudio CH32X035 CoreBoard、SPR PPS 7.0 V / 1.0 A
- 関連仕様: USB PD 3.2 v1.2 (2026-08-19) §4.4.1、§9.2.4.1、§9.2.4.8、§9.2.4.9。特に§9.2.4図の注記3（復帰にはVBUSがvSafe0V→vSafe5Vを経由したことの検出が必要）

## 現在確認されているハードウェア構成（2026-11-07訂正）

**旧版の「VBUS ADC回路がない」という説明は誤り。** WeAct CoreBoardの基板回路図単体には外付け監視回路が見えないものの、実験回路としてVBUS→100kΩ→PA0→10kΩ→GNDの分圧回路が追加されている。INA226＋R010もPA2/PA3のソフトウェアI²Cで接続されている。`16b_source_caps_integrity`の実機実績コードを本版へ取り込んだ。

**現段階は測定系の統合試験であって、Hard Reset自動復旧の試験ではない。** ADC校正範囲は約4～9Vであり、vSafe0V近傍の誤差・閾値・放電時間、およびVBUS低下時のMCU・INA226電源維持は未検証。INA226のBUS電圧は分圧されたPA0と測定点が異なる可能性もある。現段階はHard Resetラッチ後のPE_RECOVERY安全停止を維持し、物理CC切断後だけ再初期化する。

Hard Resetを故意に起こさず、まず正常PPS 7Vと物理切断でのADC/INA値を、通信中はRAM保持・切断後にのみprintfする実機試験を行う。

## M12dからの製品コード変更

1. `src/pd_policy.c`: Hard Reset受信時に契約、Request、PDO/RDO、要求電圧・電流、Soft Reset保留、タイマーを初期化。状態は安全側の`PE_RECOVERY`。
2. `src/pd_phy_ch32x035.c`: `IF_RX_RESET`を受けたら保留RXイベントのキューを破棄し、Hard Resetイベントを先頭に置く（キュー満杯でも失わない）。送信GoodCRC待ち、再送、Message ID履歴をクリア。
3. `src/pd_phy_ch32x035.c`: Hard Resetラッチ中のSOPを無応答で破棄（GoodCRCを送らずSourceCapsも届けない）。CCの物理的な切断による`phy_reset_after_detach()`が行われるまでラッチを維持。
4. `src/pd_phy_ch32x035.c`: `phy_send()`の`hard_reset`判定をUSBPD割り込み禁止の内側に移し、判定直前の割り込み競合で古いRequestを送信することを抑止。

### ホストテスト

`tests/test_m13a_hardreset.c` は本番の`pd_phy_ch32x035.c`と`pd_policy.c`をCH32X035レジスタモックで実行する11シナリオのテスト。

```bash
sh tests/run_m13a_host_test.sh
```

検証結果:
- **M12dベースライン**: 11件中4 PASS / 7 FAIL（修正対象を検出）
- **M13a修正版**: 11 PASS / 0 FAIL
- M12b: 6/6 PASS
- M12c: 5/5 PASS
- M12d: 13/13 PASS
- 既存Policy Engineテスト: PASS
- GCCおよびClang `-fsanitize=undefined`: PASS

ホストテストは電気信号・実充電器・MCU割り込みレイテンシを模擬していない。RISC-V xPack環境でのBuild/Flashと実機通信は別途要確認。

### 実機検証の段取り

**試験1: 充電器を外した状態**
1. M13aのZIPをワークスペースへ展開する。
2. Build/Flashを行い、WCH-LinkEで`PD17: alive, charger detached...`のheartbeatを確認する。
3. Build/Flash/Logの結果を確認するまでは充電器を繋がない。

**試験2: 正常系の回帰（試験1 PASS後）**
1. UGREEN USB-C1を直接接続、他ポート未使用。
2. 約6秒で取り外す。
3. 切断後のTRACEを回収し、PPS Request、Accept、PS_RDY、周期更新、欠落カウンターを評価。

**禁止する検証**
- 本ファームウェアでは復旧判定と自動復帰が未実装なので、強制Hard ResetやUSB-C接続中の電源線操作を自動復帰試験として実行すること。
- タイマー/CCレベルだけでVBUS復旧済みと見なすこと。
- USBインライン電力計の挿入（CC波形を変える懸念があるため）。

## 未完了項目（M13a測定系検証後）

- ADC/INA226の新統合版の実機測定とvSafe0V→vSafe5V閾値の検証。その後、同一CC接続でのProtocol Layer再初期化。
- Sink側からのHard Reset送信・試行回数制限と仕様上のタイマー処理。
- Hard Reset時の実負荷遮断。現行のファームウェアが制御する外部負荷スイッチは存在せず、試験は無負荷で実施する。
