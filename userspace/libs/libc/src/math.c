/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <math.h>
#include <stdint.h>

#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_LN2 0.69314718055994530942

typedef union
{
    double d;
    uint64_t u;
} double_bits_t;

double fabs(double x)
{
    return (x < 0.0) ? -x : x;
}

double floor(double x)
{
    long long i = (long long) x;
    if ((double) i > x)
        i--;
    return (double) i;
}

double ceil(double x)
{
    long long i = (long long) x;
    if ((double) i < x)
        i++;
    return (double) i;
}

double fmod(double x, double y)
{
    if (y == 0.0)
        return 0.0;

    long long q = (long long) (x / y);
    return x - (double) q * y;
}

double sqrt(double x)
{
    if (x <= 0.0)
        return 0.0;

    double guess = (x > 1.0) ? x : 1.0;
    for (int i = 0; i < 24; i++) {
        guess = 0.5 * (guess + x / guess);
    }
    return guess;
}

static double scale_pow2(double x, int k)
{
    if (x == 0.0)
        return 0.0;

    double_bits_t bits;
    bits.d = x;
    int exp = (int) ((bits.u >> 52) & 0x7ff);
    if (exp == 0) {
        if (k > 0) {
            while (k--)
                x *= 2.0;
        } else {
            while (k++)
                x *= 0.5;
        }
        return x;
    }

    exp += k;
    if (exp <= 0)
        return 0.0;
    if (exp >= 0x7ff)
        return (x > 0.0) ? 1e308 : -1e308;

    bits.u = (bits.u & ((1ULL << 52) - 1)) | ((uint64_t) exp << 52);
    return bits.d;
}

static double log_approx(double x)
{
    if (x <= 0.0)
        return 0.0;

    double_bits_t bits;
    bits.d = x;
    int exp = (int) ((bits.u >> 52) & 0x7ff) - 1023;
    bits.u = (bits.u & ((1ULL << 52) - 1)) | (1023ULL << 52);
    double m = bits.d;

    double y = (m - 1.0) / (m + 1.0);
    double y2 = y * y;
    double term = y;
    double sum = y;

    term *= y2;
    sum += term / 3.0;
    term *= y2;
    sum += term / 5.0;
    term *= y2;
    sum += term / 7.0;
    term *= y2;
    sum += term / 9.0;

    return 2.0 * sum + (double) exp * M_LN2;
}

static double exp_approx(double x)
{
    if (x == 0.0)
        return 1.0;

    int k;
    if (x >= 0.0) {
        k = (int) (x / M_LN2 + 0.5);
    } else {
        k = (int) (x / M_LN2 - 0.5);
    }

    double r = x - (double) k * M_LN2;
    double r2 = r * r;
    double poly = 1.0 + r + r2 * 0.5 + r2 * r / 6.0 + r2 * r2 / 24.0
                  + r2 * r2 * r / 120.0 + r2 * r2 * r2 / 720.0;
    return scale_pow2(poly, k);
}

static double pow_int(double x, long long n)
{
    if (n == 0)
        return 1.0;
    if (n < 0)
        return 1.0 / pow_int(x, -n);

    double result = 1.0;
    double base = x;
    long long exp = n;

    while (exp > 0) {
        if (exp & 1)
            result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

double pow(double x, double y)
{
    if (y == 0.0)
        return 1.0;
    if (x == 0.0)
        return 0.0;

    double yi = floor(y);
    if (fabs(y - yi) < 1e-9)
        return pow_int(x, (long long) yi);

    if (x < 0.0) {
        if (fabs(y - yi) < 1e-9) {
            double res = pow_int(-x, (long long) yi);
            return (((long long) yi) & 1) ? -res : res;
        }
        return 0.0;
    }

    return exp_approx(y * log_approx(x));
}

double cos(double x)
{
    double ax = fabs(x);
    double two_pi = 2.0 * M_PI;

    if (ax > two_pi)
        ax = fmod(ax, two_pi);
    if (ax > M_PI)
        ax = two_pi - ax;

    int sign = 1;
    if (ax > M_PI_2) {
        ax = M_PI - ax;
        sign = -1;
    }

    double x2 = ax * ax;
    double approx = 1.0 - x2 * 0.5 + x2 * x2 / 24.0 - x2 * x2 * x2 / 720.0;
    return sign * approx;
}

static double asin_approx(double x)
{
    if (x < 0.0)
        return -asin_approx(-x);
    if (x > 1.0)
        x = 1.0;

    if (x > 0.5) {
        double t = sqrt((1.0 - x) * 0.5);
        return M_PI_2 - 2.0 * asin_approx(t);
    }

    double x2 = x * x;
    return x + x * x2 * (1.0 / 6.0) + x * x2 * x2 * (3.0 / 40.0)
           + x * x2 * x2 * x2 * (5.0 / 112.0);
}

double acos(double x)
{
    if (x > 1.0)
        x = 1.0;
    if (x < -1.0)
        x = -1.0;
    return M_PI_2 - asin_approx(x);
}
