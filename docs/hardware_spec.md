# Hardware Specification (ESP32)
- Target Board: ESP-WROOM-32
- Framework: Arduino (PlatformIO)
- Pin Map:
    - GPIO0 : Unused. Digital outout. set to 0.
    - GPIO2 : Digit1 output. Digital Output. Active High.
    - GPIO4 : Digit2 output. Digital Output. Active High.
    - GPIO5 : Latch Enable output for HV509. Digital Ooutput. Active Low.
    - GPIO12 : SPI MISO signal for BME280.
    - GPIO13 : SPI MOSI signal for BME280.
    - GPIO14 : SPI SCK signal for BME280.
    - GPIO15 : SPI CS signal for BME280. Active High.
    - GPIO16 : Digit3 output. Digital Output. Active High.
    - GPIO17 : Digit4 output. Digital Output. Active High.
    - GPIO18 : CLK signal for HV509.
    - GPIO19 : POL_N signal for HV509. Active Low.
    - GPIO21 : Digit5 output. Digital Output. Active High.
    - GPIO22 : Digit6 output. Digital Output. Active High.
    - GPIO23 : DOUT signal for HV509.
    - GPIO25 : Interrupt signal for RTC(RX8900CE). Active Low.
    - GPIO26 : Nixie-tube current control signal. Analog output.
    - GPIO27 : High Voltage Enable signal for Nixie-tube. Active High.
    - GPIO32 : I2C SCL signal for RTC(RX8900CE) and FRAM(FM24CL64J).
    - GPIO33 : I2C SDA signal for RTC(RX8900CE) and FRAM(FM24CL64J).
    - GPIO34 : Low voltage detect signal. Digital input. Active Low.
    - GPIO36 : Ambient light sensor. Analog input.
    
- Other ICs:
    - RTC : RX8900CE
    - FRAM : FM24CL64J
    - BME280
    - Nixie-Tube Cathode driver : HV509
    - Nixie-Tube Anode driver : PhotoCoupler(TLP188)x6

- I2C bus
    - Device : RX8900CE, FM24CL64J
    - Address :
        - RX8900CE : 0x32
        - FM24CL64J : 0x50

- SPI bus
    - Device : BME280
    - Address : - 

- Nixie-Tube Driver
    - Cathode driver : HV509
        - Active Low logic.
        - Pin Map:
            - HV_OUT1 : Cathode 1
            - HV_OUT2 : Cathode 2
            - HV_OUT3 : Cathode 3
            - HV_OUT4 : Cathode 4
            - HV_OUT5 : Cathode 5
            - HV_OUT6 : Cathode 6
            - HV_OUT7 : Cathode 7
            - HV_OUT8 : Cathode 8
            - HV_OUT9 : Cathode 9
            - HV_OUT10 : Cathode 0
            - HV_OUT11 : Cathode Left dot
            - HV_OUT12 : Cathode Right dot
            - HV_OUT13-16 : no connection
    - Anode driver : PhotoCoupler(TLP188)x6
        - Digit1 : Most left digit
        - Digit6 : Most right digit
    - Current control : 
        - Output current = Vadc/100 [mA]. Vdac(max) is 3.3V. So max output current is 3mA.

## Software Requirements
1. digit1-6をすべてLとし、アノードの出力を遮断した状態とする
2. 高圧電源のイネーブルをHにし、高圧電源をイネーブルとする
3. 100ms程度待つ（高圧電源の立ち上げ待ち）
4. カソードをすべて消灯(Hレベル)に設定する
5. アノードの出力電流を3mAに設定する

### ダイナミック点灯処理
初期設定を行った上で、以下の処理を繰り返し行います。
全桁の表示は1/60秒としてください。
1. 一度すべてのアノードの出力を遮断する
2. ゴースト対策のブラン時間を設ける（100us程度?）
3. 点灯桁の表示数値に対応したカソードをONする
4. 点灯桁のアノードをONする
5. 一定時間待機する
6. 次の桁に対して1.に戻って実行する