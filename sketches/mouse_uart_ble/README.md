# mouse_type8用 UART–BLE中継

[mouse_type8 / VScode-gamma](https://github.com/satoshihamasuna/mouse_type8/tree/VScode-gamma)のUARTを、HY0020でBLEへ中継するサンプルです。最初は専用のPCシェルツールでコマンドを送受信し、必要に応じて既存GUIも利用できます。以前のUART版・BLE文字列版は別フォルダーに残しています。

```mermaid
flowchart LR
    PC[PC専用シェル] <-->|BLE NUS| HY[HY0020]
    HY <-->|UART 115200 8N1| Mouse[mouse_type8]
```

デバイス名は **`HY0020-MOUSE`** です。`HY0020-BLE`のHello送信版とは異なり、マウス側から受けたデータだけを通知します。マウスが何も出力していなければ、接続しただけでは文字列は届きません。

## 確認した相手側の仕様

2026年11月3日に、参照ブランチのコミット **`b9387437b0d0cbaa7381d2fd9044add478dad97f`** を調べました。

| 項目 | 仕様 |
|---|---|
| UART | USART1、115200bps、8bit、パリティなし、ストップ1bit、フロー制御なし |
| マウスの端子 | PA9=TX、PA10=RX（MCU端子名） |
| シェルコマンド | ASCII。PC側ツールはコマンド末尾にCR（`0x0D`）を付加 |
| シェル開始 | アイドルのモード選択中に `shell` + CRでシェルへ移行 |
| 既存GUIの送信間隔 | コマンドは設定値（メインGUI既定80ms/文字）、迷路アップロードは2ms/バイト |
| 迷路バイナリ | 32×32=1024バイト。NUL、CR、LF等もデータとして保持 |
| ログバイナリ | `<HH` + 51個の`H`、計106バイト。先頭Magic=`0xA55A`、終端Magic=`0xFFFF` |

参照元：

- [UARTの初期化](https://github.com/satoshihamasuna/mouse_type8/blob/b9387437b0d0cbaa7381d2fd9044add478dad97f/Core/Src/usart.c)
- [UARTの送受信と128バイトのリングバッファ](https://github.com/satoshihamasuna/mouse_type8/blob/b9387437b0d0cbaa7381d2fd9044add478dad97f/Peripheral/Src/communicate.cpp)
- [シェル入出力](https://github.com/satoshihamasuna/mouse_type8/blob/b9387437b0d0cbaa7381d2fd9044add478dad97f/System/Src/myshell.cpp)
- [モード選択とshellコマンド](https://github.com/satoshihamasuna/mouse_type8/blob/b9387437b0d0cbaa7381d2fd9044add478dad97f/System/Src/mode.cpp)
- [既存GUIの文字間隔・改行・バイナリ形式](https://github.com/satoshihamasuna/mouse_type8/blob/b9387437b0d0cbaa7381d2fd9044add478dad97f/tools/myshell_gui.py)

相手のファームウェアやプロトコルは変更していません。ブランチが更新されて通信設定が変わった場合は再確認してください。

## 配線

電源を切って接続します。

| mouse_type8 | HY0020 |
|---|---|
| USART1 TX / PA9 | **P0.08 / RX** |
| USART1 RX / PA10 | **P0.06 / TX** |
| GND | GND |

HY0020の3V3には安定した3.3Vを供給します。信号電圧も3.3V系に揃えます。PA9/PA10はMCUの端子名なので、マウス基板上のコネクター番号は基板の回路図で確認してください。

送信と受信を交差させ、GNDを共通にします。HY0020に5VやRS-232の信号を直接入力しません。給電元は1つにします。SWD用FT232Hは書き込み時に使用し、通常のBLE中継にはPCのBluetoothアダプターを使います。

## ビルドと書き込み

以下はすべてHY0020プロジェクトのルートで実行します。

```powershell
powershell -ExecutionPolicy Bypass -File .\sketches\mouse_uart_ble\build.ps1
```

出力は **`build/mouse_uart_ble/zephyr/zephyr.hex`** です。基板設定はこのサンプル内に独立して置いています。UARTはEasyDMAによる連続受信、コンソール・ログ・起動バナーは無効です。UARTへデバッグ文を混ぜるとバイナリを壊すため、出力していません。

DMA受信バッファは各128バイトです。nRF52832のUARTEは`MAXCNT`が8bitで、1回に指定できる長さは255バイトまでです。初版の256バイト指定はこの上限を超えていたため修正し、上限のビルド時チェックも追加しました。修正後はマウス給電・UART接続でも広告と双方向通信を確認しています。

電源を切ってマウスとのUART配線を外し、[SWD配線](../../README.md#4-swd書き込み用に配線する)に切り替えます。

```powershell
$openocdExe = Join-Path $env:USERPROFILE 'Downloads\xpack-openocd-0.12.0-7-win32-x64\xpack-openocd-0.12.0-7\bin\openocd.exe'
& $openocdExe -f openocd/ft232h-hy0020.cfg -c 'adapter speed 100; program build/mouse_uart_ble/zephyr/zephyr.hex verify reset exit'
```

`Verified OK`とリセット完了を確認します。APPROTECTが有効な場合は、[全消去・保護解除の説明](../../README.md#62-approtectで書き込めない場合のみ全消去保護解除)を確認してください。書き込みスキルからは `-Firmware mouse` で選択できます。

書き込み後は電源を切り、上のマウス用UART配線に戻します。

## 専用PCシェルで確認する

Python 3.10以上とBleakを使います。既存のBLEモニターと同じ依存ライブラリです。この環境のSDK同梱Pythonを使う例を示します。複数のツールチェーンがある場合は、使いたいPythonのパスを指定してください。

```powershell
$blePython = (Get-ChildItem 'C:\ncs\toolchains\*\opt\bin\python.exe' | Sort-Object FullName | Select-Object -First 1).FullName
& $blePython -m pip install --target .tools/ble_python -r tools/requirements-ble.txt
```

PCのBluetoothをオンにして、スマホや他のBLEモニターからは切断します。マウスはアイドルのモード選択状態にしておきます。

### 最初の確認：バージョンを取得

```powershell
& $blePython tools/mouse_ble_shell.py --enter-shell --command 'info ver'
```

`--enter-shell`は、既存GUIと同じく `shell` + CRを送ってから0.5秒待ちます。この開始コマンド自体には応答を要求しません。マウスがすでにシェルに入っていれば、`--enter-shell`は省略できます。

すでにシェルへ入っている場合、`shell`に対して`Unknown command found.`が返ることがあります。続く`info ver`等に正しい応答があれば通信はできています。以後の接続では`--enter-shell`を省略します。

次にコマンド一覧や電圧を確認します。

```powershell
& $blePython tools/mouse_ble_shell.py --command 'help'
& $blePython tools/mouse_ble_shell.py --command 'info battery'
```

PCが接続できるだけでは、UARTの配線やマウス側シェルの動作まで確認できたことにはなりません。応答がない場合は、TX/RX、GND、電源、マウス側のモードを確認してください。

### 対話モード

```powershell
& $blePython tools/mouse_ble_shell.py --enter-shell
```

接続後にコマンドを入力し、Enterで送信します。

```text
info ver
info battery
help
/quit
```

`/quit`はPCツールだけを終了します。マウス側のシェルも終了する場合は、その前に `end exe` を送ります。`Ctrl+C`でもPC側を停止できます。

- コマンド末尾にCRを自動で1個付けます。ASCII以外のコマンドは送信しません。
- `--char-delay 0.08`が既定です。調整済みの相手には`--char-delay 0.005`等を指定できます。
- コマンド指定モードでは、受信後1秒間データが来なくなった時点を応答区切りとします。長い待ちのあるコマンドは対話モードを使うか、`--idle-timeout` / `--response-timeout`を調整してください。
- `--raw-output response.bin`を付けると、受信したバイト列をそのまま新しいファイルに保存します。既存ファイルは上書きしません。画面表示はUTF-8としてデコードするので、バイナリの確認には保存ファイルを使ってください。
- コマンドは自動で繰り返したり、接続し直して再実行したりしません。

## 中継の動作と制限

- CR/LF、NUL、エスケープシーケンスを変換せず、そのまま転送します。BLEパケットの境界にUARTプロトコル上の意味はありません。
- HY0020→マウスは既定で各バイトの間に最低2msの待ちを入れます。相手側の小さい受信バッファへ一括送信しないためです。`CONFIG_MOUSE_UART_BYTE_GAP_US`で変更できます。実際の間隔にはOSのスケジューリング時間も加わります。
- マウス→PCは16KBのリングバッファから、MTUに収まる長さに分割して通知します。受信側のMTUは247を推奨します。WindowsではOSのBLEスタックがMTUを交渉します。
- 無接続時や通知無効時のUART出力は保存・再生しません。コマンドは接続後に送ってください。
- 受信バッファのあふれ、UARTエラー、通知送信の継続失敗を検出するとBLEを切断します。PCツールはステータスも確認します。失敗した転送は正常完了として扱わず、再接続後に状態を確認してください。
- BLE→UARTはWrite Requestを使います。キューが満杯ならATTエラー`0x11`で拒否し、PCツールは拒否されたチャンクだけを最大10秒間再試行します。結果の分からないタイムアウトは再送しません。
- 切断時にまだUARTへ送っていないキューは破棄します。すでにマウスへ届いた文字を取り消すことはできません。
- 接続は1台、ペアリング・暗号化を要求しない開発用サンプルです。

NUSのサービス／TX／RX UUIDは文字列サンプルと同じです。追加の読み取り特性 `6e400004-b5a3-f393-e0a9-e50e24dcca9e` は、UART破棄バイト数、UARTエラー数、キュー満杯回数、BLE送信エラー数の4個のlittle-endian uint32です。ステータスはUARTの中には出しません。

## 既存GUIを使う場合（オプション）

専用シェルでの確認後、既存GUIを使う場合は以下の2つを別ターミナルで動かします。元のmouse_type8のソースを変更する必要はありません。

ターミナル1：BLE–localhost中継を起動します。

```powershell
& $blePython tools/mouse_ble_gateway.py
```

`Ready: socket://127.0.0.1:8765`が表示されたら、ターミナル2でGUIを起動します。`$mouseRepo`は手元のVScode-gammaチェックアウトに合わせてください。

```powershell
$mouseRepo = (Resolve-Path '..\mouse_type8').Path
python tools/mouse_gui_ble.py --mouse-repo $mouseRepo --gui main
```

GUIのPortで **`BLE`** を選び、Baudを115200にしてConnectします。`--gui`は`main` / `debug` / `turnpattern` / `search`を選べます。タイムアウトは大きめ（例：120秒）にしてください。通常のCOMポートも元の処理へ渡します。

GUI用Pythonにはpyserial・numpy・Tkinterと、選んだGUI自身の依存関係が必要です。この環境では通常のPythonに導入済みでした。SDK同梱PythonはTkのDLLが不足していたため、BLE用PythonとGUI用Pythonを別プロセスに分けています。

中継はlocalhostのみで待ち受け、GUIは1クライアントです。GUIが切断すると中継も終了します。再接続する場合は中継を起動し直してください。これはOSの仮想COMポートを新設する仕組みではなく、起動したGUIプロセス内の`BLE`というポートだけを置き換える方式です。

## 検証範囲

- nRF Connect SDK v3.4.1でコンパイル・リンク・HEX生成を確認。DMA受信バッファ修正後はFlash 116776バイト / 512KB、RAM 50792バイト / 64KB。
- ホスト側では、ASCII+CR、20バイト分割、ATTキュー満杯時の再試行、結果不明時の再送抑止を確認。
- localhost上で1024バイトの迷路、106バイトのログ、NULを含むデータが変化せず通ることを確認。BLEの代わりに模擬相手を使った双方向転送も確認。
- 2026年11月3日に実機へ書き込み、`Verified OK`とリセット完了、BLE広告 **`HY0020-MOUSE`** の検出を確認しました。APPROTECTを10kHzで解除した後、100kHzで書き込みました。
- 初版はマウス接続時にBLE広告が見えなくなる症状を確認しました。同じ給電環境で`ble_text`はPCへの10行・190バイトのBLE送信に成功しています。
- DMA受信バッファ修正版も2026年11月3日に書き込み・照合・リセットが成功しました。FT232H給電では広告検出・BLE接続・ステータス読み取りが成功し、破棄バイト数・UARTエラー数・キュー満杯回数・BLEエラー数はすべて0でした。
- 続いてマウス給電・UART接続へ戻し、PCからBLE越しに`info ver`、`info battery`、`help`を送受信できました。バージョンは`Version 0.0.0`、マウスのバッテリーは`voltage_mv:7672 limit_mv:7000 status:OK`でした。受信した生データ899バイト内に各応答があることを確認し、通信後の4個のエラーカウンターもすべて0でした。PC側のBLE接続は正常終了しています。
- **実マウスを使った迷路・ログのバイナリ転送、走行コマンドは未確認**です。

テスト：

```powershell
& $blePython -m unittest discover -s tests -v
```
