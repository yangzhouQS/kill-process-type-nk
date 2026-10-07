# add_sys.py — sys_bridge.c 追加两个函数
import io

P = r"src\sys_bridge.c"
with io.open(P, encoding="utf-8") as f:
    t = f.read()

if "SysRestartElevated" in t:
    print("already present")
else:
    fn = '''
void SysClampWindowRect(int *x, int *y, int *w, int *h)
{
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (*w > vw) *w = vw;
    if (*h > vh) *h = vh;
    if (*x < vx) *x = vx;
    if (*x + *w > vx + vw) *x = vx + vw - *w;
    if (*y < vy) *y = vy;
    if (*y + *h > vy + vh) *y = vy + vh - *h;
}

int SysRestartElevated(void)
{
    WCHAR exe[MAX_PATH];
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH))
        return 2;
    {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOASYNC;
        sei.lpVerb = L"runas";
        sei.lpFile = exe;
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&sei)) {
            DWORD e = GetLastError();
            return (e == ERROR_CANCELLED) ? 1 : 2;
        }
    }
    return 0;
}
'''
    with io.open(P, "w", encoding="utf-8", newline="") as f:
        f.write(t + fn)
    print("added")
