# 逆向过程与寄存器表

## 入口

DSDT 里两个设备：

| 设备 | HID | 作用 |
|---|---|---|
| `\_SB.DCHU` | `CLV0001` | `_DSM` 主分派 |
| `\_SB.WMI` | `PNP0C14` | 通用 WMI 映射（`_WDG` 只有 `ABBC0F6B/6C/6D` 三个通用 GUID） |

`_DSM` UUID：`93f224e4-fbdc-4bbf-add6-db71bdc0afad`

`_DSM(Arg0=UUID, Arg1=rev, Arg2=命令, Arg3=Package{参数})` 内部按 `Arg2` 分派：

```
0x02 → CEVT   0x04 → PEVT   0x0C → DEVT
0x0D → EEVT   0x0E → FEVT   0x11 → AMVT
其余 → ZEVT
```

`CLV` = Clevo。所以 `acer_wmi`（按 Acer 专属 GUID 找）永远匹配不上，
而 TUXEDO 的 `tuxedo-drivers` 认 `CLV0001`，正好可用。

## EC 内存窗口

| 区域 | 物理地址 | 长度 |
|---|---|---|
| `RAM` | 0xFE708100 | 0x1000 |
| `RAM2` | 0xFE708300 | 0x0100（在 RAM 的 0x300 处） |

字段（偏移来自 DSDT 的 Field 声明）：

| 字段 | 偏移 | 含义 |
|---|---|---|
| `FAN0`/`FAN1` | 0x02（位） | 风扇存在位 |
| `RPM3` | 0xE0 | 转速（16 位，实测恒为 0，未启用） |
| `XFAN` | 0xE7 | 手动风扇值（0–255） |
| `FANT` | 0x94 | 风扇温度 |
| `KLCR/G/B` | 0x280 | 左区 RGB |
| `KMCR/G/B` | 0x283 | 中区 RGB |
| `KRCR/G/B` | 0x286 | 右区 RGB |
| `KBNS` | 0x289 | — |
| `FANC` | 0x28A | 风扇控制状态 |
| `KBLD` | 0x28B | 键盘灯亮度 |
| `FANB` | 0x300 | **风扇曲线表，112 字节** |

注意：运行时写颜色/风扇后，0x280 那组寄存器**不会跟着变**（实测恒为 `00 00 C8`），
它更像固件启动时写入的默认快照，不能用来验证运行时状态。

## EC 命令表（来自 clevo_interfaces.h）

| 命令 | 值 | 参数 |
|---|---|---|
| `GET_FANINFO1/2/3` | 0x63 / 0x64 / 0x6e | 无 |
| `GET_EVENT` | 0x01 | 无 |
| `GET_SPECS` | 0x0D | 无（返回 buffer） ← 本机失败，会回退 |
| `GET_BIOS_FEATURES_1` | 0x52 | 无 |
| `GET_BIOS_FEATURES_2` | 0x7A | 无 |
| `SET_FANSPEED_VALUE` | 0x68 | 三字节：风扇0、风扇1、风扇2（0–255）|
| `SET_FANSPEED_AUTO` | 0x69 | **风扇位掩码**，`0xFF` = 全部 |
| `SET_KB_RGB_LEDS` | 0x67 | 高字节 `0xF0/0xF1/0xF2` 区号 + 颜色 |
| `SET_KB_WHITE_LEDS` | 0x27 | 亮度 |
| `OPT` | 0x79 | 子命令 `0x19` = 性能模式 |

`GET_FANINFO1` 返回 32 位：`[CPU温度, GPU温度, 风扇值]`（各 8 位）。

## 失败过的路

- `acer_wmi`：`No or unsupported WMI interface` —— 本机没有 Acer 专属 WMI GUID
- `Linuwu-Sense`（逆向 PredatorSense 的内核模块）：同样死在 WMI GUID 匹配
- `/dev/mem` 直读 EC 窗口：被 Secure Boot lockdown 拒绝
- `acpi_call` 调 `ZEVT 0x79`：该方法需要 Buffer 参数，acpi_call 只支持整数
- 强改三区 RGB：EC 只认 `ZONE_0`，其余区命令被忽略 —— 键盘**物理单区**
