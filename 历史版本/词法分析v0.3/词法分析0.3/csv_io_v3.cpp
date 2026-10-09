#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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

/*v0.3 词法分析（当前版本）
    逐字符扫描，不依赖空格分隔：连续字母数字组成标识符/关键字，
    数字支持小数点（正实数），运算符支持双字符（>= <= != == && ||）
    二元组 token = (token.class 种别编码, token.seman 单词值)
    全部 token 按出现顺序存入结构体数组 -> output.csv（token表序列）

v0.2 旧设计（已淘汰）：/*设计词法分析：目前只能是空格分隔的token导出到output.csv
一般是逐个字符扫描连续处理　存入同一个单词数组单元／单词便利（但是不支持注释与空格）
ｘｉａｎｘｉｎｇｃｕｎｒｕ　符号表，这样在第二次遇到同一个符号／关键字／变量可以根据查找找到对应的token
最后要求是二元组/记号：单词的种别/类型与值

伪代码：
函数: tokenize(input_stream)
    初始化 token 列表
    ch = 读取第一个字符

    While ch != EOF:
        跳过空白和注释
        If ch == '#': break   // 遇到 # 停止（你的需求）

        Switch 字符分类:

        Case 'a'-'z','A'-'Z','_':
            进入"标识符状态"
            连续读取字母、数字、下划线
            构成标识符
            如果是关键字 → TOKEN_KEYWORD
            否则 → TOKEN_IDENTIFIER

        Case '0'-'9':
            进入"数字状态"
            连续读取数字 [0-9]
            可选小数点 → 浮点数
            可选指数 e+/-
            构成数字 → TOKEN_NUMBER

        Case '"':
            进入"字符串状态"
            读取直到闭合的 " 或 EOF
            支持转义 \\、\n、\t 等
            构成字符串 → TOKEN_STRING

        Case '\'':
            进入"字符状态"
            读取单个字符
            读取闭合的 '
            构成字符 → TOKEN_CHAR

        Case '(', ')', '{', '}', '[', ']', ';', ',':
            单字符分隔符 → TOKEN_DELIMITER

        Case '+', '-', '*', '/', '=', '<', '>', '!', '&', '|':
            进入"运算符尝试状态"
            查看下一个字符是否为 =
            (+=, -=, ==, <=, >=, !=, &&, || 等)
            构成运算符 → TOKEN_OPERATOR

        Default:
            无法识别的字符 → 报错或作为特殊符号

        记录 token 和行号
        读取下一个字符

    返回 token 列表

*/

// ================= token 二元组与序列（结构体数组） =================
#define MAX_SEM 64          // 单词值最大长度

typedef struct {
    int tokenClass;         // 种别编码 1-38（class 是 C++ 关键字，故用 tokenClass）
    char seman[MAX_SEM];    // 单词的符号值
} Token;// 单个token的属性

typedef struct {
    Token* items;           // token 结构体数组
    int size;               // 当前 token 个数
    int capacity;           // 容量
} TokenList;

// 初始化
void tlInit(TokenList* tl) {
    tl->capacity = 64;
    tl->size = 0;
    tl->items = (Token*)malloc(tl->capacity * sizeof(Token));
}

// 释放
void tlFree(TokenList* tl) {
    free(tl->items);
    tl->items = NULL;
    tl->size = 0;
    tl->capacity = 0;
}

// 向序列尾部追加一个 token（自动扩容）
void tlAdd(TokenList* tl, int cls, const char* seman) {
    if (tl->size >= tl->capacity) {
        tl->capacity *= 2;
        tl->items = (Token*)realloc(tl->items, tl->capacity * sizeof(Token));
    }
    Token* t = &tl->items[tl->size++];
    t->tokenClass = cls;
    strncpy(t->seman, seman, MAX_SEM - 1);
    t->seman[MAX_SEM - 1] = '\0';
}

// ================= 关键字表与查找 =================
int isKeywordClass(const char* word) {
    static const struct { const char* name; int cls; } KEYWORDS[] = {
        {"if", 1}, {"else", 2}, {"for", 3}, {"while", 4}, {"break", 5},
        {"return", 6}, {"continue", 7}, {"float", 8}, {"int", 9},
        {"char", 10}
    };
    for (unsigned i = 0; i < sizeof(KEYWORDS) / sizeof(KEYWORDS[0]); i++) {
        if (strcmp(word, KEYWORDS[i].name) == 0) {
            return KEYWORDS[i].cls;
        }
    }
    return 0;   // 不是关键字
}

// ================= 词法扫描：逐字符识别 38 种编码 =================

没读取一个字符 先排除#，在判断\n 这两是一定的token分割点
顺着伪代码的思路，标识符关键字放在一起，数字常熟类放一起
int scanTokens(const char* filename, TokenList* tl) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filename);
        return -1;
    }

    int errors = 0;
    int ch;
    while ((ch = fgetc(fp)) != EOF) {

        if (ch == '#') {                    // 38 文件结束符
            tlAdd(tl, 38, "#");
            break;
        }
        if (ch == '\n') {                   // 37 换行符（存为可见的 \n）
            tlAdd(tl, 37, "\\n");
            continue;
        }
        if (isspace((unsigned char)ch)) {   // 空格/制表符/回车符 跳过
            continue;
        }

        // ---------- 标识符 / 关键字（字母或下划线开头） ----------
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
            if (c2 != EOF) ungetc(c2, fp);  // 多读的分隔字符退回
            int kc = isKeywordClass(word);
            tlAdd(tl, kc != 0 ? kc : 11, word);  // 关键字 1-10，否则标识符 11
        }
        // ---------- 数字：正整数 / 零 / 正实数（均为 12） ----------
        else if (isdigit((unsigned char)ch)) {
            char num[MAX_SEM];
            int n = 0;
            num[n++] = (char)ch;
            int c2;
            while ((c2 = fgetc(fp)) != EOF
                   && isdigit((unsigned char)c2)) {
                if (n < MAX_SEM - 1) num[n++] = (char)c2;
            }
            // 小数点后必须还有数字才算正实数，否则点号单独成 token
            if (c2 == '.') {
                int c3 = fgetc(fp);
                if (c3 != EOF && isdigit((unsigned char)c3)) {
                    if (n < MAX_SEM - 1) num[n++] = '.';
                    if (n < MAX_SEM - 1) num[n++] = (char)c3;
                    while ((c3 = fgetc(fp)) != EOF
                           && isdigit((unsigned char)c3)) {
                        if (n < MAX_SEM - 1) num[n++] = (char)c3;
                    }
                    if (c3 != EOF) ungetc(c3, fp);
                } else {
                    if (c3 != EOF) ungetc(c3, fp);
                    ungetc('.', fp);
                }
            } else if (c2 != EOF) {
                ungetc(c2, fp);
            }
            num[n] = '\0';
            tlAdd(tl, 12, num);
        }
        // ---------- 运算符与分隔符 ----------
        else {
            switch (ch) {
            case '+': tlAdd(tl, 13, "+"); break;                    // 13 加号
            case '-': tlAdd(tl, 14, "-"); break;                    // 14 减号
            case '*': tlAdd(tl, 15, "*"); break;                    // 15 乘号
            case '/': tlAdd(tl, 16, "/"); break;                    // 16 除号
            case '%': tlAdd(tl, 17, "%"); break;                    // 17 取余
            case '>': {                                            // 18/19
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 19, ">=");
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 18, ">"); }
                break;
            }
            case '<': {                                            // 20/21
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 21, "<=");
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 20, "<"); }
                break;
            }
            case '!': {                                            // 22/24
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 22, "!=");
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 24, "!"); }
                break;
            }
            case '=': {                                            // 23/28
                int c2 = fgetc(fp);
                if (c2 == '=') tlAdd(tl, 23, "==");
                else { if (c2 != EOF) ungetc(c2, fp); tlAdd(tl, 28, "="); }
                break;
            }
            case '&': {                                            // 25
                int c2 = fgetc(fp);
                if (c2 == '&') tlAdd(tl, 25, "&&");
                else {
                    if (c2 != EOF) ungetc(c2, fp);
                    printf("[词法错误] 孤立的 '&'（逻辑与应为 &&）\n");
                    errors++;
                }
                break;
            }
            case '|': {                                            // 26
                int c2 = fgetc(fp);
                if (c2 == '|') tlAdd(tl, 26, "||");
                else {
                    if (c2 != EOF) ungetc(c2, fp);
                    printf("[词法错误] 孤立的 '|'（逻辑或应为 ||）\n");
                    errors++;
                }
                break;
            }
            case ',': tlAdd(tl, 27, ","); break;                    // 27 逗号
            case '[': tlAdd(tl, 29, "["); break;                    // 29 左方括号
            case ']': tlAdd(tl, 30, "]"); break;                    // 30 右方括号
            case '(': tlAdd(tl, 31, "("); break;                    // 31 左圆括号
            case ')': tlAdd(tl, 32, ")"); break;                    // 32 右圆括号
            case '{': tlAdd(tl, 33, "{"); break;                    // 33 左花括号
            case '}': tlAdd(tl, 34, "}"); break;                    // 34 右花括号
            case ';': tlAdd(tl, 35, ";"); break;                    // 35 分号
            case '.': tlAdd(tl, 36, "."); break;                    // 36 点号
            default:
                printf("[词法错误] 无法识别字符 '%c' (ASCII %d)\n", ch, ch);
                errors++;
                break;
            }
        }
    }

    // 文件直接结束而没有 '#'：补一个文件结束 token
    if (tl->size == 0 || tl->items[tl->size - 1].tokenClass != 38) {
        tlAdd(tl, 38, "#");
    }
    fclose(fp);
    return errors;
}

// ================= 控制台打印 token 序列 =================
void printTokens(const TokenList* tl) {
    printf("\n================ token 序列（共 %d 个）================\n",
           tl->size);
    printf("%-6s  %-12s  %s\n", "序号", "token.class", "token.seman");
    for (int i = 0; i < tl->size; i++) {
        printf("%-6d  %-12d  %s\n", i + 1, tl->items[i].tokenClass,
               tl->items[i].seman);
    }
}

// ================= 输出 CSV：token 表序列 =================
// 写单个 CSV 字段，逗号/引号/换行自动加双引号转义
void writeCsvField(FILE* fp, const char* s) {
    int needQuote = 0;
    for (const char* p = s; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            needQuote = 1;
            break;
        }
    }
    if (!needQuote) {
        fputs(s, fp);
        return;
    }
    fputc('"', fp);
    for (const char* p = s; *p; p++) {
        if (*p == '"') fputc('"', fp);
        fputc(*p, fp);
    }
    fputc('"', fp);
}

int writeCsv(const TokenList* tl, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("无法写入文件: %s\n", filename);
        return 0;
    }
    fprintf(fp, "token_no,token_class,token_seman\n");  // 表头
    for (int i = 0; i < tl->size; i++) {
        fprintf(fp, "%d,%d,", i + 1, tl->items[i].tokenClass);
        writeCsvField(fp, tl->items[i].seman);
        fputc('\n', fp);
    }
    fclose(fp);
    printf("已保存 token 表序列到 %s（%d 个 token，可直接用 Excel 打开）\n",
           filename, tl->size);
    return 1;
}

// ================= 输出可读文本表 =================
int writeTable(const TokenList* tl, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("无法写入文件: %s\n", filename);
        return 0;
    }
    fprintf(fp, "token 序列表（共 %d 个）\n", tl->size);
    fprintf(fp, "%-6s  %-12s  %s\n", "序号", "token.class", "token.seman");
    for (int i = 0; i < tl->size; i++) {
        fprintf(fp, "%-6d  %-12d  %s\n", i + 1, tl->items[i].tokenClass,
                tl->items[i].seman);
    }
    fclose(fp);
    return 1;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);  // 控制台输出编码为UTF-8
    SetConsoleCP(CP_UTF8);        // 控制台输入编码为UTF-8

    printf("词法分析 v0.3：扫描 input.txt，遇 '#' 结束\n");

    TokenList tl;
    tlInit(&tl);
    int errors = scanTokens("input.txt", &tl);
    if (errors < 0) {             // 文件打开失败
        tlFree(&tl);
        return 1;
    }

    printTokens(&tl);
    writeCsv(&tl, "output.csv");  // token 表序列（主输出）
    writeTable(&tl, "output.txt");// 同样内容的可读文本表

    tlFree(&tl);

    if (errors > 0) {
        printf("\n完成，但发现 %d 个词法错误，请检查源程序。\n", errors);
        return 1;
    }
    printf("\n词法分析成功。\n");
    return 0;
}
