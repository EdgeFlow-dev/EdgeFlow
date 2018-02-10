#!/usr/bin/env python

'''
produce a txt file of random integers.
for test join and window sum of nums, etc.

can gen both k/v or k1/k2/v
'''

import random
import os
from time import gmtime, strftime
import socket

out_fname = "kv.txt"

def print_comment_block(f):
	f.write('''# Auto Generated  \n''')
	f.write('''# Generator %s  \n''' %(os.path.abspath(__file__)))
	f.write('''# Time %s   \n''' %(strftime("%Y-%m-%d %H:%M:%S", gmtime())))
	f.write('''# Host: %s  \n''' %(socket.gethostname()))

if __name__ == "__main__":
	afile = open(out_fname, "w")

	# total # of numbers, in millions
	cnt = 10

	# range
	low = 1
	high = 1000

	print "gen %d million kv pairs to %s..." %(cnt, out_fname)

	# first line is the # of ints
	#afile.write("%s\n" %(cnt * 1000 * 1000))

	# -- comment --- #
	print_comment_block(afile)
	afile.write("# #kvpairs: %d million\n" %(cnt))
	
	# -- no header -- #

	for i in range(int(cnt * 1000 * 1000)):
		  line = "%s, %s" %(str(random.randint(low, high)), str(random.randint(low, high))) # kv
		  #line = "%s, %s, %s" %(str(random.randint(low, high)), str(random.randint(low, high)), str(random.randint(low, high))) # k2v
		  afile.write("%s\n" %line)

	afile.close()
