#!/usr/bin/env bash
# Acer Shadow SH16-41 Linux 控制台 —— 一键安装
#
# 做四件事：
#   1. 取 TUXEDO tuxedo-drivers 源码，打上本项目补丁
#   2. 组装成 DKMS 包（含本项目自己的 clevo_ecwin 模块），编译并签名
#   3. 装控制台程序、权限脚本、systemd 服务、udev 规则
#   4. 设为开机加载
#
# 需要在仓库根目录执行，且要求 root。
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG=clevo-ec
VER=1.0
SRC=/usr/src/$PKG-$VER
BUILD=/tmp/clevo-ec-build
TD_REF=v4.14.0
MOK_DIR=/var/lib/shim-signed/mok

log()  { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m错误:\033[0m %s\n' "$*" >&2; exit 1; }

[[ $EUID -eq 0 ]] || die "请用 sudo 运行"

log "检查依赖"
for pkg in dkms git make gcc; do
    command -v "$pkg" >/dev/null || die "缺少 $pkg，先装：apt install dkms git build-essential"
done
[[ -d /lib/modules/$(uname -r)/build ]] || die "缺少内核头文件：apt install linux-headers-$(uname -r)"

log "获取 tuxedo-drivers $TD_REF"
rm -rf "$BUILD"
git clone --depth 1 --branch "$TD_REF" \
    https://gitlab.com/tuxedocomputers/development/packages/tuxedo-drivers.git "$BUILD"

log "应用补丁"
patch -d "$BUILD" -p1 < "$REPO_DIR/patches/tuxedo-drivers.patch"

log "组装 DKMS 源码包到 $SRC"
rm -rf "$SRC"
mkdir -p "$SRC"
( cd "$BUILD/src" && tar cf - --exclude='*.ko' --exclude='*.o' \
    --exclude='*.mod*' --exclude='modules.order' --exclude='.*.cmd' . ) | tar xf - -C "$SRC"
cp "$REPO_DIR/dkms/clevo_ecwin.c" "$REPO_DIR/dkms/dkms.conf" "$SRC/"
# Kbuild 里加进我们自己的模块
printf 'obj-m += clevo_ecwin.o\n%s\n' "$(cat "$SRC/Kbuild")" > "$SRC/Kbuild.new"
mv "$SRC/Kbuild.new" "$SRC/Kbuild"

log "配置 DKMS 签名密钥"
mkdir -p /etc/dkms/framework.conf.d
if [[ -f $MOK_DIR/MOK.priv && -f $MOK_DIR/MOK.der ]]; then
    cat > /etc/dkms/framework.conf.d/99-acer-console-signing.conf <<EOF
mok_signing_key="$MOK_DIR/MOK.priv"
mok_certificate="$MOK_DIR/MOK.der"
EOF
    log "使用已有 MOK 密钥"
else
    log "警告：没找到 $MOK_DIR/MOK.{priv,der}，模块将不被签名"
    log "      Secure Boot 开启时无法加载，需要先生成并登记 MOK"
fi

log "DKMS 编译安装（首次约 1-2 分钟）"
dkms remove -m "$PKG" -v "$VER" --all >/dev/null 2>&1 || true
dkms add -m "$PKG" -v "$VER"
dkms build -m "$PKG" -v "$VER"
dkms install -m "$PKG" -v "$VER" --force
depmod -a

log "安装控制台与工具"
TARGET_USER="${SUDO_USER:-$USER}"
TARGET_HOME="$(getent passwd "$TARGET_USER" | cut -d: -f6)"
if [[ -n $TARGET_HOME && -d $TARGET_HOME ]]; then
    install -Dm755 "$REPO_DIR/console/acer-console" "$TARGET_HOME/bin/acer-console"
fi
install -Dm755 "$REPO_DIR/console/acer-console" /usr/local/bin/acer-console
cc -O2 -o /usr/local/bin/acer-fan "$REPO_DIR/tools/fan.c"
install -Dm755 "$REPO_DIR/scripts/acer-perms.sh" /usr/local/sbin/acer-perms.sh
install -Dm644 "$REPO_DIR/systemd/acer-console-perms.service" \
    /etc/systemd/system/acer-console-perms.service
install -Dm644 "$REPO_DIR/udev/99-acer-console.rules" /etc/udev/rules.d/99-acer-console.rules

log "开机自动加载"
printf 'clevo_ecwin\nclevo_acpi\ntuxedo_io\n' > /etc/modules-load.d/clevo.conf

log "启用权限服务"
systemctl daemon-reload
systemctl enable --now acer-console-perms.service

log "完成。"
echo
echo "  控制台：acer-console（或应用列表「Acer Shadow 控制台」）"
echo "  命令行：acer-fan show / set <0-255> / auto"
echo
echo "  注意：风扇值是 0-255，不是百分比；要满速用 255。"
