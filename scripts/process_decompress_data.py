import os
import re
import xlwt

target_dir = "/home/cxz/result/compress_result"

host_target_pattern = r"host_p(\d+)_b(\d+)"
bf3_target_pattern = r"bf3_p(\d+)_b(\d+)"

inflight_map = {1: 1, 2: 2, 4: 3, 8: 4, 16: 5, 32: 6, 64: 7}

decompress_type = ["deflate"]
on_dpu = False


class Vividict(dict):
    def __missing__(self, key):
        value = self[key] = type(self)()
        return value


def get_band_result(file_name, is_hardware=False):
    bw_list = []
    pattern = ""
    if is_hardware:
        pattern = r'\s*hw \w+ bandwidth (-?\d+(?:\.\d+)?) MB\/s$'
    else:
        pattern = r'\s*sw \w+ bandwidth (-?\d+(?:\.\d+)?) MB\/s$'

    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(1)))
    f.close()
    return sum(bw_list) / len(bw_list)


def get_lat_result(file_name, is_hardware=False):
    bw_list = []
    pattern = ""
    if is_hardware:
        pattern = r'^\s*hw \w+ latency (-?\d+(?:\.\d+)?) us$'
    else:
        pattern = r'^\s*sw \w+ latency (-?\d+(?:\.\d+)?) us$'

    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(1)))
    f.close()
    return sum(bw_list) / len(bw_list)


def generate_band_result(target_sheet, target_dir, pattern, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))

            sw_result = get_band_result(target_dir + "/" + fi, False)
            hw_result = get_band_result(target_dir + "/" + fi, True)

            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = [hw_result, sw_result]

    target_sheet.write(row_offset, col_offset, "[hw]payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    row = 1
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key][0])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1

    row_offset += 2 + row
    row = 1

    target_sheet.write(row_offset, col_offset, "[sw]payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key][1])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1


def generate_lat_result(target_sheet, target_dir, pattern, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))

            sw_result = get_lat_result(target_dir + "/" + fi, False)
            hw_result = get_lat_result(target_dir + "/" + fi, True)

            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = [hw_result, sw_result]

    target_sheet.write(row_offset, col_offset, "[hw]payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    row = 1
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key][0])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1

    row_offset += 2 + row
    row = 1

    target_sheet.write(row_offset, col_offset, "[sw]payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key][1])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1


if __name__ == '__main__':
    workbook = xlwt.Workbook(encoding='utf-8')
    if not on_dpu:
        file_pattern = host_target_pattern
    else:
        file_pattern = bf3_target_pattern

    for item_type in decompress_type:
        now_band = workbook.add_sheet("{}".format(item_type))

        row_offset = 0

        generate_band_result(now_band, target_dir, file_pattern, inflight_map, row_offset, 0)
        generate_lat_result(now_band, target_dir, file_pattern, inflight_map, row_offset, 15)

    workbook.save('result.xls')
