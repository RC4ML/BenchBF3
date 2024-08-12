import time

def read_hex_value(filename):
    with open(filename, 'r') as file:
        hex_value = file.read().strip()
        return int(hex_value, 16)

def calculate_hex_diff(old_value, new_value):
    return  (new_value - old_value) *64 / (1024 * 1024) 

def print_decimal_value(desp, value1, value2):
    print(f"{desp} Read: {value1:.2f} MB   Write: {value2:.2f} MB") # 保留两位小数并打印单位

bfperf = "hwmon4"
# 文件路径
cache_read_file1 = '/sys/class/hwmon/{0}/llt0/counter0'.format(bfperf)
cache_read_file2 = '/sys/class/hwmon/{0}/llt0/counter1'.format(bfperf)

cache_write_file1 = '/sys/class/hwmon/{0}/llt0/counter2'.format(bfperf)
cache_write_file2 = '/sys/class/hwmon/{0}/llt0/counter3'.format(bfperf)


cache_miss_read_file1 = '/sys/class/hwmon/{0}/llt_miss0/counter0'.format(bfperf)
cache_miss_write_file1 = '/sys/class/hwmon/{0}/llt_miss0/counter1'.format(bfperf)

# 初始值
old_cache_read = read_hex_value(cache_read_file1) + read_hex_value(cache_read_file2)
old_cache_write = read_hex_value(cache_write_file1) + read_hex_value(cache_write_file2)
old_cache_miss_read = read_hex_value(cache_miss_read_file1)
old_cache_miss_write = read_hex_value(cache_miss_write_file1)

while True:
    # 读取新的十六进制值
    new_cache_read = read_hex_value(cache_read_file1) + read_hex_value(cache_read_file2)
    new_cache_write = read_hex_value(cache_write_file1) + read_hex_value(cache_write_file2)
    new_cache_miss_read = read_hex_value(cache_miss_read_file1)
    new_cache_miss_write = read_hex_value(cache_miss_write_file1)

    # 计算数值变化
    diff1 = calculate_hex_diff(old_cache_read, new_cache_read)
    diff2 = calculate_hex_diff(old_cache_write, new_cache_write)
    diff3 = calculate_hex_diff(old_cache_miss_read, new_cache_miss_read)
    diff4 = calculate_hex_diff(old_cache_miss_write, new_cache_miss_write)

    # 打印十进制值
    print_decimal_value("cache", diff1, diff2)
    print_decimal_value("miss ", diff3, diff4)
    # 更新旧值
    old_cache_read = new_cache_read
    old_cache_write = new_cache_write
    old_cache_miss_read = new_cache_miss_read
    old_cache_miss_write = new_cache_miss_write


    # 暂停1秒
    time.sleep(1)