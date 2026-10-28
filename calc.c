// Kamil Guluzada 251ADB067
// https://github.com/kamilxfT/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Referenced guides on geeksforgeeks.org for C string processing, file handling, and expression parsing

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

/* ---------- values ---------- */

// long long because some test numbers are pretty big.
// int division in C already truncates toward zero (7/2 = 3), so that's what the lab wants anyway.
typedef struct {
    int       is_float;
    long long i;
    double    d;
} Value;

static Value make_int(long long v)  { Value r = { 0, v, 0.0 }; return r; }
static Value make_float(double v)   { Value r = { 1, 0, v };   return r; }
static double as_double(Value v)    { return v.is_float ? v.d : (double)v.i; }
/* ================================================================= */
/* Tokenizer                                                         */
/* ================================================================= */

typedef enum {
    T_NUM, T_PLUS, T_MINUS, T_STAR, T_SLASH, T_POW,
    T_LPAREN, T_RPAREN, T_EOL, T_BAD
} TokType;

typedef struct {
    TokType type;
    size_t  pos;    // 1-based index. We track exact char positions (including '\n') so errors point exactly where the problem is.
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

// We only save the very first error we hit. If we keep parsing after an error, 
// the parser state is broken anyway, and we might accidentally overwrite the true error position.
static void fail(Parser *p, size_t pos)
{
    if (!p->failed) {
        p->failed = 1;
        p->err_pos = pos;
    }
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

// We check for a '.' to decide if it's a float or int. 
// If there's a dot, we use double to keep the fraction; otherwise, we stick to long long.
static Value read_number(Parser *p)
{
    size_t start = p->i;
    while (p->i < p->len && is_digit(p->src[p->i]))
        p->i++;

    int is_float = 0;
    if (p->i + 1 < p->len && p->src[p->i] == '.' && is_digit(p->src[p->i + 1])) {
        is_float = 1;
        p->i++;
        while (p->i < p->len && is_digit(p->src[p->i]))
            p->i++;
    }

    size_t n = p->i - start;
    char small[64];
    char *buf = n < sizeof small ? small : malloc(n + 1);
    if (!buf) return make_float(0.0);
    
    memcpy(buf, p->src + start, n);
    buf[n] = '\0';

    Value res;
    if (is_float) {
        res = make_float(strtod(buf, NULL));
    } else {
        // Using strtoll instead of a manual loop here to avoid potential 
        // signed integer overflow crashes if the input has huge numbers.
        res = make_int(strtoll(buf, NULL, 10));
    }
    
    if (buf != small) free(buf);
    return res;
}

// Fetching tokens one by one (lazy parsing) instead of copying the whole string first. 
// This is much faster and handles multi-megabyte files without slowing down.
static void next_token(Parser *p)
{
    while (p->i < p->len && (p->src[p->i] == ' ' || p->src[p->i] == '\t'))
        p->i++;

    Token t;
    t.pos = p->i + 1; // Rules say positions are 1-based
    t.val = make_int(0);

    if (p->i >= p->len || p->src[p->i] == '\n') {
        t.type = T_EOL;
        p->tok = t;
        return;
    }

    char c = p->src[p->i];
    if (is_digit(c)) {
        t.type = T_NUM;
        t.val = read_number(p);
    } else {
        p->i++;
        switch (c) {
        case '+': t.type = T_PLUS;   break;
        case '-': t.type = T_MINUS;  break;
        case '/': t.type = T_SLASH;  break;
        case '(': t.type = T_LPAREN; break;
        case ')': t.type = T_RPAREN; break;
        case '*':
            if (p->i < p->len && p->src[p->i] == '*') { p->i++; t.type = T_POW; }
            else t.type = T_STAR;
            break;
        default:  t.type = T_BAD;    break;
        }
    }
    p->tok = t;
}