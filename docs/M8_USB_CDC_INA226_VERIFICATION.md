# M8 USB CDC / INA226 実機検証記録

| 項目 | 内容 |
|---|---|
| 記録日 | 2026-11-09（JST） |
| 実験 | `firmware/experiments/21_usb_cdc_ina226` |
| 評価機 | WeActStudio CH32X035F8U6 CoreBoard V1.0、INA226（R010） |
| ホスト | Windows 11 / USB CDC仮想COM9 |
| 配線基準 | [`HARDWARE_WIRING.md`](HARDWARE_WIRING.md) の第4・5節 |
| 検証範囲 | M8 Build、CDCコマンド、INA226識別・レジスタ読み出し、数値応答 |

## 1. Build

初回は`m8_ina226.c`で`Delay_Us()`を外部関数として宣言したためリンクエラー。ch32funの`ch32fun.h`にある`Delay_Us`マクロを使うよう修正後、次の結果を確認した。

| 項目 | 結果 |
|---|---|
| `-Action Build` | **PASS** |
| FLASH | **5,476B / 62KB（8.63%）** |
| RAM | **1,000B / 20KB（4.88%）** |
| BIN生成 | PASS |
| Flashログ | 本記録には未掲載。M8識別子を返した実機通信から稼働を確認 |

## 2. COM9経由のコマンド試験

実行：

```powershell
Set-Location 'C:\pj\pd_pps\src\pd_pps'
.\firmware\experiments\21_usb_cdc_ina226\Test-M8-CdcMeasure.ps1 -Port COM9
```

| 項目 | 実測結果 | 判定 |
|---|---|---|
| INFO | `INFO board=CH32X035F8U6 firmware=m8_usb_cdc_measure protocol=3` | PASS |
| STATUS | `STATUS rx_packets=2 rx_bytes=12 tx_packets=1 tx_bytes=63 rx_overflows=0 errors=0` | PASS |
| MEASURE | `MEASURE status=ok voltage_mV=49 current_mA=0` | PASS（取得機能） |
| PING / HELP | 期待どおり | PASS |
| 未知・長すぎるコマンド | 適切に拒否 | PASS |
| PING追加20往復 | 全件PASS | PASS |
| MEASURE追加10回 | `status=ok`の形式・整合性を確認 | PASS |

**センサーの認識と読み出しには成功したが、計測精度は未検証。** `49mV`、`0mA`について、測定点の物理的条件、基準DMM値、負荷条件、センサーVBSの結線状態が記録されていない。

## 3. 今後の受入試験

1. 無電圧、独立した既知の5V、5V＋100Ω/1Wの小電流負荷などの条件で、DMMとM8値を比較する。
2. IN+／IN−の高側シャントとVBSの測定点を導通確認する。**既存J1とPC用USB-Cを同時接続しない。**
3. シャントの誤差、温度上昇、配線抵抗、符号、無負荷オフセットを記録する。
4. COMの切断再接続、長時間測定、I²C切断／再接続時のエラー復旧を確認する。
5. 最終的なPD/PPS連携はUSBデータ線の分離・電源保護設計後に独立試験する。

### 判定

- **確認済み**：M8がWindows 11からINA226値を取得する機能。
- **未確認**：電圧・電流の絶対精度、負荷定格、USB PDとの同時運用、製品用回路としての適合性。
