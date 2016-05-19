#!/usr/bin/env python
import os

fixtures = [
	0,
	1,
	2 ** 7 - 1,
	2 ** 7,
	2 ** 8 - 1,
	2 ** 8,
	2 ** 15 - 1,
	2 ** 15,
	2 ** 16 - 1,
	2 ** 16,
	2 ** 31 - 1,
	2 ** 31,
	2 ** 32 - 1,
	2 ** 32,
	2 ** 63 - 1,
	2 ** 63,
	2 ** 64 - 1,
	2 ** 64,
	2 ** 127 - 1,
	2 ** 127,
	2 ** 128 - 1,
	2 ** 128,
]

def bytes_le(num, minbytes=0):
	while num or minbytes > 0:
		yield num & 0xFF
		num >>= 8
		minbytes -= 1

def bytearray_le(num, minbytes=0):
	return bytearray(bytes_le(num, minbytes))

def writenum(fl, num):
	bts = bytearray_le(num)
	fl.write(bytearray_le(len(bts), 4))
	fl.write(bts)

if not os.path.exists('bin'):
	os.mkdir('bin')

with open('bin/fixtures.bin', 'wb') as fl:
	fl.write(bytearray_le(len(fixtures), 4))
	for f in fixtures:
		writenum(fl, f)

	for a in fixtures:
		for b in fixtures:
			writenum(fl, a + b)
			writenum(fl, a * b)

	for a in fixtures:
		for b in fixtures:
			if a > b:
				fl.write('>')
			elif a < b:
				fl.write('<')
			else:
				fl.write('=')

