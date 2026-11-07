# M13b 通常運用向けSink統合 — 技術メモと検証ゲート

**日付**：2026-11-07　**対象**：`17_pd_spr_sink_rebuild`（CH32X035F8U6）

## 2026-11-07 実機確認の追記（M13b最終ゲート）

このメモの「Windows実機での次の確認（未実施）」は**作成当初の計画・履歴**。その後の実機ログにより、Windows/xPack Build/Flash/heartbeat（FLASH 34,540B、RAM 3,388B）と、UGREEN 90551 USB-C1（CC2、6 PDO、PPS 7V/1A、周期更新2回、INA226 7.000V）およびダイソー GaN 20W（CC1、4 PDO、PPS 7V/1A、周期更新3回、INA226 6.961V）での通常通信がPASSした。両者とも意図的なHard Reset送信なし、VBUS低下の早期契約失効、ADCタイムアウト0、TRACE欠落0。**Source起因Hard Resetの実機試験は引き続き未実施**。次段階は`M13C_STABILITY_AND_FAIL_CLOSED.md`を参照。

## 目的と前提

M13a-7は、Sinkから自動Hard Resetを発生させるベンチ版であり、普段のPPS Sinkとしては使用しない。UGREEN 90551 USB-C1とダイソー GaN 20Wの2台でHard Reset後のVBUS低下・復帰とPD再交渉を実機観測した。M13bは、通常のSinkで不必要な試験用PD刺激を抑止し、Source起因のHard Resetについて保守的な復旧経路を残す。

## M13bで変更した動作

| 項目 | M13a-7 | M13b |
| --- | --- | --- |
| Sink発Hard Reset | 起動後1回、自動 | **なし**（`main.c`に送信呼び出しなし） |
| M11b Get_Source_Cap | 初回Readyから約1秒後に1回 | **なし** |
| M13a-7 PPS再取得 | Hard Reset後、固定5V Readyから約1秒後に1回 | **なし**（PPSなしは固定5V維持） |
| Source発Hard Reset受信 | PHY/PEと監視器のコード経路は存在 | 同経路を有効化。**実機未検証** |
| Source発Hard Reset復旧上限 | MCU起動後1回 | **物理CC接続あたり1回**、2回目はfail-closed |
| 復旧成功ログ | `auto_recovery`がPPS復帰のみ示す | `auto_recovery`はPD Ready、`pps_recovery`はPPS Ready |
| 既存のVBUS低下監視 | 10ms、3点連続で契約停止 | 維持 |

通常のSourceCaps再受信（Source側の自発送信）は依然として評価する。M11bの能動的Get_Source_Capを止めることは、SourceCaps自体の受信停止ではない。

**重要**：通常メインからSink発Hard Resetを削除しても、PHYの送信関数は独立したテストのためにソース内に残る。M13b Makefileには`pd_hr_bench.c`と`pd_hr_pps_probe.c`を含めない。M13a-7の実験版が必要な場合は元のZIPを使用する。

## Source起因Hard Resetの安全条件

1. `IF_RX_RESET`を検出するとPHYは通常送信・GoodCRCをゲートし、キューを消去して`PE_EV_HARD_RESET_RX`を通知。
2. PEの契約を失効。保留AMSとRDO、古いPDOを消去。
3. ADC分圧（VBUS→100kΩ→PA0→10kΩ→GND）を10ms周期で測り、raw≤70を3点、その後raw=540..610を3点確認。これは**暫定的な保守閾値であり、USB PD規格適合を認定するものではない**。
4. 低電圧を確認した後、SourceCapsの早着に備えPHY受信とGoodCRCを先に許可するが、**Request送信は5V安定確認後のみ**。
5. 5V確認後にPHYのSOP Message IDを更新初期化し、保持SourceCapsをPEへ渡す。提示能力が合えばPPS 7V/1A、なければ固定5V/1A。
6. 予期せぬADC停止、監視タイムアウト、再交渉失敗、同一CCセッションで2回目のHard Resetではfail-closed。**ソフトウェアタイマーだけでPE_RECOVERYを抜けない**。
7. 新しい物理CC Attachイベントでのみ1回分の復旧枠を初期化する。**SourceのHard Reset自体や電圧復帰だけでは枠を戻さない**。

本試作には、USB-IF完全準拠のHardResetCounter、全種類の拡張メッセージ処理、負荷保護、完成品レベルの電源断・復帰回路は実装していない。

## 診断

充電器を切断し、500ms経過後に出力される。

- `PD17 MODE normal=1 bench_hr_tx=0 bench_get_caps=0 pps_reprobe=0 hr_rx_max_per_attach=1`
- `PD17 HR VBUS ... recovery_handoff=... auto_recovery=... pps_recovery=...`
- `PD17 SENSOR READY ...` と `PD17 VBUS ... timeouts=...`
- PHY trace `code=3`はSource Hard Reset受信、`code=19/20`はSink Hard Reset送信（**M13bでは出ないはず**）、`code=22/23`は低電圧・5V復帰に関係するイベント。

## ホスト検証

`sh tests/run_m13b_host_test.sh`

テストは本番PHYの割り込み経路、契約失効、SourceCaps早着、復旧後のPPS Readyと固定5V Ready、2回目Hard Reset停止、物理CC再接続で再試行可能を確認する。また`main.c`に試験用Hard Reset/Get_Source_Cap呼び出しがないこと、Makefileでベンチ専用コードをリンクしないことを検査する。従来のM12/M13aホストテストは別途再実行する。

## Windows実機での次の確認（未実施）

最初は**充電器未接続でBuild→Flash→heartbeat**のみ行い、ログを提出する。

```
Set-Location 'C:\pj\pd_pps\src\pd_pps'
$Project = '17_pd_spr_sink_rebuild'
.\dev-workspace\pd-pps.ps1 -Action Build -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project $Project
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

充電器接続・負荷・実機Source Hard Reset試験は、上記成功とコードレビューを確認してから別実験で扱う。UGREEN/ダイソーの過去ログはM13a-7のものでありM13b実機結果ではない。
