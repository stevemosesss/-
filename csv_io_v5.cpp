#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>//在 Windows/MSVC 下，这些宏实际由 vadefs.h 提供底层实现（如 _crt_va_start、_crt_va_arg），stdarg.h 只是转发定义。x86 上的典型实现就是指针算术：va_start 让指针指向 fmt 之后的地址，va_arg 按 _INTSIZEOF 对齐后向前移动并取出内容。
#include <iostream>
#include <windows.h> //包含ConsoleOutputCP和ConsoleCP函数
// utf8编译： g++ -finput-charset=UTF-8 -fexec-charset=UTF-8 csv_io.cpp -o io.exe
using namespace std;
/*
/*     关键字if: token.class=1  */
/*   关键字else: token.class=2  */
/*   关键字for: token.class=3  */
/* 关键字while: token.class=4  */
/* 关键字break: token.class=5  */
/*关键字return: token.class=6  */
/* 关键字continue: token.class=7 */
/*  关键字float: token.class=8  */
/*    关键字int: token.class=9  */
/*   关键字char: token.class=10 */
/*      标识符: token.class=11  */
/*      正整数: token.class=12  */
/*      正实数: token.class=12  */
/*         零: token.class=12  */
/*        加号: token.class=13  */
/*        减号: token.class=14  */
/*        乘号: token.class=15  */
/*        除号: token.class=16  */
/*        取余: token.class=17  */
/*      大于号: token.class=18  */
/*  大于等于号: token.class=19  */
/*      小于号: token.class=20  */
/*  小于等于号: token.class=21  */
/*    不等于号: token.class=22  */
/*    等于号==: token.class=23  */
/*      非号!: token.class=24  */
/*     逻辑与&&: token.class=25 */
/*     逻辑或||: token.class=26 */
/*        逗号: token.class=27  */
/*      赋值号: token.class=28  */
/*      左括号[: token.class=29 */
/*      右括号]: token.class=30 */
/*      左圆括号(: token.class=31 */
/*      右圆括号): token.class=32 */
/*      左花括号{: token.class=33 */
/*      右花括号}: token.class=34 */
/*        分号: token.class=35  */
/*        点号: token.class=36  */
/*      换行符: token.class=37  */
/*  文件结束'#': token.class=38 */

/*v0.6 gui图形化
z最初设想：
`g++ -O2 -finput-charset=UTF-8 -fexec-charset=UTF-8 csv_io_v5.cpp -o lex-v5.exe`
设想成立：GUI 负责“选文件 + 触发 + 展示”，subprocess 负责“把文件路径/内容交给 exe + 收回结果”，exe 在中间完成对文本文件的读写。这套“Python 外壳 + exe 内核”的架构在自动化批处理、工具封装等场景中已被广泛使用。
流程：一个“GUI 按钮 → exe 处理文件 → 展示/保存结果”的闭环
读取/选择文件：用 filedialog 让用户选文件
执行 exe：subprocess.run([exe, input_path, output_path])，exe 完成对文本文件的读写
检查返回结果：查看 returncode、stdout、stderr 判断成功与否
回读输出文件：exe 写出的 output.txt 再由 Python 读取，展示到 GUI 或做后续处理

/*v0.6b 在 v0.6 基础上的更新（cpp 侧）：
1. token 表 CSV 新增第 3 列 Seman（属性语义）：标识符(class=11)填它在 nameL 中的编号，
   常数(class=12)填它在 constL 中的编号；关键字、运算符、分隔符、换行、'#' 等一律填 NULL。
   新表头：token_no,token_seman,Seman,token_desc,line_no
2. 修复 v0.5 遗留：errAdd() 此前没有用 va_list/vsnprintf 把格式串写入 ErrRec.msg，
   导致 error_log_*.txt 的“错误信息”列为空；现已正确落盘，供 GUI 错误日志选项卡预览。
（输出文件命名规则、返回码约定 0=无错 / 1=有词法错误 均与 v0.5 保持不变）

/*v0.5 词法分析：
新增了汇总表input-output.txt 在main里面进行保存可读综合文本
新增了ErrLog结构体，保错误记录，写入错误err-log表，
现z在支持识别
非法字符@或者其他的不在编码表里的符合
double 编码表以外的关键字。但是当成标识符处理感觉怪怪的 当成上面一起处理了
数字常量格式错误 十进制和十六进制（0x）
注释报错两种：孤立右注释和没有闭合的左注释
另外：规范化文件输出命名格式fileTag函数，读取输入，分四个表（cpp也是可以做到的）

v0.4 词法分析——三表结构token nameL constl
    逐字符扫描tian添加了记录行号方便后续纠错，所有的表按首次出现的编号，后续去重
    ① token 表  (编号, 单词, 编码描述, 行号)     —— token 序列
    ② nameL 表 (编号, 变量名单词)                —— 标识符符号表
    ③ constL表 (编号, 常数)                      —— 常数表(含整数/实数各种类型)
    瞎改改成了说换行符 class=37 不再作为 token 输出，因为行号列已携带换行信息，
发现了测试里多谢了个double，应该判断为非法字符报错（比如编码里面没有冒号和@号

/*v0.3 词法分析 token 二元组(编码,值) + 行内顺序数组
    逐字符扫描，不依赖空格分隔：连续字母数字组成标识符/关键字，
    数字包括常数，小数等等（正实数），运算符支持双字符（>= <= != == && ||）
    二元组 token = (token.class 种别编码, token.seman 单词值)
    全部 token 按出现顺序存入结构体数组 -> output.csv（token表序列）

v0.2  /*设计词法分析：目前只能空格分隔的token导出到output.csv
寻找新思路是逐个字符扫描连续处理　存入同一个单词数组单元／单词便利（但是不支持注释与空格）
ｘｉａｎｘｉｎｇｃｕｎｒｕ　符号表，这样在第二次遇到同一个符号／关键字／变量可以根据查找找到对应的token
最后要求是二元组/记号：单词的种别/类型与值

v0.1 实现csv的输入输出和简单的空格分隔
*/


// ================= 通用结构 =================
#define MAX_SEM 64          // 单词值最大长度

// token：编号=数组下标+1，另存单词、编码、行号
typedef struct {
    int tokenClass;         // 种别编码 1-38（class 是 C++ 关键字，故用 tokenClass）
    char seman[MAX_SEM];    // 单词的符号值
    int line;               // 所在源程序行号（从 1 起）
} Token;

typedef struct {
    Token* items;
    int size, capacity;
} TokenList;

// 变量名表 / 常数表（字符串列）
typedef struct {
    char (*items)[MAX_SEM];
    int size, capacity;
} NameTable;

void tlInit(TokenList* tl) {
    tl->capacity = 64;
    tl->size = 0;
    tl->items = (Token*)malloc(tl->capacity * sizeof(Token));
}
// 释放当前tl（顺序数组而不是链表，tl->items 是一块连续的内存（通过 malloc/calloc/realloc 一次性分配），所以只需要一次 free() 就能整块归还内存
void tlFree(TokenList* tl) {
    free(tl->items);
    tl->items = NULL;
    tl->size = tl->capacity = 0;
}
// 初始64的token列表扩容，自动传参入表
void tlAdd(TokenList* tl, int cls, const char* seman, int line) {
    if (tl->size >= tl->capacity) {
        tl->capacity *= 2;
        tl->items = (Token*)realloc(tl->items, tl->capacity * sizeof(Token));
    }
    Token* t = &tl->items[tl->size++];
    t->tokenClass = cls;
    t->line = line;
    strncpy(t->seman, seman, MAX_SEM - 1);
    t->seman[MAX_SEM - 1] = '\0';//字符串尾部
}
// nameL 初始化32个关键字和标识符
void ntInit(NameTable* nt) {
    nt->capacity = 32;
    nt->size = 0;
    nt->items = (char(*)[MAX_SEM])malloc(nt->capacity * MAX_SEM);
}
void ntFree(NameTable* nt) {
    free(nt->items);
    nt->items = NULL;
    nt->size = nt->capacity = 0;
}
// 已存在同名项则不重复添加（按单词去重），返回其编号(1起)
int ntAddUnique(NameTable* nt, const char* word) {
    for (int i = 0; i < nt->size; i++) {
        if (strcmp(nt->items[i], word) == 0) return i + 1;
    }
    if (nt->size >= nt->capacity) {
        nt->capacity *= 2;
        nt->items = (char(*)[MAX_SEM])realloc(nt->items, nt->capacity * MAX_SEM);
    }
    strncpy(nt->items[nt->size], word, MAX_SEM - 1);
    nt->items[nt->size][MAX_SEM - 1] = '\0';
    return ++nt->size;
}
// v0.6b：查某个单词在 nameL / constL 中的编号（1 起），查不到返回 0 —— 供 token 表 Seman 列使用
int ntIndex(const NameTable* nt, const char* word) {
    for (int i = 0; i < nt->size; i++) {
        if (strcmp(nt->items[i], word) == 0) return i + 1;
    }
    return 0;
}

// ================= v0.5 新增错误日志结构（单元：错误记录与去重输出） =================
#define MAX_MSG 160

typedef struct {
    int line;               // 错误所在行号
    char msg[MAX_MSG];      // 错误信息
} ErrRec;

typedef struct {
    ErrRec* items;
    int size, capacity;
} ErrLog;//保存错误记录的数组结构体

// 错误日志初始化
void errInit(ErrLog* el) {
    el->capacity = 16;
    el->size = 0;
    el->items = (ErrRec*)malloc(el->capacity * sizeof(ErrRec));
}
// 错误日志释放
void errFree(ErrLog* el) {
    free(el->items);
    el->items = NULL;
    el->size = el->capacity = 0;
}
// 追加一条错误（printf 风格格式串），同时在控制台打印
void errAdd(ErrLog* el, int line, const char* fmt, ...) {
    if (el->size >= el->capacity) {
        el->capacity *= 2;
        el->items = (ErrRec*)realloc(el->items, el->capacity * sizeof(ErrRec));
    }
    ErrRec* r = &el->items[el->size++];
    r->line = line;
    // v0.6b 修复：v0.5 漏了把可变参数格式串真正写进 r->msg，错误日志信息列为空
    va_list ap;                          // stdarg.h：声明可变参数指针
    va_start(ap, fmt);                   // 令 ap 指向 fmt 之后的第一个可变参数
    vsnprintf(r->msg, MAX_MSG, fmt, ap); // 按 fmt 格式化进 r->msg，自带长度截断防溢出
    va_end(ap);
    r->msg[MAX_MSG - 1] = '\0';
    printf("[词法错误] 第%d行: %s\n", line, r->msg);
}
// 将全部错误写入 log 文件（无错误也写文件说明）
int writeErrLog(const ErrLog* el, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("无法写入 %s\n", filename); return 0; }
    fprintf(fp, "词法分析错误日志\n共 %d 个错误\n", el->size);
    for (int i = 0; i < el->size; i++) {
        fprintf(fp, "[%d] 行号: %d  错误信息: %s\n",
                i + 1, el->items[i].line, el->items[i].msg);
    }
    if (el->size == 0) fprintf(fp, "（无错误）\n");
    fclose(fp);
    printf("已保存错误日志 -> %s（%d 个错误）\n", filename, el->size);
    return 1;
}

// ================= 关键字表与查找 =================
int keywordClass(const char* word) {
    static const struct { const char* name; int cls; } KEYWORDS[] = {
        {"if", 1}, {"else", 2}, {"for", 3}, {"while", 4}, {"break", 5},
        {"return", 6}, {"continue", 7}, {"float", 8}, {"int", 9},
        {"char", 10}
    };
    for (unsigned i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        if (strcmp(word, KEYWORDS[i].name) == 0) return KEYWORDS[i].cls;
    }
    return 0;
}

// v0.5：识别"像关键字但编码表里没有的"的单词（如 double），返回 1=未支持
int unsupportedKeyword(const char* word) {
    static const char* UNSUPPORTED[] = {
        "double", "void", "long", "short", "unsigned", "signed", NULL
    };
    for (int i = 0; UNSUPPORTED[i]; i++) {
        if (strcmp(word, UNSUPPORTED[i]) == 0) return 1;
    }
    return 0;
}

// ================= 编码描述（token 表第 3 列） =================
void classDesc(int cls, const char* seman, char* out) {
    if (cls >= 1 && cls <= 10) {                 // 关键字
        sprintf(out, "关键字%s: token.class=%d", seman, cls);
    } else if (cls == 11) {
        strcpy(out, "标识符: token.class=11");
    } else if (cls == 12) {                      // 数字：按形态区分
        if (strncmp(seman, "0x", 2) == 0 || strncmp(seman, "0X", 2) == 0)
            strcpy(out, "十六进制常量: token.class=12");
        else if (strchr(seman, 'e') || strchr(seman, 'E'))
            strcpy(out, "科学计数法常量: token.class=12");
        else if (strchr(seman, '.')) strcpy(out, "正实数: token.class=12");
        else strcpy(out, "正整数: token.class=12");  // 含零（按实验举例0记为正整数）
    } else {
        static const char* NAMES[] = {
            "", "", "", "", "", "", "", "", "", "", "", "", "",
            "加号", "减号", "乘号", "除号", "取余",
            "大于号", "大于等于号", "小于号", "小于等于号", "不等于号",
            "等于号==", "非号!", "逻辑与&&", "逻辑或||", "逗号",
            "赋值号", "左括号[", "右括号]", "左圆括号(", "右圆括号)",
            "左花括号{", "右花括号}", "分号", "点号", "换行符",
            "文件结束'#'"
        };
        sprintf(out, "%s: token.class=%d",
                (cls >= 13 && cls <= 38) ? NAMES[cls] : "未知", cls);
    }
}

// ================= v0.5 数字常量读取与校验（单元：十六进制/小数/科学计数法） =================
// 从 fp 读取一个以数字开头的数字常量到 out；*valid=1 合法、0 格式错误
void readNumber(FILE* fp, int first, char* out, int outSize, int* valid) {
    int n = 0, c;
    *valid = 1;//1为合法
    out[n++] = (char)first;

    // ---- 十六进制：0x 开头 ----
    if (first == '0') {
        c = fgetc(fp);
        if (c == 'x' || c == 'X') {
            out[n++] = (char)c;
            int hexCount = 0;
            c = fgetc(fp);
            while (c != EOF && isxdigit((unsigned char)c)) {
                if (n < outSize - 1) out[n++] = (char)c;
                hexCount++;
                c = fgetc(fp);
            }
            // 非法后缀（如 G、字母、下划线）一并收入，便于报错定位
            if (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
                while (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
                    if (n < outSize - 1) out[n++] = (char)c;
                    c = fgetc(fp);
                }
                *valid = 0;//更改为非法状态
            }
            if (hexCount == 0) *valid = 0;       // 0x 后缺少十六进制数字
            if (c != EOF) ungetc(c, fp);
            out[n] = '\0';
            return;
        }
        if (c != EOF) ungetc(c, fp);             // 非 0x，退回按普通数处理
    }

    // ---- 整数部分 ----
    c = fgetc(fp);
    while (c != EOF && isdigit((unsigned char)c)) {
        if (n < outSize - 1) out[n++] = (char)c;
        c = fgetc(fp);
    }

    // ---- 小数部分：小数点后必须有数字 ----
    if (c == '.') {
        int c2 = fgetc(fp);
        if (c2 != EOF && isdigit((unsigned char)c2)) {
            if (n < outSize - 1) out[n++] = '.';
            if (n < outSize - 1) out[n++] = (char)c2;
            c = fgetc(fp);
            while (c != EOF && isdigit((unsigned char)c)) {
                if (n < outSize - 1) out[n++] = (char)c;
                c = fgetc(fp);
            }
        } else {
            if (c2 != EOF) ungetc(c2, fp);
            ungetc('.', fp);                     // 点号单独成 token
        }
    }

    // ---- 科学计数法：e 后可选正负号，必须有数字 ----
    if (c == 'e' || c == 'E') {
        if (n < outSize - 1) out[n++] = (char)c;
        int c2 = fgetc(fp);
        if (c2 == '+' || c2 == '-') {
            if (n < outSize - 1) out[n++] = (char)c2;
            c2 = fgetc(fp);
        }
        int expCount = 0;
        while (c2 != EOF && isdigit((unsigned char)c2)) {
            if (n < outSize - 1) out[n++] = (char)c2;
            expCount++;
            c2 = fgetc(fp);
        }
        if (expCount == 0) {                     // 残缺科学计数法（如 1.05e）
            *valid = 0;                          // 保留残缺文本用于报错
        }
        if (c2 != EOF) {
            if (isalnum((unsigned char)c2) || c2 == '_') *valid = 0;
            ungetc(c2, fp);
        }
    } else if (c != EOF) {
        if (isalpha((unsigned char)c) || c == '_') {
            // 数字带非法字母后缀（如 12ab）：收进来报错
            while (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
                if (n < outSize - 1) out[n++] = (char)c;
                c = fgetc(fp);
            }
            *valid = 0;
        }
        if (c != EOF) ungetc(c, fp);
    }
    out[n] = '\0';
}

// ================= v0.5 块注释处理（单元：读取并校验 /* */ 闭合） =================
// 读到 "/*" 后调用，消费直到 "*/"；返回 1=正常闭合，0=缺少 */（未闭合）
int readBlockComment(FILE* fp, int* line) {
    int prev = 0, cur;
    while ((cur = fgetc(fp)) != EOF && cur != '#') {
        if (cur == '\n') (*line)++;
        if (prev == '*' && cur == '/') return 1;
        prev = cur;
    }
    if (cur == '#') ungetc(cur, fp);             // # 不被注释吞掉，留作文件结束
    return 0;
}

// ================= v0.3 词法扫描：逐字符识别 38 种编码 =================
// 没读取一个字符 先排除#，在判断\n 这两是一定的token分割点
// 顺着伪代码的思路，标识符关键字放在一起，数字常熟类放一起
int scanTokens(const char* filename, TokenList* tl,
               NameTable* nameL, NameTable* constL, ErrLog* err) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filename);
        return -1;
    }

    int errors = 0;
    int line = 1, ch;
    while ((ch = fgetc(fp)) != EOF) {

        if (ch == '#') {                        // 38 文件结束符
            tlAdd(tl, 38, "#", line);
            break;
        }
        if (ch == '\n') {                       // v0.5：换行符恢复 class=37 输出
            tlAdd(tl, 37, "\\n", line);
            line++;
            continue;
        }
        if (isspace((unsigned char)ch)) continue;

        // ---------- 标识符 / 关键字 ----------
        if (isalpha((unsigned char)ch) || ch == '_') {
            char word[MAX_SEM];
            int n = 0;
            word[n++] = (char)ch;
            int c2;
            while ((c2 = fgetc(fp)) != EOF
                   && (isalnum((unsigned char)c2) || c2 == '_')) {
                if (n < MAX_SEM - 1) word[n++] = (char)c2;
            }
            word[n] = '\0';
            if (c2 != EOF) ungetc(c2, fp);
            int kc = keywordClass(word);
            if (kc) {
                tlAdd(tl, kc, word, line);
            } else if (unsupportedKeyword(word)) {
                // double 等：属于其他语言关键字但不在本编码表 -> 错误，不进三表
                errAdd(err, line, "未支持的关键字 '%s'（不在编码表中）", word);
                errors++;
            } else {
                // swich / retrun 等拼写错误按一般标识符处理，不报错
                tlAdd(tl, 11, word, line);
                ntAddUnique(nameL, word);        // 标识符登记 nameL
            }
        }
        // ---------- 数字（常数，登记 constL） ----------
        else if (isdigit((unsigned char)ch)) {
            char num[MAX_SEM];
            int valid = 1;
            readNumber(fp, ch, num, MAX_SEM, &valid);
            if (!valid) {
                errAdd(err, line, "数字常量格式错误: '%s'", num);
                errors++;
            } else {
                tlAdd(tl, 12, num, line);
                ntAddUnique(constL, num);        // 常数登记 constL
            }
        }
        // ---------- 运算符与分隔符 ----------
        else {
            switch (ch) {
            case '+': tlAdd(tl, 13, "+", line); break;
            case '-': tlAdd(tl, 14, "-", line); break;
            case '*': {                         // v0.5：*/ 缺少 /* 报错
                int c2 = fgetc(fp);
                if (c2 == '/') {
                    errAdd(err, line, "注释缺少 /*：出现孤立的 \"*/\"");
                    errors++;
                } else {
                    if (c2 != EOF) ungetc(c2, fp);
                    tlAdd(tl, 15, "*", line);
                }
                break;
            }
            case '/': {                         // v0.5：/* 开始块注释
                int c2 = fgetc(fp);
                if (c2 == '*') {
                    int startLine = line;
                    if (!readBlockComment(fp, &line)) {
                        errAdd(err, startLine, "块注释缺少 */：注释未闭合");
                        errors++;
                    }
                } else {
                    if (c2 != EOF) ungetc(c2, fp);
                    tlAdd(tl, 16, "/", line);
                }
                break;
            }
            case '%': tlAdd(tl, 17, "%", line); break;
            case '>': {
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 19, ">=", line);
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 18, ">", line); }
                break;
            }
            case '<': {
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 21, "<=", line);
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 20, "<", line); }
                break;
            }
            case '!': {
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 22, "!=", line);
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 24, "!", line); }
                break;
            }
            case '=': {
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 23, "==", line);
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 28, "=", line); }
                break;
            }
            case '&': {
                int c2 = fgetc(fp);
                if (c2 == '&') tlAdd(tl, 25, "&&", line);
                else {
                    if (c2 != EOF) ungetc(c2, fp);
                    errAdd(err, line, "孤立的 '&'（逻辑与应为 &&）");
                    errors++;
                }
                break;
            }
            case '|': {
                int c2 = fgetc(fp);
                if (c2 == '|') tlAdd(tl, 26, "||", line);
                else {
                    if (c2 != EOF) ungetc(c2, fp);
                    errAdd(err, line, "孤立的 '|'（逻辑或应为 ||）");
                    errors++;
                }
                break;
            }
            case ',': tlAdd(tl, 27, ",", line); break;
            case '[': tlAdd(tl, 29, "[", line); break;
            case ']': tlAdd(tl, 30, "]", line); break;
            case '(': tlAdd(tl, 31, "(", line); break;
            case ')': tlAdd(tl, 32, ")", line); break;
            case '{': tlAdd(tl, 33, "{", line); break;
            case '}': tlAdd(tl, 34, "}", line); break;
            case ';': tlAdd(tl, 35, ";", line); break;
            case '.': tlAdd(tl, 36, ".", line); break;
            default:                            // v0.5：@、: 等编码表外字符
                errAdd(err, line, "无法识别的非法字符 '%c' (ASCII %d)", ch, ch);
                errors++;
                break;
            }
        }
    }

    if (tl->size == 0 || tl->items[tl->size - 1].tokenClass != 38) {
        tlAdd(tl, 38, "#", line);
    }
    fclose(fp);
    return errors;
}

// ================= CSV 工具 =================
// 单个字段：逗号/引号/换行出现时整体加双引号，内部双引号转义为两个
void writeCsvField(FILE* fp, const char* s) {
    int needQuote = 0;
    for (const char* p = s; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needQuote = 1;
            break;
        }
    }
    if (!needQuote) { fputs(s, fp); return; }
    fputc('"', fp);
    for (const char* p = s; *p; p++) {
        if (*p == '"') fputc('"', fp);
        fputc(*p, fp);
    }
    fputc('"', fp);
}

// ① token 表 CSV（v0.6b）：编号,单词,属性语义Seman,编码描述,行号
// Seman 规则：标识符 -> nameL 编号；常数 -> constL 编号；其余（关键字/运算符/分隔符/换行/#）-> NULL
int writeTokenCsv(const TokenList* tl, const NameTable* nameL,
                  const NameTable* constL, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("无法写入 %s\n", filename); return 0; }
    fprintf(fp, "token_no,token_seman,Seman,token_desc,line_no\n");
    for (int i = 0; i < tl->size; i++) {//一个个写
        char desc[128];
        int cls = tl->items[i].tokenClass;
        classDesc(cls, tl->items[i].seman, desc);
        fprintf(fp, "%d,", i + 1);
        writeCsvField(fp, tl->items[i].seman);
        fputc(',', fp);
        // v0.6b：只有标识符(11)/常数(12)能在符号表中查到编号，其余写 NULL
        int semanRef = 0;
        if (cls == 11) {
            semanRef = ntIndex(nameL, tl->items[i].seman);
        } else if (cls == 12) {
            semanRef = ntIndex(constL, tl->items[i].seman);
        }
        if (semanRef > 0) fprintf(fp, "%d", semanRef);
        else fputs("NULL", fp);
        fputc(',', fp);
        writeCsvField(fp, desc);
        fprintf(fp, ",%d\n", tl->items[i].line);
    }
    fclose(fp);
    printf("已保存 token 表 -> %s（%d 个 token，含 Seman 属性语义列）\n", filename, tl->size);
    return 1;
}

// ② nameL 表 CSV：编号,变量名单词
int writeNameCsv(const NameTable* nt, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("无法写入 %s\n", filename); return 0; }
    fprintf(fp, "name_no,name\n");
    for (int i = 0; i < nt->size; i++) {
        fprintf(fp, "%d,", i + 1);
        writeCsvField(fp, nt->items[i]);
        fputc('\n', fp);
    }
    fclose(fp);
    printf("已保存 nameL 变量名表 -> %s（%d 个变量名）\n", filename, nt->size);
    return 1;
}

// ③ constL 表 CSV：编号,常数
int writeConstCsv(const NameTable* nt, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("无法写入 %s\n", filename); return 0; }
    fprintf(fp, "const_no,const_value\n");
    for (int i = 0; i < nt->size; i++) {
        fprintf(fp, "%d,", i + 1);
        writeCsvField(fp, nt->items[i]);
        fputc('\n', fp);
    }
    fclose(fp);
    printf("已保存 constL 常数表 -> %s（%d 个常数）\n", filename, nt->size);
    return 1;
}

// ================= 控制台 + output.txt 综合报告 =================
//可读性，用于打印到控制台）也可传 `fopen` 得到的文件指针，比csv还顺眼的
void dumpReport(FILE* fp, const TokenList* tl,
                const NameTable* nameL, const NameTable* constL) {
    fprintf(fp, "================ token 表（共 %d 个）================\n", tl->size);
    fprintf(fp, "%-6s  %-10s  %-30s  %s\n", "编号", "单词", "编码描述", "行号");
    for (int i = 0; i < tl->size; i++) {
        char desc[128];
        classDesc(tl->items[i].tokenClass, tl->items[i].seman, desc);
        fprintf(fp, "%-6d  %-10s  %-30s  %d\n", i + 1, tl->items[i].seman,
                desc, tl->items[i].line);
    }
    fprintf(fp, "\n================ nameL 变量名表（共 %d 个）================\n",
            nameL->size);
    fprintf(fp, "%-6s  %s\n", "编号", "变量名单词");
    for (int i = 0; i < nameL->size; i++) {
        fprintf(fp, "%-6d  %s\n", i + 1, nameL->items[i]);
    }
    fprintf(fp, "\n================ constL 常数表（共 %d 个）================\n",
            constL->size);
    fprintf(fp, "%-6s  %s\n", "编号", "常数");
    for (int i = 0; i < constL->size; i++) {
        fprintf(fp, "%-6d  %s\n", i + 1, constL->items[i]);
    }
}

// v0.5：取文件名主干（去目录、去扩展名）作为输出文件后缀
void fileTag(const char* path, char* tag, int tagSize) {
    const char* base = path;
    for (const char* p = path; *p; p++) {
        if (*p == '\\' || *p == '/') base = p + 1;
    }
    strncpy(tag, base, tagSize - 1);
    tag[tagSize - 1] = '\0';
    char* dot = strrchr(tag, '.');
    if (dot) *dot = '\0';
}

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // v0.5：可命令行传入输入文件，缺省 input.txt；输出按输入名加后缀，
    // 使用条件（三目）运算符 ? :，作用是：如果用户在命令行提供了参数，就用它作为输入文件名；否则默认使用 "input.txt"。
    //  argv[1]，即第一个用户提供的命令行参数
    const char* inputName = (argc > 1) ? argv[1] : "input.txt";
    char tag[80];
    fileTag(inputName, tag, sizeof(tag));
    printf("词法分析 v0.6b：扫描 %s，遇 '#' 结束，输出三表(token 含 Seman 列)+错误日志\n", inputName);

    char fname[160];
    TokenList tl;
    NameTable nameL, constL;
    ErrLog err;
    tlInit(&tl);
    ntInit(&nameL);
    ntInit(&constL);
    errInit(&err);

    int errors = scanTokens(inputName, &tl, &nameL, &constL, &err);
    if (errors < 0) {
        tlFree(&tl); ntFree(&nameL); ntFree(&constL); errFree(&err);
        return 1;
    }

    // 控制台报告
    dumpReport(stdout, &tl, &nameL, &constL);

    // 三个 CSV（按输入名区分）；v0.6b 起 token 表带 Seman 属性语义列
    snprintf(fname, sizeof(fname), "output_token_%s.csv", tag);
    writeTokenCsv(&tl, &nameL, &constL, fname);
    snprintf(fname, sizeof(fname), "output_nameL_%s.csv", tag);
    writeNameCsv(&nameL, fname);
    snprintf(fname, sizeof(fname), "output_constL_%s.csv", tag);
    writeConstCsv(&constL, fname);

    // v0.5：错误日志文件
    snprintf(fname, sizeof(fname), "error_log_%s.txt", tag);
    writeErrLog(&err, fname);

    // 可读综合文本
    snprintf(fname, sizeof(fname), "output_%s.txt", tag);
    FILE* rep = fopen(fname, "w");
    if (rep) { dumpReport(rep, &tl, &nameL, &constL); fclose(rep); }

    tlFree(&tl);
    ntFree(&nameL);
    ntFree(&constL);
    errFree(&err);

    if (errors > 0) {
        printf("\n完成，但发现 %d 个词法错误。\n", errors);
        return 1;
    }
    printf("\n词法分析成功。\n");
    return 0;
}
