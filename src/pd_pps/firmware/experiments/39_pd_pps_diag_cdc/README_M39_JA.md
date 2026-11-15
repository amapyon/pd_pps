# M39：PD/PPS + USB CDC経由の停止理由診断

## 目的と変更範囲

M38A warnings-fixed版を元に、PD停止理由およびPPS更新状況の読み取り専用CDCコマンドを追加。
M38Aで実測した「VBUS約6.9Vを約10秒維持した後、約0Vを経て約5Vへ復帰」を切り分ける。
**PD Policy Engine、PD PHY、ADC監視、Hard Reset復旧、PD状態遷移は一切変更していない。**

- ベース：`M38A_PD_PPS_minimal_CDC_warnings_fixed.zip`
- 元の M38A は保持。新規フォルダ `39_pd_pps_diag_cdc` として追加。
- USB VID:PID = `1209:D035`、bcdDevice=`0039`、Serial=`039`、Product=`PD PPS M39 Diagnostic CDC`。
- 追加したコマンドはすべて観測用。ISRではカウンター加算と受信バッファ処理のみ。
- `DELTA valid=0` のとき、無効な差分を32ビット符号なしラップ値で表示しないようにした。
- `DELTA valid=1` でも既存M38Aで「0」と表示された点は未解決。USB通信判定には`PING`と`COUNT`を使う。

## デバイス配置

`C:\pj\pd_pps\src\pd_pps\firmware\experiments\39_pd_pps_diag_cdc\`

## コマンド

| コマンド | 内容 |
|---|---|
| `PING` | `PONG M39` |
| `STAT` | `a,pe,pps,mv,ma,cfg`：PD接続/Policy Engine/契約/USB構成 |
| `COUNT` | USBFSイベント累積カウンター |
| `DELTA` | 接続区間のUSBFS差分（既存の制約あり） |
| `PDSTOP` | 最初の停止理由、発生時刻、前後PE状態、HR受信数 |
| `PDSTAB` | 接続時間、PPS_READY数、PPS更新開始/完了回数 |
| `PDERR` | Hard Reset受信、復旧遷移、VBUS喪失、ADC異常など |

### `PDSTOP` のフィールド

`active`=CC接続中か、`found`=最初の停止理由を記録済みか、`reason`=理由コード、
`ms`=CC接続開始からその停止までの時間[ms]、`before/after`=PE状態、
`aux`=理由別の補助情報、`hr`=受信Hard Reset数、`detach`=物理切断が検出されたか、
`after_ms`=最初の停止から切断までの時間[ms]。
**`found=0`なら`reason=0`と`ms=0`は停止未記録を意味し、正常動作を保証するものではない。**

`reason` 対応（`src/pd_stop_diag.h` に準拠）：

| 値 | 名前 | 意味 |
|---:|---|---|
| 0 | `PD_STOP_NONE` | 記録なし |
| 1 | `PD_STOP_VBUS_LOSS` | VBUS喪失を検出 |
| 2 | `PD_STOP_ADC_MONITOR` | ADC監視異常 |
| 3 | `PD_STOP_PE_CAPS` | Source Caps処理などでPEがRecoveryへ |
| 4 | `PD_STOP_PE_TX_FAILED` | PD送信失敗 |
| 5 | `PD_STOP_PE_TIMEOUT_CAPS` | Source Caps待ち時間超過 |
| 6 | `PD_STOP_PE_TIMEOUT_ACCEPT` | Accept待ち時間超過 |
| 7 | `PD_STOP_PE_TIMEOUT_PS_RDY` | PS_RDY待ち時間超過 |
| 8 | `PD_STOP_PE_TIMEOUT_SOFT_RESET` | Soft Reset待ち時間超過 |
| 9 | `PD_STOP_PE_OTHER` | その他PE原因 |
| 10 | `PD_STOP_HR_SECOND_RX` | 同一接続で2回目のHR受信 |
| 11 | `PD_STOP_HR_VBUS` | HR中のVBUS安全確認失敗 |
| 12 | `PD_STOP_HR_RX_ARM_TIMEOUT` | HR後のRX再有効化時間超過 |
| 13 | `PD_STOP_HR_COMMIT_TIMEOUT` | HR後のPE/PHYコミット時間超過 |
| 14 | `PD_STOP_HR_BAD_PE_STATE` | HR後のPE状態不一致 |

PE状態：0=DETACHED、1=WAIT_CAPS、2=WAIT_ACCEPT、3=WAIT_PS_RDY、
4=READY、5=SOFT_RESET_WAIT、6=RECOVERY。

### `PDSTAB` と `PDERR`

- `PDSTAB ref_start/ref_done`：PPS更新開始/完了。**初回PPS契約成立は`pps`に含まれるが、`ref_done`には含まない。**
- `PDERR hr`：充電器からのHard ResetをPD PHYで認識した回数。
- `PDERR recovery`：PEがRecoveryへ入った回数。
- `PDERR vbus_loss/adc_fault`：監視側イベント回数。
- `PDERR faults`：接続中はPolicy Engineの現在値、PD切断後は`pd_stability_detach()`が保存した値。
- `PDERR trace_drop/event_lost`：**切断確定後**の累積差分。接続中は未確定のため0等になることがある。
- STOP/STAB/ERRは最新接続のスナップショット。次のCC接続でリセットされるので、**再接続前にログを取得すること**。

## Windows 11での適用・実行

1. ZIPから `39_pd_pps_diag_cdc` フォルダを、`firmware\experiments\` 配下へ展開。
2. 構造・ネイティブCテスト：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\39_pd_pps_diag_cdc\Test-M39-Host.ps1"
```

3. クロスコンパイル（既存の作業環境で実施）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\39_pd_pps_diag_cdc\Build-M39.ps1"
```

4. M38AでCOM11が動作中でも、M39は別のUSBシリアル番号`039`のため、COM番号が変わる。**実際に認識したポートを指定**する。
5. 書き込み時はPD充電器・PC側USBを外し、WCH-LinkEを安全な接続構成で使用：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\39_pd_pps_diag_cdc"
```

6. USB CDC接続確認後、PD充電器接続前後と、6.9V→0V→5Vのイベント後、**新しいCOM番号**で：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\39_pd_pps_diag_cdc\Read-M39-Cdc.ps1" -Port COM12
```

`COM12`は例。実際のポート番号で置換。

経時ログ（PD充電器接続直前に開始して約30秒間記録）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\39_pd_pps_diag_cdc\Watch-M39-Cdc.ps1" -Port COM12 -Seconds 30 -IntervalMs 1000 -LogPath ".\M39_session.txt"
```

## 安全条件

- 今回切断したDNはそのまま。追加の基板加工は不要。PD用J1のDPはUSBFS側に残るので、両端のD+/D−を完全絶縁した状態ではない。
- PC側VBUSとPD充電器側VBUSは接続しない。
- 切断加工箇所の短絡・導通を無通電で再確認。通電中にプローブで隣接端子をショートさせない。
- VBUSが0Vへ低下して復帰する症状が続くなら、観測を短時間に限定。発熱・異常電圧・不安定な動作があれば中止。
- 本パッケージはUSB Type-C/PD規格適合性や電源安全性を保証しない実験用。
- ユーザー環境でRISC-Vクロスコンパイルと実機評価が必要。
