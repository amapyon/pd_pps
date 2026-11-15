# M31A：USBFSのPWRクロック設定だけを修正する比較実験

## 目的
M30で確認した「PD接続中にCOM10がUnknownになる」問題を、USBFSSetupのPWRクロック誤設定だけ修正して比較する。**M30、M29、M28、PD17は変更しない**。

## 変更した箇所（m28_fsusb.c、CH32X03xブロック）

変更前：`RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC | RCC_APB1Periph_PWR;`

変更後：
```c
RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC;
RCC->APB1PCENR |= RCC_APB1Periph_PWR;
```

PVDによるPHY 3.3V/5V切替は**まだ変更していない**。M30ログの`AFIO CTLR=00040388`のうち `USB_PHY_V33=0`、`UDP_PUE=10` が、M31Aで変化するかを確認する。

## インストール
ZIPの`31a_pd_usbfs_pwrclock`を次の場所に配置：
`C:\pj\pd_pps\src\pd_pps\firmware\experiments\31a_pd_usbfs_pwrclock`

## テスト・ビルド（リポジトリルートで実行）
```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\firmware\experiments\31a_pd_usbfs_pwrclock\Test-M31A-Host.ps1'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '.\firmware\experiments\31a_pd_usbfs_pwrclock\Build-M31A.ps1'
```
ビルドPASS前に書き込まない。BINとFlash容量を確認する。実機書き込みは既存の`pd-pps.ps1 -Action Flash -Project 'firmware\experiments\31a_pd_usbfs_pwrclock'`を使用。

## 実機比較
1. M30のBINを復旧用に保全。PD充電器とPC USBを取り外し、安全なWCH-LinkE給電状態でM31Aを書き込む。
2. PD未接続でLED点滅、Windows COM認識、CDC応答を確認。
3. 回路のD+/D-分離・電源経路が維持されている場合だけPD充電器を接続。COM状態、LED点滅、VDD33、VBUSを確認。異常があれば中止。
4. PD充電器を切断し、COM復帰後にRead-M31A-Diag.ps1で`DIAG`12件を取得。起動直後記録はUSB設定済み状態とは限らない。
5. M30とM31AのPD接続中`AFIO CTLR`（特にbit6とbit[3:2]）、USBFS UDEV_CTRL、COM状態を比較。

## 注意
- この実験はUSBFSのPWRクロック誤設定を単独修正するもので、USB通信の安定性は未検証。
- `DIAG`/`INFO`表示はM30/M28から引き継いでおり、M31A固有の識別文字列には変更していない。ファイル・試験記録で区別する。
- `M31B`（3.3V PHY固定）を行う場合は、M31Aの結果を確認してから別実験として作成する。
- USB通信はUSBホストの動作とプルアップの強度、配線容量にも依存する。D+/D-のDMM電圧だけではUSB波形品質を判定できない。
