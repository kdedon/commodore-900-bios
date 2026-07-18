# Commodore 900 BIOS

This repository is intended to capture work on reverse engineering and
modification of the Commodore 900 BIOS. 

## Building

The BIOS requires the original MWC Coherent toolchain (cc / as / nld) 
running on the C900 itself. Once loaded on the C900:

    make rom        # compile, link, flatten to the 32 KB ROM image
    make prom
    prom rom        # split -> rom.H (even bytes) + rom.L (odd bytes)

md5(rom.L) == boot-L_V_1.0.bin, md5(rom.H) == boot-H_V_1.0.bin.

## Goals

The efforts captured by this repository include:

1. A byte exact decompilation of the BIOS, allowing for recreation of
the shipping BIOS. 
1. Extensions to the BIOS, such as more hard drive layouts.
1. Expansion of the BIOS to 64K.

## AI Disclosure

The initial decompilation of the BIOS was done with support using AI
tooling. 