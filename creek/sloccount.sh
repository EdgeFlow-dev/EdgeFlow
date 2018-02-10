#!/bin/sh
sloccount \
*.[cpp,h] \
micro/*.[cpp,h] \
test/*.[cpp,h] \
test/seq/*.[cpp,h] \
Join/*.[cpp,h] \
Mapper/*.[cpp,h] \
ParDo/*.[cpp,h] \
Select/*.[cpp,h] \
Sink/*.[cpp,h] \
Source/*.[cpp,h] \
Win/*.[cpp,h] \
WinKeyReducer/*.[cpp,h] \
WinSum/*.[cpp,h] \
core/*.[cpp,h] \
# --details

# seq type
sloccount --cached --details|grep "SeqBuf.[h|cpp]" | print_sum
sloccount --cached --details|grep "SeqBundle.[h|cpp]" | print_sum
sloccount --cached --details|grep "SeqWinKeyFrag" | print_sum

# Seq impl
sloccount --cached --details|grep "Seq" | print_sum


# test
sloccount --cached --details|grep "test/seq" | print_sum

