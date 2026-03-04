import linecache
import sys
path = sys.argv[1]
for i in range(int(sys.argv[2]), int(sys.argv[3]) + 1):
    line = linecache.getline(path, i)
    print(i, repr(line))
