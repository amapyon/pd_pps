# M32 USBFS/PD interference diagnostic

M31Aを保全し、M31AのPWRクロック修正を引き継いだ実験32。

## 変更内容
- USB/PD制御レジスタへの書き込みは追加しない。
- `XDIAG` でUSBFS、AFIO、RCC（AHB/APB1/APB2）、GPIOC CFGXR、USB割り込み数/リセット数を取得。
- RAM最大12スナップショット：起動時、直近PD接続前、接続直後、250msごと、切断時。
- M30 `DIAG` とM29 LEDを保持。
- USB ISRで行う追加処理はvolatileカウンターと時刻キャッシュの更新のみ。時刻は直近foregroundで更新したもので厳密な割り込み時刻ではない。
- 追加ログは起動後最初の1回のPD接続セッションだけ。長時間接続中の後半は記録しない。電源断/MCUリセットでデータ消失。
- USB CDCが戻ったときに取得。取得失敗時に何度もUSBを接続し直さず、ログ等を確認。

## 実行
```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\32_pd_usbfs_pd_interference\Test-M32-Host.ps1"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\32_pd_usbfs_pd_interference\Build-M32.ps1"
```
テスト・ビルド成功後のみ、従来と同じ安全な手順でFlash。
```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\dev-workspace\pd-pps.ps1" -Action Flash -Project "firmware\experiments\32_pd_usbfs_pd_interference"
```
PD接続後、PDを外してCOM復帰したら次を実行：
```
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\firmware\experiments\32_pd_usbfs_pd_interference\Read-M32-Diag.ps1" -Port COM10
```
`XDIAG` は件数と累積割り込み数、`XDIAG 0`は通常スナップショット、`XDIAG 0 R`はRCC/GPIO、`XDIAG 0 I`はIRQリセット数を返す。ソース中の `last` は前景処理が最後に更新した時刻でありハードウェアで取得した割り込みタイムスタンプではない。

### 注意
- Windowsでクロスビルド未検証。成功まで書き込まない。
- 比較ではPD未接続時のUSB通信を先に確認。
- PDケーブル側D+/D-とPC側D+/D-は物理的に分離済みであること。
- VBUSがUSBPC側の配線に印加されない構成であること。
