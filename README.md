# polycubes
generates polycubes. a challenge from a computerphile video

usage
-b [int] default value 2. begin at n=this number. use when you've already done some calculations and have the file on hand. this bugs out the progress% for the first n

-f [string] default value output1.polycubes. a file for where the initial data should come from. used in conjunction with -b

-n [int] default value 16. calculate up to n=this number

-l [int] default value 450000000. limits the size of the hash set to this number. **if set too high then the reserve call at the start will crash.** default value should work on a 32GB machine that has no other processes eating up RAM.

blenderShowPolycube.py is a simple blender addon that will build the polycube for you.

it is very simple, do not expect much out of it. the file path is hardcoded so you have to change it

the button to use it will show up in the objects menu

some benchmarks on my 8700k

n    seconds

9    0.119615

10   0.938319

11   7.28307

12   55.8535

13   460.84

14 was like an hour or 2

15 took over a day

16 I gave up on this

for smaller n it runs a bit faster when -l is set smaller. for example n=11 took 6.46 seconds when -l was 3 million.
