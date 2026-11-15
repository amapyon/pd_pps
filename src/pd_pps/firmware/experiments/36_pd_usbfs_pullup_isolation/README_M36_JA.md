# M36: USB D+プルアップ単独停止・PHY全停止の3段階比較診断

## 目的と前提

M35では、PD充電器のみを接続しUSB PHYを無効化すると、D+とD−がともに約3Vから0Vに変化しました。
M36では (A) 通常動作、(B) USBFSを有効に保ちUDP_PUE[1:0]のみ00へ、(C) USBFS/端子MUXを無効に、の3条件を比較します。
ソースはM35をベースにしていますが、M35の診断コードはM36で置き換え、M29～M34の機能は維持しました。
CH32X035RM V1.9 AFIO CTLR p.75-76: UDP_PUE[1:0] 00はプルアップ無効。実際の端子電圧はGPIOの設定や他回路の影響を受けるのでDMM測定も必須です。

**必須：PD試験中はPC通信用USB-Cケーブルを物理的に取り外すこと。**
PD側とPC側のD+/D−の分離はユーザーが確認済みですが、この実験ではPCは使いません。
USBFSの接続や再列挙には使用できません。USB PHYのプルアップ設定変更中にPCを接続してはいけません。
VDD33が3.3Vで安定し、PD側VBUSが約6.97Vであることを監視してください。異常・過熱があれば中止。
M35のBINを復旧用に保存し、すべてのUSBを抜いてWCH-LinkE経由で書き込んでください。

## 30秒ごとの動作（PD接続検出時から）

| 状態 | PD接続からの時間 | PB12 LED | USBFS | UDP_PUE | 測定 |
|---|---|---|---|---|---|
| A | 0～30秒 | 約2Hz | 有効 | 元の3.3V用1.5kΩ | D+、D−、VDD33、VBUS |
| B | 30～60秒 | 約5Hz | 有効のまま | 00（無効） | 同上 |
| C | 60秒以降 | 約1Hz | 無効・端子フローティング | 00 | 同上 |

LED周期はトグル間隔による概算です（A:250ms、B:100ms、C:500ms）。各状態でDMMを測定します。
Bでは`USB_IOEN`、`USB_PHY_V33`、USBFS->BASE_CTRL/INT_EN/UDEV_CTRL、GPIOC->CFGXR/BSXRおよびD−設定は変更しません。
CではM35と同等のUSBFS全停止・PC16/17入力化を行います。PC14/15のUSB PD端子設定は変更しません。

PD充電器を外した500ms後、USBFSSetupを再実行してUSB CDCを復旧させます。
RAMログが残るにはPD切断後もVDD33が維持される必要があります。
ログ回収はPDを外した後、PC通信用ケーブルを接続して行ってください。

## 配置

`<repo>\firmware\experiments\36_pd_usbfs_pullup_isolation\`。
ZIPに含まれる実験フォルダーをそのまま配置。M35は上書きしません。

## 検証・ビルド

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\36_pd_usbfs_pullup_isolation\Test-M36-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\36_pd_usbfs_pullup_isolation\Build-M36.ps1"
```

**注意：** 静的・ホスト模擬テストは配布元で実施しますが、RISC-V実機ビルドと実機試験はWindows環境で要確認です。

ビルド成功後、PDとPCのUSBケーブルを取り外した状態でWCH-LinkE接続：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\36_pd_usbfs_pullup_isolation"
```

## RAMログの取得（PD切断後のみ）

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\36_pd_usbfs_pullup_isolation\Read-M36-Diag.ps1" -Port COM10
```

- `PUDIAG n=... state=5 blocked=0`（完全な診断後）
- `VS i=N ph=... t=... a=... pe=... b=... u=... e=...`
- `VR i=N f=... g=...`
- 記録順: A（接続時）・B（PU変更前）・U（PU変更後）・C（全停止前）・O（全停止後）・D（PD切断）・R（USB復帰）
- `f`: AFIO->CTLR、`g`: GPIOC->CFGXR、`b`: BASE_CTRL、`u`: UDEV_CTRL、`e`: INT_EN。

### 判断

- Bで**D+・D−の両方**が低下: D+プルアップと両端子電圧の連動が疑われる（D+/D−が物理的に短絡している証明ではない）。
- BでD+のみ低下: D−には独立したバイアス/駆動が存在する可能性。
- Bで変化せず、Cで0V: プルアップの設定ビット以外のUSB PHY出力/端子MUXの寄与を示唆。
- BとCで低下せず: 外部負荷、測定点、フローティング端子の残留電圧等を再調査。

DMMは平均電圧です。USB D+/D−の波形、瞬時レベル、短絡有無は分かりません。
