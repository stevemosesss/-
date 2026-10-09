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

/*v0.4 词法分析——三表结构token nameL constl

    逐字符扫描tian添加了记录行号方便后续纠错，所有的表按首次出现的编号，后续去重
    ① token 表  (编号, 单词, 编码描述, 行号)     —— token 序列
    ② nameL 表 (编号, 变量名单词)                —— 标识符符号表
    ③ constL表 (编号, 常数)                      —— 常数表(含整数/实数各种类型)
    当前选择分析说换行符 class=37 不再作为 token 输出，由于行号列已携带换行信息，
        但扫描时仍用它更新行号。

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

// tl：tokenList
void tlInit(TokenList* tl) {
    tl->capacity = 64;
    tl->size = 0;
    tl->items = (Token*)malloc(tl->capacity * sizeof(Token));
}
void tlFree(TokenList* tl) {
    free(tl->items);
    tl->items = NULL;
    tl->size = tl->capacity = 0;
}
void tlAdd(TokenList* tl, int cls, const char* seman, int line) {
    if (tl->size >= tl->capacity) {
        tl->capacity *= 2;
        tl->items = (Token*)realloc(tl->items, tl->capacity * sizeof(Token));
    }
    Token* t = &tl->items[tl->size++];
    t->tokenClass = cls;
    t->line = line;
    strncpy(t->seman, seman, MAX_SEM - 1);
    t->seman[MAX_SEM - 1] = '\0';
}

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

// ================= 编码描述（token 表第 3 列） =================
void classDesc(int cls, const char* seman, char* out) {
    if (cls >= 1 && cls <= 10) {                 // 关键字
        sprintf(out, "关键字%s: token.class=%d", seman, cls);
    } else if (cls == 11) {
        strcpy(out, "标识符: token.class=11");
    } else if (cls == 12) {                      // 数字：按形态区分正整数/正实数
        if (strchr(seman, '.')) strcpy(out, "正实数: token.class=12");
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

// ================= 词法扫描：逐字符识别 38 种编码 =================
// 没读取一个字符 先排除#，在判断\n 这两是一定的token分割点
// 顺着伪代码的思路，标识符关键字放在一起，数字常熟类放一起
int scanTokens(const char* filename, TokenList* tl,
               NameTable* nameL, NameTable* constL) {
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
        if (ch == '\n') {                       // 37 不产出 token，仅更新行号
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
            if (kc) tlAdd(tl, kc, word, line);
            else {
                tlAdd(tl, 11, word, line);
                ntAddUnique(nameL, word);        // 标识符登记 nameL
            }
        }
        // ---------- 数字（常数，登记 constL） ----------
        else if (isdigit((unsigned char)ch)) {
            char num[MAX_SEM];
            int n = 0;
            num[n++] = (char)ch;
            int c2;
            while ((c2 = fgetc(fp)) != EOF && isdigit((unsigned char)c2)) {
                if (n < MAX_SEM - 1) num[n++] = (char)c2;
            }
            if (c2 == '.') {                    // 小数点后须有数字才是实数
                int c3 = fgetc(fp);
                if (c3 != EOF && isdigit((unsigned char)c3)) {
                    if (n < MAX_SEM - 1) num[n++] = '.';
                    if (n < MAX_SEM - 1) num[n++] = (char)c3;
                    while ((c3 = fgetc(fp)) != EOF && isdigit((unsigned char)c3)) {
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
            tlAdd(tl, 12, num, line);
            ntAddUnique(constL, num);            // 常数登记 constL
        }
        // ---------- 运算符与分隔符 ----------
        else {
            switch (ch) {
            case '+': tlAdd(tl, 13, "+", line); break;
            case '-': tlAdd(tl, 14, "-", line); break;
            case '*': tlAdd(tl, 15, "*", line); break;
            case '/': tlAdd(tl, 16, "/", line); break;
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
                    printf("[词法错误] 第%d行: 孤立的 '&'（逻辑与应为 &&）\n", line);
                    errors++;
                }
                break;
            }
            case '|': {
                int c2 = fgetc(fp);
                if (c2 == '|') tlAdd(tl, 26, "||", line);
                else {
                    if (c2 != EOF) ungetc(c2, fp);
                    printf("[词法错误] 第%d行: 孤立的 '|'（逻辑或应为 ||）\n", line);
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
            default:
                printf("[词法错误] 第%d行: 无法识别字符 '%c' (ASCII %d)\n",
                       line, ch, ch);
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

// ① token 表 CSV：编号,单词,编码描述,行号
int writeTokenCsv(const TokenList* tl, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) { printf("无法写入 %s\n", filename); return 0; }
    fprintf(fp, "token_no,token_seman,token_desc,line_no\n");
    for (int i = 0; i < tl->size; i++) {
        char desc[128];
        classDesc(tl->items[i].tokenClass, tl->items[i].seman, desc);
        fprintf(fp, "%d,", i + 1);
        writeCsvField(fp, tl->items[i].seman);
        fputc(',', fp);
        writeCsvField(fp, desc);
        fprintf(fp, ",%d\n", tl->items[i].line);
    }
    fclose(fp);
    printf("已保存 token 表 -> %s（%d 个 token）\n", filename, tl->size);
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

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    printf("词法分析 v0.4：扫描 input.txt，遇 '#' 结束，输出三表\n");

    TokenList tl;
    NameTable nameL, constL;
    tlInit(&tl);
    ntInit(&nameL);
    ntInit(&constL);

    int errors = scanTokens("input.txt", &tl, &nameL, &constL);
    if (errors < 0) {
        tlFree(&tl); ntFree(&nameL); ntFree(&constL);
        return 1;
    }

    // 控制台报告
    dumpReport(stdout, &tl, &nameL, &constL);

    // 三个 CSV
    writeTokenCsv(&tl, "output_token.csv");
    writeNameCsv(&nameL, "output_nameL.csv");
    writeConstCsv(&constL, "output_constL.csv");

    // 可读综合文本
    FILE* rep = fopen("output.txt", "w");
    if (rep) { dumpReport(rep, &tl, &nameL, &constL); fclose(rep); }

    tlFree(&tl);
    ntFree(&nameL);
    ntFree(&constL);

    if (errors > 0) {
        printf("\n完成，但发现 %d 个词法错误。\n", errors);
        return 1;
    }
    printf("\n词法分析成功。\n");
    return 0;
}
