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
#include <cpu/cpu.h>
#include <memory/paddr.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

static int cmd_si(char *args){
  if (args == NULL){
    cpu_exec(1);
  }
  else{
    char *endptr = NULL;
    uint64_t n = 0;
    n = strtoull(args, &endptr, 10);
    if (*endptr != '\0'){
      printf("\"%s\" is not a valid number!\n", args);
      return 0;
    }  
    cpu_exec(n);
  }
  return 0;
}

static int cmd_info(char *args){
  if (*args == 'r'){
    isa_reg_display();
  }
  else if (*args == 'w'){
    printf("Function not implemented yet!\n");
  }
  else printf("Invalid argument!\n");
  return 0;
}

static int cmd_x(char *args){
  char *endptr = NULL;
  char *n_char = strtok(NULL, " ");
  char *paddr_char = strtok(NULL, " ");
  if (n_char == NULL || paddr_char == NULL){
    printf("Missing argument(s)!\n");
    return 0;
  }
  
  if (paddr_char[0] == '0' && (paddr_char[1] == 'x' || paddr_char[1] == 'X')){
    paddr_char = paddr_char + 2;
  }
  
  paddr_t paddr = 0;
  if (strtoul(paddr_char, &endptr, 16) < PMEM_LEFT || strtoul(paddr_char, &endptr, 16) > PMEM_RIGHT){
    printf("Given address overflow!\n");
    return 0;
  }
  else if (*endptr != '\0'){
    printf("\"%s\" is not a valid number!\n", paddr_char);
    return 0;
  }
  else{
    paddr = strtoul(paddr_char, &endptr, 16);
  }
  
  uint32_t n = strtoul(n_char, &endptr, 10);
  if (*endptr != '\0'){
    printf("\"%s\" is not a valid number!\n", n_char);
    return 0;
  }
  else if (paddr + 4*n - 1 > PMEM_RIGHT)
  {
    printf("Memory overflow!\n");
    return 0;
  }

  uint8_t *mem = guest_to_host(paddr);

  for(int i = 0; i < n; i++){
    printf("0x%08X: %02X %02X %02X %02X\n", paddr + 4*i, *(mem + 4*i + 3), *(mem + 4*i + 2), *(mem + 4*i + 1), *(mem + 4*i));
  }
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Single-step execution mode, run given number of instructions, default value is 1", cmd_si },
  { "info", "r: Print register status\nw: print watchpoint information", cmd_info },
  { "x", "Take the given hexadecimal number as the starting memory address, and output the contents of N consecutive 4-byte blocks in hexadecimal format", cmd_x },
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
