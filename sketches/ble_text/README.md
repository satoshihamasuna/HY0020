# HY0020 BLE文字列送信サンプル

既存のUARTテストとは別の、nRF Connect SDK v3.4.1 / Zephyrを使ったBLEサンプルです。接続先がNordic UART Service（NUS）の通知を有効にすると、約1秒ごとに文字列を送信します。

```text
Hello HY0020 0000
Hello HY0020 0001
Hello HY0020 0002
```

デバイス名は **`HY0020-BLE`** です。カウンターは0000～9999で繰り返し、再起動で0に戻ります。切断後も再接続できるよう広告を再開します。

これはBluetooth Low EnergyのGATT通知です。Bluetooth ClassicのSPPや仮想COMポートとして接続する方式ではありません。[Nordic UART Serviceの仕様](https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/libraries/bluetooth/services/nus.html)

## 既存のUART版との使い分け

| サンプル | ソース | ビルド | 書き込み用HEX |
|---|---|---|---|
| UART単独テスト | `sketches/uart_test/src/main.c` | `sketches/uart_test/build.ps1` | `build/uart_test/uart_test.hex` |
| BLE文字列送信 | `sketches/ble_text/src/main.c` | `sketches/ble_text/build.ps1` | `build/ble_text/zephyr/zephyr.hex` |

表のパスはプロジェクトルートからの相対パスです。UART版のソース・ビルドスクリプト・HEXはそのまま残しています。HY0020上で動作するファームウェアは、最後に書き込んだ方です。UART版に戻すには`build/uart_test/uart_test.hex`を書き戻します。

BLE版にも **P0.06 / TX、115200bps・8N1** のUARTログがあります。既存の`tools/uart_monitor.py`で起動・広告・接続・通知の状態を確認できます。LEDやボタンは使用しません。

## ビルド

先に[ルートのREADME](../../README.md)の開発ツール・WinUSB・Pythonの導入を済ませてください。BLE版はSDKのモジュールとライブラリも利用するため、nRF Connect SDK一式が必要です。

プロジェクトルートで実行します。

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\ble_text\build.ps1
```

設定を変更してビルドを作り直す場合は次のようにします。対象は`build/ble_text`で、UART版の生成物には影響しません。

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\ble_text\build.ps1 -Pristine
```

既定のSDKは`C:\ncs\v3.4.1`、ツールチェーンは`C:\ncs\toolchains\4f5b6ad6dd`です。別の場所なら指定します。

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\ble_text\build.ps1 -SdkRoot 'C:\ncs\v3.4.1' -ToolchainRoot 'C:\ncs\toolchains\4f5b6ad6dd'
```

ツールチェーンのハッシュ名は環境によって異なります。スクリプトはそのフォルダーの`environment.json`から必要なPATH等を設定し、同梱Pythonからwestを実行します。確認時はSDK同梱のPython 3.12.4を使用しました。UART受信用のPythonとは別です。システムやユーザーの環境変数を恒久的に変更しません。

ビルド対象は **`hy0020_dip/nrf52832`** です。DKの設定を流用せず、512KB Flash / 64KB RAM、UART TX=P0.06、LDO動作の専用ボード設定を`sketches/ble_text/boards/fdk/hy0020_dip`に置いています。低速クロックは校正付きの内部RCを使います。MCUbootは使わず、Flashの先頭から起動します。

## 書き込み

PythonのUARTモニターを終了し、電源を切ってFT232HをSWD配線にします。

| FT232H | HY0020 |
|---|---|
| D0 | SWDCLK |
| D1 | 220～470Ωの抵抗を介してSWDIO |
| D2 | 同じSWDIOへ直結 |
| GND | GND |

HY0020には別途3.3Vを供給します。詳しい配線図は[ルートREADMEの4章](../../README.md#4-swd書き込み用に配線する)を参照してください。

```powershell
$openocdExe = Join-Path $env:USERPROFILE 'Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe'
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'adapter speed 100; program build/ble_text/zephyr/zephyr.hex verify reset exit'
```

`Verified OK`とリセット完了を確認します。BLE版の書き込み・照合は **100kHzで実機確認済み** です。共有設定の既定値10kHzでは、大きなBLE版の転送中に`Failed to write memory`で中断したため、上のコマンドで100kHzに上書きしています。

HY0020に入っている既存ファームウェアのFlash内容を上書きします。通常はrecoverを実行しません。APPROTECTが出た場合は[ルートREADMEの保護解除手順](../../README.md#62-approtectで書き込めない場合のみ全消去保護解除)を確認してください。

全消去してよい場合に限り、BLE版への書き込みまで続けるコマンドは次のとおりです。最初は共有設定の10kHzで保護解除し、その後100kHzに切り替えて書き込みます。

```powershell
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'init; nrf52_recover; adapter speed 100; reset halt; program build/ble_text/zephyr/zephyr.hex verify reset exit'
```

## Windows PCで受信・送信する

PC内蔵またはUSBのBLE対応Bluetoothアダプターを使います。FT232Hは有線の書き込み・UART用なので、BLE通信用アダプターの代わりにはなりません。HY0020にBLE版を書き込み、3.3Vで給電しておきます。

PC用ツールは`tools/ble_monitor.py`です。Bluetoothをオンにして実行すると、`HY0020-BLE`の検索、接続、NUS TX通知の有効化、文字列表示まで自動で行います。Windows設定での事前ペアリングは不要です。

### 初回の準備

Bleak 3.0.2を使うため、Windows 11（ビルド22000以降）とPython 3.10以上が必要です。このPCではSDK同梱のPython 3.12.4を使います。普段の`python`コマンドは3.8.3なので、以下では実行ファイルを明示しています。[Bleakの対応環境](https://pypi.org/project/bleak/)

プロジェクトルートで実行します。ライブラリはプロジェクト内に導入済みです。新しい環境で用意する場合は次を実行します。

```powershell
$blePython = 'C:\ncs\toolchains\4f5b6ad6dd\opt\bin\python.exe'
& $blePython -m pip install --target .tools/ble_python -r tools/requirements-ble.txt
```

FT232HのWinUSBドライバーを変更する必要はありません。UART用の`.tools/python`とBLE用の`.tools/ble_python`は分けています。

### PCのBluetoothを有効にする

Windowsの **設定 → Bluetoothとデバイス** でBluetoothをオンにします。スキャン時にサービスが無効というエラーが出る場合は、Windowsの「サービス」で **Bluetoothサポートサービス（bthserv）** のスタートアップを「手動」にして開始します。管理者PowerShellなら次の操作に相当します。

```powershell
Set-Service -Name bthserv -StartupType Manual
Start-Service -Name bthserv
```

2026年11月2日の初回確認では、bthservがDisabled / Stoppedだったためスキャンできませんでした。その後、Windowsの管理者承認を経て **Manual / Running** に変更し、このPCからHY0020への接続・通知受信に成功しています。

### 検索と受信

```powershell
$blePython = 'C:\ncs\toolchains\4f5b6ad6dd\opt\bin\python.exe'
# NUS機器を検索するだけ（接続しない）
& $blePython tools/ble_monitor.py --scan --timeout 10

# HY0020-BLEに接続し、30秒間受信する
& $blePython tools/ble_monitor.py --seconds 30
```

期待する表示：

```text
Searching for HY0020-BLE...
Connected to XX:XX:XX:XX:XX:XX; receiving NUS text
Hello HY0020 0000
Hello HY0020 0001
```

`Ctrl+C`で終了できます。接続は終了時に解除します。同名機器が複数ある場合は、検索結果のアドレスを`--address XX:XX:XX:XX:XX:XX`で指定できます。

スマホが接続中なら先に切断してください。HY0020の接続枠は1台なので、PCとスマホを同時には接続できません。

### PCからHY0020へ送信

```powershell
& $blePython tools/ble_monitor.py --send 'Hello from PC' --seconds 30
```

PCからNUS RXへ1回書き込み、その後もHY0020からの通知を受信します。送る文字列はUTF-8で1～20バイトです。今回のHY0020側の処理は、受け取った文字列を有線UARTへ表示します。BLEへのエコーバックは行いません。

参考：[BleakのスキャンAPI](https://bleak.readthedocs.io/en/latest/api/scanner.html)、[接続・通知・書き込みAPI](https://bleak.readthedocs.io/en/latest/api/client.html)

## スマホで受信する

1. スマホにNordicの **nRF Connect for Mobile** を導入し、Bluetoothとアプリに必要な権限を有効にします。
2. アプリのスキャンで **`HY0020-BLE`** を探して接続します。OSのBluetooth設定画面ではなく、BLEを扱えるアプリから接続します。
3. **Nordic UART Service** を開きます。
4. **TX Characteristic** の **Notify / 通知** を有効にします。
5. 受信値をUTF-8 / ASCIIとして表示し、`Hello HY0020 0000`のような文字列が約1秒ごとに届くことを確認します。
6. 通知を無効にすると送信が止まります。切断後は再スキャン・接続し、再び通知を有効にします。

アプリの画面表示や通知ボタンの位置はOS・バージョンによって異なります。UUIDで確認する場合は次の値です。

| 用途 | UUID |
|---|---|
| NUSサービス | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| HY0020 → スマホ：TX通知 | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |
| スマホ → HY0020：RX書き込み | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |

RXへテキストを書き込むと、内容をHY0020のUARTへ表示します。BLEへエコーバックする処理はありません。

この開発サンプルではペアリング・ボンディング・暗号化を要求しません。接続台数は1台です。通知1回のデータは19バイト（CRLF込み）なので、既定のATT MTUでも受信できます。

参考：[NordicのスマホでのNUS確認手順](https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrf/samples/bluetooth/peripheral_uart/README.html#testing)

## UARTでも状態を確認する

BLEは無線なので、FT232HをUART受信用に配線し直すと、スマホとのBLE通信中に有線ログを確認できます。FT232HのUSBドライバーはWinUSBのままです。

| HY0020 | FT232H |
|---|---|
| P0.06 / TX | D1 / RX |
| GND | GND |

SWD用の抵抗とD0・D2の配線を外してから実行します。

```powershell
python tools/uart_monitor.py --seconds 30
```

出力例：

```text
HY0020 BLE text test (NUS)
Advertising as HY0020-BLE
BLE connected; enable NUS TX notifications
NUS notifications enabled
BLE TX: Hello HY0020 0000
BLE TX: Hello HY0020 0001
```

送信文字列は`sketches/ble_text/src/main.c`の`snprintk`付近、デバイス名は`sketches/ble_text/prj.conf`の`CONFIG_BT_DEVICE_NAME`で変更できます。既定のMTUに収めるため、変更時もCRLFを含め20バイト以下にしてください。

## UART単独版に戻す

FT232HをSWD配線へ戻して実行します。

```powershell
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'program build/uart_test/uart_test.hex verify reset exit'
```

UART版HEXがない場合は、`sketches/uart_test/build.ps1`から再生成できます。

## 確認範囲

2026年11月1日に、nRF Connect SDK v3.4.1でBLE版のビルドとHEX生成が成功しました。Flash使用量は126528バイト / 512KB、RAM使用量は22544バイト / 64KBです。HEXのチェックサム、Flash範囲、先頭のスタックポインターとリセットベクターも確認しました。

2026年11月2日にBLE版を実機へ書き込み、`Verified OK`とリセット完了まで確認しました。APPROTECTが再び有効だったため10kHzで全消去・保護解除した後、100kHzで書き込み・照合しました。既存UART版のソースとHEXはPC側に残しています。

2026年11月2日に、このPCから`tools/ble_monitor.py`で`HY0020-BLE`へ接続し、30秒間の通知受信を確認しました。`Hello HY0020 0000`～`Hello HY0020 0029`の30行、合計570バイトを受信し、終了コード0で切断まで完了しています。

スマホからの接続、PCからの`--send`による送信、有線UARTでのBLE版ログ表示は未確認です。UART単独版での書き込み・UART受信は確認済みです。
