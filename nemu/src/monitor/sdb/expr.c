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

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_NUM, TK_EQ, TK_MULTIPLY, TK_DIVIDE, TK_PLUS, TK_MINUS, TK_PARL, TK_PARR
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +",  TK_NOTYPE},              // spaces
  {"(0|[1-9][0-9]*)", TK_NUM},     // numbers
  {"==",  TK_EQ},                  // equal
  {"\\*", TK_MULTIPLY},            // multiply
  {"/",   TK_DIVIDE},              // divide
  {"\\+", TK_PLUS},                // plus
  {"\\-", TK_MINUS},               // minus
  {"\\(", TK_PARL},                // left parenthesis
  {"\\)", TK_PARR},                // right parenthesis

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[65536] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;
static bool legal_flag __attribute__((used)) = 1;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;
  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;
        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        tokens[nr_token].type = 0;
        for (int i = 0; i < 32; i++)
        {
          tokens[nr_token].str[i] = '\0';
        }
        
        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case TK_NUM:
            tokens[nr_token].type = rules[i].token_type;
            for (int index = 0; index < substr_len; index++)
            {
              tokens[nr_token].str[index] = *(substr_start + index);
            }
            nr_token++;
            break;
          default:
            tokens[nr_token].type = rules[i].token_type;
            nr_token++;
            break;
        } 
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  nr_token--;
  return true;
}

static bool check_parentheses(int p, int q){
  bool check_par_flag = true;
  int cnt = 0;
  while (p < q){
    if (tokens[p].type == TK_PARL) cnt++;
    else if (tokens[p].type == TK_PARR) cnt--;
    if (cnt == 0) check_par_flag = false;
    if (cnt < 0) legal_flag = 0; 
    p++;
  }
  if (tokens[p].type == TK_PARR) cnt--;
  if (cnt != 0)
  {
    check_par_flag = false;
    legal_flag = 0;
  }
  return check_par_flag;
}

word_t eval(int p, int q){
  if (legal_flag == 0) return 0;
  if (p > q)
  {
    legal_flag = 0;
    return 0;
  }
  else if (p == q)
  {
    if(tokens[p].type != TK_NUM)
    {
      legal_flag = 0;
      return 0;
    }
    else
    {
      int substr_len = strlen(tokens[p].str);
      word_t value = 0;
      for (int index = 0; index < substr_len; index++)
      {
        value = value * 10 + (tokens[p].str[index] - '0');
      }
      return value; 
    }
  }
  else if (check_parentheses(p, q) == 1)
  {
    return eval(p + 1, q - 1);
  }
  else
  {
    int op = p, index = p;
    int in_par = 0;
    while (index <= q)
    {
      if (tokens[index].type == TK_PARL) in_par++;
      else if (tokens[index].type == TK_PARR) in_par--;
      
      if (in_par == 0)
      {
        if ((tokens[index].type == TK_PLUS) || (tokens[index].type == TK_MINUS))
        {
          op = index;
        }
        else if ((tokens[index].type == TK_MULTIPLY) || (tokens[index].type == TK_DIVIDE))
        {
          if ((tokens[op].type != TK_PLUS) && (tokens[op].type != TK_MINUS)) op = index;
        }
      }
      index++;
    }
    
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);

    switch (tokens[op].type)
    {
    case TK_PLUS:
      return val1 + val2;
    case TK_MINUS:
      return val1 - val2;
    case TK_MULTIPLY:
      return val1 * val2;
    case TK_DIVIDE:
      return val1 / val2;
    default:
      legal_flag = 0;
      return 0;
    }
  }
}


word_t expr(char *e, bool *success) {
  
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  *success = true;

  /* TODO: Insert codes to evaluate the expression. */
  legal_flag = 1;
  word_t result = 0;
  result = eval(0, nr_token);

  if (legal_flag == 0)
  {
    printf("Illegal expression!\n");
    return 0;
  }

  return result;
}
