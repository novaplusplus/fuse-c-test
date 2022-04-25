#!/bin/bash
clang -std=c99 -I/usr/include/fuse3 -lfuse3 -lpthread -o fuse_test_c fuse_test.c
