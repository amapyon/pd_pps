# M13c-2：Source発Hard Reset復旧ゲートのタイムアウトと停止原因の記録

**対象**：`17_pd_spr_sink_rebuild` / CH32X035 / M13c-1通常運用版への差分
**日付**：2026-11-07
**状態**：ホストテストPASS。Windows/xPack Build、Flash、実機通信は**未検証**。

## 背景と設計意図

M13c-1はUGREEN 90551 USB-C1（CC2、28,214ms、PPS更新5/5）とダイソー GaN 20W（CC1、39,272ms、PPS更新7/7）の2機種で、通常PPS 7V/1A、累積統計、ケーブル切断時のVBUS早期契約失効を実機確認した。**約10分以上の連続運転は依頼により別途実施として保留**する。

一方、Source発Hard Reset復旧には実機未検証の2段階ゲートがある：

1. 0V相当を3連続ADCサンプルで確認後、PHYを早期受信へ移行する `phy_hr_arm_rx()`。
2. 5V相当を3連続ADCサンプルで確認後、`phy_hr_complete_recovery()` によりPolicy EngineへSourceCapsを引き渡す。

以前はこれらがFalse（例えばPHYがGoodCRC送信中）を返した場合、無期限に繰り返し試みる状態だった。M13c-2では**最初の拒否から20ms以内**は再試行するが、継続するとfail-closed停止する（送信・GoodCRCを許可しない）。20msは**実験上の保守的な上限**であり、USB PD規格のタイマそのものを表す値ではない。

## 変更内容

- `pd_hr_recovery.[ch]`：ARM_RX、COMMITのBusy待ちを開始時刻と状態で管理。**32bitミリ秒折り返し対応**。20msで失敗、成功時にタイマ解除。2回目のSource発Hard Resetは従来どおり物理CC接続中は停止。
- `pd_stop_diag.[ch]`：通信中の割り込み・UART出力・メモリ割り当てなしで、**最初の停止理由・発生時間・停止直前後のPE状態**をスナップショット。後続の障害で最初の原因を上書きしない。CC新規接続でクリアし、確定切断時に凍結する。
- `main.c`：新ゲートのタイムアウト、Source発HR重複、VBUS消失、ADC故障、SourceCaps解釈失敗、TX失敗、通常PEタイムアウトの診断を追加。送信条件・PPS要求7V/1A・5秒更新は変更しない。
- **現行通常ファームで意図的Hard Reset / Get_Source_Capは送らない。** ソフトウェアの強制ADC故障注入も行わない。

## 新しい診断行（CC切断500ms後のみ）

```
PD17 STOP reason=... first_rel_attach_ms=... pe_before=... pe_after=... aux=... hr_rx=... detach_seen=... detach_after_stop_ms=...
```

`first_rel_attach_ms`は**最初の停止記録**の物理CC接続確定からの経過ms。`detach_after_stop_ms`はその記録からCC切断確定までの経過ms。`hr_rx`はSource発Hard Reset受信イベント件数。**`reason=1`は正常なUSBケーブル引き抜きでも記録される**ため、それだけで故障とは判定しない。通常のケーブル切断では`reason=1`、`pe_before=4 (PE_READY)`、`pe_after=6 (PE_RECOVERY)`、`detach_after_stop_ms`がおおむね100～150ms程度が予想される（後者は実験環境依存）。

| reason | 意味 |
|---:|---|
| 0 | 停止記録なし |
| 1 | VBUS低下検出（正常なケーブル抜去も含む） |
| 2 | ADC起動・完了失敗 |
| 3 | SourceCaps入力によるPE停止（不正/競合） |
| 4 | TX_FAILEDイベントによる停止 |
| 5 | SourceCaps待ちタイムアウト |
| 6 | RequestのAccept待ちタイムアウト |
| 7 | PS_RDY待ちタイムアウト |
| 8 | Soft Reset応答待ちタイムアウト |
| 9 | その他のPE停止 |
| 10 | 同じ物理CC接続中の2回目のSource発Hard Reset |
| 11 | HR中のADC電圧遷移失敗（`aux`: 上位8bit=HR観測状態、下位8bit=エラー） |
| 12 | HR後の受信再開ゲートが20ms超Busy |
| 13 | HR後のプロトコル復帰ゲートが20ms超Busy |
| 14 | HR復旧時のPE状態矛盾 |

`PD17 STABILITY ERR`の`policy_faults=1`等は、通常抜去に伴うVBUS低下でも増える既存カウンタであり、今回の停止スナップショットと合わせて解釈する。診断は**観測した内部状態**であり、USB PD規格適合やハードウェア電源遮断の証明ではない。

## ホストテスト

```
sh tests/run_m13c2_host_test.sh
```

- `test_m13c2_stop_diag.c`：最初の停止理由維持、時刻折り返し、切断との時間差、セッション再初期化、PE停止理由の分類。
- `test_m13c2_hr_gate.c`：**実PHY**のUSBPD IRQモックを使用。ARM_RX一時Busy→成功、20ms超Busy→停止、COMMIT継続Busy→停止、CC再接続時だけ復旧回数を再許可。
- M12～M13c-1の従来ホストテストとClang UBSanもPASS。

## 実機ゲート（1回に1操作）

1. 充電器未接続でWindows/xPack Build→Flash→`PD17: alive, charger detached...`を確認。
2. UGREEN 90551 USB-C1、外部負荷・USB電力計なしで約12～15秒の正常PPSを確認し、USB-Cを取り外す。新`PD17 STOP`と既存`PD17 STABILITY`が両方出ることを確認。**Source発HRの強制注入はしない**。
3. UGREENで合格後にダイソー GaN 20Wを同一条件で別試験。Source発HRの実機検証は専用治具・安全条件が整うまで保留。

**長時間連続運転は本段階では行わず、別途計画して試験する。**
