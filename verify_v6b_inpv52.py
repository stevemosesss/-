# -*- coding: utf-8 -*-
"""
verify_v6b.py —— 词法分析 GUI v0.6b 自动验证脚本

验证项：
    [1] lex-v5.exe 存在（UTF-8 编译产物）
    [2] token CSV 表头为 5 列且第 3 列名为 Seman
    [3] Seman 语义交叉校验：
        标识符行 -> Seman 为整数且指向 nameL 中同名条目；
        常量行   -> Seman 为整数且指向 constL 中同值条目；
        关键字/运算符/分隔符/换行/# -> Seman 必须为 NULL
    [4] 错误日志：input-v5-2.txt rc=1，error_log 解析出 2 条非空错误（v0.6b 修复点）
    [5] GUI 冒烟：默认分析方式=结构化判断；选「自动机分割」被弹回；语法/语义禁用；
        批量运行后状态 ok/warn；错误日志选项卡联动显示 2 条红字 / 无错输入显示（无词法错误）

运行：python verify_v6b.py
"""

import re
import time
import tkinter as tk
from pathlib import Path

import lexer_v5_gui as gui
from lexer_v5_gui import (
    APP_DIR, ERR_TAB_TITLE, METHODS, LexerV6bApp,
    expected_outputs, parse_err_log, read_csv_rows, run_one,
)

gui_root = None


def wait_until(cond, timeout=20.0, interval=0.03):
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


def main():
    global gui_root

    # ---- [1] exe ----------------------------------------------------------
    step(1, f"lex-v5.exe: {gui.EXE_PATH}")
    assert gui.EXE_PATH.exists(), "lex-v5.exe 不存在，请先 g++ 编译"

    # ---- [2] token CSV 表头 ----------------------------------------------
    inp = APP_DIR / "input.txt"
    rc, _out, err = run_one(inp)
    outs = expected_outputs(inp)
    header, tokens = read_csv_rows(outs["token"])
    step(2, f"input.txt rc={rc}，token 表头：{header}")
    assert rc == 0, err
    assert header == ["token_no", "token_seman", "Seman", "token_desc", "line_no"], header

    # ---- [3] Seman 交叉校验 ----------------------------------------------
    _h, name_rows = read_csv_rows(outs["nameL"])
    _h, const_rows = read_csv_rows(outs["constL"])
    name_map = {r[0]: r[1] for r in name_rows}    # 编号 -> 变量名
    const_map = {r[0]: r[1] for r in const_rows}  # 编号 -> 常数值
    ident_n = const_n = null_n = 0
    for row in tokens:
        no, seman, seman_ref, desc, line = row
        # 描述里统一带 token.class=N：11=标识符，12=数字常量，其余不进符号表
        m_cls = re.search(r"token\.class=(\d+)", desc)
        cls = int(m_cls.group(1)) if m_cls else -1
        if cls == 11:
            assert seman_ref.isdigit(), f"标识符 Seman 应为编号: {row}"
            assert name_map[seman_ref] == seman, f"Seman 指向错误: {row}"
            ident_n += 1
        elif cls == 12:                          # 正整数/正实数/十六进制/科学计数法
            assert seman_ref.isdigit(), f"常量 Seman 应为编号: {row}"
            assert const_map[seman_ref] == seman, f"Seman 指向错误: {row}"
            const_n += 1
        else:                                     # 关键字/运算符/分隔符/换行/#
            assert seman_ref == "NULL", f"非符号表 token 应为 NULL: {row}"
            null_n += 1
    step(3, f"Seman 交叉校验通过：标识符 {ident_n} 行、常量 {const_n} 行、NULL {null_n} 行")
    # 同一标识符第二次出现应指向同一 nameL 编号
    a_refs = {r[2] for r in tokens if r[1] == "a" and r[3].startswith("标识符")}
    assert a_refs == {"1"}, a_refs

    # ---- [4] 错误日志 -----------------------------------------------------
    bad = APP_DIR / "input-v5-2.txt"
    rc_bad, _, _ = run_one(bad)
    total, errs = parse_err_log(expected_outputs(bad)["error_log"])
    step(4, f"input-v5-2 rc={rc_bad}，错误日志解析：共 {total} 条")
    assert rc_bad == 1
    assert total == 2 and len(errs) == 2
    for no, line, msg in errs:                    # v0.6b：错误信息必须非空
        assert msg.strip(), f"错误信息为空（errAdd 修复未生效）: {(no, line, msg)}"
        print(f"    [{no}] 行号 {line}: {msg}")

    # ---- [5] GUI 冒烟 -----------------------------------------------------
    step(5, "创建 GUI …")
    gui_root = tk.Tk()
    app = LexerV6bApp(gui_root)
    gui_root.update_idletasks()
    gui_root.update()

    # 分析方式：默认结构化判断，自动机分割待开发
    assert app.method_box.current() == 0
    assert "结构化判断" in METHODS[0][0] and METHODS[0][2] is True
    assert METHODS[1][2] is False and "自动机分割" in METHODS[1][0]
    # 模拟选中第二项：应被弹回 0 且状态栏提示待开发
    app.method_box.current(1)
    app._on_method_change()
    gui_root.update_idletasks()
    gui_root.update()
    assert app.method_box.current() == 0, "自动机分割应被弹回，不可选"
    assert "待开发" in app.status_var.get()
    print("    分析方式：默认 lex-v5.exe；选「自动机分割」被弹回并提示")

    # 阶段按钮状态
    assert "disabled" in app.btn_syn.state()
    assert "disabled" in app.btn_sem.state()
    assert "disabled" not in app.btn_lex.state()
    print("    语法/语义按钮灰色禁用；词法按钮可用")

    # 4 个选项卡且 token 表含 Seman 列
    tabs = list(app.trees)
    assert tabs == ["token 表", "nameL 变量名表", "constL 常数表", ERR_TAB_TITLE], tabs
    assert "Seman" in app.trees["token 表"]["columns"]
    print(f"    选项卡：{tabs}")

    # 加入有错输入并批量运行
    app._add_inputs([bad])
    app.on_run()
    finished = wait_until(
        lambda: not app._busy and app.status_var.get().startswith("全部完成"))
    assert finished, "批量运行超时"
    statuses = [it["status"] for it in app._inputs]
    assert statuses == ["ok", "warn"], statuses
    print(f"    {app.status_var.get()}")

    # 选中有错输入 -> 错误日志选项卡 2 条红字
    app.lbox.selection_clear(0, "end")
    app.lbox.selection_set(1)
    app._refresh_preview()
    gui_root.update()
    err_tree = app.trees[ERR_TAB_TITLE]
    err_items = err_tree.get_children()
    assert len(err_items) == 2
    vals = [err_tree.item(i)["values"] for i in err_items]
    assert vals[0][1] == 2 and vals[1][1] == 3
    # Treeview 单 tag 可能返回字符串 "err" 或元组 ("err",)，用包含判断兼容两种情况
    assert all("err" in err_tree.item(i)["tags"] for i in err_items)
    n_tok = len(app.trees["token 表"].get_children())
    print(f"    错误日志选项卡：{n_tok=}，错误行 {[v[2] for v in vals]}")

    # 切回无错输入 -> 显示（无词法错误），token 行 5 列
    app.lbox.selection_clear(0, "end")
    app.lbox.selection_set(0)
    app._refresh_preview()
    gui_root.update()
    ok_err_vals = err_tree.item(err_tree.get_children()[0])["values"]
    assert "无词法错误" in ok_err_vals[2], ok_err_vals
    first_token = app.trees["token 表"].item(
        app.trees["token 表"].get_children()[1])["values"]  # 第 2 行：标识符 a
    assert len(first_token) == 5 and first_token[2] == 1
    print(f"    无错输入错误日志显示：{ok_err_vals[2]}；token 行 5 列正常")

    print("[6] 全部断言通过，窗口展示 1.2 秒后自动关闭 …")
    gui_root.after(1200, gui_root.destroy)
    gui_root.mainloop()
    print("验证完成 ✓")


if __name__ == "__main__":
    main()
