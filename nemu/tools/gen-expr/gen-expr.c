/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static int pointer = 0;
static bool overflow_flag = 0;
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static uint32_t choose(uint32_t n){
  return rand() % n;
}

static void buf_clean(){
  for (int i = 0; i < pointer; i++)
  {
    buf[i] = '\0';
  }
  pointer = 0;
}

static bool check_overflow(int pointer, uint32_t substr_len){
  if (pointer + substr_len > 65534) return 1;
  return 0;
}

static void gen_num(){
  if (overflow_flag == 1) return;
  uint32_t substr_len = choose(9); 
  overflow_flag = check_overflow(pointer, substr_len + 1);
  int ascii_zero = (int)'0';
  if (overflow_flag == 0)
  {
    if (substr_len == 0)
    {
      buf[pointer++] = (char)(choose(10) + ascii_zero);
    }
    else
    {
      buf[pointer++] = (char)(choose(9) + 1 + ascii_zero);
      for (uint32_t i = 1; i <= substr_len; i++)
      {
        buf[pointer++] = (char)(choose(10) + ascii_zero);
      }  
    }
  }
}

static void gen_parl(){
  if (overflow_flag == 1) return; 
  overflow_flag = check_overflow(pointer, 1);
  if (overflow_flag == 0)
  {
    buf[pointer++] = '(';
  }
}

static void gen_parr(){
  if (overflow_flag == 1) return; 
  overflow_flag = check_overflow(pointer, 1);
  if (overflow_flag == 0)
  {
    buf[pointer++] = ')';
  }
}

static void gen_rand_op(){
  if (overflow_flag == 1) return; 
  overflow_flag = check_overflow(pointer, 1);
  if (overflow_flag == 0)
  {
    switch (choose(4))
    {
      case 0:
        buf[pointer++] = '+';
        break;
      case 1:
        buf[pointer++] = '-';
        break;
      case 2:
        buf[pointer++] = '*';
        break;  
      default:
        buf[pointer++] = '/';
        break;
    }
  }
}

static void gen_space(){
  if (overflow_flag == 1) return; 
  uint32_t space_num = choose(4);
  overflow_flag = check_overflow(pointer, space_num);
  if (overflow_flag == 0)
  {
    for (int i = 0; i < space_num; i++)
    {
      buf[pointer++] = ' ';
    }
  }
}

static void gen_rand_expr() {
  if (overflow_flag == 1) return;
  gen_space();
  switch (choose(3))
  {
  case 0:
    gen_num();
    break;
  case 1:
    gen_parl();
    gen_rand_expr();
    gen_parr();
    break;
  default:
    gen_rand_expr();
    gen_rand_op();
    gen_rand_expr();
    break;
  }
  gen_space();
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  
  FILE *fp = NULL;
  int i = 0;
  int result = 0;
  
  for (i = 0; i < loop; i ++) {
    int ret = 1;
    while (ret != 0)
    {
      buf_clean();
      gen_rand_expr();
      while (overflow_flag == 1)
      {
        overflow_flag = 0;
        buf_clean();
        gen_rand_expr();
      }
      buf[pointer] = '\0';
      
      sprintf(code_buf, code_format, buf);

      FILE *fp = fopen("/tmp/.code.c", "w");
      assert(fp != NULL);
      fputs(code_buf, fp);
      fclose(fp);

      ret = system("gcc -Wall -Werror /tmp/.code.c -o /tmp/.expr >/dev/null 2>&1");
    }
    
    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    ret = fscanf(fp, "%d", &result);
    pclose(fp);
    printf("%u %s\n", result, buf);
  }
  return 0;
}
