// Kamil Guluzada 251ADB067
// https://github.com/kamilxfT/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: GeeksforGeeks (C string processing, file handling, and expression parsing)

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#define STUDENT_NAME     "Kamil"
#define STUDENT_LASTNAME "Guluzada"
#define STUDENT_ID       "251ADB067"

typedef struct {
    int       is_float;
    long long i;
    double    d;
} Value;

static Value make_int(long long v)  { Value r = { 0, v, 0.0 }; return r; }
static Value make_float(double v)   { Value r = { 1, 0, v };   return r; }
static double as_double(Value v)    { return v.is_float ? v.d : (double)v.i; }

typedef enum {
    T_NUM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_POW,
    T_LPAREN, T_RPAREN, T_EOL, T_BAD
} TokType;

typedef struct {
    TokType type;
    size_t  pos;
    Value   val;    
} Token;

typedef struct {
    const char *src;
    size_t      len;
    size_t      i;        
    Token       tok;      
    int         failed;   
    size_t      err_pos;
} Parser;

static void fail(Parser *p, size_t pos)
{
    if (!p->failed) {
        p->failed = 1;
        p->err_pos = pos;
    }
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }