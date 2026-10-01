1：稳定实现了ONENET上报及接收
2：稳定实现了USB打印及debug
debug流程

## 命令一览（大小写不敏感，行尾 `\r`/`\n`）

| 命令                      | 示例                     | 效果                                     |
| :------------------------ | :----------------------- | :--------------------------------------- |
| `read <名>`               | `read temperature`       | 打印变量值                               |
| `write <名> <值>`         | `write temperature 56.5` | 写入变量值                               |
| `<PBxx> Press <ms>`       | `PB12 Press 2000`        | PB12 按下，2000ms 后自动松开             |
| `press <PBxx> <ms>`       | `press PB13 500`         | 同上                                     |
| `write <PBxx> press <ms>` | `write PB14 press 800`   | 同上                                     |
| `release <PBxx>`          | `release PB12`           | 立即松开                                 |
| `write <PBxx> <0/1>`      | `write PB12 1`           | 直接设置按键状态（不自动松开）           |
| `adc on|off`              | `adc off`                | 关掉 ADC 刷新，让 write 的传感器值保持住 |
| `vars`                    | `vars`                   | 打印全部变量 + 3 个按键状态              |
| `help`                    | `help`                   | 帮助                                     |

## 使用效果示例

```
> read fanS
[DBG] fanS = 0.00
> write fanS 1
[DBG] fanS = 1.00
> PB12 Press 2000
[DBG] PB12 PRESS, auto release in 2000 ms
[DBG] PB12 RELEASE (auto)          <- 约 2s 后
> read PB12
[DBG] PB12 = 0 (RELEASE)
```

你现在要帮我实现业务逻辑
PB12是index增加按键
PB13是index减少按键
PB14是确认按键
PB15是返回按键

主界面有四个选项，每个选项一行
1：测量界面
2：曲线界面
3：阈值界面
4：控制界面
进去每个界面（不同index就可以进不同界面，进入界面后index进行清0）
测量界面---实现测量数值显示
曲线界面
进来也有四个选项
1：温度曲线
2：湿度曲线
3：光照曲线
4：水位曲线 通过index和确认按键进行不同内容的曲线显示
阈值界面
四个阈值的上下限，假如都有上下限，那么就是8个变量，每行一个变量，index为0~7
短按PB12和PB13是index增加和减少，
如果是PB12长按且在阈值界面，那么就是对应增加
如果是PB13长按且在阈值界面，那么就是对应减少

控制界面 --- 执行机构
1：风扇
2：光照
3：灌溉
如果是PB12长按且在控制界面，那么就是对应打开
如果是PB13长按且在控制界面，那么就是对应关闭
通过确认和返回进行界面之间的切换。

之前的对应IO口是

USB PA11 PA12  

 SWD PA13 PA14

OLED PB6 PB7

KEY1~KEY4 PB12~PB15

ESP8266  PA2 PA3

ADC PA0 PA1 PA4 PB0

增加DHT11   --- PB1

增加超声波   T---PB8   E----PB9

增加光照传感器   PB6 PB7

增加gps解析 ---  USART3

增加MPU6050角度解析

增加flash上电读取阈值

增加RC522, PA15 --- SDA  PA5---CLK  PA6 MISO PA7 MOSI  IRQ和复位都接上拉

