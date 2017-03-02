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
lib/        - Library of serial port


Build
-----------
Run under terminal or console:
# make

The test module CPU and MEM call 3rd-party utilities "memtester" and
"stresscpu2". These utlities shall be placed under the same directory of
lirc-itest or the Linux executable search path (defined by $PATH environment
variable) like "/usr/local/bin/" or "/root/bin/".


Usage
-----------
To run the program:
# ./lirc-itest
Run test on main CCM without MSM

# ./lirc-itest -msm
Run test on main CCM with MSM

# ./lirc-cpu
Run test on secondary CCM

This program shall be run on both machine A and B.
