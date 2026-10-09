# -*- coding: utf-8 -*-
"""
词法分析 v0.5 · 简化版 GUI（Tkinter + subprocess）
z最初设想：
设想完全成立：GUI 负责“选文件 + 触发 + 展示”，subprocess 负责“把文件路径/内容交给 exe + 收回结果”，exe 在中间完成对文本文件的读写。这套“Python 外壳 + exe 内核”的架构在自动化批处理、工具封装等场景中已被广泛使用。
流程：一个“GUI 按钮 → exe 处理文件 → 展示/保存结果”的闭环

读取/选择文件：用 filedialog 让用户选文件
执行 exe：subprocess.run([exe, input_path, output_path])，exe 完成对文本文件的读写
检查返回结果：查看 returncode、stdout、stderr 判断成功与否
回读输出文件：exe 写出的 output.txt 再由 Python 读取，展示到 GUI 或做后续处理

对接 csv_io_v5.cpp 编译出的 lex-v5.exe：
    lex-v5.exe <输入文件>          # 输出写到「工作目录」，按输入文件名主干加后缀：
                                   #   output_token_<tag>.csv   token 表
                                   #   output_nameL_<tag>.csv   变量名表
                                   #   output_constL_<tag>.csv  常数表
                                   #   error_log_<tag>.txt      词法错误日志
                                   #   output_<tag>.txt         可读报告
特点：
    1. 顶部三个阶段按钮：词法分析（可用）/ 语法分析、语义分析（灰色「待学习」不可点）
    2. 支持一次选择多个 input，后台逐个调用 exe —— 每个输入产出一套独立的三表 CSV
    3. 右侧 Notebook 按输入行切换预览 token / nameL / constL 三张 CSV 表
    4. exe 与产物均为 UTF-8 编码（g++ -finput-charset=UTF-8 -fexec-charset=UTF-8）
"""

import csv                      # 标准库：读取 lex-v5 产出的 CSV（自动处理逗号/引号转义）
import queue                    # 标准库：后台线程 -> 主线程的线程安全结果通道
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
FONT = "Microsoft YaHei UI"

# 三张预览表与 exe 产出 CSV 的对应关系：(选项卡标题, CSV 文件名模板, 列定义)
PREVIEW_TABLES = [
    ("token 表", "output_token_{tag}.csv",
     (("token_no", "序号", 70, "center"),
      ("token_seman", "单词", 150, "w"),
      ("token_desc", "编码描述", 320, "w"),
      ("line_no", "行号", 70, "center"))),
    ("nameL 变量名表", "output_nameL_{tag}.csv",
     (("name_no", "编号", 90, "center"),
      ("name", "变量名", 300, "w"))),
    ("constL 常数表", "output_constL_{tag}.csv",
     (("const_no", "编号", 90, "center"),
      ("const_value", "常数", 300, "w"))),
]


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
    """按 v5 的命名规则，返回该输入对应的 5 个产物路径（不要求已存在）。"""
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


# ----------------------------------------------------------------------------
# GUI
# ----------------------------------------------------------------------------
class LexerV5App:
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
        self.root.title("编译原理综合实验 · 词法分析v0.5b")
        w, h = 860, 860
        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() - w) // 2
        y = max(0, (self.root.winfo_screenheight() - h) // 4)
        self.root.geometry(f"{w}x{h}+{x}+{y}")
        self.root.minsize(720, 520)
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

        # 第 1 行：三个阶段按钮（语法/语义预留，灰色不可点，文案标明「待学习」）
        stages = ttk.Frame(outer)
        stages.grid(row=1, column=0, sticky="w", pady=(10, 4))
        self.btn_lex = ttk.Button(stages, text="词法分析", style="StageOn.TButton",
                                  command=self.on_run)
        self.btn_lex.grid(row=0, column=0, padx=(0, 10))
        # state="disabled"：按钮整体置灰且不响应点击；文字直接写「待学习」
        self.btn_syn = ttk.Button(stages, text="语法分析（待学习）",
                                  style="StageOff.TButton", state="disabled")
        self.btn_syn.grid(row=0, column=1, padx=(0, 10))
        self.btn_sem = ttk.Button(stages, text="语义分析（待学习）",
                                  style="StageOff.TButton", state="disabled")
        self.btn_sem.grid(row=0, column=2)
        ttk.Label(outer, text="后两个阶段将在后续课程开放，当前仅词法分析可用",
                  style="Hint.TLabel").grid(row=2, column=0, sticky="w", pady=(0, 10))

        # 主体：左右分栏。左=输入文件列表（固定宽），右=CSV 预览（随窗口伸缩）
        body = ttk.Frame(outer)
        body.grid(row=3, column=0, sticky="nsew")
        outer.rowconfigure(3, weight=1)
        body.columnconfigure(1, weight=1)
        body.rowconfigure(0, weight=1)

        self._build_left(body)
        self._build_right(body)

        # 底部状态栏
        self.status_var = tk.StringVar(value="就绪：添加一个或多个 input 后点击「词法分析」")
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

        # Listbox：显示「文件名  …  状态」，点选即预览该输入的 CSV
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

        ttk.Label(card, text="输出 CSV 预览（三表独立产出在各输入同目录）",
                  background=C_CARD, font=(FONT, 10, "bold")).grid(
            row=0, column=0, sticky="w", pady=(0, 8))

        # Notebook：三个选项卡对应三张 CSV
        self.nb = ttk.Notebook(card)
        self.nb.grid(row=1, column=0, sticky="nsew")
        self.trees = {}        # 表名 -> Treeview
        for title, _tpl, cols in PREVIEW_TABLES:
            page = ttk.Frame(self.nb, style="Card.TFrame")
            page.rowconfigure(0, weight=1)
            page.columnconfigure(0, weight=1)
            tree = ttk.Treeview(page, columns=tuple(c[0] for c in cols),
                                show="headings", selectmode="browse")
            for key, text, width, anchor in cols:
                tree.heading(key, text=text)
                # 最后一列 stretch=True 吃掉多余宽度
                tree.column(key, width=width, anchor=anchor,
                            stretch=(key == cols[-1][0]))
            tree.tag_configure("alt", background=C_ALT)
            tree.grid(row=0, column=0, sticky="nsew")
            sb = ttk.Scrollbar(page, orient="vertical", command=tree.yview)
            sb.grid(row=0, column=1, sticky="ns")
            tree.configure(yscrollcommand=sb.set)
            self.nb.add(page, text=title)
            self.trees[title] = tree

        self.hint_var = tk.StringVar(value="尚未运行")
        ttk.Label(card, textvariable=self.hint_var, background=C_CARD,
                  foreground=C_MUTED).grid(row=2, column=0, sticky="w", pady=(8, 0))

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
        if not self._inputs:
            messagebox.showinfo("提示", "请先添加至少一个输入文件")
            return
        self._set_busy(True)
        # 重置所有行状态
        for i, item in enumerate(self._inputs):
            item["status"] = "排队中"
            self._update_row(i)
        self.status_var.set(f"开始批量分析 {len(self._inputs)} 个输入 …")
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
            # v5 约定：rc=0 无错；rc=1 表示扫描完成但存在词法错误（CSV 仍正常产出）
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
        color = {"ok": C_OK, "warn": C_WARN, "error": "#b91c1c"}.get(status, C_TEXT)
        self.lbox.itemconfig(i, fg=color)

    def _after_all_done(self):
        ok = sum(1 for it in self._inputs if it["status"] == "ok")
        warn = sum(1 for it in self._inputs if it["status"] == "warn")
        err = len(self._inputs) - ok - warn
        self.status_var.set(
            f"全部完成：成功 {ok} 个，有词法错误 {warn} 个，异常 {err} 个；CSV 已各自写入输入同目录")
        self._set_busy(False)
        self._refresh_preview()

    # ---- CSV 预览 ----------------------------------------------------------
    def _selected_input(self):
        sel = self.lbox.curselection()
        if not sel:
            return None
        return self._inputs[sel[0]]["path"]

    def _refresh_preview(self):
        """把选中输入对应的三张 CSV 读入 Notebook 各选项卡。"""
        path = self._selected_input()
        if path is None:
            return
        outs = expected_outputs(path)
        for title, tpl, cols in PREVIEW_TABLES:
            tree = self.trees[title]
            tree.delete(*tree.get_children())
            csv_path = outs[{"token 表": "token",
                             "nameL 变量名表": "nameL",
                             "constL 常数表": "constL"}[title]]
            _header, rows = read_csv_rows(csv_path)
            for r_i, row in enumerate(rows, start=1):
                # 行数不足/有余都容错；隔行加斑马纹
                values = (row + [""] * len(cols))[:len(cols)]
                tags = ("alt",) if r_i % 2 == 0 else ()
                tree.insert("", "end", values=values, tags=tags)
        tag = path.stem
        total = len(self.trees["token 表"].get_children())
        self.hint_var.set(f"当前预览：{path.name}（tag={tag}，token {total} 个）")

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


def main():
    root = tk.Tk()
    LexerV5App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
