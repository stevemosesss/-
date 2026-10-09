/*词法分析方式*/
/*  标识符: token.class=1 */
/*  无符号数: token.class=2 */
/*    加号: token.class=3 */
/*    分号: token.class=4 */
/*    冒号: token.class=5 */
/*  赋值号: token.class=6 */
/*  小于号: token.class=7 */
/*小于等于号: token.class=8 */
/*文件结束符'#': token.class=9*/
/*没有被识别的符号则直接报错
scanner is false! */
#include "stdio.h"
#include "conio.h"
#include "string.h"
#define MAX 100
struct a{
      int class;
      char seman[10];
} token[80];

char input[MAX];
int i,k=0;

int isletter(char x)
{
  if(x>='a'&&x<='z') return 1;
    else return 0;
}

int isdigit(char x)
{
  if(x>='0'&&x<='9') return 1;
    else return 0;
}
     /*输出识别结果*/
void print_token(int k)
{
  int i;
  printf("\n token list:");
  for(i=0;i<k;i++)

        printf("\n %3d token.class:%5d  token.seman:%s",i+1,token[i].class,token[i].seman );

}

void error(int k)
{
  printf(" \n\nscanner is false! ");
  print_token(k);
  getch();
  exit(0);
}


int next_token()
{
  char ch,name[10] ;
  int state,l;
  ch=input[i];
  while(ch==' '||ch=='\t'||ch=='\n') ch=input[++i];
  state=0;
  while(1)
   {
    switch(state)
     {
       case 0:if(isletter(ch)){ l=0;name[l++]=ch;state=1;}
               else if(isdigit(ch)) { l=0;name[l++]=ch;state=2;}
                else if(ch=='+') state=3;
                 else if(ch==';') state=4;
                  else if(ch=='=') state=9;
                   else if(ch=='<') state=6;
                    else if(ch=='#') {i--;return;}
                     else  error(k);  break;
       case 1:ch=input[++i];
              if(isletter(ch)||isdigit(ch)){ name[l++]=ch; state=1;}
                else state=7; break;
       case 2:ch=input[++i];
              if(isdigit(ch)) { name[l++]=ch; state=2;}
                else state=8; break;
       case 3:token[k].class=3;strcpy(token[k].seman,"+");k++;return;
       case 4:token[k].class=4;strcpy(token[k].seman,";");k++;return;
       //case 5://ch=input[++i];
               //state=9;  break;
       case 6:ch=input[++i];
              if(ch=='=') state=11; else state=12; break;
       case 7:token[k].class=1;name[l]='\0';strcpy(token[k].seman,name); k++;i--; return;
       case 8:token[k].class=2;name[l]='\0';strcpy(token[k].seman,name);k++;i--;return;
       case 9:token[k].class=6;strcpy(token[k].seman,"=");k++; return;
       case 10:token[k].class=5;strcpy(token[k].seman,":");k++;i--;return;
       case 11:token[k].class=8;strcpy(token[k].seman,"<=");k++;return;
       case 12:token[k].class=7;strcpy(token[k].seman,"<");k++;i--;return;
       default: error(k);
     }
   }
}

main()
{
    int j=0;
    char ch;
       /*输入源程序,除空格、换行符外以'#'结束*/
    printf("input source program and end of #:");

    while((ch=getchar())!='#') input[j++]=ch;
    input[j]='#';
       /*词法分析开始*/
    i=0;
    while(input[i]!='#')
      {
        next_token();
        i++;
       }
    if(input[i]=='#')
       { token[k].class=9; strcpy(token[k].seman,"#");
         k++;
         printf("\n\nscanner is succend!");
         print_token(k);
       }

  getch();
}