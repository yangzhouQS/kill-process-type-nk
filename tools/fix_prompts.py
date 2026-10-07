# fix_prompts.py — AI prompt 精简化（kilo run 长参数 bug 规避）
import io

P = r"src\ai_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

miss = []

def rep(old, new, tag):
    global t
    if old in t:
        t = t.replace(old, new, 1)
    else:
        miss.append(tag)

# 精简行（无路径/命令行，只 pid+name+mem）
rep('''static BOOL AppendProcLine(char *buf, int cap, int *used, const BridgeProc *p)
{
    char line[512];
    int n = snprintf(line, sizeof(line),
                     "PID=%lu 名称=%s 内存=%.0fMB 路径=%s 命令行=%s\\n",
                     (unsigned long)p->pid, p->name,
                     (double)p->memBytes / 1048576.0,
                     p->path[0] ? p->path : "(无法读取)",
                     p->cmdline[0] ? p->cmdline : "-");
    if (*used + n >= cap) return FALSE;
    strcat(buf + *used, line);
    *used += n;
    return TRUE;
}''',
'''static BOOL AppendProcLine(char *buf, int cap, int *used, const BridgeProc *p)
{
    /* 精简格式：kilo run 对长命令行参数有内部 bug（>2000 字符易触发
     * 内部错误/路径误判），故不携带路径与命令行，且总长受限 */
    char line[160];
    int n = snprintf(line, sizeof(line), "%lu|%s|%.0fMB\\n",
                     (unsigned long)p->pid, p->name,
                     (double)p->memBytes / 1048576.0);
    if (*used + n >= cap) return FALSE;
    strcat(buf + *used, line);
    *used += n;
    return TRUE;
}''', "procline")

# RiskScan：限制条数与总长
rep('''    int count = 0;
    for (size_t i = 0; i < pl->count && used < (int)sizeof(buf) - 600; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0) {
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\\n");
    }
    AiLaunch(buf);
}

void AiStartLogReview''',
'''    int count = 0;
    int total = 0;
    for (size_t i = 0; i < pl->count; i++)
        if (pl->items[i].type != 0) total++;
    for (size_t i = 0; i < pl->count && used < 700 && count < 18; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (total > count)
        used += snprintf(buf + used, sizeof(buf) - used, "（其余 %d 个略）\\n", total - count);
    if (count == 0)
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\\n");
    AiLaunch(buf);
}

void AiStartLogReview''', "riskscan-limit")

# LogReview：日志行精简 + 限条数
rep('''    for (size_t i = 0; i < logs->count && used < (int)sizeof(buf) - 400; i++) {
        const BridgeLog *l = &logs->items[i];
        used += snprintf(buf + used, sizeof(buf) - used, "%s|%s|%s|%lu|%s|%s\\n",
                         l->timeText, l->source, l->name,
                         (unsigned long)l->pid, l->ok ? "OK" : "FAIL",
                         l->path[0] ? l->path : "-");
    }''',
'''    {
        size_t shown = 0;
        for (size_t i = 0; i < logs->count; i++) {
            const BridgeLog *l = &logs->items[i];
            if (used >= 700 || shown >= 18) {
                used += snprintf(buf + used, sizeof(buf) - used,
                                 "（其余 %d 条略）\\n", (int)(logs->count - shown));
                break;
            }
            used += snprintf(buf + used, sizeof(buf) - used, "%s|%s|PID%lu|%s\\n",
                             l->timeText, l->name, (unsigned long)l->pid,
                             l->ok ? "OK" : "FAIL");
            shown++;
        }
    }''', "logreview-limit")

# Diag：限 12 条
rep('''        if (best == pl->count) break;
        usedFlag[best] = 1;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[best]);
        taken++;''',
'''        if (best == pl->count) break;
        usedFlag[best] = 1;
        if (used < 700)
            AppendProcLine(buf, sizeof(buf), &used, &pl->items[best]);
        taken++;''', "diag-limit")

# CleanStrategy：同 RiskScan 限制
rep('''    int count = 0;
    for (size_t i = 0; i < pl->count && used < (int)sizeof(buf) - 600; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0) {
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\\n");
    }
    AiLaunch(buf);
}

int AiParseRiskJson''',
'''    int count = 0;
    for (size_t i = 0; i < pl->count && used < 700 && count < 18; i++) {
        if (pl->items[i].type == 0) continue;
        AppendProcLine(buf, sizeof(buf), &used, &pl->items[i]);
        count++;
    }
    if (count == 0)
        used += snprintf(buf + used, sizeof(buf) - used, "(无 Node/Python 进程)\\n");
    AiLaunch(buf);
}

int AiParseRiskJson''', "strategy-limit")

# Analyze（单进程）：精简
rep('''    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理专家。分析以下进程并评估终止它的风险，"
             "用中文分点简洁回答（300字内）："
             "1)该进程是什么（服务/程序/常见用途）；"
             "2)终止风险评级：低/中/高；"
             "3)评级依据（系统关键性、父进程关系、未保存数据丢失、是否会自动重启、"
             "对监听服务的影响）；4)建议操作。"
             "进程信息：名称=%s；PID=%lu；父PID=%lu；内存=%.1fMB；类型=%s；路径=%s；"
             "监听端口=%s。",
             p->name, (unsigned long)p->pid, (unsigned long)p->ppid,
             (double)p->memBytes / 1048576.0, ProcTypeStr(p->type),
             p->path[0] ? p->path : "(无法读取)",
             portsText && portsText[0] ? portsText : "无");''',
'''    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理专家。分析以下进程终止风险，"
             "中文分点简洁回答（300字内）：1)是什么 2)风险：低/中/高 "
             "3)依据 4)建议。进程：%s PID=%lu 内存=%.0fMB 类型=%s 端口=%s",
             p->name, (unsigned long)p->pid,
             (double)p->memBytes / 1048576.0, ProcTypeStr(p->type),
             portsText && portsText[0] ? portsText : "无");''', "analyze-short")

# Chat 精简系统前缀
rep('''    snprintf(prompt, sizeof(prompt),
             "你是Windows进程管理工具「Node/Python 进程终结者」的内置助手。"
             "当前机器上有多个 node/python 进程。请用中文简洁回答用户问题。\\n\\n"
             "用户问题：%s", msg);''',
'''    snprintf(prompt, sizeof(prompt),
             "你是进程管理工具内置助手，用中文简洁回答。问题：%s", msg);''', "chat-short")

with io.open(P, "w", encoding="utf-8", newline="") as f:
    f.write(t)
print("MISS:", miss if miss else "none")
