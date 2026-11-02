# UART単独テスト

Arduinoを使わず、HY0020のP0.06から約1秒ごとに文字列を送信するサンプルです。BLEは使用しません。

```text
HY0020 UART test started (115200 8N1)
HY0020 alive: count=0
HY0020 alive: count=1
```

## ファイル

| ファイル | 用途 |
|---|---|
| `src/main.c` | UART初期化・文字列とカウンターの出力 |
| `src/startup.c` | 起動処理と割り込みベクター |
| `nrf52832.ld` | Flash 512KB / RAM 64KBのメモリ配置 |
| `build.ps1` | SDK内のARM GCCを使ったビルド |

以下のコマンドはすべて **プロジェクトルート `HY0020_test`** で実行します。詳しい導入・配線・エラー対処は[全体のREADME](../../README.md)を参照してください。

## ビルド

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\uart_test\build.ps1
```

出力先は`build/uart_test/`です。書き込むファイルは **`build/uart_test/uart_test.hex`** です。

## 書き込み

FT232HをSWD配線にして実行します。既存のHY0020上のファームウェアを置き換えます。

```powershell
$openocdExe = Join-Path $env:USERPROFILE 'Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe'
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'program build/uart_test/uart_test.hex verify reset exit'
```

## UART受信

電源を切ってSWD用の抵抗と配線を外し、HY0020の **P0.06→FT232H D1、GND→GND** に変更します。HY0020は3.3Vで給電します。D0・D2は未接続です。

```powershell
python tools/uart_monitor.py --seconds 10
```

115200bps・8N1・フロー制御なしで受信します。FT232HのドライバーはWinUSBのままで、COMポートへの切り替えは不要です。

## BLE版

BLEで文字列を送信するサンプルは、別フォルダーの [../ble_text](../ble_text/README.md) にあります。
