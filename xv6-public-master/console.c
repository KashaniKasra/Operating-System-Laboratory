// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

//PAGEBREAK: 50
#define BACKSPACE 0x100
#define LEFTARROW 0xe4
#define RIGHTARROW 0xe5
#define UPARROW 0xe2
#define DOWNARROW 0xe3
#define CRTPORT 0x3d4
#define C(x) ((x)-'@')  // Control-x
#define INPUT_BUF 128

static void consputc(int);
static ushort *crt = (ushort*)P2V(0xb8000);  // CGA memory
static int panicked = 0;
static int lefts = 0;

static struct {
  struct spinlock lock;
  int locking;
} cons;

struct NON{
  int sign_n1;
  int sign_n2;
  uint S_n1;
  uint E_n1;
  uint S_n2;
  uint E_n2;
  uint op;
} non;

struct Input {
  char buf[INPUT_BUF];
  char buf_copy[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
  int flag_copy;
  int flag_paste;

} input = {.flag_copy = 0 , .flag_paste = 0};

struct History
{
  struct Input hist[10];
  int index ;
  int count ;
  int last ;
};

struct History history = {.index = -1 , .count = 0, .last = -1};


static void update_cursor(int move)
{
  int pos;

  // Get current cursor position
  outb(CRTPORT, 14);
  pos = inb(CRTPORT + 1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT + 1);

  switch(move){
    case 0:
      pos --;
      break;
    case 1:
      pos ++;
      break;
    default:
      break;
  }

  // Reset cursor position
  outb(CRTPORT, 14);
  outb(CRTPORT + 1, pos >> 8);
  outb(CRTPORT, 15);
  outb(CRTPORT + 1, pos);
}

static void shift_left_next_chars(int pos)
{
  for (int i = pos - 1; i < pos + lefts; i++)
    crt[i] = crt[i + 1];

  for (int i = input.e - lefts; i < input.e; i++){
    input.buf[i] = input.buf[i + 1];
    input.buf_copy[i] = input.buf_copy[i + 1];
  }
}

static void shift_right_next_chars(int pos)
{
  for (int i = pos + lefts; i > pos ; i--)
    crt[i] = crt[i - 1];

  for (int i = input.e + 1; i > input.e - lefts; i--){
    input.buf[i] = input.buf[i - 1];
    input.buf_copy[i] = input.buf_copy[i - 1];
  }
}

static void shift_left_previous_histories(){
  for (int i = 0; i < 9; i++) {
    history.hist[i] = history.hist[i+1]; 
  }
}

static void move_through_history(int move)
{
  lefts = 0;
  for ( int i = input.e ; i > input.w ; i-- ){
    if (input.buf[i - 1] != '\n'){
      consputc(BACKSPACE);
    }
  }
  if (move == 0) //Up
  {
    history.index --;

    input = history.hist[history.index];
    input.e -- ;
    input.buf[input.e] = '\0';
  }
  if (move == 1) //Down
  {
    if(history.index == -1)
      history.index ++;
    history.index ++;

    input = history.hist[history.index];
    input.e -- ;
    input.buf[input.e] = '\0';

  }

  for(int i = input.r; i < input.e; i++){
      consputc(input.buf[i]);
  }
}

static void XZ(){
  char* a = "XXX";
  release(&cons.lock);
  cprintf(a);
  acquire(&cons.lock);
}

static int is_command_history(){
  int j = 0;
  char hist[7] = {'h', 'i', 's', 't', 'o', 'r', 'y'};
  for(int i = input.r; i < input.e - 1; i++){
    if(input.buf[i] != hist[j])
      return 0;
    j++;
  }
  return 1;
}

static void print_history(){

  for(int i = 0; i < history.count; i++){
    release(&cons.lock);
    cprintf(&history.hist[i].buf[history.hist[i].r]);
    acquire(&cons.lock);
  }
}

static void clear_buf_copy(){
  for(int i = input.r; i < input.e; i++){
    input.buf_copy[i] = '\0';
  }
}

static int get_last_index_copy(){
  for(int i = input.e; i >= input.r; i--){
    if(input.buf_copy[i] == '1')
      return i;
  }
}

int is_c_in_numbers(char c){
  char numbers[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
  for(int i = 0; i < 10; i++){
    if(c == numbers[i])
      return 1;
  }
  return 0;
}

int is_c_in_operations(char c){
  char operations[4] = {'+', '-', '*', '/'};
  for(int i = 0; i < 4; i++){
    if(c == operations[i])
      return 1;
  }
  return 0;
}

static void
printint(int xx, int base, int sign)
{
  static char digits[] = "0123456789abcdef";
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    consputc(buf[i]);
}
//PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void
cprintf(char *fmt, ...)
{
  int i, c, locking;
  uint *argp;
  char *s;

  locking = cons.locking;
  if(locking)
    acquire(&cons.lock);

  if (fmt == 0)
    panic("null fmt");

  argp = (uint*)(void*)(&fmt + 1);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      consputc(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(*argp++, 10, 1);
      break;
    case 'x':
    case 'p':
      printint(*argp++, 16, 0);
      break;
    case 's':
      if((s = (char*)*argp++) == 0)
        s = "(null)";
      for(; *s; s++)
        consputc(*s);
      break;
    case '%':
      consputc('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      consputc('%');
      consputc(c);
      break;
    }
  }

  if(locking)
    release(&cons.lock);
}

static void print_NON_result(int result){
  int num_of_digits = 0;
  float temp = result;

  if(result == 0)
    num_of_digits = 1;

  while(temp >= 1){
    num_of_digits ++;
    temp = temp / 10.0;
  }

  int digit;
  char dig;
  char consoled_result[INPUT_BUF];

  for(int i = num_of_digits - 1; i >= 0; i--){
    digit = result % 10;
    dig = (char)(digit + 48);
    consoled_result[i] = dig; 
    result = result / 10;
  }
  for(int i = 0; i < num_of_digits; i++){
    consputc(consoled_result[i]);
    input.buf[(input.e - lefts) % INPUT_BUF] = consoled_result[i];
    if(input.flag_paste){
      input.buf_copy[(input.e - lefts) % INPUT_BUF] = '1';
    }
    input.e++;
  }
}

static void replacing_NON(float result){
  int flag_negative = 0;
  int i = non.S_n1;

  if(result < 0){
    flag_negative = 1;
    result = -result;
  }

  if(non.sign_n1 == 1)
    i--;

  for(int j = non.E_n2 + 2; j >= i; j--)
    consputc(BACKSPACE);


  if(flag_negative){
    consputc('-');
    input.buf[(input.e - lefts) % INPUT_BUF] = '-';
    if(input.flag_paste){
      input.buf_copy[(input.e - lefts) % INPUT_BUF] = '1';
    }
    input.e++;
  }

  int integer = (int)result;
  int fraction = (int)((result - (float)integer) * 10.0);

  print_NON_result(integer);

  if(fraction != 0){
    consputc('.');
    input.buf[(input.e - lefts) % INPUT_BUF] = '.';
    if(input.flag_paste){
      input.buf_copy[(input.e - lefts) % INPUT_BUF] = '1';
    }
    input.e++;
    print_NON_result(fraction);
  }
}

static void getting_NON_values(){
  int num1 = 0;
  int num2 = 0;
  int digit;
  float result;
 
  for(int i = non.S_n1; i <= non.E_n1; i++){
    digit = ((int)input.buf[i]) - 48;
    num1 = (num1 * 10) + digit;
  }

  for(int i = non.S_n2; i <= non.E_n2; i++){
    digit = ((int)input.buf[i]) - 48;
    num2 = (num2 * 10) + digit;
  }

  if(non.sign_n1 == 1)
    num1 = -num1;

  if(non.sign_n2 == 1)
    num2 = -num2;

  char operator = input.buf[non.op];

  if(operator == '+')
    result = (float)num1 + (float)num2;
  else if(operator == '-')
    result = (float)num1 - (float)num2;
  else if(operator == '*')
    result = (float)num1 * (float)num2;
  else if(operator == '/')
    result = (float)num1 / (float)num2;
  
  replacing_NON(result);
}

static int check_NON_pattern(){
  int flag1 = 0, flag2 = 0;

  if(input.buf[input.e - lefts - 2] != '='){
    return 0;
  }

  if(!is_c_in_numbers(input.buf[input.e - lefts - 3]))
    return 0;

  for (int i = input.e - lefts - 4; i >= input.r; i--){
    if(!is_c_in_numbers(input.buf[i])){
      if(is_c_in_operations(input.buf[i])){
        if(is_c_in_operations(input.buf[i - 1])){
          if(input.buf[i] == '-'){
          non.op = i - 1;
          non.S_n2 = i + 1;
          non.E_n2 = input.e - lefts - 3;
          flag1 = 1;
          non.sign_n2 = 1;
          break;
          }
          else{
            return 0;
          }
        }
        else{
          non.op = i;
          non.S_n2 = i + 1;
          non.E_n2 = input.e - lefts - 3;
          flag1 = 1;
          non.sign_n2 = 0;
          break;
        }
      }
      else
        return 0;
    }
  }

  if(!flag1)
    return 0;

  if(!is_c_in_numbers(input.buf[non.op - 1])){
    return 0;
  }

  for (int i = non.op - 2; i >= input.r; i--){
    if(!is_c_in_numbers(input.buf[i])){
      if(input.buf[i] == '-'){
        non.sign_n1 = 1;
      }
      else{
        non.sign_n1 = 0;
      }
      non.S_n1 = i + 1;
      non.E_n1 = non.op - 1;
      flag2 = 1;
      break;
    }
  }

  if(!flag2){
    non.S_n1 = input.r;
    non.E_n1 = non.op - 1;
  }
  
  return 1;
}

void
panic(char *s)
{
  int i;
  uint pcs[10];

  cli();
  cons.locking = 0;
  // use lapiccpunum so that we can call panic from mycpu()
  cprintf("lapicid %d: panic: ", lapicid());
  cprintf(s);
  cprintf("\n");
  getcallerpcs(&s, pcs);
  for(i=0; i<10; i++)
    cprintf(" %p", pcs[i]);
  panicked = 1; // freeze other CPU
  for(;;)
    ;
}

static void
cgaputc(int c)
{
  int pos;

  // Cursor position: col + 80*row.
  outb(CRTPORT, 14);
  pos = inb(CRTPORT+1) << 8;
  outb(CRTPORT, 15);
  pos |= inb(CRTPORT+1);

  if(c == '\n'){
    pos += 80 - pos%80;
    lefts = 0;
  }
  else if(c == BACKSPACE){
    shift_left_next_chars(pos);
    if(pos > 0) --pos;
  } 
  else{
    shift_right_next_chars(pos);
    crt[pos++] = (c&0xff) | 0x0700;  // black on white
  }

  if(pos < 0 || pos > 25*80)
    panic("pos under/overflow");

  if((pos/80) >= 24){  // Scroll up.
    memmove(crt, crt+80, sizeof(crt[0])*23*80);
    pos -= 80;
    memset(crt+pos, 0, sizeof(crt[0])*(24*80 - pos));
  }

  outb(CRTPORT, 14);
  outb(CRTPORT+1, pos>>8);
  outb(CRTPORT, 15);
  outb(CRTPORT+1, pos);
  crt[pos + lefts] = ' ' | 0x0700;
}

void
consputc(int c)
{
  if(panicked){
    cli();
    for(;;)
      ;
  }
  
  if(c == BACKSPACE){
    input.e --;
    input.buf[(input.e - lefts) % INPUT_BUF] = '\0';
    input.buf_copy[(input.e - lefts) % INPUT_BUF] = '\0';
    uartputc('\b'); uartputc(' '); uartputc('\b');
  }
  else
    uartputc(c);
  cgaputc(c);
}

void
consoleintr(int (*getc)(void))
{
  int c, doprocdump = 0, temp;
  char temp_char;

  acquire(&cons.lock);
  while((c = getc()) >= 0){
    switch(c){
    case C('P'):  // Process listing.
      // procdump() locks cons.lock indirectly; invoke later
      doprocdump = 1;
      break;
    case C('U'):  // Kill line.
      int pos;

      // Reset cursor to the end of the line
      outb(CRTPORT, 14);
      pos = inb(CRTPORT+1) << 8;
      outb(CRTPORT, 15);
      pos |= inb(CRTPORT+1);

      pos += lefts;
      lefts = 0;

      outb(CRTPORT, 14);
      outb(CRTPORT + 1, pos >> 8);
      outb(CRTPORT, 15);
      outb(CRTPORT + 1, pos);

      while(input.e != input.w &&
            input.buf[(input.e-1) % INPUT_BUF] != '\n'){
        consputc(BACKSPACE);
      }
      break;
    case C('H'): case '\x7f':  // Backspace
      if((input.e - lefts) > input.w){
        consputc(BACKSPACE);
      }
      break;
    case C('S'):  // Copy
      clear_buf_copy();
      input.flag_copy = 1;
      input.flag_paste = 1;
      break;
    case LEFTARROW:  // Left
      if((input.e - lefts) > input.w){
        update_cursor(0);
        lefts ++;
      }
      break;
    case RIGHTARROW:  // Right
      if(lefts > 0){
        update_cursor(1);
        lefts --;
      }
      break;
    case UPARROW:  // Up
      if ((history.count > 0 ) && (history.index > 0))
        move_through_history(0);
      break;
    case DOWNARROW:  // Down
      if ((history.count > 0 ) && (history.index < history.count - 1))
        move_through_history(1);
      break;
    default:
      if(c != 0 && input.e-input.r < INPUT_BUF){
        c = (c == '\r') ? '\n' : c;

        if(c == C('F')){ //Paste
          if(input.flag_copy){
            temp = input.e;
            for(int i = input.r; i < temp; i++){
              if(input.buf_copy[i] == '1'){
                temp_char = input.buf[i];
                consputc(input.buf[i]);
                input.buf[(input.e - lefts) % INPUT_BUF] = temp_char;
                input.buf_copy[(input.e - lefts) % INPUT_BUF] = '\0';
                if((input.e - lefts) - 1 < i){
                  i++;
                }
                if((input.e - lefts) - 1 < get_last_index_copy()){
                  temp++;
                }
                input.e ++;
              }
            }
            input.flag_paste = 0;
          }
        }
        else{
          consputc(c);
          input.buf[(input.e - lefts) % INPUT_BUF] = c;
          if(input.flag_paste){
            input.buf_copy[(input.e - lefts) % INPUT_BUF] = '1';
          }
          input.e ++;
          if(c == '?'){
            if(check_NON_pattern()){
              getting_NON_values();
            }
          } 
        }

        if(c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF){
          input.flag_copy = 0;
          
          if(is_command_history())
            print_history();

          if (history.count == 10)
            shift_left_previous_histories();

          if(history.count < 10){
            history.count ++;
            history.last ++;
            history.index = history.last + 1;
          }
          else
            history.index = 10;

          history.hist[history.last] = input;

          input.w = input.e;
          wakeup(&input.r);
        }
      }
      break;
    }
  }
  release(&cons.lock);
  if(doprocdump) {
    procdump();  // now call procdump() wo. cons.lock held
  }
}

int
consoleread(struct inode *ip, char *dst, int n)
{
  uint target;
  int c;

  iunlock(ip);
  target = n;
  acquire(&cons.lock);
  while(n > 0){
    while(input.r == input.w){
      if(myproc()->killed){
        release(&cons.lock);
        ilock(ip);
        return -1;
      }
      sleep(&input.r, &cons.lock);
    }
    c = input.buf[input.r++ % INPUT_BUF];
    if(c == C('D')){  // EOF
      if(n < target){
        // Save ^D for next time, to make sure
        // caller gets a 0-byte result.
        input.r--;
      }
      break;
    }
    *dst++ = c;
    --n;
    if(c == '\n')
      break;
  }
  release(&cons.lock);
  ilock(ip);

  return target - n;
}

int
consolewrite(struct inode *ip, char *buf, int n)
{
  int i;

  iunlock(ip);
  acquire(&cons.lock);
  for(i = 0; i < n; i++)
    consputc(buf[i] & 0xff);
  release(&cons.lock);
  ilock(ip);

  return n;
}

void
consoleinit(void)
{
  initlock(&cons.lock, "console");

  devsw[CONSOLE].write = consolewrite;
  devsw[CONSOLE].read = consoleread;
  cons.locking = 1;

  ioapicenable(IRQ_KBD, 0);
}