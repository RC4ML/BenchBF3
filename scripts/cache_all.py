import time

def read_hex_value(filename):
    with open(filename, 'r') as file:
        hex_value = file.read().strip()
        return int(hex_value, 16)

def calculate_hex_diff(new_value, old_value):
    return  round((new_value - old_value) *64 / (1024 * 1024), 1) 

def print_decimal_value(desp, value1):
    print(f"{desp} {value1} MB") # 保留两位小数并打印单位


# 初始值

old_cache_read_all = [0] * 8
old_cache_write_all = [0] * 8
old_cache_miss_read_all = [0] * 8
old_cache_miss_write_all = [0] * 8

diff_cache_read_all = [0] * 8
diff_cache_write_all = [0] * 8
diff_cache_miss_read_all = [0] * 8
diff_cache_miss_write_all = [0] * 8

bfperf = "hwmon4"

while True:
    # 读取新的十六进制值
    for i in range(8):
        cache_read_file1 = '/sys/class/hwmon/{0}/llt'.format(bfperf) + str(i) + '/counter0'
        cache_read_file2 = '/sys/class/hwmon/{0}/llt'.format(bfperf) + str(i) + '/counter1'
        cache_write_file1 = '/sys/class/hwmon/{0}/llt'.format(bfperf) + str(i) + '/counter2'
        cache_write_file2 = '/sys/class/hwmon/{0}/llt'.format(bfperf) + str(i) + '/counter3'
        cache_miss_read_file1 = '/sys/class/hwmon/{0}/llt_miss'.format(bfperf) + str(i) + '/counter0'
        cache_miss_write_file1 = '/sys/class/hwmon/{0}/llt_miss'.format(bfperf) + str(i) + '/counter1'

        new_cache_read = read_hex_value(cache_read_file1) + read_hex_value(cache_read_file2)
        new_cache_write = read_hex_value(cache_write_file1) + read_hex_value(cache_write_file2)
        new_cache_miss_read = read_hex_value(cache_miss_read_file1)
        new_cache_miss_write = read_hex_value(cache_miss_write_file1)
        diff_cache_read_all[i] = calculate_hex_diff(new_cache_read, old_cache_read_all[i])
        diff_cache_write_all[i] = calculate_hex_diff(new_cache_write, old_cache_write_all[i])
        diff_cache_miss_read_all[i] = calculate_hex_diff(new_cache_miss_read, old_cache_miss_read_all[i])  
        diff_cache_miss_write_all[i] = calculate_hex_diff(new_cache_miss_write, old_cache_miss_write_all[i])

        old_cache_read_all[i] = new_cache_read
        old_cache_write_all[i] = new_cache_write
        old_cache_miss_read_all[i] = new_cache_miss_read
        old_cache_miss_write_all[i] = new_cache_miss_write

    # 打印十进制值
    print_decimal_value("Cache Read", diff_cache_read_all)
    print_decimal_value("Cache Write", diff_cache_write_all)
    print_decimal_value("Read Miss", diff_cache_miss_read_all)
    print_decimal_value("Write Miss", diff_cache_miss_write_all)
    print("")
    print(f"Cache Read: {sum(diff_cache_read_all):.2f} MB   Write: {sum(diff_cache_write_all):.2f} MB") # 保留两位小数并打印单位
    print(f"Miss Read: {sum(diff_cache_miss_read_all):.2f} MB   Write: {sum(diff_cache_miss_write_all):.2f} MB") # 保留两位小数并打印单位

    print("---------------")
    # 更新旧值

    # 暂停1秒
    time.sleep(1)