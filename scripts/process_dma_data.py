import os
import re
import math
import xlwt

target_dir = "/home/cxz/result/dma_copy_2.7.0_new"

host_read_target_pattern = r"host0-bf3_0_p(\d+)_b(\d+)_t(\d+)_read_import"
host_write_target_pattern = r"host0-bf3_0_p(\d+)_b(\d+)_t(\d+)_write_import"

bf3_read_target_pattern = r"host0-bf3_0_p(\d+)_b(\d+)_t(\d+)_read_import"
bf3_write_target_pattern = r"host0-bf3_0_p(\d+)_b(\d+)_t(\d+)_write_import"

inflight_map = {1: 1, 2: 2, 4: 3, 8: 4, 16: 5, 32: 6, 64: 7, 128: 8}

thread_list = [1,2,3,4,5,6,7,8]

on_dpu = False


class Vividict(dict):
    def __missing__(self, key):
        value = self[key] = type(self)()
        return value


def get_band_result(file_name):
    bw_list = []
    pattern = r'^\s*(-?\d+(\.\d+)?)\s+(-?\d+(\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(1)))
    f.close()
    return sum(bw_list[1:]) / len(bw_list[1:])


def get_lat_result(file_name):
    result_avg = 0
    pattern = r'^\s*latency\s+(-?\d+(\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                result_avg = float(match.group(1))

    f.close()
    return round(result_avg, 2) 


def generate_band_result(target_sheet, target_dir, pattern, thread_num, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))
            num_thread = int(m.group(3))
            if thread_num != num_thread:
                continue 
            sum_result = get_band_result(target_dir + "/" + fi)
            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = sum_result
            else:
                target_map[payload][batch_size] = max(target_map[payload][batch_size], sum_result)

    target_sheet.write(row_offset, col_offset, "payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    row = 1
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1


def generate_lat_result(target_sheet, target_dir, pattern, thread_num, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))
            num_thread = int(m.group(3))
            if thread_num != num_thread:
                continue
            lat_result = get_lat_result(target_dir + "/" + fi)
            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = lat_result
            else:
                target_map[payload][batch_size] = max(target_map[payload][batch_size], lat_result)

    target_sheet.write(row_offset, col_offset, "payload/inflight")
    for i in now_inflight_map.keys():
        target_sheet.write(row_offset, now_inflight_map[i] + col_offset, i)
    row = 1
    for payload_key in sorted(target_map):
        target_sheet.write(row + row_offset, col_offset, payload_key)
        for num_thread_key in sorted(target_map[payload_key]):
            if num_thread_key in now_inflight_map:
                target_sheet.write(row + row_offset, now_inflight_map[num_thread_key] + col_offset,
                                   target_map[payload_key][num_thread_key])
            else:
                print("error: not find thread num {} in thread map".format(num_thread_key))
        row = row + 1


if __name__ == '__main__':
    workbook = xlwt.Workbook(encoding='utf-8')
    read_sheet_band = workbook.add_sheet('read')
    write_sheet_band = workbook.add_sheet('write')

    if not on_dpu:
        read_pattern = bf3_read_target_pattern
        write_pattern = bf3_write_target_pattern
    else:
        read_pattern = host_read_target_pattern
        write_pattern = host_write_target_pattern

    row_offset = 0
    for thread_num in thread_list:
        generate_band_result(read_sheet_band, target_dir, read_pattern, thread_num, inflight_map,
                             row_offset, 0)
        generate_lat_result(read_sheet_band, target_dir, read_pattern, thread_num, inflight_map, row_offset,
                            15)
        row_offset += 20

    row_offset = 0
    for thread_num in thread_list:
        generate_band_result(write_sheet_band, target_dir, write_pattern, thread_num, inflight_map,
                             row_offset, 0)
        generate_lat_result(write_sheet_band, target_dir, write_pattern, thread_num, inflight_map,
                            row_offset, 15)
        row_offset += 20

    workbook.save('result.xls')
