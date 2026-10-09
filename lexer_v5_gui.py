# -*- coding: utf-8 -*-
"""
词法分析 v0.6b · 简化版 GUI（Tkinter + subprocess）

对接 csv_io_v5.cpp（v0.6b）编译出的 lex-v5.exe：
    lex-v5.exe <输入文件>          # 输出写到「工作目录」，按输入文件名主干加后缀：
                                   #   output_token_<tag>.csv   token 表（v0.6b 起含 Seman 列）
                                   #   output_nameL_<tag>.csv   变量名表
                                   #   output_constL_<tag>.csv  常数表
                                   #   error_log_<tag>.txt      词法错误日志
                                   #   output_<tag>.txt         可读报告
特点：
    1. 顶部三个阶段按钮：词法分析（可用）/ 语法分析、语义分析（灰色「待学习」不可点）
    2. 词法分析按钮右侧新增「分析方式」下拉：默认=结构化判断(lex-v5.exe)；
       另一个「自动机分割」待开发（exe 尚不存在），选中即弹回、不可用
    3. 支持一次选择多个 input，后台逐个调用 exe —— 每个输入产出一套独立的三表 CSV
    4. 右侧 Notebook 按输入切换预览 token / nameL / constL 三张 CSV 表 + 错误日志（v0.6b 新增）
    5. exe 与产物均为 UTF-8 编码（g++ -finput-charset=UTF-8 -fexec-charset=UTF-8）

v0.6b 相比 v0.6 的 GUI 变更：
    ① token 表预览新增 Seman 列（标识符->nameL 编号、常数->constL 编号、其余 NULL）
    ② 错误日志 error_log_<tag>.txt 进入点选预览（新增第 4 个选项卡）
    ③ 词法分析右侧新增分析方式配置下拉（自动机分割待开发、不可选）
"""

import csv                      # 标准库：读取 lex-v5 产出的 CSV（自动处理逗号/引号转义）
import queue                    # 标准库：后台线程 -> 主线程的线程安全结果通道
import re                       # 标准库：解析 error_log_*.txt 中的 [n] 行号/错误信息
import subprocess               # 标准库：调用外部 lex-v5.exe
import threading                # 标准库：后台线程批量跑多个输入，界面不卡死
from pathlib import Path        # 标准库：路径拼接 / 取文件名主干

import tkinter as tk                           # Tkinter 主库
from tkinter import ttk, filedialog, messagebox  # ttk 主题控件 / 文件选择框 / 错误弹窗

# ----------------------------------------------------------------------------
# 路径与配色（简约扁平）
# ----------------------------------------------------------------------------
APP_DIR = Path(__file__).resolve().parent     # 本脚本所在目录（lex-v5.exe 也在这里）
EXE_PATH = APP_DIR / "lex-v5.exe"

C_BG = "#f4f6fa"            # 窗口底色
C_CARD = "#ffffff"          # 卡片/表格底
C_PRIMARY = "#2563eb"       # 当前阶段（词法）蓝
C_PRIMARY_ACTIVE = "#1d4ed8"
C_TEXT = "#1f2937"
C_MUTED = "#9ca3af"         # 次要文字 / 待学习灰
C_LINE = "#e5e7eb"          # 分隔线
C_ALT = "#f8fafc"           # 表格隔行
C_HEAD = "#eef2ff"          # 表头
C_OK = "#15803d"            # 成功绿
C_WARN = "#b45309"          # 有错橙
C_ERR = "#b91c1c"           # 错误红（错误日志行）
FONT = "Microsoft YaHei UI"

# v0.6b：分析方式配置。enabled=False 的方式 exe 尚不存在，禁止选中
# 每项 = (下拉显示文字, exe 文件名或 None, 是否可用)
METHODS = [
    ("结构化判断（lex-v5.exe）", "lex-v5.exe", True),
    ("自动机分割（待开发，暂无 exe）", None, False),
]
METHOD_DEFAULT_INDEX = 0

# 三张 CSV 预览表与 exe 产物的对应关系：(选项卡标题, 产物 key, 列定义)
# 列定义 = (列 key, 表头文字, 宽, 对齐)
PREVIEW_TABLES = [
    ("token 表", "token",
     (("token_no", "序号", 55, "center"),
      ("token_seman", "单词", 120, "w"),
      ("Seman", "Seman 属性", 90, "center"),   # v0.6b 新增：nameL/constL 编号或 NULL
      ("token_desc", "编码描述", 300, "w"),
      ("line_no", "行号", 55, "center"))),
    ("nameL 变量名表", "nameL",
     (("name_no", "编号", 90, "center"),
      ("name", "变量名", 300, "w"))),
    ("constL 常数表", "constL",
     (("const_no", "编号", 90, "center"),
      ("const_value", "常数", 300, "w"))),
]
# v0.6b：第 4 个选项卡不是 CSV，而是 error_log_<tag>.txt
ERR_TAB_TITLE = "错误日志 error_log"
ERR_COLUMNS = (("no", "序号", 70, "center"),
               ("line", "行号", 90, "center"),
               ("msg", "错误信息", 300, "w"))


# ----------------------------------------------------------------------------
# 业务函数（不依赖界面，验证脚本可直接 import）
# ----------------------------------------------------------------------------
def run_one(input_path: Path, exe_path: Path = EXE_PATH):
    """
    对单个输入文件调用一次 lex-v5.exe。
    关键：cwd 设为「输入文件所在目录」，argv 只传文件名 ——
    这样每套独立的 output_*_<tag>.csv 都产出在对应输入旁边。
    返回 (返回码, stdout 文本, stderr 文本)。
    """
    if not exe_path.exists():
        raise FileNotFoundError(f"找不到 {exe_path.name}，请先用 UTF-8 字符集编译")
    if not input_path.exists():
        raise FileNotFoundError(f"输入文件不存在：{input_path}")

    # subprocess.run：同步执行外部程序并捕获管道输出
    result = subprocess.run(
        [str(exe_path), input_path.name],     # 列表传参，不经 shell；cwd 内只传文件名
        cwd=str(input_path.parent),           # 三表 CSV / 错误日志都写在该目录
        capture_output=True,                  # 重定向 stdout/stderr 以便程序捕获
        encoding="utf-8",                     # exe 已按 UTF-8 编译（SetConsoleOutputCP 也是 UTF-8）
        errors="replace",
        timeout=30,
    )
    return result.returncode, result.stdout or "", result.stderr or ""


def expected_outputs(input_path: Path):
    """按 v5/v0.6b 的命名规则，返回该输入对应的 5 个产物路径（不要求已存在）。"""
    tag = input_path.stem                     # 与 cpp 中 fileTag() 等价：去目录去扩展名
    d = input_path.parent
    return {
        "token": d / f"output_token_{tag}.csv",
        "nameL": d / f"output_nameL_{tag}.csv",
        "constL": d / f"output_constL_{tag}.csv",
        "error_log": d / f"error_log_{tag}.txt",
        "report": d / f"output_{tag}.txt",
    }


def read_csv_rows(csv_path: Path):
    """读取一张 CSV，返回 (表头, 数据行)；文件不存在时返回 (None, [])。"""
    if not csv_path.exists():
        return None, []
    # newline='' 是 csv 模块官方推荐写法，encoding 必须与 exe 产物一致用 utf-8
    with open(csv_path, "r", encoding="utf-8", newline="") as f:
        rows = list(csv.reader(f))
    if not rows:
        return [], []
    return rows[0], rows[1:]


def parse_err_log(log_path: Path):
    """
    v0.6b：解析 error_log_<tag>.txt，返回 (错误总数, [(序号, 行号, 信息), ...])。
    文件格式：
        词法分析错误日志
        共 2 个错误
        [1] 行号: 2  错误信息: ...
    文件不存在返回 (None, [])。
    """
    if not log_path.exists():
        return None, []
    total = None
    records = []
    with open(log_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            m_cnt = re.match(r"共\s*(\d+)\s*个错误", line)
            if m_cnt:
                total = int(m_cnt.group(1))
                continue
            m = re.match(r"\[(\d+)\]\s*行号:\s*(\d+)\s*错误信息:\s*(.*)$", line)
            if m:
                records.append((m.group(1), m.group(2), m.group(3)))
    return total, records


# ----------------------------------------------------------------------------
# GUI
# ----------------------------------------------------------------------------
class LexerV6bApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self._busy = False                    # 批量运行期间为 True，锁定按钮
        self._queue = queue.Queue()           # 后台线程结果通道
        self._inputs = []                     # 与左侧列表行一一对应：[{path, status}]
        self._setup_window()
        self._setup_style()
        self._build_layout()
        # 默认带上同目录 input.txt，方便直接演示（可删）
        default_in = APP_DIR / "input.txt"
        if default_in.exists():
            self._add_inputs([default_in])
        self.root.after(50, self._poll_queue)

    # ---- 窗口与样式 --------------------------------------------------------
    def _setup_window(self):
        self.root.title("编译原理综合实验 · 词法分析 v0.6b")
        w, h = 920, 660
        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() - w) // 2
        y = max(0, (self.root.winfo_screenheight() - h) // 4)
        self.root.geometry(f"{w}x{h}+{x}+{y}")
        self.root.minsize(760, 520)
        self.root.configure(bg=C_BG)

    def _setup_style(self):
        s = ttk.Style()
        s.theme_use("clam")
        s.configure("TFrame", background=C_BG)
        s.configure("TLabel", background=C_BG, foreground=C_TEXT, font=(FONT, 10))
        s.configure("Title.TLabel", font=(FONT, 15, "bold"))
        s.configure("Hint.TLabel", foreground=C_MUTED, font=(FONT, 9))
        s.configure("Card.TFrame", background=C_CARD)

        # 阶段按钮：当前阶段蓝底白字；未开放阶段灰底灰字
        s.configure("StageOn.TButton", foreground="white", background=C_PRIMARY,
                    borderwidth=0, font=(FONT, 10, "bold"), padding=(20, 9))
        s.map("StageOn.TButton", background=[("active", C_PRIMARY_ACTIVE),
                                             ("disabled", C_PRIMARY_ACTIVE)])
        s.configure("StageOff.TButton", foreground="#c7ccd6", background="#e9edf3",
                    borderwidth=0, font=(FONT, 10), padding=(20, 9))
        # 普通小按钮
        s.configure("TButton", font=(FONT, 9), padding=(10, 5))
        # 分析方式下拉（readonly）扁平风格
        s.configure("TCombobox", fieldbackground=C_CARD, background=C_CARD,
                    font=(FONT, 10), padding=4)

        # Treeview 表格
        s.configure("Treeview", background=C_CARD, fieldbackground=C_CARD,
                    foreground=C_TEXT, rowheight=25, font=(FONT, 10), borderwidth=0)
        s.configure("Treeview.Heading", background=C_HEAD, foreground="#374151",
                    font=(FONT, 10, "bold"), relief="flat")
        s.map("Treeview",
              background=[("selected", "#bfdbfe")],
              foreground=[("selected", "#1e3a8a")])
        # Notebook 扁平化
        s.configure("TNotebook", background=C_BG, borderwidth=0)
        s.configure("TNotebook.Tab", padding=(16, 7), font=(FONT, 10),
                    background=C_BG, foreground=C_MUTED)
        s.map("TNotebook.Tab",
              background=[("selected", C_CARD), ("active", "#e8edf6")],
              foreground=[("selected", C_TEXT)])

    # ---- 布局 --------------------------------------------------------------
    def _build_layout(self):
        outer = ttk.Frame(self.root, padding=16)
        outer.grid(row=0, column=0, sticky="nsew")
        self.root.rowconfigure(0, weight=1)
        self.root.columnconfigure(0, weight=1)

        # 第 0 行：标题
        ttk.Label(outer, text="编译器前端实验台", style="Title.TLabel").grid(
            row=0, column=0, sticky="w")

        # 第 1 行：阶段按钮 + 分析方式配置（v0.6b 新增于词法分析右侧）
        stages = ttk.Frame(outer)
        stages.grid(row=1, column=0, sticky="w", pady=(10, 4))
        self.btn_lex = ttk.Button(stages, text="词法分析", style="StageOn.TButton",
                                  command=self.on_run)
        self.btn_lex.grid(row=0, column=0, padx=(0, 12))

        # —— v0.6b：分析方式子列表（紧贴「词法分析」右边）——
        ttk.Label(stages, text="      分析方式：").grid(row=1, column=0, padx=(0, 4))
        self.method_box = ttk.Combobox(
            stages, state="readonly", width=24,
            values=[m[0] for m in METHODS])
        self.method_box.current(METHOD_DEFAULT_INDEX)   # 默认：结构化判断 lex-v5.exe
        self.method_box.grid(row=1, column=1, padx=(0, 14))
        # 第二项「自动机分割」待开发：一旦被选中立即弹回默认项并提示
        self.method_box.bind("<<ComboboxSelected>>", self._on_method_change)

        # state="disabled"：按钮整体置灰且不响应点击；文字直接写「待学习」
        self.btn_syn = ttk.Button(stages, text="语法分析（待开放）",
                                  style="StageOff.TButton", state="disabled")
        self.btn_syn.grid(row=0, column=1, padx=(0, 8))
        self.btn_sem = ttk.Button(stages, text="语义分析（待学习）",
                                  style="StageOff.TButton", state="disabled")
        self.btn_sem.grid(row=0, column=2)
        ttk.Label(outer, text="后两个阶段待学习中；分析方式中的「自动机分割」待开发（暂无 exe，不可选）",
                  style="Hint.TLabel").grid(row=2, column=0, sticky="w", pady=(0, 10))

        # 主体：左右分栏。左=输入文件列表（固定宽），右=预览（随窗口伸缩）
        body = ttk.Frame(outer)
        body.grid(row=3, column=0, sticky="nsew")
        outer.rowconfigure(3, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)

        self._build_left(body)
        self._build_right(body)

        # 底部状态栏
        self.status_var = tk.StringVar(
            value="就绪：添加一个或多个 input 后点击「词法分析」，完成后点选各选项卡预览（含错误日志）")
        ttk.Label(outer, textvariable=self.status_var, style="Hint.TLabel").grid(
            row=4, column=0, sticky="w", pady=(10, 0))

    def _build_left(self, parent):
        card = ttk.Frame(parent, style="Card.TFrame", padding=10)
        card.grid(row=0, column=0, sticky="ns")

        ttk.Label(card, text="输入文件（可多选）", background=C_CARD,
                  font=(FONT, 10, "bold")).grid(row=0, column=0, columnspan=3,
                                                 sticky="w", pady=(0, 8))
        # 文件操作小按钮
        ttk.Button(card, text="＋ 添加", command=self.on_add).grid(
            row=1, column=0, padx=(0, 4), sticky="ew")
        ttk.Button(card, text="移除选中", command=self.on_remove).grid(
            row=1, column=1, padx=4, sticky="ew")
        ttk.Button(card, text="清空", command=self.on_clear_inputs).grid(
            row=1, column=2, padx=(4, 0), sticky="ew")

        # Listbox：显示「文件名  …  状态」，点选即预览该输入的各产物
        self.lbox = tk.Listbox(card, activestyle="none", selectmode="browse",
                               width=34, height=16, relief="flat",
                               font=(FONT, 10), bg=C_CARD, fg=C_TEXT,
                               highlightthickness=1, highlightbackground=C_LINE,
                               selectbackground="#bfdbfe", selectforeground="#1e3a8a")
        self.lbox.grid(row=2, column=0, columnspan=3, sticky="ns", pady=(8, 0))
        card.rowconfigure(2, weight=1)
        # <<ListboxSelect>>：切换预览对象
        self.lbox.bind("<<ListboxSelect>>", lambda e: self._refresh_preview())

    def _build_right(self, parent):
        card = ttk.Frame(parent, style="Card.TFrame", padding=(12, 10))
        card.grid(row=0, column=1, sticky="nsew", padx=(12, 0))
        card.columnconfigure(0, weight=1)
        card.rowconfigure(1, weight=1)

        ttk.Label(card, text="输出预览（三表 CSV + 错误日志，产物外存在输入同目录）",
                  background=C_CARD, font=(FONT, 10, "bold")).grid(
            row=0, column=0, sticky="w", pady=(0, 8))

        # Notebook：三张 CSV 选项卡 + v0.6b 新增错误日志选项卡
        self.nb = ttk.Notebook(card)
        self.nb.grid(row=1, column=0, sticky="nsew")
        self.trees = {}        # 选项卡标题 -> Treeview
        for title, _key, cols in PREVIEW_TABLES:
            self._add_table_tab(title, cols, tag_zebra=True)
        # 错误日志选项卡（有错误的行用红字）
        self._add_table_tab(ERR_TAB_TITLE, ERR_COLUMNS, tag_zebra=False)
        self.trees[ERR_TAB_TITLE].tag_configure("err", foreground=C_ERR)

        self.hint_var = tk.StringVar(value="尚未运行")
        ttk.Label(card, textvariable=self.hint_var, background=C_CARD,
                  foreground=C_MUTED).grid(row=2, column=0, sticky="w", pady=(8, 0))

    def _add_table_tab(self, title, cols, tag_zebra):
        """在 Notebook 中新建一个带滚动条的 Treeview 选项卡。"""
        page = ttk.Frame(self.nb, style="Card.TFrame")
        page.rowconfigure(0, weight=1)
        page.columnconfigure(0, weight=1)
        tree = ttk.Treeview(page, columns=tuple(c[0] for c in cols),
                            show="headings", selectmode="browse")
        for key, text, width, anchor in cols:
            tree.heading(key, text=text)
            tree.column(key, width=width, anchor=anchor,
                        stretch=(key == cols[-1][0]))  # 最后一列吃掉多余宽度
        if tag_zebra:
            tree.tag_configure("alt", background=C_ALT)
        tree.grid(row=0, column=0, sticky="nsew")
        sb = ttk.Scrollbar(page, orient="vertical", command=tree.yview)
        sb.grid(row=0, column=1, sticky="ns")
        tree.configure(yscrollcommand=sb.set)
        self.nb.add(page, text=title)
        self.trees[title] = tree

    # ---- 分析方式配置（v0.6b） ---------------------------------------------
    def _on_method_change(self, _event=None):
        """只允许选择已实现的方式；选中「自动机分割」立即弹回默认项。"""
        idx = self.method_box.current()
        if idx < 0:
            return
        if not METHODS[idx][2]:                 # enabled=False -> 待开发，不可选
            self.status_var.set("「自动机分割」方式待开发：对应 exe 尚不存在，已保持默认「结构化判断」")
            # after_idle：等本次选择事件处理完再弹回，避免 combobox 显示残留
            self.root.after_idle(lambda: self.method_box.current(METHOD_DEFAULT_INDEX))
            return
        exe_name = METHODS[idx][1]
        self.status_var.set(f"分析方式：{METHODS[idx][0]}（{exe_name}）")

    # ---- 输入列表维护 ------------------------------------------------------
    def on_add(self):
        """弹出多选文件框，把所选 .txt 加入列表（按绝对路径去重）。"""
        paths = filedialog.askopenfilenames(
            title="选择一个或多个输入文件",
            initialdir=str(APP_DIR),
            filetypes=[("文本文件", "*.txt"), ("所有文件", "*.*")])
        if paths:                       # askopenfilenames 返回路径字符串元组
            self._add_inputs([Path(p) for p in paths])

    def _add_inputs(self, paths):
        existing = {item["path"].resolve() for item in self._inputs}
        added = 0
        for p in paths:
            rp = p.resolve()
            if rp in existing:
                continue
            existing.add(rp)
            self._inputs.append({"path": rp, "status": "待运行"})
            self.lbox.insert("end", f"{rp.name}    · 待运行")
            added += 1
        if added:
            self.status_var.set(f"已添加 {added} 个输入，共 {len(self._inputs)} 个")

    def on_remove(self):
        sel = self.lbox.curselection()
        if not sel:
            return
        # 从后往前删，避免下标位移
        for i in reversed(sel):
            self.lbox.delete(i)
            del self._inputs[i]
        self._refresh_preview()

    def on_clear_inputs(self):
        self.lbox.delete(0, "end")
        self._inputs.clear()
        self._clear_all_trees()
        self.hint_var.set("已清空输入")

    # ---- 批量运行 ----------------------------------------------------------
    def on_run(self):
        if self._busy:
            return
        # 双保险：只有已实现的分析方式（当前仅结构化判断 lex-v5.exe）允许运行
        idx = self.method_box.current()
        if idx < 0 or not METHODS[idx][2]:
            self.status_var.set("该分析方式待开发，暂不可执行")
            return
        if not self._inputs:
            messagebox.showinfo("提示", "请先添加至少一个输入文件")
            return
        self._set_busy(True)
        # 重置所有行状态
        for i, item in enumerate(self._inputs):
            item["status"] = "排队中"
            self._update_row(i)
        self.status_var.set(f"开始批量分析 {len(self._inputs)} 个输入（方式：{METHODS[idx][0]}）…")
        # 一个后台线程顺序跑完所有输入，进度经队列回传
        threading.Thread(target=self._run_worker,
                         args=([dict(item) for item in self._inputs],),
                         daemon=True).start()

    def _run_worker(self, snapshot):
        """后台线程：逐个调用 exe。只碰队列，不碰任何 Tk 控件。"""
        for i, item in enumerate(snapshot):
            path = Path(item["path"])
            try:
                rc, out, err = run_one(path)
            except Exception as exc:
                self._queue.put(("file", i, "error", str(exc)))
                continue
            print("=" * 60)
            print(f"[{i + 1}/{len(snapshot)}] {path.name}")
            print(out, end="" if out.endswith("\n") else "\n")
            # v5/v0.6b 约定：rc=0 无错；rc=1 表示扫描完成但存在词法错误（CSV 仍正常产出）
            if rc == 0:
                status = "ok"
            elif rc == 1:
                status = "warn"
            else:
                status = f"失败(rc={rc}) {err.strip()[:60]}"
            self._queue.put(("file", i, status, ""))
        self._queue.put(("all_done",))

    def _poll_queue(self):
        """主线程：消费后台进度并刷新界面。"""
        try:
            while True:
                msg = self._queue.get_nowait()
                if msg[0] == "file":
                    _, i, status, _detail = msg
                    self._inputs[i]["status"] = status
                    self._update_row(i)
                    # 当前选中行有结果时顺手刷新预览
                    self._refresh_preview()
                elif msg[0] == "all_done":
                    self._after_all_done()
        except queue.Empty:
            pass
        if self.root.winfo_exists():
            self.root.after(50, self._poll_queue)

    def _update_row(self, i):
        """重绘列表第 i 行的「文件名 · 状态」文字与颜色。"""
        item = self._inputs[i]
        name = item["path"].name
        status = item["status"]
        text_map = {
            "待运行": "· 待运行", "排队中": "· 排队中…",
            "ok": "· ✓ 成功", "warn": "· ⚠ 有词法错误", "error": "· ✗ 调用异常",
        }
        suffix = text_map.get(status, f"· {status}")
        self.lbox.delete(i)
        self.lbox.insert(i, f"{name}    {suffix}")
        # Listbox 用 itemconfig 给单行上色
        color = {"ok": C_OK, "warn": C_WARN, "error": C_ERR}.get(status, C_TEXT)
        self.lbox.itemconfig(i, fg=color)

    def _after_all_done(self):
        ok = sum(1 for it in self._inputs if it["status"] == "ok")
        warn = sum(1 for it in self._inputs if it["status"] == "warn")
        err = len(self._inputs) - ok - warn
        self.status_var.set(
            f"全部完成：成功 {ok} 个，有词法错误 {warn} 个，异常 {err} 个；三表 CSV 与错误日志已写入输入同目录")
        self._set_busy(False)
        self._refresh_preview()

    # ---- 预览（三表 CSV + 错误日志） ---------------------------------------
    def _selected_input(self):
        sel = self.lbox.curselection()
        if not sel:
            return None
        return self._inputs[sel[0]]["path"]

    def _refresh_preview(self):
        """把选中输入对应的三张 CSV 与错误日志读入 Notebook 各选项卡。"""
        path = self._selected_input()
        if path is None:
            return
        outs = expected_outputs(path)

        # 三张 CSV
        for title, key, cols in PREVIEW_TABLES:
            tree = self.trees[title]
            tree.delete(*tree.get_children())
            _header, rows = read_csv_rows(outs[key])
            for r_i, row in enumerate(rows, start=1):
                # 行数不足/有余都容错；隔行加斑马纹
                values = (row + [""] * len(cols))[:len(cols)]
                tags = ("alt",) if r_i % 2 == 0 else ()
                tree.insert("", "end", values=values, tags=tags)

        # v0.6b：错误日志选项卡
        err_tree = self.trees[ERR_TAB_TITLE]
        err_tree.delete(*err_tree.get_children())
        total, err_rows = parse_err_log(outs["error_log"])
        if total is None:
            err_tree.insert("", "end", values=("—", "—", "尚未产出错误日志（先运行分析）"))
        elif total == 0:
            err_tree.insert("", "end", values=("—", "—", "（无词法错误）"))
        else:
            for r in err_rows:
                err_tree.insert("", "end", values=r, tags=("err",))  # 有错行红字

        tag = path.stem
        n_token = len(self.trees["token 表"].get_children())
        err_txt = "错误日志缺失" if total is None else f"{total} 个错误"
        self.hint_var.set(
            f"当前预览：{path.name}（tag={tag}，token {n_token} 个，{err_txt}）")

    def _clear_all_trees(self):
        for tree in self.trees.values():
            tree.delete(*tree.get_children())
        self.hint_var.set("尚未运行")

    # ---- 忙碌状态 ----------------------------------------------------------
    def _set_busy(self, busy):
        self._busy = busy
        stage = "disabled" if busy else "!disabled"
        self.btn_lex.state([stage])
        # 语法/语义按钮永远 disabled，不在这里切换
        # 分析方式下拉在运行期间锁定，结束后恢复（防止跑到一半切换方式）
        self.method_box.configure(state="disabled" if busy else "readonly")


def main():
    root = tk.Tk()
    LexerV6bApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()
