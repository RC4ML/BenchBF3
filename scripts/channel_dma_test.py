# please warning don't use sudo to run this scripts

import paramiko
import threading
import time

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
pcie_rep_addrs = ["", "01:00.0"]

machine_names = ["host", "bf3"]

program_name = "channel_bench"

output_file_format = "/home/cxz/result/channel_dma_new/{}_p{}_b{}_t{}_{}_{}"
payload = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072]
batch = [1, 2, 4, 8, 16, 32, 64, 128]
thread = [1, 2, 4, 8]

server_live = 15
client_live = 10

is_read_test_loop = [True, False]

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo channel_bench > {0}/test_suite/build_app && echo dma > {0}/test_suite/channel_bench/build_app "
        "&& rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
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


def host_run(ssh, program, thread_num, batch_size, payload_size, is_read, is_export, output_file):
    read_str = ""
    if is_read:
        read_str = "--g_is_read"

    export_str = ""
    life_time = client_live
    if is_export:
        export_str = "--g_is_exported"

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && ./{1} "
        "--g_thread_num {2} "
        "--g_batch_size {3} "
        "--g_payload {4} "
        "{5} "
        "{6} "
        "--g_pci_address {7} "
        "--g_life_time {8} > {9}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
                                         pcie_addrs[0], life_time,
                                         output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, thread_num, batch_size, payload_size, is_read, is_export, output_file):
    read_str = ""
    if is_read:
        read_str = "--g_is_read"

    export_str = ""
    life_time = server_live
    if is_export:
        export_str = "--g_is_exported"

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && ./{1} "
        "--g_thread_num {2} "
        "--g_batch_size {3} "
        "--g_payload {4} "
        "--g_is_server "
        "{5} "
        "{6} "
        "--g_pci_address {7} "
        "--g_rep_pci_address {8} "
        "--g_life_time {9} > {10}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
                                          pcie_addrs[1], pcie_rep_addrs[1], life_time,
                                          output_file)
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

    create_dir(ssh_host, output_file_format.format(1, 1, 1, 1, 1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1, 1, 1, 1, 1))
    print("create output file success")

    for t_i in thread:
        for b_i in batch:
            for index, p_i in enumerate(payload):
                for is_read_test in is_read_test_loop:
                    t1 = threading.Thread(target=host_run,
                                          args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, True,
                                                output_file_format.format(
                                                    machine_names[test_index[0]] + "-" + machine_names[test_index[1]],
                                                    p_i,
                                                    b_i, t_i, "read" if is_read_test else "write", "export")))
                    t2 = threading.Thread(target=bf3_run,
                                          args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, False,
                                                output_file_format.format(
                                                    machine_names[test_index[0]] + "-" + machine_names[test_index[1]],
                                                    p_i,
                                                    b_i, t_i, "read" if is_read_test else "write", "import")))

                    t2.start()
                    time.sleep(1)
                    t1.start()

                    t2.join()
                    t1.join()

                    print("finish one side")

                    t3 = threading.Thread(target=host_run,
                                          args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, False,
                                                output_file_format.format(
                                                    machine_names[test_index[0]] + "-" + machine_names[test_index[1]],
                                                    p_i,
                                                    b_i, t_i, "read" if is_read_test else "write", "import")))
                    t4 = threading.Thread(target=bf3_run,
                                          args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, True,
                                                output_file_format.format(
                                                    machine_names[test_index[0]] + "-" + machine_names[test_index[1]],
                                                    p_i,
                                                    b_i, t_i, "read" if is_read_test else "write", "export")))

                    t4.start()
                    time.sleep(1)
                    t3.start()

                    t4.join()
                    t3.join()

                    print("finish: thread {} batch {} payload {}  read {} \n".format(t_i, b_i, p_i, is_read_test))

    ssh_host.close()
    ssh_bf3.close()
