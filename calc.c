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