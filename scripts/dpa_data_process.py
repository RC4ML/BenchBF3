import sys

def get_data(lines:[str], key:str):
	for line in lines:
		if key in line:
			for item in line.split():
				if item.isdigit():
					return int(item)
	print("Error,",key," not found")
	exit()
def get_thread_id(lines):
	return int(lines[0].split('/')[1])

# file_prefix = sys.argv[1]
# num = int(sys.argv[2])
# key = sys.argv[3]

file_prefix = "./log/mt_memory_"
num = int(sys.argv[1])
keys = ["Words per second:","Duration:"]

results = [[] for key in keys]
threads = []

for i in range(num):
	file_name = file_prefix+str(i)+".txt"
	f = open(file_name,"r")
	lines = f.readlines()
	for j in range(len(keys)):
		results[j].append(get_data(lines,keys[j]))
	threads.append(get_thread_id(lines))
for i in range(len(keys)):
	print(keys[i])
	print(results[i])
	print("Max:",max(results[i]))
	print("Min:",min(results[i]))
	print("Num:",len(results[i]))

threads.sort()
print(threads)
print("Threads range:",threads[-1]-threads[0]+1)
