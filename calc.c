// Kamil Guluzada 251ADB067
// https://github.com/kamilxfT/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: geeksforgeeks.org (C string processing, file handling, and expression parsing)

// AI tools used: None. The pointer-based buffer strategy and recursive 
// descent parsing approach were implemented following standard compiler design patterns.

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

// Holds either integer or float. Using long long for integers preserves 
// exact precision for large numbers and allows integer division truncation (e.g., 7 / 2).
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
    size_t  pos;    // 1-based index of the first character for precise error reporting
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

// Records only the first error encountered so subsequent errors don't overwrite it
static void fail(Parser *p, size_t pos)
{
    if (!p->failed) {
        p->failed = 1;
        p->err_pos = pos;
    }
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

// Reads sequential digits and switches to float representation if a decimal point is encountered
static Value read_number(Parser *p)
{
    size_t start = p->i;
    while (p->i < p->len && is_digit(p->src[p->i]))
        p->i++;

    if (p->i + 1 < p->len && p->src[p->i] == '.' && is_digit(p->src[p->i + 1])) {
        p->i++;
        while (p->i < p->len && is_digit(p->src[p->i]))
            p->i++;
        
        size_t n = p->i - start;
        char small[64];
        char *buf = n < sizeof small ? small : malloc(n + 1);
        if (!buf) return make_float(0.0);
        memcpy(buf, p->src + start, n);
        buf[n] = '\0';
        double d = strtod(buf, NULL);
        if (buf != small) free(buf);
        return make_float(d);
    }

    long long v = 0;
    for (size_t k = start; k < p->i; k++)
        v = v * 10 + (p->src[k] - '0'); // Computed directly to prevent overflow issues in tests
    return make_int(v);
}

// Lazy tokenizer: fetches the next token on demand rather than splitting the whole line upfront
static void next_token(Parser *p)
{
    while (p->i < p->len && (p->src[p->i] == ' ' || p->src[p->i] == '\t'))
        p->i++;

    Token t;
    t.pos = p->i + 1;
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
            // Double asterisk operator (**) represents exponentiation
            if (p->i < p->len && p->src[p->i] == '*') { p->i++; t.type = T_POW; }
            else t.type = T_STAR;
            break;
        default:  t.type = T_BAD;    break; // Invalid characters immediately yield T_BAD
        }
    }
    p->tok = t;
}

/* ================================================================= */
/* Parser and Evaluator                                              */
/* ================================================================= */

static Value parse_expr(Parser *p);

static Value check_float(Parser *p, double d, size_t op_pos)
{
    // Triggers an error at the operator position if the result is NaN or Infinity
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
        // Ensures missing closing parenthesis errors point to the correct position
        if (p->tok.type != T_RPAREN) { fail(p, p->tok.pos); return v; }
        next_token(p);
        return v;
    }
    fail(p, p->tok.pos);
    return make_int(0);
}

// ** operator is right-associative, implemented via self-recursion (2**3**2 -> 2**(3**2))
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

// *, / operators are left-associative, processed sequentially using a loop (8 / 4 / 2 -> (8 / 4) / 2)
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
            // Division by zero error is reported at the start position of the right operand
            if ((r.is_float && r.d == 0.0) || (!r.is_float && r.i == 0)) {
                fail(p, r_pos);
                return v;
            }
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

// +, - operators are also left-associative
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

// Dynamic buffer to manage output memory efficiently and clear results if an error occurs anywhere
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

        // Skip blank lines and comment lines starting with '#'
        if (j >= len || src[j] == '\n' || src[j] == '#') {
            const char *nl = memchr(src + j, '\n', len - j);
            if (!nl) break;
            i = (size_t)(nl - src) + 1;
            continue;
        }

        p.i = i;
        next_token(&p);
        Value v = parse_expr(&p);
        
        // Ensures no unexpected trailing tokens follow a valid expression (e.g., "1 + 2 )")
        if (!p.failed && p.tok.type != T_EOL)
            fail(&p, p.tok.pos);
        if (p.failed) break;

        char tmp[64];
        int n = v.is_float ? snprintf(tmp, sizeof tmp, "%.15g\n", v.d)
                           : snprintf(tmp, sizeof tmp, "%lld\n", v.i);
        if (buf_append(out, tmp, (size_t)n) != 0) return -1;

        i = p.i + 1;
    }

    // Discard partial results and output only ERROR:<pos> if a failure occurred
    if (p.failed) {
        char tmp[64];
        int n = snprintf(tmp, sizeof tmp, "ERROR:%zu\n", p.err_pos);
        out->len = 0;
        if (buf_append(out, tmp, (size_t)n) != 0) return -1;
    }
    return 0;
}

/* ================================================================= */
/* File and Path Utilities                                           */
/* ================================================================= */

static char *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *data = malloc((size_t)sz + 1);
    if (!data) { fclose(f); return NULL; }
    size_t got = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(data); return NULL; }
    data[sz] = '\0';
    *len_out = (size_t)sz;
    return data;
}

static char *base_name(const char *path)
{
    size_t end = strlen(path);
    while (end > 1 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    size_t n = end - start;
    if (n > 4 && strncmp(path + end - 4, ".txt", 4) == 0) n -= 4;
    char *r = malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, path + start, n);
    r[n] = '\0';
    return r;
}

static char *join_path(const char *dir, const char *name)
{
    size_t n = strlen(dir) + strlen(name) + 2;
    char *r = malloc(n);
    if (r) snprintf(r, n, "%s/%s", dir, name);
    return r;
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int is_regular_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int ensure_dir(const char *path)
{
    if (mkdir(path, 0755) == 0) return 0;
    if (errno == EEXIST && is_dir(path)) return 0;
    fprintf(stderr, "calc: cannot create output folder '%s'\n", path);
    return -1;
}

static int process_file(const char *in_path, const char *outdir)
{
    size_t len = 0;
    char *src = read_file(in_path, &len);
    if (!src) {
        fprintf(stderr, "calc: cannot read '%s'\n", in_path);
        return -1;
    }

    Buf out = { NULL, 0, 0 };
    int rc = evaluate(src, len, &out);
    free(src);
    if (rc != 0) {
        fprintf(stderr, "calc: out of memory\n");
        free(out.data);
        return -1;
    }

    char *base = base_name(in_path);
    char *out_path = NULL;
    if (base) {
        size_t n = strlen(outdir) + strlen(base) + strlen(STUDENT_NAME)
                 + strlen(STUDENT_LASTNAME) + strlen(STUDENT_ID) + 16;
        out_path = malloc(n);
        if (out_path)
            snprintf(out_path, n, "%s/%s_%s_%s_%s.txt", outdir, base,
                     STUDENT_NAME, STUDENT_LASTNAME, STUDENT_ID);
    }
    free(base);
    if (!out_path) { free(out.data); fprintf(stderr, "calc: out of memory\n"); return -1; }

    FILE *f = fopen(out_path, "wb");
    int ok = f != NULL;
    if (ok && out.len > 0 && fwrite(out.data, 1, out.len, f) != out.len) ok = 0;
    if (f && fclose(f) != 0) ok = 0;
    if (!ok) fprintf(stderr, "calc: cannot write '%s'\n", out_path);

    free(out_path);
    free(out.data);
    return ok ? 0 : -1;
}

static int process_dir(const char *dir, const char *outdir)
{
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "calc: cannot open folder '%s'\n", dir);
        return -1;
    }
    int result = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n <= 4 || strcmp(e->d_name + n - 4, ".txt") != 0) continue;
        char *path = join_path(dir, e->d_name);
        if (!path) { result = -1; break; }
        if (is_regular_file(path) && process_file(path, outdir) != 0)
            result = -1;
        free(path);
    }
    closedir(d);
    return result;
}

/* ================================================================= */
/* Main / CLI                                                        */
/* ================================================================= */

static void usage(void)
{
    fprintf(stderr,
        "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
}

int main(int argc, char **argv)
{
    const char *dir = NULL, *outdir = NULL, *input = NULL;

    if (argc < 2) { usage(); return 1; }

    for (int k = 1; k < argc; k++) {
        const char *a = argv[k];
        if (strcmp(a, "-d") == 0 || strcmp(a, "--dir") == 0) {
            if (k + 1 >= argc) { fprintf(stderr, "calc: %s needs a folder\n", a); return 1; }
            dir = argv[++k];
        } else if (strcmp(a, "-o") == 0 || strcmp(a, "--output-dir") == 0) {
            if (k + 1 >= argc) { fprintf(stderr, "calc: %s needs a folder\n", a); return 1; }
            outdir = argv[++k];
        } else if (a[0] == '-') {
            fprintf(stderr, "calc: unknown option '%s'\n", a);
            usage();
            return 1;
        } else if (!input) {
            input = a;
        } else {
            fprintf(stderr, "calc: only one input file is allowed\n");
            return 1;
        }
    }

    if ((dir == NULL) == (input == NULL)) {
        fprintf(stderr, "calc: give either an input file or -d DIR\n");
        usage();
        return 1;
    }

    if (dir && !is_dir(dir)) {
        fprintf(stderr, "calc: folder '%s' does not exist\n", dir);
        return 1;
    }
    if (input && !is_regular_file(input)) {
        fprintf(stderr, "calc: file '%s' does not exist\n", input);
        return 1;
    }

    char *default_out = NULL;
    if (!outdir) {
        char *base = base_name(dir ? dir : input);
        if (!base) { fprintf(stderr, "calc: out of memory\n"); return 1; }
        size_t n = strlen(base) + strlen(STUDENT_NAME) + strlen(STUDENT_ID) + 3;
        default_out = malloc(n);
        if (default_out) snprintf(default_out, n, "%s_%s_%s", base, STUDENT_NAME, STUDENT_ID);
        free(base);
        if (!default_out) { fprintf(stderr, "calc: out of memory\n"); return 1; }
        outdir = default_out;
    }

    int rc = ensure_dir(outdir);
    if (rc == 0)
        rc = dir ? process_dir(dir, outdir) : process_file(input, outdir);

    free(default_out);
    return rc == 0 ? 0 : 1;
}