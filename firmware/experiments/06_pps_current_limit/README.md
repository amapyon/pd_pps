# 06_pps_current_limit：PPS電流制限実験

## 目的

ダイソー20W USB PD/PPS充電器とWeAct CH32X035F8U6 CoreBoardを使用し、
PPSの「要求電流上限」を切り替えます。

**要求電流は、負荷へ強制的に流す電流ではありません。**
実電流は負荷で決まり、設定値を超える条件で初めて電流制限の影響を観測できる可能性があります。

## 動作仕様

| 項目 | 内容 |
|---|---|
| PPS要求電圧 | 7.00V固定 |
| 初期の要求電流上限 | 1.80A |
| 切り替え先 | 1.00A |
| 電流制限値の切り替え間隔 | 10秒 |
| PPS契約の更新間隔 | 3秒（切り替えがない間） |
| LED | PB12（CH32X035F8U6の物理ピン15） |
| LED点灯 | 1.80Aを要求中 |
| LEDゆっくり点滅 | 1.00Aを要求中 |
| LED 2回点滅＋休止 | 対応PPS APDOなし |
| LED 5回点滅＋休止 | 初期化・通信・応答の失敗 |

LEDは**送信した設定値**を表し、VBUSや実電流の測定結果ではありません。

## ライブラリの変更

添付された`usbpd.h`の複製を`usbpd_current.h`として同梱し、次のAPIだけを追加しました。

```c
USBPD_Result_e USBPD_SelectPPS(
    uint8_t index,
    uint32_t voltageInmV,
    uint32_t currentInmA
);
```

- 元の`../ch32fun/`は変更しません。
- PPS APDOの電圧範囲・最大電流をチェックします。
- 電圧は20mV単位、電流は50mA単位で指定します。
- PPS要求を送った後、`Accept`／`PS_RDY`の状態遷移を待ちます。
- ただし`PS_RDY`は電圧・電流の実測を意味しません。

## 配置

```text
~/pj/pps/
├── ch32fun/
├── 05_pps_sweep/
└── 06_pps_current_limit/
    ├── pps_current_limit.c
    ├── usbpd_current.h
    ├── Makefile
    ├── funconfig.h
    ├── tests/
    │   └── test_rdo.c
    └── README.md
```

ZIPが`~/Downloads/06_pps_current_limit.zip`にある場合：

```bash
cd ~/pj/pps
unzip -o ~/Downloads/06_pps_current_limit.zip -d .
cd 06_pps_current_limit
make test-rdo
make clean
make build
# 必ず充電器と負荷を外してから実行
make flash
```

デバッガへの給電とUSB-C給電が衝突しない構成で端末出力を取得します。

```bash
../ch32fun/minichlink/minichlink -T
```

## 期待するログ（例。実機結果ではありません）

```text
BOOT: CH32X035 PPS current-limit experiment
PD: init=OK
PD: negotiate=OK, state=Power Supply Ready
PD: PDO[3] PPS 5000-11000 mV, max 1800 mA
PPS: start request 7000mV / 1800mA, send=OK, state=Waiting for Accept
PPS: start PS_RDY 7000mV / 1800mA
PPS: refresh request 7000mV / 1800mA, ...
PPS: switch request 7000mV / 1000mA, ...
PPS: switch PS_RDY 7000mV / 1000mA
```

## 抵抗器による実験

手持ちの30Ω・1Wを20本使用する構成：

- 2本直列で60Ωの枝を作り、これを10組並列にする。
- 合成抵抗は約6Ω。
- 7V時の抵抗負荷電流は約1.17A、合計消費電力は約8.17W。
- 30Ω抵抗1本の消費電力は約0.408W。
- CH32X035ボード自身の電流などは上記の1.17Aに含まれません。
- 充電器が電流制限動作をする場合、VBUSが低下したり、過負荷保護が働いて停止したりする可能性があります。

**安全上の注意：**

1. まず外部負荷なしでVBUSが約7Vで安定することを確認してください。
2. 抵抗器は互いに離して耐熱・不燃性の台に置き、短時間の通電から確認します。
3. 大電流の接続にはブレッドボードを使わず、はんだ付けした配線と適切な端子・電流容量のあるVBUS取り出し口を使用してください。
4. 抵抗の接続や取り換えは必ず充電器を抜いて行ってください。
5. 充電器出力のVBUSをMCUのGPIOや3.3V端子に接続してはいけません。
6. WCH-LinkEからの3.3V給電とUSB-C充電器の出力を不用意に並列にしないでください。
7. WeActボードのVBUS経路を負荷の大電流経路に使う場合、基板の許容電流を確認してください。

`test-rdo`はPC上でのPPS RDOビット配置テストで、実機通信やクロスコンパイルは検証しません。

## 既知の制約

- `usbpd.h`の切断検出・再接続処理は限定的です。実験中にUSB-Cを抜いた場合は、停止して再接続・リセットしてください。
- 電流制限値の設定は独立した保護回路の代わりにはなりません。
- 物理的な電流制限特性・精度は未測定です。FNB-C2でVBUS・IBUSを観測してください。
