/*
 * Copyright (c) 2025, Ibrahim KAIKAA <ibrahimkaikaa@gmail.com>
 * SPDX-License-Identifier: GPL-3.0
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(int argc, char **argv)
{
    syscall0(SYSCALL_HELLO);
    exit();
}
