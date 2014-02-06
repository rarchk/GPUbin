#!/usr/bin/env python

import os
import sys

# This script automatically generates the AMDIL C++ AST code.

if len(sys.argv) != 2:
	print('Usage: exec.py <Kernel Name>')
	sys.exit(1)

kernel = sys.argv[1]
ilFile=raw_input("IlFile:\nO-- NULL");
if not os.path.isfile(kernel):
	print('File %s does not exist' % kernel)
	sys.exit(1)
os.system("./clbingenerate "+kernel+" "+ilFile);
os.system("./clbinuse "+kernel+".bin");
