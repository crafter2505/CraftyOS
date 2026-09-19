# craftyOS

A hobby operating system built from scratch.

## Current status

- ✅ Milestone 1: 512-byte boot sector
- ✅ Milestone 2: two-stage bootloader

## Build

    make
    make run

Requires: nasm, qemu-system-x86_64.

## Structure

    bootloader/
      stage1.asm    - BIOS boot sector, loads stage2
      stage2.asm    - loaded by stage1, prints message
    Makefile