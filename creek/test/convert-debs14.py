#!/usr/bin/env python
'''
extract certain fields from the debs14 data file to suit our needs

schema:
input (debs14raw.txt)
id, timestamp, value, property(0-work;1-load), plug_id, household_id, house_id

output
timestamp, house_id, household_id, plug_id, value

houseid, houseid<<16|householdid<<8|plugin, value
'''

import sys
import os
from time import gmtime, strftime
import socket

# # output lines. in millions
n_max_lines = 100
maxhid = -1
maxhhid = -1 
maxpid = -1

minhid = 9999
minhhid = 9999
minpid = 9999

def print_comment_block(f):
	f.write('''# Auto Generated  \n''')
	f.write('''# Generator %s  \n''' %(os.path.abspath(__file__)))
	f.write('''# Time %s   \n''' %(strftime("%Y-%m-%d %H:%M:%S", gmtime())))
	f.write('''# Host: %s  \n''' %(socket.gethostname()))

if __name__ == "__main__":
	f = file(sys.argv[1])
	afile = open("debs14.txt", "w")

	n_lines = 0
	#afile.write("%s\n" %(int(n_max_lines * 1000 * 1000)))

	# -- comment --- #
	print_comment_block(afile)
	afile.write("# targeted kvpairs: %d million\n" %(n_max_lines))
	afile.write("# format: hid, (hid<<16)|(hhid<<8)|pid, value\n")

	for line in f.readlines():
		line=line.replace("\n", "")
		tokens=line.split(",")

		if tokens[3] == 0: # we care about "load" only
			continue
		else:
			ts = int(tokens[1])
			hid = int(tokens[6])
			hhid = int(tokens[5])
			pid = int(tokens[4])
			v = int(float(tokens[2]) * 1000)  # we make value in mW (x1000)

			#afile.write("%d %d %d %d %d" %(ts, hid, hhid, pid, v))

			assert(hid<256 and hhid<256 and pid<256)
			afile.write("%d, %d, %d\n" %(hid, (hid<<16)|(hid<<8)|pid, v))

			if hid > maxhid:
				maxhid = hid
			if hid < minhid:
				minhid = hid

			if hhid > maxhhid:
				maxhhid = hhid
			if hid < minhhid:
				minhhid = hhid

			if pid > maxpid:
				maxpid = pid
			if pid < minpid:
				minpid = pid

		n_lines += 1
		if n_lines == n_max_lines * 1000 * 1000:
			break
	
	#if n_lines != n_max_lines * 1000 * 1000:
	#	print "error. no enough data from the input file for %d M output lines" %n_max_lines
	#	sys.exit(1)
	
	# --- tail comment ---
	afile.write("# #kvpairs: %d K" %(n_lines/1000))

	print "houseid [%d,%d] householdid [%d,%d] plug [%d,%d]" %(minhid,maxhid,minhhid,maxhhid,minpid,maxpid)

	afile.close()
	f.close()
