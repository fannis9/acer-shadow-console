# 图标

- 图形：**Lucide** 的 `gauge`（[lucide.dev](https://lucide.dev)），**ISC 许可**
- 底板与配色：本项目自制（石墨渐变圆角方块）
- 渲染：`acer-console.svg` → 512px 主图 → Lanczos 缩放到 16/32/48/64/128/256

尺寸文件都是预渲染的 PNG，安装时直接复制，不需要 ImageMagick。

重新生成（可选）：

```bash
# 用任意渲染器把 svg 出成 512，再缩放
convert base512.png -filter Lanczos -resize 128x128 acer-console-128.png
```
