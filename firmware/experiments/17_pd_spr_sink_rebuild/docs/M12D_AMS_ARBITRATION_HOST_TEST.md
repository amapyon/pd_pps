# M12d: AMS競合／Message Discarding ホスト検証

基準: USB Power Delivery Release 3.2 v1.2 (2026-08-19) §§7.1.1, 7.3, 9.1.2.3.5, 9.2.4.6, 9.2.5.2。
対象: `17_pd_spr_sink_rebuild` (M12b, M12c) のソースから最小差分を作成。

## 変更ファイル

- `src/pd_policy.c`：処理中AMSの予期しないCAPS上書きを拒否。現在は `PE_RECOVERY` に遷移し、契約と送信コマンドを無効化。
- `src/pd_phy_ch32x035.c`：新規SOP受信時、GoodCRC待ちの保留SOP送信を破棄。送信IDの更新、再送タイマー解除、Soft Reset時のリセット処理を補強。
- `src/pd_phy_ch32x035.h`：イベント `PDTR_TX_DISCARDED=17` 追加（既存イベント番号は維持）。
- `tests/test_ams_arbitration.c`：実際のPHY + PEをモックレジスタで13シナリオ試験。
- `tests/run_m12d_host_test.sh`：Linux/macOS等のホストGCC用。Windows上でこのシェルを実行する必要はない。

## テスト結果 (2026-11-07)

| 試験 | 結果 |
|---|---|
| M12bベースのM12d新テスト | 6/13 PASS（7件 FAIL） |
| M12d修正版のM12d新テスト | 13/13 PASS |
| 既存 `tests/run_host_test.sh` | PASS |
| 既存 `tests/run_m12b_host_test.sh` | 6/6 PASS |
| 既存 `tests/run_m12c_host_test.sh` | 5/5 PASS |
| GCC `-fsanitize=undefined` | 13/13 PASS |
| Clang `-Wall -Wextra -Werror` | 13/13 PASS |
| Windows/xPack/ch32fun実機Build | 未実施 |
| CH32X035実機PPS契約の回帰 | 未実施 |

## 制限・安全上の注意

- モックテストは論理的なイベント順序の再現であり、実際のBMC波形や割り込み競合、ハードウェアTX/RXの同時動作を保証しない。
- USB PD規格は電源遷移中のプロトコルエラーにHard Resetを要求する。本版はM13までの一時的な fail-closed 停止であり、**フル準拠ではない**。
- `PE_RECOVERY`への遷移は保留中の新規Requestを止めるが、規格に従ったSourceの電圧復帰や電力制御の代替にはならない。開発試験専用。
- 受信イベントキューのOverflow、物理RX/TX衝突、電力遷移中の異常負荷は未検証。
- M11bのGet_Source_Cap単発試験コード、M9非同期ログ、M12a/bの再送タイマーは維持。
- 旧 `16b_source_caps_integrity`、ch32fun、Makefile、VS Code設定、dev-workspaceは変更しない。

## Windowsでの導入（USB-C充電器を外し、Log終了後）

ZIPを `C:\pj\pd_pps\PD17_M12d_ams_arbitration.zip` に保存してから：

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
Expand-Archive -LiteralPath 'C:\pj\pd_pps\PD17_M12d_ams_arbitration.zip' -DestinationPath . -Force
.\dev-workspace\pd-pps.ps1 -Action Build -Project '17_pd_spr_sink_rebuild'
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
.\dev-workspace\pd-pps.ps1 -Action Flash -Project '17_pd_spr_sink_rebuild'
if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
.\dev-workspace\pd-pps.ps1 -Action Log
```

**第1段階は未接続時heartbeatの確認のみ。** 充電器は次の指示まで接続しない。

## 次の確認

1. Windows Build/Flash、未接続heartbeatを確認。
2. 実機PPS初回契約、Ready中同一SourceCaps再取得、周期更新を短時間回帰。
3. Soft Reset、異常系タイマー、Hard Reset復旧は別実験。
