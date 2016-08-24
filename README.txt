The integration test software of CPCI-1504(LiRC-3)
==================================================

Description
-----------
This project implements integration test softwares of CPCI-1504(LiRC-3).

NOTES: To run this utility, you need to be root user.


File list
-----------
Makefile    - Makefile for whole project in this repository.
README.txt  - This file
main.c      - The main program
log.c       - The log module
cfg.c       - The configuration module
common.c    - Define some common functions
led/        - Test module: led
hsm/        - Test module: hsm
msm/        - Test module: msm
nim/        - Test module: nim
sim/        - Test module: sim
mem/        - Test module: mem
cpu/        - Test module: cpu
lirc.cfg    - Configuation file
lib/        - Library of serial port
