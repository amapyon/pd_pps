# M38A：PD/PPS + 最小USB CDC（ファームウェア切り分け実験）

## 目的

M29〜M37では、PD接続中にUSB CDCが消失し、USBFS側でD+とD−が連動してHighになることが確認された。M38Aは、**PD17のPD Policy Engine・VBUS監視・Hard Reset保護をそのまま残し、USB CDCのアプリケーション処理だけを単純化**する比較用ファームウェア。根本原因の修正を保証するものではない。

- CDCコマンドを `PING` / `STAT` / `COUNT` / `DELTA` に限定。すべて読み取り専用。
- CDCの既存リカバリー処理、送信タイムアウト制御、M30〜M37の診断処理を除去。
- M31AのUSB PHY/PWRクロック修正、USBFS下位の列挙/エンドポイント処理は基本的に維持。
- **USB CDCの割り込みハンドラー内からPD処理を一切呼ばない。** ISRはバスイベント計数と小さな受信キューだけ。
- PD接続期間のUSB割り込み統計をRAMに保持し、PD切断後に読み出せる。
- LED PB12を250msごとに反転（約2Hz点滅）。

## 安全条件

1. PD17/M37の復旧用 `.bin` と既存ソースを保管し、M38Aは別実験として追加。
2. 書き込み時はPD充電器とPC通信用USB-Cを取り外し、既存のWCH-LinkEの安全な構成を使用。
3. 初回通電はPD充電器なしでUSB CDC列挙とコマンド応答を確認。
4. PC側とPD側のD+/D−およびCC1/CC2の分離と電源経路の安全性が確認できている場合に限り、PD充電器との同時接続を試す。
5. VDD33、PD側VBUS、発熱、PDのREADY状態を監視。異常時は停止。**PC側VBUSとPD側VBUSを直結しない。**
6. PC側USB-CのVBUS検出を追加する修正は本実験では行わない。USB Type-C規格への適合性が保証されるわけではない。

## 配置

`C:\pj\pd_pps\src\pd_pps\firmware\experiments\38a_pd_pps_min_cdc\`

## テストとビルド（Windows 11 PowerShell）

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38a_pd_pps_min_cdc\Test-M38A-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38a_pd_pps_min_cdc\Build-M38A.ps1"
```

Python構造テストと、ネイティブGCC/Clangがある場合のCランタイムテストを行う。M38AのRISC-Vクロスビルドと実機試験はユーザー環境での実行が必要。

## 書き込み（ビルドPASS後のみ）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\38a_pd_pps_min_cdc"
```

USBデバイスの `bcdDevice` を `0x0038`、USBシリアルを `038` にしたため、WindowsのCOMポート番号がCOM10以外へ変わることがある（VID:PID は実験系の 1209:D035 のまま）。

## 手順

1. PD未接続でLED点滅、COMの列挙を確認。
2. 実際のCOM番号で以下を実行。`PING` に `PONG M38A` が返ることを確認。
3. PD充電器を接続し、COMポートが残るか、USB CDCコマンドが応答するか確認。PD側VBUS（以前は約6.97V）とVDD33（約3.3V）を確認。
4. PD接続中にCOMが認識されなくなる場合は、PC側USBはそのまま、PD充電器を取り外す。CDCが復帰したら、再度コマンドを実行。
5. 結果と `COUNT`、`DELTA` の出力を記録。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38a_pd_pps_min_cdc\Read-M38A-Cdc.ps1" -Port COM10
```

`PING` はCDC往復確認、`STAT` はPD/USB状態、`COUNT` は累積USB割り込み数、`DELTA` はPD接続検出〜切断検出のイベント差分を示す。`DELTA valid=1` のときだけ期間差分は有効。ソフトウェアのCC接続検出時刻と物理抜き差しの時刻には差があり、切断直前のUSB復旧イベントが含まれる可能性がある。

## 判断

- PD未接続でもCDCが列挙されない：M38Aの新しいUSB CDC実装またはUSBFS基礎部分の問題。PDを接続せずM37へ戻す。
- PD接続中もCDCが維持される：旧CDCランタイム、再接続処理、診断計装の影響が疑われる。差分を小さく戻す追試が必要。
- PD接続中にCDCが消失する：USB CDCアプリ層の単純化だけでは解消しない。USBFS物理層、共通クロック/割り込み、PD接続中の電気的状態を重点調査。ただしハードウェア不良の証明にはならない。
- PDの契約・安全監視が以前と異なる：直ちに試験を止め、M37へ復旧。

## この実験で**変更していない**こと

PD Policy EngineとPHY通信・安全監視モジュールの `.c/.h` はM37由来のバイト列をそのまま採用。既存のUSBFS低レベルハンドラーの列挙/エンドポイント処理は維持し、診断コールバックのみ入れ替え。追加ポートのCC抵抗、PC側VBUS検出回路などのハードウェア設計は変更できない。

## 注意

この成果物は診断実験用。USB接続の製品動作、PD/PPS電力制御の妥当性、USB Type-C規格適合性について保証・認証するものではない。

## 修正履歴：M38A warnings-fix（2026-11-14）

`src/main.c` に残っていた未使用の `m28_sample_valid` / `m28_sample_ms` /
`m28_sample_mv` / `m28_sample_ma` / `m28_sample_epoch` と、それらへの代入のみ削除。
`pd_sensors_capture_ready()`、`pd_sensors_capture_detached()`、ADC電圧監視、
PD Policy Engine、Hard Reset保護、CDC処理は変更していない。
`tests/test_m38_structure.py` に未使用キャッシュの再混入と安全処理保持のテストを追加。
RISC-V GCCによる最終ビルドはWindows環境で実施すること。
