# HY0020 開発環境・書き込み・UART動作確認

秋月電子の **AE-HY0020-DIP（nRF52832搭載）** を、Arduinoを使わずに開発するための手順です。FT232HをOpenOCDのSWDアダプターとして使ってCプログラムを書き込み、配線を切り替えて同じFT232HでUARTログを受信します。

**FT232HのUSBドライバーはWinUSBのまま、書き込みとUART受信の両方に使用できます。** UARTはCOMポートを経由せず、付属のPythonツールから直接受信します。SWDとUARTの同時利用はしません。

2026年11月1日に、全消去・保護解除、UARTテスト用HEXの書き込み・照合、カウンターの連続受信まで実機で確認しました。

## サンプルの選択

Codex用の書き込みスキルは [skills/hy0020-flash/SKILL.md](skills/hy0020-flash/SKILL.md) に保存しています。SWD配線、UART/BLEの選択、10kHzでの保護解除と100kHzでのBLE書き込み、動作確認の要点をまとめています。

| サンプル | ビルド方法 | 書き込み用HEX | 手順 |
|---|---|---|---|
| UART単独テスト（既存） | `sketches/uart_test/build.ps1` | `build/uart_test/uart_test.hex` | [UARTサンプル](sketches/uart_test/README.md) |
| BLE文字列送信（NUS） | `sketches/ble_text/build.ps1` | `build/ble_text/zephyr/zephyr.hex` | [BLEサンプルのREADME](sketches/ble_text/README.md) |
| mouse_type8のUART–BLE中継 | `sketches/mouse_uart_ble/build.ps1` | `build/mouse_uart_ble/zephyr/zephyr.hex` | [中継サンプルと専用シェル](sketches/mouse_uart_ble/README.md) |

既存のUARTプログラムとHEXを残して、BLEサンプルを追加しています。HY0020で起動するのは最後に書き込んだファームウェアです。BLE版はスマホ等から接続し、通知を有効にすると文字列を送信します。

Windows PCからのBLE接続・送受信には`tools/ble_monitor.py`を使います。[PCとの通信手順](sketches/ble_text/README.md#windows-pcで受信送信する)を参照してください。

mouse_type8のUARTをBLEへ中継する場合は、[mouse_uart_ble](sketches/mouse_uart_ble/README.md)を使います。専用PCツール`tools/mouse_ble_shell.py`から`info ver`等を送受信できます。

2026年11月3日に、DMA受信バッファを128バイトへ修正した版で、マウス給電・UART接続からBLE越しに`info ver`、`info battery`、`help`の応答を確認しました。検証範囲は[中継サンプルのREADME](sketches/mouse_uart_ble/README.md#検証範囲)を参照してください。

## 1. 準備するもの

| 項目 | 使用するもの |
|---|---|
| PC | Windows x64、PowerShell |
| ターゲット | [HY0020 DIP化キット AE-HY0020-DIP](https://akizukidenshi.com/catalog/g/g131342/) |
| USBアダプター | [Adafruit FT232H（秋月販売コード108942）](https://akizukidenshi.com/catalog/g/g108942/) |
| 電源 | HY0020に供給する安定した3.3V電源 |
| 配線 | USBケーブル、ジャンパー線、ブレッドボード等 |
| 抵抗 | SWD配線用に220～470Ωを1本。配線例では470Ωを使用 |
| 開発ソフト | VS Code、nRF Connect for VS Code、nRF Connect SDKとツールチェーン |
| 書き込みソフト | xPack OpenOCD、ZadigによるWinUSBドライバー |
| UART受信ソフト | Python、PyFtdi、PyUSB、libusb-package |

LED、Arduino IDE、J-Link、追加のUSB-UART変換器は、この手順では不要です。FT232HはJ-Linkとして認識させるのではなく、OpenOCDのFTDIアダプターとして使います。

### 電源と配線の共通ルール

- 配線を変更するときは電源を切ります。
- HY0020の **3V3に3.3V、GNDに電源のGND** を接続します。SWD信号線やUART信号線だけでは給電されません。
- FT232HとHY0020のGNDを共通にします。
- HY0020の3V3やGPIOへ5Vを直接接続しません。UART信号も3.3V系です。
- 複数の電源出力をつなぎ合わせず、給電元は1つにします。
- DIP端子の位置は[秋月のピン配置図](https://akizukidenshi.com/goodsaffix/PINOUTS_20260209.pdf)と[回路図](https://akizukidenshi.com/goodsaffix/%E5%9B%9E%E8%B7%AF%E5%9B%B3_20250918.pdf)で確認します。以下の表は基板上の端子名です。

## 2. 開発ツールを導入する

### 2.1 VS CodeとnRF Connect SDK

1. [Visual Studio Code](https://code.visualstudio.com/)をインストールします。
2. VS Codeの拡張機能で、Nordic Semiconductorの **nRF Connect for VS Code Extension Pack** をインストールします。
3. nRF Connectの画面で **Install SDK** を選び、SDKと対応するツールチェーンをまとめてインストールします。すでに導入済みの場合は、Manage SDKs等から確認できます。
4. このプロジェクトの再現には **nRF Connect SDK v3.4.1** を使用します。SDKの既定パスは `C:\ncs\v3.4.1` です。

詳細は[NordicのSDKインストール手順](https://docs.nordicsemi.com/r/bundle/nrf-connect-vscode/page/get_started/quick_setup.html/installing-sdk-and-toolchain-for-the-first-time)を参照してください。

今回のプログラムはベアメタルのCプログラムです。SDKからARMコンパイラ、CMSISヘッダー、Nordicの起動時初期化コードを利用します。Zephyr、BLE、SoftDeviceは組み込んでいません。`west build`ではなく、付属の`sketches/uart_test/build.ps1`でビルドします。

### 2.2 OpenOCD

1. [xPack OpenOCDのReleases](https://github.com/xpack-dev-tools/openocd-xpack/releases/)からWindows x64用ZIPをダウンロードします。
2. 確認に使用したファイルは `xpack-openocd-0.12.0-7-win32-x64.zip` です。`Source code`ではなく実行ファイル入りのZIPを選びます。
3. ZIPを展開します。`openocd.exe`だけを移動せず、DLLやスクリプトを含むフォルダー全体を残します。

ユーザー名を含めない配置例は次のとおりです。`%USERPROFILE%`は各ユーザーのホームフォルダーを表します。

```text
%USERPROFILE%\Downloads\xpack-openocd-0.12.0-7-win32-x64\
└─ xpack-openocd-0.12.0-7\
   ├─ bin\
   │  ├─ openocd.exe
   │  └─ libusb-1.0.dll
   └─ openocd\scripts\
```

以降のPowerShellコマンドは、プロジェクトのルートフォルダーで実行します。`$openocdExe`は使うPowerShellごとに設定してください。別の場所に展開した場合はパスを変更します。

```powershell
# 展開・クローンしたHY0020_testフォルダーで実行します。
$openocdExe = Join-Path $env:USERPROFILE 'Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe'
& $openocdExe --version
```

バージョンが表示されれば起動確認は完了です。PATHへの追加は不要です。[xPackの導入手順](https://xpack-dev-tools.github.io/openocd-xpack/docs/install/)も参照してください。

### 2.3 FT232HをWinUSBへ変更する（最初の1回）

1. FT232HをPCへUSB接続します。
2. [Zadig公式サイト](https://zadig.akeo.ie/)からダウンロードして起動します。Windowsの管理者確認を許可します。
3. **Options → List All Devices** を有効にします。
4. **USB Serial Converter** を選び、USB IDが **`0403:6014`** であることを確認します。他のUSB機器は選ばないでください。
5. Driver欄の右側で **WinUSB** を選択します。
6. **Replace Driver** または **Install Driver** を押し、完了を待ちます。

変更後は左側にもWinUSBと表示されます。同じ設定でReinstall Driverを押す必要はありません。

USBポートを変更した際、同じFT232HでもFTDI用ドライバーに戻って認識された実績があります。`LIBUSB_ERROR_NOT_SUPPORTED`が出た場合は、その接続位置でのドライバーも確認し、必要ならWinUSBを再割り当てしてください。

WinUSBにすると、通常のCOMポートとしては表示されません。今回のPythonツールはその状態で動作します。UART受信のためにFTDIのVCPドライバーへ戻す必要はありません。[OpenOCDのWindows向けドライバー説明](https://github.com/openocd-org/openocd/blob/master/README.Windows.md)

### 2.4 PythonとUART受信用ライブラリ

Pythonとpipが使用できることを確認します。

```powershell
python --version
python -m pip --version
```

未導入の場合は[PythonのWindows向け配布ページ](https://www.python.org/downloads/windows/)から64bit版を導入し、PowerShellで`python`を実行できる状態にします。

次のコマンドで、プロジェクト内に受信用ライブラリを導入します。

```powershell
python -m pip install --target .tools/python pyftdi==0.55.4 pyusb==1.2.1 pyserial==3.5 libusb-package==1.0.26.4
```

上記は実機確認時のパッケージバージョンです。このPCではPython 3.8.3で受信できました。`.tools/python`はこのPCでは導入済みですが、Git管理対象外のため、新しい環境では導入が必要です。新しいPython環境での互換性は別途確認してください。

PyFtdiのWindows対応は公式にはテスト対象外ですが、**本プロジェクトではWinUSB＋libusb-packageで実機受信を確認しています**。[PyFtdiの対応環境](https://eblot.github.io/pyftdi/)、[UART API](https://eblot.github.io/pyftdi/api/uart.html)

## 3. テストプログラムをビルドする

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\uart_test\build.ps1
```

ビルドスクリプトは`C:\ncs\toolchains`内のARM Zephyr GCCを探索します。確認時のコンパイラはGCC 14.3.0でした。SDKやコンパイラの場所が違う場合は明示します。

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\uart_test\build.ps1 -SdkRoot 'C:\ncs\v3.4.1' -CompilerBin 'C:\ncs\toolchains\4f5b6ad6dd\opt\zephyr-sdk\gnu\arm-zephyr-eabi\bin'
```

ツールチェーンのハッシュ名は環境ごとに異なるため、上記はこのPCの例です。

| 出力ファイル | 用途 |
|---|---|
| `build/uart_test/uart_test.hex` | OpenOCDで書き込むファイル |
| `build/uart_test/uart_test.elf` | シンボル情報付きのデバッグ用ファイル |
| `build/uart_test/uart_test.bin` | 生のバイナリ |
| `build/uart_test/uart_test.map` | メモリ配置の確認 |

UART版の確認時サイズはFlash 1192バイト、BSS 4バイトでした。以前のLED版`blink.hex`が残っていても、今回書き込むのは **`uart_test.hex`** です。

## 4. SWD書き込み用に配線する

HY0020は3.3Vで給電し、次のように接続します。UART用の配線は外します。

| FT232H端子 | HY0020端子 | 接続方法 |
|---|---|---|
| D0 / ADBUS0 | SWDCLK | 直結 |
| D1 / ADBUS1 | SWDIO | 抵抗を介して接続 |
| D2 / ADBUS2 | SWDIO | 直結 |
| GND | GND | 共通接続 |

```text
FT232H                         HY0020
D0 --------------------------- SWDCLK
D1 ---- [220～470Ω] ----+------ SWDIO
D2 --------------------+
GND -------------------------- GND

3.3V電源の＋ ----------------- 3V3
3.3V電源の－ ----------------- GND
```

**D1は抵抗を介し、D2は同じSWDIOへ直結します。** D1とD2を抵抗なしで短絡しません。抵抗は470Ω固定ではなく、[OpenOCDの接続例](https://github.com/openocd-org/openocd/blob/master/tcl/interface/ftdi/swd-resistor-hack.cfg)では220～470Ω程度です。RESET線は今回の設定では接続しません。

FT232Hは1チャンネルなので、D0とD1の役割は動作モードにより変わります。

| FT232H端子 | SWD用の役割 | UARTモードでの役割 |
|---|---|---|
| D0 | SWDクロック出力 | TX |
| D1 | SWDデータ出力 | RX |
| D2 | SWDデータ入力 | 今回は未使用 |

## 5. SWD接続を確認する

UARTモニターなど、FT232Hを使用する他のプログラムを終了してから実行します。

```powershell
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'init; shutdown'
```

このコマンドは全消去・書き込みを行いません。正常に接続できると、次のようなログが出ます。

```text
Info : clock speed 10 kHz
Info : SWD DPIDR 0x2ba01477
Info : [nrf52.cpu] Cortex-M4 r0p1 processor detected
```

設定ファイル`openocd/ft232h-hy0020.cfg`は、実機で書き込みに成功した **10kHz** を既定値にしています。100kHzでの全消去はタイムアウトしました。速度指定はターゲット設定の読み込み後に置き、上書きされないようにしています。

## 6. 書き込み・照合・起動

### 6.1 通常の書き込み

保護解除済みなら、次のコマンドを使います。

```powershell
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'program build/uart_test/uart_test.hex verify reset exit'
```

このプログラムはFlashの先頭`0x00000000`から動作します。書き込み対象ページの既存内容を置き換えるため、出荷時のSoftDevice・ブートローダー構成での動作は維持されません。

成功時には次のログが出て、プログラムが起動します。

```text
** Programming Finished **
** Verify Started **
** Verified OK **
** Resetting Target **
shutdown command invoked
```

### 6.2 APPROTECTで書き込めない場合のみ：全消去・保護解除

今回の個体では、最初の接続時に以下が表示されました。

```text
nRF52 device has AP lock engaged (see UICR APPROTECT register).
Debug access is denied.
```

この場合は`nrf52_recover`で全消去と保護解除を行います。

**この操作はFlash全域とUICRを消去します。出荷時のブートローダーや設定も失われます。** 元に戻すには対応するファームウェア・設定の復元が必要です。保護が有効な状態では通常のSWD読み出しによるバックアップもできません。消去してよい個体にだけ実行してください。

```powershell
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'init; nrf52_recover; reset halt; program build/uart_test/uart_test.hex verify reset exit'
```

成功すると`device has been successfully erased and unlocked.`に続いて、書き込みと`Verified OK`が表示されます。実行開始時にはまだ保護中なので、途中に保護エラーが表示されても、後続の解除・書き込み・照合まで確認します。

**このPCに接続した個体は、すでに全消去・保護解除・書き込み済みです。** 通常の再書き込みで毎回recoverを実行する必要はありません。消去がタイムアウトした場合は、消去前の内容が残っていると決めつけず、状態を確認してください。

## 7. UART受信用に配線を切り替える

1. OpenOCDが終了していることを確認します。
2. 電源を切ります。
3. SWD用のD0・D1・D2と抵抗の配線を外します。
4. 以下のUART配線に変更します。
5. HY0020を再び3.3Vで給電します。

| HY0020端子 | FT232H端子 | 用途 |
|---|---|---|
| P0.06 / TX | D1 / RX | HY0020からPCへのログ |
| GND | GND | 共通GND |
| P0.08 / RX | 未接続 | このテストは送信専用 |

```text
HY0020                         FT232H
P0.06 / TX ------------------- D1 / RX
GND -------------------------- GND
                               D0：未接続
                               D2：未接続
```

SWD用の抵抗はUART配線では使用しません。HY0020の電源配線は維持します。FT232HのUSBを抜き差ししても、WinUSBドライバーのままで構いません。

## 8. UARTログを受信して動作確認する

```powershell
python tools/uart_monitor.py --seconds 10
```

60秒間確認する場合は`--seconds 60`とします。途中で終了する場合は`Ctrl+C`です。

受信条件はスクリプトに設定済みです。

| 設定 | 値 |
|---|---|
| ボーレート | 115200bps |
| データ長 | 8bit |
| パリティ | なし |
| ストップビット | 1bit |
| フロー制御 | なし |
| 改行 | CRLF |

起動時と、約1秒ごとに次のログを送信します。

```text
HY0020 UART test started (115200 8N1)
HY0020 alive: count=0
HY0020 alive: count=1
HY0020 alive: count=2
```

受信を始めるまでに起動行を見逃しても、カウンターは出続けます。電源を入れ直すとカウンターは0からになります。

実機では、WinUSBのまま次の連続受信を確認しました。

```text
FT232H UART RX: D1, 115200 8N1 (WinUSB)
HY0020 alive: count=399
HY0020 alive: count=400
HY0020 alive: count=401
```

FT232Hはこの構成ではCOMポートとして表示されないため、Tera Term等でCOMポートを選んで受信する方法は使いません。付属の`tools/uart_monitor.py`を使用します。

別の3.3V対応USB-UART変換器を使う場合は、HY0020のP0.06をその変換器のRXへ、GND同士を接続し、上記の受信条件でCOMポートを開くこともできます。

## 9. よくある問題と確認箇所

| 現象・ログ | 確認すること |
|---|---|
| `LIBUSB_ERROR_NOT_SUPPORTED` | FT232HのドライバーがWinUSBか。Zadigで対象のUSB ID `0403:6014`を確認する |
| `unable to open ftdi device` | USB接続、ドライバー、他のプログラムがFT232Hを使用していないか |
| `Error connecting DP: cannot read IDR` | SWDの接続先、D1側の抵抗、D2の直結、共通GND、HY0020の3V3–GND間に約3.3Vあるか |
| `Could not find MEM-AP`とAPPROTECT警告 | 読み書き保護。全消去してよい場合だけ6.2のrecoverを行う |
| `Waiting for chip erase...`の後にrecovery failed | 低速10kHzでの再試行、配線・電源の確認。今回は10kHzで成功した |
| COMポートがない | WinUSB利用時は正常。Pythonツールから受信する |
| `ModuleNotFoundError` | 同じPythonで2.4のライブラリ導入を行ったか。`.tools/python`はGit管理外 |
| `No backend available` | libusb-packageを導入したか。付属スクリプトは同梱DLLのバックエンドを読み込む |
| `Received 0 bytes` | P0.06→D1、共通GND、HY0020の電源、UART版を書き込んだか。SWD配線が残っていないか |
| UARTが文字化けする | 115200bps・8N1、3.3V信号、GND、配線長。起動直後の一部だけでなく継続する行も確認する |

SWD書き込みに戻るときはPythonモニターを終了し、電源を切って4章の配線に戻します。**ドライバーの切り替えは不要です。**

## 10. ファイル構成

```text
HY0020_test/
├─ README.md                    この手順書
├─ skills/hy0020-flash/          Codex用の書き込みスキルと補助スクリプト
├─ sketches/                    サンプルごとに独立したフォルダー
│  ├─ uart_test/                UART単独テスト
│  │  ├─ README.md
│  │  ├─ build.ps1
│  │  ├─ nrf52832.ld
│  │  └─ src/
│  │     ├─ startup.c
│  │     └─ main.c
│  ├─ mouse_uart_ble/           mouse_type8の双方向UART–BLE中継
│  └─ ble_text/                 BLE文字列送信（NUS）
│     ├─ README.md
│     ├─ build.ps1
│     ├─ CMakeLists.txt
│     ├─ prj.conf
│     ├─ boards/fdk/hy0020_dip/  HY0020専用ボード設定
│     └─ src/main.c
├─ openocd/
│  └─ ft232h-hy0020.cfg         FT232Hの抵抗式SWD設定（10kHz）
├─ tools/
│  ├─ uart_monitor.py           共通：WinUSBのまま有線UART受信
│  ├─ ble_monitor.py            PCのBluetoothでNUS送受信
│  ├─ mouse_ble_shell.py        mouse_type8の専用BLEシェル
│  ├─ mouse_ble_gateway.py      オプション：BLEとlocalhost TCPの中継
│  ├─ mouse_gui_ble.py          オプション：既存GUIにBLEポートを追加
│  └─ requirements-ble.txt      PC用BLEライブラリ
├─ build/                       生成物（Git管理外）
│  ├─ uart_test/                UART版HEX・ELF・BIN・MAP
│  ├─ ble_text/zephyr/          BLE版HEX・ELF等
│  ├─ mouse_uart_ble/zephyr/    中継版HEX・ELF等
│  ├─ zephyr-cache/            SDKのビルドキャッシュ
│  └─ legacy_led/              以前のLED版生成物（保存用）
└─ .tools/                      Pythonライブラリ（Git管理外）
   ├─ python/                  有線UART用
   └─ ble_python/              PCのBLE通信用（Python 3.10以上）
```

`main.c`はP0.06の送信だけを使用し、外部32MHz水晶の起動を待ってから文字列を送ります。`printf`や標準ライブラリの代わりにUARTへ直接送信します。今回の動作確認にはBLE通信や省電力評価は含みません。

## 参考資料

- [秋月 HY0020 DIP化キット](https://akizukidenshi.com/catalog/g/g131342/)
- [FDK HY0020データシート](https://www.fdk.co.jp/product/electronic_components/module/pdf/HY0020_Datasheet.pdf)
- [秋月 FT232Hモジュール](https://akizukidenshi.com/catalog/g/g108942/)
- [Adafruit FT232HのUARTピン配置](https://learn.adafruit.com/adafruit-ft232h-breakout/serial-uart)
- [OpenOCD FTDIアダプターの設定](https://openocd.org/doc/html/Debug-Adapter-Configuration.html)
- [PyFtdi UART API](https://eblot.github.io/pyftdi/api/uart.html)
