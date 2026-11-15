# M8D～M8F USB CDC障害と自動復旧の実機検証記録

更新日：**2026-11-12**。対象：CH32X035F8U6／Windows 11／USB CDC-ACM／INA226（R010）。

## 1. 対象と位置付け

- 実験22 `firmware/experiments/22_usb_cdc_ep3_trace`：EP3 Bulk IN送信トレース（M8D）。
- 実験23 `firmware/experiments/23_usb_cdc_irq_trace`：USBFS割り込み・SRAMトレース（M8E）。
- 実験24 `firmware/experiments/24_usb_cdc_auto_recover`：USB通信の自動復旧（M8F）。

これらのM8D～M8Fは**製品ロードマップの正式M8（全機能統合）とは別体系**。正式M5（PC制御・データ取得）の通信基盤検証として記録する。

## 2. USB通信障害の確認

実験21／22でEP3送信完了割り込みが返らず `busy=1` に残る状態を観測した。実験23の100回試験では47回目の`MEASURE`正常応答の後、PowerShellの`SerialPort.Write()`が「ポートは閉じられています」で異常終了した。

| 時刻（JST） | 観測 |
| --- | --- |
| 22:24:33（2026-11-12） | Windows Kernel-PnP Event 1010：USBハブ`VID_1A40&PID_0101`が突然取り外された（対象5デバイス） |
| 22:24:33.970 | 47回目の`MEASURE`応答がEP3 INで正常完了 |
| 22:24:33.973 | EP3 INで`0xC0000011`（USBトランザクションエラー） |
| 22:24:37.190 | Windowsがデバイス記述子の再取得を開始 |
| 22:25:00 | Kernel-PnP Event 1010：CH32X035（`VID_1209&PID_D035`）の突然の取り外しを記録 |

`pnputil /enum-devices /relations`で、CH32X035（COM9）の**直接の親が上記USBハブ**であり、同じハブ配下に`VID_1A86&PID_8010`があることを確認した。

**判断**：ハブまたは上流USB経路の接続喪失とCOMポートの切断が整合する。ただし、接触不良、ハブ電源、ハブ本体のいずれが原因かは確定していない。過去のEP3固着が同じ原因とも断定しない。利用者判断で根本原因調査を終了し、ソフトウェアによる復帰を優先した。

## 3. 実験24で追加した対策

- USBバスRESETで、旧接続の部分コマンド・未送信応答を破棄する。
- EP3 IN送信が**2,500ms**完了しない場合、MCUのUSBFSを一度切断して再初期化・再列挙する。EP3の`busy`フラグだけを強制解除しない。
- `HEALTH bus_resets=... tx_timeouts=... reconnects=...`を提供する。この`reconnects`は**MCU自体が再初期化した回数**であり、Windows監視プログラムの再接続回数ではない。
- Windowsの`Monitor-M8F-Cdc.ps1`は`VID_1209&PID_D035`を再検出し、`INFO protocol=6`と`HEALTH`を確認後に測定を再開する。COM番号変更に対応し、切断を記録する。
- 自動再試行は冪等な`MEASURE`／`PING`に限定する。**将来のPD/PPS出力設定コマンドは確認なく再送しない**。

## 4. 実機受け入れ結果

| 項目 | 結果 |
| --- | --- |
| 通常時の100回／250ms試験 | **PASS: 100/100; disconnects=0** |
| 切断を伴う100回／250ms試験 | **PASS: 100/100; disconnects=3** |
| 切断を検出したサンプル | 6、24、64 |
| 検出時刻（JST） | 23:15:02.231、23:15:09.108、23:15:23.974 |
| 復帰後 | COM9に再接続。100件の正常応答を取得、測定番号に欠落なし |
| USBPcap | 各切断付近のEP3 IN `0xC0000004`（STALL_PID）、デバイス再列挙、HEALTH・MEASURE応答を確認 |
| HEALTH（bus_resets / tx_timeouts / reconnects） | 開始`4/0/0`→`6/0/0`→`10/0/0`→`12/0/0` |

**受け入れ判定**：Windows側の切断検出・自動再接続・測定再開、およびUSBバスRESET後のMCU通常復帰は実機**PASS**。

**未検証**：MCU独自のEP3 2.5秒タイムアウトによる強制再初期化（`tx_timeouts=0`, `reconnects=0`で今回は発動せず）、長時間／別PC／別ハブでの安定性、USB物理層の異常原因、USB PDとの同時動作、INA226計測精度。100件の値はすべて0mV/0mAであり、測定精度の合格ではない。通信切断中の時間的な欠測も残る。

ローカル診断資料：`261112_2226.pcapng`、`261112_2315.pcapng`、`M8E-USBFS-IRQ-Failed-20261112-222459-117.zip`、`M8F_MeasureOnly_20261112-231459-818.csv`と`.txt`。**これらの生キャプチャ・SRAMダンプは公開Gitに含めない**。

## 5. 再試験と配線安全

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File '.\firmware\experiments\24_usb_cdc_auto_recover\Monitor-M8F-Cdc.ps1' `
  -Mode MeasureOnly -Count 100 -IntervalMs 250 -MaxReconnectSeconds 90
```

現在の試作機ではCoreBoard J1と追加PC用USB-CがD+/D−を共有する。**J1のPD電源とPC通信用USB-Cを同時接続しない**。別経路からの確認済み給電を使用し、INA226の独立5V試験電源をCoreBoard VBUSへ接続しない。詳細は[回路・配線リファレンス](HARDWARE_WIRING.md)を参照。

## 6. 次工程

正式M5は「通信基盤の実証済み、PC制御APIの統合は進行中」とする。次にPDO/APDO取得、PPS設定値と実測値の区別、安全な出力変更、通信断後の再同期、UIとの排他、ログ取得を設計する。**現基板でPD電源用J1とPC用USBを同時に接続しての試験は不可**。正式M6（独自ファーム更新）と正式M8（全機能統合）は未完了。
