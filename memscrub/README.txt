
AHB memory scrubber (memscrub) and FT DDR2 RTEMS demo application
-----------------------------------------------------------------

1. Overview

  This is an RTEMS demo application written to show how the Memscrub AHB 
  memory scrubber can be used together with the fault-tolerant version 
  of the DDR2SPA memory controller.

  This demo application starts upp the scrubber over a memory area, 
  enables EDAC and then periodically injects faults into the memory. The 
  demo prints status messages from events caused by scrubber interrupts 
  and also peiodically prints the total corrected error count from the 
  scrubber.


2. Hardware Requirements

  The hardware to run on must be a GRLIB system with a FT DDR2 controller 
  and a memory scrubber on the same bus.


3. Source code structure

  The code is made up of three modules:
   - memscrub.c/h - Generic driver/wrapper for the memscrub register interface
   - memscrub_rtems.c/h - RTEMS driver for memscrub
   - scrubtest.c - The demo program

  The source code is quite heavily commented, so see the code for further 
  details.


4. Running

  First, build the binary by running "make" 

  Two versions of the program are built by the Makefile, one called scrubtest 
  configured to scrub a memory area starting at 0x40000000, and one called 
  scrubtest_ram0 configured for a memory area starting at address 0.

  Before the program is started, the memory area must have been cleared to 
  initialize the checkbits. This can be done from grmon using the "scrub 
  clear" command, or if booting from PROM by supplying a dedicated bdinit 
  routine to mkprom2 (see bdinit_scrub.c for an example).

  From grmon:
    scrub clear 0x40000000 0x43ffffff
    load scrubtest
    run

  For the ram0 version:
    scrub clear 0 0x3ffffff
    load scrubtest_ram0
    run
  

5. Sample output

  grlib> load scrubtest_ram0
  section: .text at 0x0, size 143776 bytes
  section: .data at 0x231a0, size 4224 bytes
  section: .jcr at 0x24220, size 4 bytes
  total size: 148004 bytes (850.1 kbit/s)
  read 900 symbols
  entry point: 0x00000000
  grlib> run
  -- Scrubber RTEMS test application --
  Config: Mem range: 00000000-03ffffff
  Scrubber dev 0x268a0, regs @ 0xffe01000, mem @ 00000000-03ffffff, state 1, autostart=1
  FT DDR2 controller found, Memory bar 2048 MB @ 00000000, regs @ ffe00000

  [F] Counts: total_inj=0, total_ce=0
  [F] --> Injecting 1 errors <-- 
  [F] Counts: total_inj=1, total_ce=1
  [F] --> Injecting 2 errors <-- 
  [F] Counts: total_inj=3, total_ce=3
  [F] --> Injecting 3 errors <-- 
  [F] Counts: total_inj=6, total_ce=6
  [F] --> Injecting 4 errors <-- 
  [F] Counts: total_inj=10, total_ce=10
  [F] --> Injecting 5 errors <-- 
  [F] Counts: total_inj=15, total_ce=15
  [F] --> Injecting 6 errors <-- 
  [F] Counts: total_inj=21, total_ce=21
  [F] --> Injecting 7 errors <-- 
  [F] Counts: total_inj=28, total_ce=28
  [F] --> Injecting 8 errors <-- 
  [F] Counts: total_inj=36, total_ce=36
  [F] --> Injecting 9 errors <-- 
  [F] Counts: total_inj=45, total_ce=45
  [F] --> Injecting 10 errors <-- 
  [R] Scrubber switched to regeneration mode
  [R] Scrubber iteration done, errcount=6
  [F] Counts: total_inj=55, total_ce=55
  [F] --> Injecting 11 errors <-- 
  [R] Scrubber switched to regeneration mode
  [R] Scrubber iteration done, errcount=7
  [F] Counts: total_inj=66, total_ce=66
  [F] --> Injecting 12 errors <-- 
  [R] Scrubber switched to regeneration mode
  [R] Scrubber iteration done, errcount=10
  [F] Counts: total_inj=78, total_ce=78
  [F] --> Injecting 13 errors <-- 
  [R] Scrubber switched to regeneration mode
  [R] Scrubber iteration done, errcount=7
  [R] Scrubber switched to regeneration mode


6. Notes

  The demo can be run in a more verbose mode, where each completed scrub 
  interation and error is logged. See the OPERMODE constant in 
  scrubtest.c and the comments in memscrub_rtems.h
