# -*- coding: utf-8 -*-
"""
verify_v5.py —— lexer_v5_gui.py + lex-v5.exe 的自动验证脚本

验证项：
    [1] lex-v5.exe 存在（UTF-8 编译产物）
    [2] 单输入：rc=0，5 个产物存在，三表 CSV 为 UTF-8 且表头/数据正确
    [3] 多输入独立性：在临时目录放两个不同名输入，各产出一套 5 文件，互不覆盖
    [4] 有错输入：input-v5-2.txt 返回码 1（扫描完成但有词法错误），CSV 照常产出
    [5] GUI 冒烟：真实开窗 -> 断言语法/语义按钮灰态「待学习」-> 添加第二个输入 ->
        模拟批量运行 -> 等队列处理完 -> 检查行状态与 token 预览 -> 1.2s 后自动关窗

运行：python verify_v5.py
"""

import shutil
import tempfile
import time
import tkinter as tk
from pathlib import Path

import lexer_v5_gui as gui
from lexer_v5_gui import APP_DIR, LexerV5App, expected_outputs, read_csv_rows, run_one


def wait_until(cond, timeout=15.0, interval=0.03):
    """在 Tk 事件循环里 pump 事件直到 cond() 为真或超时。"""
    end = time.time() + timeout
    while time.time() < end:
        gui_root.update()
        if cond():
            return True
        time.sleep(interval)
    return False


def step(n, msg):
    print(f"[{n}] {msg}")


# GUI 冒烟用到的根窗口（wait_until 引用）
gui_root = None


def main():
    global gui_root

    # ---- [1] exe ----------------------------------------------------------
    step(1, f"lex-v5.exe: {gui.EXE_PATH}")
    assert gui.EXE_PATH.exists(), "lex-v5.exe 不存在，请先 g++ 编译"

    # ---- [2] 单输入 -------------------------------------------------------
    inp = APP_DIR / "input.txt"
    rc, out, err = run_one(inp)
    step(2, f"单输入 input.txt rc={rc}（输出为 UTF-8，前 1 行：{out.splitlines()[0]}）")
    assert rc == 0, f"input.txt 应无错，stderr={err}"
    outs = expected_outputs(inp)
    for key, p in outs.items():
        assert p.exists(), f"缺少产物 {key}: {p.name}"
    header, rows = read_csv_rows(outs["token"])
    assert header == ["token_no", "token_seman", "token_desc", "line_no"], header
    assert len(rows) > 0 and rows[0][1] == "int", rows[0]
    _, name_rows = read_csv_rows(outs["nameL"])
    _, const_rows = read_csv_rows(outs["constL"])
    print(f"    token={len(rows)} nameL={len(name_rows)} constL={len(const_rows)}")

    # ---- [3] 多输入独立产出（临时目录，两个不同名输入） -------------------
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        a = td / "demo_alpha.txt"
        b = td / "demo_beta.txt"
        shutil.copy(inp, a)
        shutil.copy(inp, b)
        rc_a, _, _ = run_one(a)
        rc_b, _, _ = run_one(b)
        step(3, f"临时目录两输入 rc={rc_a}/{rc_b}，检查独立产物")
        assert rc_a == rc_b == 0
        for f in (a, b):
            o = expected_outputs(f)
            assert all(p.exists() for p in o.values()), f"{f.name} 产物不全"
        # 两个 tag 不同 → 产物共 10 个，互不覆盖（不计两个输入文件本身）
        made = [p.name for p in td.iterdir()
                if p.name.startswith(("output_", "error_log_"))]
        assert "output_token_demo_alpha.csv" in made
        assert "output_token_demo_beta.csv" in made
        assert len(made) == 10, made
        print(f"    临时目录产物数={len(made)}（两输入 × 5 个，独立不覆盖）")

    # ---- [4] 含词法错误的输入：rc=1 但 CSV 正常产出 -----------------------
    bad = APP_DIR / "input-v5-2.txt"
    rc_bad, _, _ = run_one(bad)
    step(4, f"input-v5-2.txt rc={rc_bad}（v5 约定：1=扫描完成但有词法错误）")
    assert rc_bad == 1
    bad_outs = expected_outputs(bad)
    assert bad_outs["token"].exists() and bad_outs["error_log"].exists()
    _h, bad_rows = read_csv_rows(bad_outs["token"])
    assert len(bad_rows) > 0
    print(f"    有错仍产出 token {len(bad_rows)} 个 + error_log，符合预期")

    # ---- [5] GUI 冒烟 -----------------------------------------------------
    step(5, "创建 GUI，检查阶段按钮状态")
    gui_root = tk.Tk()
    app = LexerV5App(gui_root)
    gui_root.update_idletasks()
    gui_root.update()

    # 语法/语义按钮：disabled 且文字含「待学习」
    assert "disabled" in app.btn_syn.state(), "语法分析按钮应为禁用态"
    assert "disabled" in app.btn_sem.state(), "语义分析按钮应为禁用态"
    assert "待学习" in app.btn_syn.cget("text")
    assert "待学习" in app.btn_sem.cget("text")
    assert "disabled" not in app.btn_lex.state(), "词法分析按钮应可用"
    print("    词法=可用；语法/语义=灰色禁用「待学习」")

    # 默认已带 input.txt，再添加一个有错输入，共 2 个
    app._add_inputs([bad])
    assert len(app._inputs) == 2
    app.lbox.selection_clear(0, "end")
    app.lbox.selection_set(0)

    app.on_run()        # 模拟点击「词法分析」（真实线程 + 真实 exe）
    finished = wait_until(lambda: not app._busy and app.status_var.get().startswith("全部完成"))
    assert finished, "批量运行超时"
    print(f"    状态栏：{app.status_var.get()}")
    statuses = [it["status"] for it in app._inputs]
    assert statuses == ["ok", "warn"], statuses

    # 选中第 1 行（有错输入），预览应能读出 token
    app.lbox.selection_clear(0, "end")
    app.lbox.selection_set(1)
    app._refresh_preview()
    gui_root.update()
    n_token = len(app.trees["token 表"].get_children())
    n_name = len(app.trees["nameL 变量名表"].get_children())
    n_const = len(app.trees["constL 常数表"].get_children())
    print(f"    预览 input-v5-2：token={n_token} nameL={n_name} constL={n_const}")
    assert n_token == len(bad_rows)

    # 切回第 0 行预览
    app.lbox.selection_clear(0, "end")
    app.lbox.selection_set(0)
    app._refresh_preview()
    gui_root.update()
    assert len(app.trees["token 表"].get_children()) == len(rows)
    print("    切换输入后预览联动正常")

    print("[6] 全部断言通过，窗口展示 1.2 秒后自动关闭 …")
    gui_root.after(1200, gui_root.destroy)
    gui_root.mainloop()
    print("验证完成 ✓")


if __name__ == "__main__":
    main()
