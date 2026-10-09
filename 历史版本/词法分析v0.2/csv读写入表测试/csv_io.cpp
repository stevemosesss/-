#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <windows.h> //包含ConsoleOutputCP和ConsoleCP函数  
// utf8编译： g++ -finput-charset=UTF-8 -fexec-charset=UTF-8 *.cpp -o io.exe
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

/*设计词法分析：目前只能是空格分隔的token导出到output.csv
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
// 字符串数组结构 存整个程序段到数组
typedef struct {
    char** items;      // 指向字符串数组
    int size;          // 当前元素个数
    int capacity;      // 容量
} StringArray;

// 初始化
void initArray(StringArray* arr) {
    arr->capacity = 10;
    arr->size = 0;
    arr->items = (char**)malloc(arr->capacity * sizeof(char*));
}

// 扩容
void resize(StringArray* arr) {
    arr->capacity *= 2;
    arr->items = (char**)realloc(arr->items, arr->capacity * sizeof(char*));
}

// 释放内存
void freeArray(StringArray* arr) {
    for (int i = 0; i < arr->size; i++) {
        free(arr->items[i]);
    }
    free(arr->items);
    arr->size = 0;
    arr->capacity = 0;
}

//分隔token 编码存储
StringArray readWordsFromFile(const char* filename) {

    StringArray arr;
    initArray(&arr);
    
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filename);
        return arr;
    }
    
    char ch;
    char word[1024];        // 单个单词缓冲区
    int wordLen = 0;
    //好像wangji 编码了
    
    while ((ch = fgetc(fp)) != EOF) {
        if (ch == '#') {    // 遇到 # 停止（可选）
            break;
        }
        
        if (isspace(ch)) {  // 空白符：单词结束
            if (wordLen > 0) {
                word[wordLen] = '\0';
                
                // 扩容检查
                if (arr.size >= arr.capacity) {
                    resize(&arr);
                }
                
                // 复制到数组
                arr.items[arr.size] = (char*)malloc((wordLen + 1) * sizeof(char));
                strcpy(arr.items[arr.size], word);
                arr.size++;
                
                wordLen = 0;    // 重置
            }
        } else {
            word[wordLen++] = ch;
        }
    }
    
    // 处理最后一个单词（文件末尾可能没有空白符）
    if (wordLen > 0) {
        word[wordLen] = '\0';
        if (arr.size >= arr.capacity) {
            resize(&arr);
        }
        arr.items[arr.size] = (char*)malloc((wordLen + 1) * sizeof(char));
        strcpy(arr.items[arr.size], word);
        arr.size++;
    }
    
    fclose(fp);
    return arr;
}

void CopyToFile(const StringArray* arr, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        printf("无法打开文件: %s\n", filename);
        return;
    }
    
    for (int i = 0; i < arr->size; i++) {
        const char* word = arr->items[i];
        
        // 检查是否需要转义（包含逗号、引号、换行）
        int needQuote = 0;
        for (const char* p = word; *p; p++) {
            if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
                needQuote = 1;
                break;
            }
        }
        
        if (needQuote) {
            fprintf(fp, "\"");
            for (const char* p = word; *p; p++) {
                if (*p == '"') {
                    fprintf(fp, "\"\"\"");  // 双引号转义
                } else {
                    fputc(*p, fp);
                }
            }
            fprintf(fp, "\",");
        } else {
            fprintf(fp, "%s,", word);
        }
    }
    
    fclose(fp);
    printf("已将 %d 个单词写入 %s\n", arr->size, filename);
}

int main(){

     SetConsoleOutputCP(CP_UTF8);//控制台输出编码为UTF-8
    SetConsoleCP(CP_UTF8);//控制台输入编码为UTF-8

     FILE *fp1;
    fp1=fopen("input.txt","r");//r读取：
     FILE *fp2;
    fp2=fopen("output.txt","w");//w写入：
    if(fp1==NULL||fp2==NULL)
    {
        cout<<"文件打开失败"<<endl;
        return 0;
    }
    char line[100];
    char ch;

    printf("词法分割：请输入内容，以 '#' 结束：\n");//用 fgetc / getchar 逐字符读取，遇到空白符（空格、制表符、换行等）时，一个单词结束，连续的空白符跳过，直到 EOF 结束
    // while ((ch = getchar()) != '#') {
    //     fputc(ch, fp2);
    // }
    StringArray words = readWordsFromFile("input.txt");
    CopyToFile(&words,"output.csv");//表中的字符串数组写入文件output.csv，并在每一个单词/符号后面加一个csv的逗号，自带转义
    
    fclose(fp1); //关闭文件
    fclose(fp2); //关闭文件
       return 0;
}

