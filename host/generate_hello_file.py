import sys

target_count = 45

size = int(sys.argv[1])
m_k = str(sys.argv[2])
file = str(sys.argv[3])

f = open(file, 'w')

if m_k == "m":
    pages = size * 64
elif m_k == "k":
    pages = int(size / 16)

for i in range(pages): 
    index_str = '12hello' + str(i)
    f.write(index_str)
    for j in range(16384 - len(index_str)):
        f.write('0')

f.close()
