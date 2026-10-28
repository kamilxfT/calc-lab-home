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

/* ================================================================= */
/* Value representation                                              */
/* ================================================================= */

// Using long long for integers because the tests can have big numbers. 
// Also, standard C integer division truncates toward zero (e.g., 7 / 2 == 3), 
// which matches the lab rules perfectly.
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

/* ================================================================= */
/* Parser and Evaluator                                              */
/* ================================================================= */

static Value parse_expr(Parser *p);

// If we get NaN or Infinity, we must report the error exactly at the operator's position, 
// just like the table in the assignment description.
static Value check_float(Parser *p, double d, size_t op_pos)
{
    if (!isfinite(d)) fail(p, op_pos);
    return make_float(d);
}

static Value parse_primary(Parser *p)
{
    if (p->tok.type == T_NUM) {
        Value v = p->tok.val;
        next_token(p);
        return v;
    }
    if (p->tok.type == T_LPAREN) {
        next_token(p);
        Value v = parse_expr(p);
        if (p->failed) return v;
        // If we expect a ')' but don't find it, the error is at the current token's position.
        if (p->tok.type != T_RPAREN) { fail(p, p->tok.pos); return v; }
        next_token(p);
        return v;
    }
    fail(p, p->tok.pos);
    return make_int(0);
}

// To make '**' right-associative (2**3**2 == 2**(3**2)), we call parse_power() 
// again for the exponent. This naturally builds the recursion from right to left.
static Value parse_power(Parser *p)
{
    Value base = parse_primary(p);
    if (p->failed || p->tok.type != T_POW) return base;

    size_t op_pos  = p->tok.pos;
    next_token(p);
    size_t exp_pos = p->tok.pos;
    Value e = parse_power(p);
    if (p->failed) return base;

    if (!base.is_float && !e.is_float) {
        // The lab rules say negative integer exponents are an error, pointing to the exponent's start.
        if (e.i < 0) { fail(p, exp_pos); return base; }
        long long r = 1, b = base.i, n = e.i;
        while (n > 0) {
            if (n & 1) r *= b;
            n >>= 1;
            if (n > 0) b *= b;
        }
        return make_int(r);
    }
    return check_float(p, pow(as_double(base), as_double(e)), op_pos);
}

// Left-associative for * and /, so we just use a while loop. 
// Also, division by zero must point to the start of the right operand.
static Value parse_term(Parser *p)
{
    Value v = parse_power(p);
    while (!p->failed && (p->tok.type == T_STAR || p->tok.type == T_SLASH)) {
        TokType op     = p->tok.type;
        size_t  op_pos = p->tok.pos;
        next_token(p);
        size_t  r_pos  = p->tok.pos;
        Value   r      = parse_power(p);
        if (p->failed) return v;

        if (op == T_SLASH) {
            if ((r.is_float && r.d == 0.0) || (!r.is_float && r.i == 0)) {
                fail(p, r_pos);
                return v;
            }
            // C's integer division naturally truncates toward zero, which matches the rules.
            if (!v.is_float && !r.is_float)
                v = make_int(v.i / r.i);
            else
                v = check_float(p, as_double(v) / as_double(r), op_pos);
        } else {
            if (!v.is_float && !r.is_float)
                v = make_int(v.i * r.i);
            else
                v = check_float(p, as_double(v) * as_double(r), op_pos);
        }
    }
    return v;
}

// '+' and '-' are left-associative.
static Value parse_expr(Parser *p)
{
    Value v = parse_term(p);
    while (!p->failed && (p->tok.type == T_PLUS || p->tok.type == T_MINUS)) {
        TokType op     = p->tok.type;
        size_t  op_pos = p->tok.pos;
        next_token(p);
        Value   r      = parse_term(p);
        if (p->failed) return v;

        if (!v.is_float && !r.is_float)
            v = make_int(op == T_PLUS ? v.i + r.i : v.i - r.i);
        else
            v = check_float(p, op == T_PLUS ? as_double(v) + as_double(r)
                                            : as_double(v) - as_double(r), op_pos);
    }
    return v;
}

/* ================================================================= */
/* Output Buffer                                                     */
/* ================================================================= */

// I use a dynamic buffer to hold the results so we don't write to the disk a million times 
// for huge files. Also, if an error happens, we can easily wipe the buffer clean and write only ERROR:<pos>.
typedef struct { char *data; size_t len, cap; } Buf;

static int buf_append(Buf *b, const char *s, size_t n)
{
    if (b->len + n > b->cap) {
        size_t cap = b->cap ? b->cap : 4096;
        while (cap < b->len + n) cap *= 2;
        char *d = realloc(b->data, cap);
        if (!d) return -1;
        b->data = d;
        b->cap  = cap;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    return 0;
}

/* ================================================================= */
/* File Evaluation                                                   */
/* ================================================================= */

static int evaluate(const char *src, size_t len, Buf *out)
{
    Parser p;
    memset(&p, 0, sizeof p);
    p.src = src;
    p.len = len;

    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && (src[j] == ' ' || src[j] == '\t')) j++;

        // Skip over blank lines and ignore lines starting with '#' (comments).
        if (j >= len || src[j] == '\n' || src[j] == '#') {
            const char *nl = memchr(src + j, '\n', len - j);
            if (!nl) break;
            i = (size_t)(nl - src) + 1;
            continue;
        }

        p.i = i;
        next_token(&p);
        Value v = parse_expr(&p);
        
        // If we finished an expression but there's still garbage left (like an extra ')'), it's an error.
        if (!p.failed && p.tok.type != T_EOL)
            fail(&p, p.tok.pos);
        if (p.failed) break;

        char tmp[64];
        int n = v.is_float ? snprintf(tmp, sizeof tmp, "%.15g\n", v.d)
                           : snprintf(tmp, sizeof tmp, "%lld\n", v.i);
        if (buf_append(out, tmp, (size_t)n) != 0) return -1;

        i = p.i + 1;
    }

    // Rule 3: If there's any error in the file, we throw away all results and only print the first error.
    if (p.failed) {
        char tmp[64];
        int n = snprintf(tmp, sizeof tmp, "ERROR:%zu\n", p.err_pos);
        out->len = 0; // Clear previous results
        if (buf_append(out, tmp, (size_t)n) != 0) return -1;
    }
    return 0;
}