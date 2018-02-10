#!/usr/bin/env python

'''
produce a txt file of random integers.
for test join and window sum of nums, etc.
'''

import random

afile = open("integers.txt", "w")

# total # of numbers, in millions
cnt = 100

# range
low = 1
high = 1000

print "gen %d million integers..." %(cnt * 1000 * 1000)

# first line is the # of ints
afile.write("%s\n" %((cnt * 1000 * 1000)))

for i in range(int(cnt * 1000 * 1000)):
    line = str(random.randint(low, high))
    afile.write("%s\n" %line)

afile.close()
