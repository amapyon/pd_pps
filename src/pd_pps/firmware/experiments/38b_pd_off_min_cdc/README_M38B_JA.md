# M38B: USBPD無効＋最小USB CDC（M38Aとの比較対照）

## 目的

M38Aでは、PD充電器の接続中（約7.172秒）、USB転送イベント SETUP / IN / OUT が0回となり、COMポートが消失した。USB CDCアプリケーション層を最小化しただけでは改善しなかった。

M38Bでは、**M38AのUSBFS低レベルドライバーを維持し、USBPD側のファームウェアだけを無効化**する。

- `phy_init()`、`pe_init()`、`pe_tick()`、PD送信、PD Policy Engine、USBPD割り込みハンドラーを一切コンパイル・実行しない。
- USBFSの初期化、EP0、CDCのSETUP / IN / OUT実装はM38Aと同じ（コード上の変更はローカル診断関数名のみ）。
- PB12 LEDは250msごとにトグル（約2Hz）。
- 読み取り専用の `PING`, `STAT`, `COUNT` を維持する。
- `STAT` は `pd=off pps=0` を返す。USBPD無効のためM38AのPD状態は取得できず、`DELTA` コマンドは提供しない。
- USBシリアル番号 `039`、bcdDevice `0039`。VID:PIDは `1209:D035`（M38Aと同じ）。COMポート番号が変わる場合がある。

## 電源とType-Cに関する安全上の重要事項

WeAct CH32X035 CoreBoard V1.0では、標準USB-CコネクタのCC1/CC2に各5.1kΩのRdが実装されている。M38Bはこれをハードウェア上で維持するため、USB PD通信をしなくても、通常のType-C給電の**初期5V**を利用できる構成と考えられる。

**M38BはPD/PPS契約をしない。7Vの要求は行わない。** 実際の電圧が5Vになることは実機で確認すること。

1. M38A/M37のBINをバックアップする。最初の書き込み前にPD充電器・PC通信用USB-Cを外す。
2. WCH-LinkEは従来の安全な構成だけを使用。書き込みはチャージャーを切った状態で行う。
3. 初回の起動は充電器なしでCDC列挙・コマンド応答を確認（基板への独立した3.3V電源が必要）。
4. PC通信用USB-CのVBUSが基板に接続されていない既存構成を前提とする。**PC側VBUSとPD充電器側VBUS、あるいはVDD33を直接接続しない**。PC側CC信号がPD側CC信号と独立していることも確認する。
5. PD充電器を接続する場合、PD側VBUSが**おおむね5V**、VDD33が**おおむね3.3V**であることを確認。PD契約がないのにVBUSが約7Vまたは5.5Vを大きく超える場合は、直ちに切断し、電源構成を再確認する。PD充電器の切断・再投入で以前のPPS契約を解除したうえで開始する。
6. 異常な発熱、LED停止、電源不安定があれば試験を中止する。PD側D+/D−とPC側D+/D−の分離を維持する。
7. **M38BにはPD通信や電圧の保護・回復制御は存在しない。** PDPPS試験ではなく5VのType-C接続がUSBFSに与える影響を調べるための限定的な対照実験。

## 配置

`C:\pj\pd_pps\src\pd_pps\firmware\experiments\38b_pd_off_min_cdc\`

元の `38a_pd_pps_min_cdc` は変更・上書きしない。

## テスト・ビルド（Windows PowerShell）

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38b_pd_off_min_cdc\Test-M38B-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38b_pd_off_min_cdc\Build-M38B.ps1"
```

静的テスト16件がPASS。Cランタイムのホストテストはネイティブgcc/clangがある場合に実行し、ない場合はスキップされる。別環境のClang (RISC-Vターゲット、代替標準ライブラリヘッダー) でCソース3件の構文チェックはPASS。**ユーザー環境のxPack RISC-V GCCによる全体ビルド、Flash書き込み、実機動作は未確認**。

## 書き込み（テスト・ビルド成功後のみ）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\38b_pd_off_min_cdc"
```

## 実験手順

1. PD充電器未接続。LED点滅、M38BのUSBデバイスが認識されることを確認。
2. デバイスの `USB\VID_1209&PID_D035\039` を確認し、新しいCOM番号で `Read-M38B-Cdc.ps1` を実行。`COUNT` の値を記録。
3. **PC通信用USBをつないだまま**、分離済みの標準USB-CへPD充電器を接続する。VBUS約5V、VDD33約3.3Vを確認。
4. COMポートを維持する場合は、M38B `PING`/`STAT`/`COUNT` を接続中に実行する。消えた場合は、PC側USBを無理に抜き差しせずPD充電器を取り外す。
5. PD充電器取り外し後にCOMが復帰したら `Read-M38B-Cdc.ps1` を再実行する。
6. COMの状態、VBUS、VDD33、可能ならD+・D−の電圧を報告する。PC未接続時のD+・D−測定は別の測定条件として分ける。

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name, PNPDeviceID
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\38b_pd_off_min_cdc\Read-M38B-Cdc.ps1" -Port COM11
```

COM番号は例。実際の番号に置き換えること。

出力例（**数値は例示であり実測値ではない**）：

```text
PING : PONG M38B
STAT : STAT pd=off pps=0 cfg=1
COUNT : COUNT set=... in=... out=... rst=... irq=... ov=...
```

`COUNT` は起動後のUSB割り込み累積値。接続期間の差分は**PD検出をしないため内部では計測しない**。前後の単純差分にはPD切断後の再列挙イベントも含まれる。

## 判定の目安

| M38A (7V PPS) | M38B (PD無効・初期5V) | 解釈 |
|---|---|---|
| 異常 | 正常 | PD通信、PPS電圧、VBUS/給電条件のいずれかが関連。PDファームウェアのバグとはまだ断定できない。次のM38CでPD有効・5V固定契約を比較。 |
| 異常 | 異常 | PDプロトコルの実行がなくても再現。USBFS基礎設定、標準USB-Cの電源・配線、5V充電器側の物理影響等を調査。 |
| 異常 | PD未接続から異常 | M38B自身のUSBCDC実装・供給電源・初期化の問題。PD接続試験は中止しM38Aへ戻す。 |

**M38AとM38BはPD出力電圧が7V対5Vで異なるため、この実験だけで「USBPDコードだけが原因」とは判定できない。** M38Cでは電圧を揃える。

## 変更・検証の範囲

- USBFS下位の `m28_fsusb.c` は、ヘッダー参照と2つの診断フック名以外をM38Aと完全同一に維持。
- USB CDCのアプリ層はM38Aの実装を基準に、PD関連ステータス表示とPD検出カウンターのみ除外。SETUP/IN/OUT処理は同じ。
- シリアル識別子はM38B固有。
- OSでのPowerShell実行、xPackビルドとフラッシュ、USB Type-C/PD規格適合の最終検証は未実施。
