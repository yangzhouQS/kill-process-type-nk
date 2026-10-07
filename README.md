# kill-process-type-nk

Node/Python 进程终结者 —— Windows 下的进程/端口管理工具（raylib 2D UI 重写版）。

识别并管理本机的 **Node.js / Python** 相关进程：一键终止、端口占用排查、孤儿进程清理、AI 辅助分析与风险评级。

![截图占位](docs/screenshot.png)

## 功能

### 进程管理
- **全部进程** 平铺视图：名称/PID/父PID/内存/**CPU%**/类型/可执行路径/命令行/AI风险
- **Node·Python** 页签：只看目标进程
- **进程树**：父子层级展示（子树内存合计），支持折叠/展开
- **项目分组**：按项目聚合进程
- **端口占用**：TCP/UDP 双栈 + 进程映射 + 系统保留区间提示
- **日志**：终止操作历史 + 按进程名聚合统计

### 操作
- 终止选中 / 终止全部 Node / 终止全部 Python
- 孤儿进程清理（手动 + 定时自动，阈值可配）
- 智能重启（杀后原参数拉起）
- 提权一键修复保留端口（winnat）/ 复制修复命令
- 右键菜单：进程详情 / AI 分析 / 复制路径命令行 / 资源管理器 / 终端打开
- 双击行查看进程详情（线程数/句柄数/启动时间/窗口标题）
- 导出全部进程 CSV

### AI 助手（可选，需 kilo CLI）
- **风险扫描**：批量评级（JSON 解析，AI风险列着色）
- **单进程分析**：右键 AI 分析此进程
- **日志复盘 / 全局诊断 / 清理策略**（策略可一键执行）
- **AI 对话**：多轮上下文，Markdown 富文本渲染
- 通过环境变量 `KILO_EXE` 指定 kilo.exe 路径（或使用内置候选路径）

### 系统集成
- 系统托盘（点 X 最小化到托盘、气泡通知、快捷菜单）
- 开机自启动、启动最小化
- 单实例互斥、窗口位置记忆
- 自动刷新（间隔可配）、异常内存增长告警（阈值可配）
- 深/浅主题一键切换
- 管理员权限重启

### CLI
```
app.exe /list          全部进程列表
app.exe /ports         端口监听 + 系统保留区间
app.exe /kill 123 ...  按 PID 终止
app.exe /ai scan       AI 风险扫描（需 kilo）
app.exe --version      版本号
```

## 构建

依赖：[MinGW-w64](https://www.mingw-w64.org/)（gcc 8+，含 windres）

```
build.bat            构建 build\app.exe
build.bat dist      构建 + 打包 dist\kill-process-type-nk.zip
```

需与旧版 `kill-process-type` 仓库同级放置（复用其 process/net/config/klog 等模块）。

## 字体

内置便携微软雅黑（`assets/fonts/msyh.ttf`），无需安装。加载优先级：项目内 → 系统雅黑 → 黑体 → Arial。

## 目录结构

```
src/           源码（main / app_shared / ui_views / ai_bridge / sys_bridge /
               data_bridge / tray_bridge / ui_text / version）
assets/        图标 + 便携字体
vendor/raylib/ raylib 源码（6.1-dev）
tools/         字体提取 / 码点生成 / 图标生成 / UI 自动化脚本
```

## 许可

仅供个人学习与工作效率工具使用。
