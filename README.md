# Acer Shadow SH16-41 Linux 控制台

给这台 **Acer 暗影骑士（Shadow SH16-41）** 做一个能在 Linux 下用的控制台。

厂商的 NitroSense 只有 Windows 版，但硬件控制通道其实一直开着——它藏在 ACPI 的一个
Clevo 通用接口后面。这个项目把那层挖出来，用开源驱动接管，还原了风扇、键盘灯、
实时温度这些功能。

> 只验证过 **Acer Shadow SH16-41**（Clevo 代工，`CLV0001` ACPI 设备）。
> 别的机器不一定适用，尤其 `patches/` 里的白名单补丁会让驱动在未测试机型上也尝试加载。

## 原理

```
GTK 控制台 (Python)
  └─ /dev/tuxedo_io  ioctl ──┐
                             ├─ clevo_acpi 内核模块
  └─ /sys/class/leds/… ──────┘        │  acpi_evaluate_dsm(uuid, rev, cmd, pkg)
                                      ▼
                         \_SB.DCHU / \_SB.WMI  (ACPI `_DSM`, UUID 93f224e4-…)
                                      ▼
                                  EC 嵌入式控制器
```

Windows 那边是 `WinUI 应用 → WMI → wmiacpi.sys → _DSM`，这里少了两层，直接在内核调 `_DSM`。

关键发现：ACPI 设备 HID 是 `CLV0001` —— **CLV = Clevo**，不是 Acer 专属。
所以 `acer-wmi`（找 Acer GUID）永远匹配不上，而 TUXEDO 的开源 `tuxedo-drivers`
恰好认的就是这个 HID。

## 功能

| 功能 | 实现 | 状态 |
|---|---|---|
| 实时温度（CPU/GPU，读 EC 内部传感器） | `FANINFO1` ioctl | ✅ |
| 风扇转速（0–255）与自动/手动 | `FANSPEED_VALUE` / `FANSPEED_AUTO` | ✅ |
| 性能模式（节能/平衡/性能） | ACPI `platform_profile` | ✅ |
| 键盘灯颜色 / 亮度 | `rgb:kbd_backlight` LED | ✅ 单区 |
| 键盘灯三通道增益校准 | 应用层 | ✅ |
| 风扇曲线编辑 | `FANB` 表（0x300） | ❌ 未实现，见下文 |
| 键盘背光超时 | EC `OPT 0x79` 子命令 | ❌ 未逆向出来 |

## 安装

前提：Ubuntu/Debian，内核头文件、DKMS、Node 不需要。

```bash
sudo apt install dkms linux-headers-$(uname -r) python3-gi python3-gi-cairo gir1.2-gtk-4.0 gir1.2-adw-1
sudo ./install.sh
```

`install.sh` 会：

1. 克隆 TUXEDO `tuxedo-drivers`（GPL-2.0）到临时目录
2. 打上 `patches/tuxedo-drivers.patch`（三处改动，见下）
3. 组装 DKMS 包并安装、签名（用本机 MOK）
4. 装入 `acer-console-perms.service`、udev 规则、控制台程序

**Secure Boot 注意**：模块要用你的 MOK 密钥签名才能加载。脚本会复用
`/var/lib/shim-signed/mok/MOK.{priv,der}`；没有的话需要自己生成并 `mokutil --import`。

## 用法

```bash
acer-console            # 图形控制台（应用列表里叫「Acer Shadow 控制台」）
sudo acer-fan show      # CPU/GPU 温度、风扇值（0–255）
sudo acer-fan set 200 200
sudo acer-fan auto      # 交还固件
```

## 三个坑（都是踩出来的）

1. **风扇值是 0–255，不是百分比**。写 `100` 只有 39% 的力，要 `255` 才是满速。
2. **`SET_FANSPEED_AUTO` 的参数是风扇位掩码**。传 `1` 只松开 0 号风扇，另一路会卡在 255
   一直全速转；必须传 `0xFF`。
3. **键盘灯写颜色后必须再抖一次 `brightness`**。驱动的颜色推送挂在 `brightness_set` 上，
   只改 `multi_intensity` 不会下发。

## 补丁说明

`patches/tuxedo-drivers.patch` 对 TUXEDO 驱动做了三处改动：

| 文件 | 改动 | 原因 |
|---|---|---|
| `tuxedo_compatibility_check.c` | `tuxedo_is_compatible()` 直接返回 true | 跳过 TUXEDO 机型白名单 |
| `clevo_leds.h` | `color_scaling()` 置空 | 原版把红压到 180/255、蓝压到 200/255，颜色偏 |
| `tuxedo_io/tuxedo_io.c` | 注册时加 `devnode` 回调 → 0666 | 默认设备节点 root-only，普通用户用不了 |

## 风扇曲线为什么没做

`FANB`（EC 窗口 0x300，112 字节）是自定义风扇曲线的存储区，但这台机器上**全是 `0xFF`**
——固件当前没启用自定义曲线，走内置自动策略。

没有一份填过真实数据的样本，就无法确定那 112 字节里「起始温度 / 结束温度 / 转速」
是怎么排布的。盲写一张结构未知的表进 EC，风险远大于收益。

想推进的话：在 Windows 的 NitroSense 里设一条自定义曲线 → 切回 Linux →
`dd if=/dev/clevo_ecwin bs=1 skip=768 count=112` 把表 dump 出来，结构一对上就能做编辑器。

## 目录

```
console/acer-console     GTK4 控制台（Python）
tools/fan.c              ioctl 命令行工具
dkms/clevo_ecwin.c       只读映射 EC 内存窗口的内核模块
dkms/dkms.conf           DKMS 包描述
patches/                 对 tuxedo-drivers 的补丁
systemd/ scripts/ udev/  设备权限相关
docs/FINDINGS.md         逆向过程与寄存器表
```

## 许可

本项目代码 GPL-2.0-or-later：它包含/依赖 TUXEDO Computers 的 GPL-2.0 内核驱动，
分发时必须保持同许可。`patches/` 里的改动同样属于 GPL-2.0 衍生作品。

驱动来自 [TUXEDO Computers](https://gitlab.com/tuxedocomputers/development/packages/tuxedo-drivers)，
EC 接口协议来自该项目的 `clevo_interfaces.h`。NitroSense 是 Acer 的商标，
本项目与 Acer 无关。
