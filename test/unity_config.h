/*
 * Unity configuration for host-based tests.
 */

#pragma once

#ifndef UNITY_OUTPUT_CHAR
int putchar(int);
#define UNITY_OUTPUT_CHAR(a) putchar(a)
#endif

#ifndef UNITY_INCLUDE_DOUBLE
#define UNITY_INCLUDE_DOUBLE 1
#endif
