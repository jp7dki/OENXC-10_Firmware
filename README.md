# OENXC-10 Nixie Clock Firmware - 完全開発仕様書（Generative AI 向け）

このドキュメントは、生成AIがゼロから全く同じプログラムを再現・実装できるように、ESP32ベースのニキシー管時計（OENXC-10）ファームウェアの「ハードウェア構成」「ソフトウェアアーキテクチャ」「内部ロジック」「依存ライブラリ」を完全に網羅した詳細な仕様書です。

---

## 1. ハードウェア構成とピンアサイン

本プロジェクトはESP32 (esp32dev) をベースとしており、各種ペリフェラルが以下のGPIOに接続されています。
実装時は `pin_config.h` にこれらを定義してください。

### ニキシー管駆動系 (HV509 シフトレジスタ・マルチプレクサ)
- `PIN_DIGIT1` (2), `PIN_DIGIT2` (4), `PIN_DIGIT3` (16), `PIN_DIGIT4` (17), `PIN_DIGIT5` (21), `PIN_DIGIT6` (22) : 各桁のアノード制御
- `PIN_HV509_LE` (5) : Latch Enable (Active Low)
- `PIN_HV509_CLK` (18) : Clock
- `PIN_HV509_POL_N` (19) : Polarity (Active Low)
- `PIN_HV509_DOUT` (23) : Data Out
- `PIN_NIXIE_CUR_CTRL` (26) : ニキシー管の電流制御用DAC出力
- `PIN_HV_EN` (27) : 高電圧電源のEnable (Active High)

### センサー・通信系
- **SPI (BME280用, HSPI)**: `PIN_SPI_MISO` (12), `PIN_SPI_MOSI` (13), `PIN_SPI_SCK` (14), `PIN_SPI_CS_BME280` (15)
- **I2C (RTC RX8900CE & FRAM FM24CL64J用)**: `PIN_I2C_SCL` (32), `PIN_I2C_SDA` (33)
- **その他**: `PIN_RTC_INT` (25, Active High割り込み), `PIN_LOW_VOLT_DET` (34, Active Low), `PIN_AMBIENT_LIGHT` (36, ADC入力)

---

## 2. 開発環境と依存ライブラリ
`platformio.ini` にて以下を指定します。
- **Platform**: `espressif32`
- **Board**: `esp32dev`
- **Framework**: `arduino`
- **Dependencies**:
  - `adafruit/Adafruit BME280 Library @ ^2.2.4`
  - `adafruit/Adafruit Unified Sensor @ ^1.1.14`
  - `mathieucarbou/ESPAsyncWebServer @ ^3.2.4`
  - `mathieucarbou/AsyncTCP @ ^3.2.14`
  - `bblanchon/ArduinoJson @ ^7.0.4`

---

## 3. ソフトウェアアーキテクチャ (FreeRTOS)

ESP32のデュアルコアを活用し、`main.cpp` で以下の3つのタスクを生成します。

### Task 1: `commTask` (Core 0, Priority 3)
- **目的**: ネットワーク処理 (WiFi, ESPAsyncWebServer, NTP同期)
- **処理内容**:
  1. WiFiManager/SmartConfig 相当の接続処理（または固定アクセスポイントへの接続）。
  2. Webサーバーの初期化（ポート80）。
  3. `sntp_set_time_sync_notification_cb` を用いたNTP同期コールバックの登録。
  4. ループ内では `vTaskDelay` を長め（100ms〜）に取り、バックグラウンドのWiFiイベントを処理。
- **NTP同期の条件**: NTP同期コールバック内において、`sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED` かつ `timeinfo.tm_year > 120` (2020年以降) を満たした場合のみ、RTC (`rx8900_setTime()`) に時刻を書き込む。

### Task 2: `sensorTask` (Core 1, Priority 1)
- **目的**: センサーのポーリングとRTCからの時刻読み出し。
- **処理内容**:
  1. I2Cバスを使用するため `i2cSpiMutex` (Semaphore) を取得して通信。
  2. RTCのINTピン (GPIO 25) の立ち下がりエッジ割り込み (`FALLING`) に `rtcInterruptISR` をアタッチ。ISR内では `vTaskNotifyGiveFromISR(sensorTaskHandle, &xHigherPriorityTaskWoken)` を呼び出し。
  3. ループ内は `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)` で待機（基本は1秒周期）。
  4. RTCから時刻を取得。
  5. 動作モードが `APP_MODE_RANDOM_DEMO` (乱数デモモード) の場合:
     - `triggerDemoFlag` が `true` になっていれば、`triggerDemoEffect = true` とし、0〜9の乱数を6桁分生成して送信。
  6. 通常モード (`APP_MODE_CLOCK`) の場合:
     - 時刻を各桁（0〜9）に分解（HHMMSS）。
     - 毎時0分0秒に `triggerHourlyEffect = true` のフラグを立てる。
  7. キュー `displayDataQueue` (型: `DisplayData`) にデータを `xQueueSend` する。
  8. BME280の温湿度気圧データ、ADC (GPIO 36) の照度データを読み取る。

### Task 3: `displayTask` (Core 1, Priority 2)
- **目的**: ニキシー管のダイナミック点灯と、ステートマシンによるアニメーション管理。
- **処理内容**:
  1. 優先度が高く、数ミリ秒単位の厳密なループを行う。
  2. `xQueueReceive(displayDataQueue, &incomingData, 0)` で非ブロッキング受信。
  3. ステートマシン (`pwrState`) に基づく処理を行う。
     - `POWER_ON`: 通常表示。
     - `HOURLY_SHUFFLE`: 毎時0分のアニメーション（3000ms間、全桁を2ms周期で乱数表示 `shuffleDigits` に切り替え）。
     - `HOURLY_RESOLVE`: 右（一番下の桁）から0.5秒ごとに予測した目標の数字へ確定していく（計3000ms）。確定後強制的に `fadeProgress=1.0` をセット。
     - `DEMO_SHUFFLE`: 乱数デモ（5000ms間シャッフル）。
     - `DEMO_RESOLVE`: 毎時エフェクト同様に右から確定し、強制的に `fadeProgress=1.0` をセット。
  4. 表示データの送信: `shiftOutHV509` でカソードをセットし、対象の `PIN_DIGITx` を HIGH にして所定時間点灯させ、OFFにする。これを6桁分高速に回す。
  5. クロスフェード: `fadeProgress` (0.0 〜 1.0) を用いて、旧数字と新数字の点灯割合をフレームごとに計算し、徐々に切り替える。

---

## 4. API エンドポイント (`src/web_server.cpp`)

`ESPAsyncWebServer` を用いて、以下のエンドポイントを実装します。全て `application/json` ベースで通信します。

- **GET `/api/status`**
  - レスポンス例: `{ "temp": 25.4, "hum": 50.2, "pres": 1013.2, "ambient": 2048, "ssid": "MyWiFi", "ntpSynced": true, "lastNtpSync": "2026-09-23 12:34:56" }`
- **GET `/api/settings`**
  - 設定値 (appModeなど) を返す。
- **POST `/api/settings`**
  - リクエストから設定値を受け取りNVS領域に保存する。
- **POST `/api/time`**
  - リクエスト例: `{ "year": 2026, "month": 9, "day": 23, "hour": 15, "min": 30, "sec": 0, "week": 3 }`
  - `rx8900_setTime()` を呼び出しRTCを手動で上書きする。
- **POST `/api/demo_shuffle`**
  - `triggerDemoFlag = true;` をセットし、`xTaskNotifyGive(sensorTaskHandle);` を呼び出して `sensorTask` を即座に起こす。

---

## 5. WebUI 実装 (`include/web_assets.h`)

UIはSPAとして機能する HTML/CSS/JS を C++ の Raw String Literal (`R"=====( ... )====="`) で定義します。

- **UIコンポーネント**:
  1. **Network & System Status**: WiFi SSIDとNTP同期時刻を表示。
  2. **手動時刻同期パネル**: 「Sync Time with this Device」と「Set Custom Time」。
  3. **Operation Mode**: `appMode` (0=Clock, 1=RandomDemo) の `<select>` タグ。
  4. **Random Demo Control**: `appMode == 1` の時のみ表示される「Trigger Random Shuffle」ボタン。押下時に `/api/demo_shuffle` を `fetch` POST。

---

## 6. NVS (Non-Volatile Storage) の管理 (`src/settings.cpp`)

設定値は `Preferences.h` を用いて内蔵フラッシュメモリに保存します。
- 保存項目: WiFi情報, アニメーション設定, 省電力スケジュール, `appMode` など。
- RTCバックアップ用FRAM (`FM24CL64J`) に累積稼働時間をインクリメントして記録します。
