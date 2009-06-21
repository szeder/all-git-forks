
import sys, re

if len(sys.argv) < 3 : 
	sys.exit(0)

f = open(sys.argv[1], 'r')
dict = {}
for line in f : 
	if len(line) >= 40 and re.match(r'^[0-9a-fA-F]{40}', line[:40]) != None : 
		dict[line[:40]] = 1

f.close()

f = open(sys.argv[2], 'r')
for line in f :
	if len(line) < 40 : 
		continue
	
	hash = line[:40]
	if re.match(r'^[0-9a-fA-F]{40}', hash) == None : 
		continue
	
	if hash in dict : 
		dict[hash] -= 1
	else : 
		dict[hash] = -1

f.close()

for k in dict.keys() : 
	if dict[k] == 0 : 
		del dict[k]

if len(dict) : 
	print dict
