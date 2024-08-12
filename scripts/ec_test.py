# please warning don't use sudo to run this scripts

import paramiko
import threading

base = "/home/cxz/study/stupid-doca"

user = "cxz"
passwd = "cxz123"

# whether rebuild from source code
init_build = True

# index of test machine
# 0: host representer 0
# 1: bf3 representer 0


test_index = [0, 1]

# use for Nexus connect
host_machine = "192.168.1.2"
bf3_machine = "192.168.1.3"

machine_ips = [host_machine, bf3_machine]
pcie_addrs = ["01:00.0", "03:00.0"]

machine_names = ["host", "bf3"]

program_name = "ec_bench"
sw_program_path = base + "/test_suite/ec_bench/sw/"
sw_program_name_amd64 = "ec_bench_sw_amd64"
sw_program_name_arm64 = "ec_bench_sw_arm64"

batch_size = [1, 2, 4, 8, 16, 32, 64]
sw_thread_size = [1, 2, 4, 8, 16]
payloads = [1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576]
blocks = 1024
k_m = [[12, 4], [8, 8], [4, 4]]

life_time = 10

output_file_format_hw = "/home/cxz/result/ec_result/hw_{}_p{}_b{}_k{}_m{}"
output_file_format_sw = "/home/cxz/result/ec_result/sw_{}_p{}_t{}_k{}_m{}"

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo ec_bench > {0}/test_suite/build_app  && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
            base,
            extra_flag,
            host_pkg_path if item_index < 1 else bf3_pkg_path))
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def create_dir(ssh, filename):
    stdin, stdout, stderr = ssh.exec_command(
        "mkdir -p $(dirname {0})".format(filename)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def host_run(ssh, program, b_size, k_size, m_size, blocks_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && echo {9} | sudo -S ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--k {4} "
        "--m {5} "
        "--blocks {6} "
        "--g_payload {7} "
        "--g_life_time {10} > {8}".format(base, program, b_size,
                                          pcie_addrs[0], k_size, m_size, blocks_size, payload,
                                          output_file, passwd, life_time)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, b_size, k_size, m_size, blocks_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && echo {9} | sudo -S ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--k {4} "
        "--m {5} "
        "--blocks {6} "
        "--g_payload {7} "
        "--g_life_time {10} > {8}".format(base, program, b_size,
                                          pcie_addrs[1], k_size, m_size, blocks_size, payload,
                                          output_file, passwd, life_time)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def host_sw_run(ssh, t_size, k_size, m_size, blocks_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0} && echo {9} | sudo -S ./{1} "
        "-cpu {2} "
        "-k {3} "
        "-m {4} "
        "-blocks {5} "
        "-size {6} "
        "-concurrent "
        "-duration {7} > {8}".format(sw_program_path, sw_program_name_amd64, t_size,
                                     k_size, m_size, blocks_size, payload, life_time,
                                     output_file, passwd)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_sw_run(ssh, t_size, k_size, m_size, blocks_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0} && echo {9} | sudo -S ./{1} "
        "-cpu {2} "
        "-k {3} "
        "-m {4} "
        "-blocks {5} "
        "-size {6} "
        "-concurrent "
        "-duration {7} > {8}".format(sw_program_path, sw_program_name_arm64, t_size,
                                     k_size, m_size, blocks_size, payload, life_time,
                                     output_file, passwd)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def ssh_connect(ip, user, passwd):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, 22, user, passwd)
    return ssh


if __name__ == '__main__':
    ssh_host = ssh_connect(machine_ips[test_index[0]], user, passwd)
    ssh_bf3 = ssh_connect(machine_ips[test_index[1]], user, passwd)

    if init_build:
        t0 = threading.Thread(target=make_and_clean,
                              args=(ssh_host, test_index[0], ""))
        m1 = threading.Thread(target=make_and_clean,
                              args=(ssh_bf3, test_index[1], "-DDPU=ON"))
        # build one by one
        t0.start()
        t0.join()

        m1.start()
        m1.join()

    create_dir(ssh_host, output_file_format_hw.format(1, 1, 1, 1, 1))
    create_dir(ssh_bf3, output_file_format_hw.format(1, 1, 1, 1, 1))

    print("create output file success")

    for k_m_item in k_m:
        for p_i in payloads:
            for b_i in batch_size:
                t1 = threading.Thread(target=host_run,
                                      args=(ssh_host, program_name, b_i, k_m_item[0], k_m_item[1], blocks, p_i,
                                            output_file_format_hw.format(
                                                machine_names[test_index[0]], p_i, b_i, k_m_item[0], k_m_item[1])))
                t2 = threading.Thread(target=bf3_run,
                                      args=(ssh_bf3, program_name, b_i, k_m_item[0], k_m_item[1], blocks, p_i,
                                            output_file_format_hw.format(
                                                machine_names[test_index[1]], p_i, b_i, k_m_item[0], k_m_item[1])))

                t1.start()
                t1.join()

                print("finish host side")

                t2.start()
                t2.join()

                print("finish: k_m {}  payload {}  batch {}\n".format(k_m_item, p_i, b_i))

    for k_m_item in k_m:
        for p_i in payloads:
            for t_i in sw_thread_size:
                t1 = threading.Thread(target=host_sw_run,
                                      args=(ssh_host, t_i, k_m_item[0], k_m_item[1], blocks, p_i,
                                            output_file_format_sw.format(
                                                machine_names[test_index[0]], p_i, t_i, k_m_item[0], k_m_item[1])))
                t2 = threading.Thread(target=bf3_sw_run,
                                      args=(ssh_bf3, t_i, k_m_item[0], k_m_item[1], blocks, p_i,
                                            output_file_format_sw.format(
                                                machine_names[test_index[1]], p_i, t_i, k_m_item[0], k_m_item[1])))

                t1.start()
                t1.join()

                print("finish host side")

                t2.start()
                t2.join()

                print("finish: k_m {}  payload {}  thread {}\n".format(k_m_item, p_i, t_i))

    ssh_host.close()
    ssh_bf3.close()
