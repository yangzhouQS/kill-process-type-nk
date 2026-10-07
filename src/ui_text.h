/* ui_text.h — 全部 UI 中文文案（集中管理，便于生成字体码点表） */
#ifndef UI_TEXT_H
#define UI_TEXT_H

/* 工具栏 */
#define T_REFRESH       "刷新"
#define T_KILL_SEL      "终止选中"
#define T_KILL_NODE     "终止全部Node"
#define T_KILL_PY       "终止全部Python"
#define T_CLEAN_ORPHAN  "清理孤儿"
#define T_AI_CHAT       "AI 对话"
#define T_SETTINGS      "设置"
#define T_THEME_DARK    "深色"
#define T_THEME_LIGHT   "浅色"

/* 页签 */
#define TT_TAB_ALL      "全部进程"
#define TT_TAB_NODEPY   "Node·Python"
#define TT_TAB_TREE     "进程树"
#define TT_TAB_PROJECT  "项目"
#define TT_TAB_PORTS    "端口占用"
#define TT_TAB_LOGS     "日志"
#define TT_TAB_AI       "AI 助手"

/* 筛选与状态栏 */
#define T_FILTER_HINT  "筛选进程名，如：node"
#define T_READY        "就绪"
#define T_FMT_STATUS   "共 %d 进程   Node: %d   Python: %d   端口: %d"
#define T_FMT_PROCS    "进程: %d   端口: %d"

/* 列标题：进程 */
#define C_NAME    "进程名"
#define C_PID     "PID"
#define C_PPID    "父PID"
#define C_MEM     "内存"
#define C_CPU     "CPU%"
#define C_TYPE    "类型"
#define C_PATH    "可执行路径"
#define C_CMDLINE "命令行"
#define C_RISK    "AI风险"
#define C_TREE    "进程名（树形）"
#define C_PROJECT "项目"

/* 列标题：端口 */
#define C_PORT  "端口"
#define C_PROTO "协议"

/* 列标题：日志 */
#define C_TIME   "时间"
#define C_SOURCE "来源"
#define C_RESULT "结果"

/* 占位文本 */
#define P_GONE     "(进程已退出)"
#define P_NOREAD   "(无法读取)"
#define P_RANGE    "(系统保留端口区间)"
#define P_NOPROJ   "未识别项目"
#define P_KILLED   "已终止"
#define T_FMT_TREE "共 %d 进程（树形）   内存列为子树合计"

/* 设置弹窗 */
#define S_TITLE        "设置"
#define S_STARTMIN     "启动时最小化到托盘"
#define S_AUTOREFRESH  "自动刷新"
#define S_INTERVAL_S   "间隔（秒）"
#define S_BALLOON      "托盘气泡通知"
#define S_ORPHAN_AUTO  "自动清理孤儿进程"
#define S_INTERVAL_M   "间隔（分钟）"
#define S_ORPHAN_NP    "仅清理 Node/Python 孤儿"
#define S_ANOMALY      "异常监控（CPU/内存告警）"
#define S_AUTORUN      "开机自启动（登录后驻留托盘）"
#define S_THEME        "主题"
#define S_SAVE         "保存"
#define S_CANCEL       "取消"
#define S_OPENLOGDIR   "打开日志文件夹"
#define S_SAVED        "设置已保存"

/* AI 弹窗 */
#define A_TITLE       "AI 助手"
#define A_ANALYZE     "分析此进程"
#define A_SMART_RE    "智能重启（杀后原参数拉起）"
#define A_CHAT_PH     "输入问题，回车发送…"
#define A_SEND        "发送"
#define A_RUNNING     "AI 思考中…"
#define A_FAIL        "AI 调用失败"
#define A_NOKILO      "未找到 kilo.exe（可设置环境变量 KILO_EXE 指定路径）"
#define A_CLOSE       "关闭"
#define A_COPY        "复制结果"
#define A_SCAN        "风险扫描"
#define A_REVIEW      "日志复盘"
#define A_DIAG        "全局诊断"
#define A_STRATEGY    "清理策略"
#define A_BASE_SAVE   "保存基线"
#define A_BASE_CMP    "基线对比"
#define A_SCAN_DONE   "风险扫描完成：已应用 %d 条评级"
#define A_SCAN_EMPTY  "当前无 Node/Python 进程可扫描"
#define A_BASE_SAVED  "基线快照已保存（%d 个进程）"
#define A_BASE_LOST   "基线文件不存在，请先保存基线"
#define A_BASE_TITLE  "基线对比结果"
#define A_APPLY       "按策略终止"
#define A_APPLY_DONE  "策略执行完成：终止 %d 个进程"
#define T_ADMIN       "管理员重启"
#define T_ADMIN_OK    "已启动管理员实例，本窗口即将关闭"
#define T_ADMIN_FAIL  "管理员重启被取消或失败"
#define T_ANOMALY_FMT "⚠ %s（PID %lu）内存持续增长：12秒内 +%luMB"

/* 右键菜单：进程行 */
#define M_DETAIL    "进程详情"
#define M_EXPORT_CSV "导出全部CSV"
#define M_KILL      "终止选中"
#define M_SMARTRE   "智能重启（杀后原参数拉起）"
#define M_AI_ANALY  "AI 分析此进程"
#define M_COPY_PATH "复制可执行路径"
#define M_COPY_CMD  "复制命令行"
#define M_EXPLORER  "在资源管理器中显示"
#define M_TERMINAL  "在终端打开所在目录"

/* 右键菜单：端口行 */
#define M_ELEVFIX   "一键修复保留端口（UAC）"
#define M_COPYFIX   "仅复制修复命令"
#define M_OPENURL   "浏览器打开 http://localhost:端口"

/* 提示消息 */
#define N_KILLED_SEL  "已终止选中进程"
#define N_KILLED_FMT  "%s清理孤儿进程：已终止 %d/%d 个"
#define N_NO_SEL      "请先选中要终止的进程"
#define N_CONFIRM_ALL "确定终止全部 %s 进程（共 %d 个）？"
#define N_COPIED      "已复制到剪贴板"
#define N_FIX_STARTED "已启动管理员修复窗口，完成后刷新端口列表验证"
#define N_FIX_CANCEL  "已取消 UAC 提权"
#define N_ORPH_NONE   "未发现孤儿进程"
#define N_AUTORUN_ON  "已开启开机自启动"
#define N_AUTORUN_OFF "已关闭开机自启动"
#define N_SAVED_WIN   "窗口位置已记忆"

/* 类型 */
#define TY_NODE   "Node.js"
#define TY_PY     "Python"
#define TY_OTHER  "-"
#define TY_SYS    "系统"

/* 主题按钮提示 */
#define TH_NOW_DARK "当前深色"
#define TH_NOW_LIGHT "当前浅色"

#endif /* UI_TEXT_H */
