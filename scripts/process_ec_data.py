import os
import re
import xlwt

target_dir = "/home/cxz/result/ec_result"

host_hw_target_pattern = r"hw_host_p(\d+)_b(\d+)_k(\d+)_m(\d+)"
bf3_hw_target_pattern = r"hw_bf3_p(\d+)_b(\d+)_k(\d+)_m(\d+)"

host_sw_target_pattern = r"sw_host_p(\d+)_t(\d+)_k(\d+)_m(\d+)"
bf3_sw_target_pattern = r"sw_bf3_p(\d+)_t(\d+)_k(\d+)_m(\d+)"

inflight_map = {1: 1, 2: 2, 4: 3, 8: 4, 16: 5, 32: 6, 64: 7, 128: 8}

k_m = [[12, 4], [8, 8], [4, 4]]

on_dpu = False


class Vividict(dict):
    def __missing__(self, key):
        value = self[key] = type(self)()
        return value


def get_band_encode_result(file_name):
    bw_list = []
    pattern = r'^\s*encode\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(2)))
    f.close()
    return sum(bw_list) / len(bw_list)


def get_band_decode_result(file_name):
    bw_list = []
    pattern = r'^\s*decode\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(2)))
    f.close()
    return sum(bw_list) / len(bw_list)


def get_lat_encode_result(file_name):
    bw_list = []
    pattern = r'^\s*encode\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(1)))
    f.close()
    return sum(bw_list) / len(bw_list)


def get_lat_decode_result(file_name):
    bw_list = []
    pattern = r'^\s*decode\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*$'
    with open(file_name, "r") as f:
        for line in f:
            match = re.match(pattern, line)
            if match is not None:
                bw_list.append(float(match.group(1)))
    f.close()
    return sum(bw_list) / len(bw_list)


def generate_band_result(target_sheet, target_dir, pattern, k_m_num, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))
            k_num = int(m.group(3))
            m_num = int(m.group(4))
            if k_m_num[0] != k_num or k_m_num[1] != m_num:
                continue

            encode_result = get_band_encode_result(target_dir + "/" + fi)
            decode_result = get_band_decode_result(target_dir + "/" + fi)

            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = [encode_result, decode_result]

    target_sheet.write(row_offset, col_offset, "[encode]payload/inflight")
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

    target_sheet.write(row_offset, col_offset, "[decode]payload/inflight")
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


def generate_lat_result(target_sheet, target_dir, pattern, k_m_num, now_inflight_map, row_offset=0, col_offset=0):
    target_map = Vividict()
    for fi in os.listdir(target_dir):
        if re.match(pattern, fi):
            m = re.match(pattern, fi)
            payload = int(m.group(1))
            batch_size = int(m.group(2))
            k_num = int(m.group(3))
            m_num = int(m.group(4))
            if k_m_num[0] != k_num or k_m_num[1] != m_num:
                continue
            encode_result = get_lat_encode_result(target_dir + "/" + fi)
            decode_result = get_lat_decode_result(target_dir + "/" + fi)

            if target_map[payload][batch_size] == {}:
                target_map[payload][batch_size] = [encode_result, decode_result]

    target_sheet.write(row_offset, col_offset, "[encode]payload/inflight")
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

    target_sheet.write(row_offset, col_offset, "[decode]payload/inflight")
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
        hw_pattern = host_hw_target_pattern
        sw_pattern = host_sw_target_pattern
    else:
        hw_pattern = bf3_hw_target_pattern
        sw_pattern = bf3_sw_target_pattern

    for k_m_item in k_m:
        now_band = workbook.add_sheet("{}-{}".format(k_m_item[0], k_m_item[1]))

        row_offset = 0

        generate_band_result(now_band, target_dir, hw_pattern, k_m_item, inflight_map,
                             row_offset, 0)
        generate_lat_result(now_band, target_dir, hw_pattern, k_m_item, inflight_map, row_offset,
                            15)
        row_offset += 30
        generate_band_result(now_band, target_dir, sw_pattern, k_m_item, inflight_map,
                             row_offset, 0)
        generate_lat_result(now_band, target_dir, sw_pattern, k_m_item, inflight_map, row_offset,
                            15)

    workbook.save('result.xls')
