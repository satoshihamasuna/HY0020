---
name: hy0020-flash
description: "HY0020（AE-HY0020-DIP / nRF52832）をWindowsのFT232HとOpenOCDでSWD書き込みし、UART・BLEで確認する。HY0020のファームウェア書き込み、APPROTECT復旧、既存UART/BLEサンプルの実機確認に使う。"
---

# HY0020の書き込みと動作確認

この環境はArduinoを使用しない。FT232HをOpenOCDのSWDアダプターとして使う。J-Linkとして扱わない。

## 対象と場所

既定プロジェクトは現在の作業ディレクトリ。`openocd/ft232h-hy0020.cfg`があるプロジェクトルートで実行する。別の場所から使う場合は補助スクリプトの `-ProjectRoot` に指定する。ユーザー名や個人のフォルダー配置を固定しない。次のパスはプロジェクトルートからの相対パス。

| 対象 | ソース | ビルドスクリプト | 書き込むHEX |
|---|---|---|---|
| UART単独版 | `sketches/uart_test/src/main.c` | `sketches/uart_test/build.ps1` | `build/uart_test/uart_test.hex` |
| BLE文字列版 | `sketches/ble_text/src/main.c` | `sketches/ble_text/build.ps1` | `build/ble_text/zephyr/zephyr.hex` |
| mouse_type8中継版 | `sketches/mouse_uart_ble/src/main.c` | `sketches/mouse_uart_ble/build.ps1` | `build/mouse_uart_ble/zephyr/zephyr.hex` |

共有設定は `openocd/ft232h-hy0020.cfg`。導入の詳細はルートの `README.md`、BLEの受信手順は `sketches/ble_text/README.md` を必要時に読む。パスやサンプルの配置が変わっていたら、現行ファイルに合わせる。

mouse_type8のUARTを中継する場合は `sketches/mouse_uart_ble/README.md` を読む。デバイス名は `HY0020-MOUSE`、P0.06/TXとP0.08/RXによる115200bpsの双方向通信。デバッグ文を同じUARTへ出すとバイナリを壊すため、通常の文字列サンプルをそのまま使わない。

既知のOpenOCDは `%USERPROFILE%\Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe`。SDKは `C:\ncs\v3.4.1`。別配置では補助スクリプトの `-OpenOcdPath` / `-ProjectRoot` を指定する。

## 書き込む前の判断

- ユーザーが指定したUART版・BLE版を選ぶ。直近の会話で決まっていれば再確認しない。手順の説明・スキルの作成だけの依頼では実機へ書き込まない。
- 対象HEXの存在を確認する。ソースを変更した場合やHEXがなければ、対応するビルドスクリプトを先に実行する。片方のサンプルを他方で置き換えない。
- 通常の書き込み依頼は、そのHEXが使うFlash領域の上書きを含む。全消去・UICR消去が必要になったら、会話内に有効な許可があるか確認し、なければ消える内容を具体的に説明して確認する。既存の許可が適用できる場合は同じ許可を取り直さない。
- FT232Hを使うUARTモニターや別のOpenOCDが終了していることを確認する。直近の会話でSWD配線が確認できていれば、接続確認のためだけに同じ確認を取り直さない。UART配線が残っている場合は、電源を切ってSWD配線へ戻してもらう。

## SWDの配線とドライバー

| FT232H | HY0020 |
|---|---|
| D0 / ADBUS0 | SWDCLK |
| D1 / ADBUS1 | 220～470Ωの抵抗を介してSWDIO |
| D2 / ADBUS2 | 同じSWDIOへ直結 |
| GND | GND |

HY0020へ別途3.3Vを供給し、GNDを共通にする。信号線だけでは給電されない。D1とD2を抵抗なしで短絡しない。配線変更時は電源を切る。UART用のP0.06→D1接続はSWD書き込み時には外す。

FT232Hの標準USB IDは `0403:6014`。OpenOCDにはWinUSBを使う。Zadigでは対象IDを確認し、右側でWinUSBを選んでReplace Driverを押す。**このプロジェクトのUART受信にもWinUSBを使えるので、VCPドライバーへ戻す案内は不要。**

USBポート変更後、FTDIBUSが再び割り当てられていた実績がある。過去にWinUSBへ変更済みでも、`LIBUSB_ERROR_NOT_SUPPORTED`なら現在の接続位置のドライバーを確認する。

## 書き込みの実行

このスキル内の `scripts/flash_hy0020.ps1` を使う。`$skillRoot`は、実際に読み込んだ本スキルのディレクトリに置き換える。スクリプトはビルドやドライバー変更を自動では行わない。

```powershell
# 接続確認だけ。Flashの消去・書き込みはしない。
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware ble -CheckOnly

# BLE版：100kHzで書き込み・照合・リセット
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware ble

# mouse_type8のUART–BLE中継版（100kHz）
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware mouse

# UART単独版：10kHzで書き込み・照合・リセット
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware uart

# 全消去が許可され、APPROTECTが確認された場合だけ
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware ble -Recover

# 実行するコマンドを表示するだけ。USBにもアクセスしない。
& "$skillRoot/scripts/flash_hy0020.ps1" -Firmware ble -Recover -WhatIf
```

実機で確認した重要な条件：

- 通常UART版は10kHzで成功。BLE版（約126KB）は10kHzだと転送中に `Failed to write memory` が発生し、100kHzで書き込み・照合に成功した。BLE版の通信エラーに対して無条件にさらに低速化しない。
- `nrf52_recover` は10kHzで成功した。BLE復旧時は **10kHzでrecover → 100kHzへ変更 → 書き込み** の順。
- mouse_type8中継版もこの順で2026年11月3日に書き込み・照合・リセットが成功し、広告名 `HY0020-MOUSE` の検出まで確認した。実マウスからのUART応答は別途確認する。
- 一度recover済みでも、後の接続時にAPPROTECTが再び有効だった実績がある。過去の復旧だけを根拠に、現在も解除済みと決めつけない。
- OpenOCD設定ではターゲット設定を読み込んだ後に速度を指定する。ターゲット設定が速度を上書きするため。

成功の判断は終了コード0、`Verified OK`、リセット完了の組み合わせ。接続できたことや `Programming Started` だけでは成功と報告しない。`nrf52_recover`はFlash全域とUICRを消し、出荷時のブートローダーも失われる。

## エラー時の分岐と停止条件

| 症状 | 次に行うこと |
|---|---|
| `LIBUSB_ERROR_NOT_SUPPORTED` | FT232HのUSBドライバーと対象IDを確認。WinUSBを使う |
| `cannot read IDR` | SWD配線・3.3V・共通GNDを確認してもらう。物理状態が変わっていない同じ試行を繰り返さない |
| `Could not find MEM-AP` + APPROTECT警告 | 通常書き込みを止め、全消去の許可の範囲を確認してrecoverへ進む |
| BLEを10kHzで転送して失敗 | 100kHzで再試行する。書き込み途中ならFlashが一部変更済みの可能性を伝える |
| 同じ設定・状態で再び失敗 | 無制限にretryせず、ログと最後に成功した段階を示し、配線・電源等の必要な変化を求める |

ファイルの読み込みやUSBアクセスが実行環境の制限で拒否された場合は、認められた実行権限で再試行する。ライブラリ読み込み失敗を、すぐにパッケージ未導入と断定しない。OS側の管理者権限が必要な設定変更は、通常のWindowsの管理者承認を使う。

## 書き込み後の確認

**UART**：電源を切ってSWD用の抵抗・D0・D2を外し、HY0020のP0.06/TX→FT232HのD1/RX、GND同士にする。115200bps・8N1・フロー制御なし。プロジェクトルートで `python tools/uart_monitor.py --seconds 10` を実行する。WinUSBのまま受信できる。

**BLE**：デバイス名は `HY0020-BLE`。NUSのTX通知を有効にすると約1秒ごとに `Hello HY0020 0000` 等を送る。接続枠は1台なので、PCで使う前にスマホ側を切断する。PCではFT232HではなくPCのBluetoothアダプターを使う。

```powershell
# プロジェクトルートで実行。普段のpythonは3.8.3なのでSDK同梱版を指定。
& 'C:\ncs\toolchains\4f5b6ad6dd\opt\bin\python.exe' tools/ble_monitor.py --seconds 30
```

PC用ツールはPython 3.10以上と `.tools/ble_python` のBleakを使う。`bthserv`がDisabledの場合はBLE通信を開始できない。このPCでは管理者承認後にManual / Runningへ変更し、2026年11月2日に30行・570バイトの受信を確認済み。書き込みだけを依頼されているときに、無関係なOS設定変更まで広げない。

書き込み・照合と、UART/BLEの実際の受信は区別して報告する。ユーザーが指定したサンプルのソースとHEXを残す。
