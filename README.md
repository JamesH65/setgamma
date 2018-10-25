# setgamma

Sets display gamma in various funky ways on Raspberry Pi. Requires appropriate firmware

Compile using

cc mailbox.c setgamma.c -ldl -lm -o setgamma

Run using

sudo ./setgamma

Has a few command line parameters

-l Number of loops
-d delay between each change in us
-c Channel to act on (r = 0, g = 1, b = 2, all = 3)
-t type of action - 0 = fader, 1 = coloury fader, 2 = posterise




