# please warning don't use sudo to run this scripts

import paramiko
import threading
import time

base = "/home/cxz/study/stupid-doca"

user = "cxz"
key_file = "/home/cxz/.ssh/id_ed25519"
# whether rebuild from source code
init_build = False

# use for Nexus connect
host_machine = "172.25.2.23"
bf3_machine = "172.25.2.131"

machine_ips = [host_machine, host_machine, bf3_machine, bf3_machine]
pcie_addrs = ["40:00.0", "40:00.1", "03:00.0", "03:00.1"]

machine_names = ["host0", "host1", "bf3_0", "bf3_1"]

port = 1234

program_name = "gdr_bench"

output_file_format = "/home/cxz/result/gdr_h100/{}_p{}_b{}_t{}_{}_{}"

# index of test machine
# 0: host representer 0
# 1: host representer 1
# 2: bf3 representer 0
# 3: bf3 representer 1

test_index_loop = [[0, 2]]
is_read_test_loop = [True, False]
payload = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072]
thread = [1,2,3,4,5,6,7,8]
batch = [32]

exporter_live = 25
importer_live = 12

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/collectx/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/collectx/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/collectx/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo gdr_bench > {0}/test_suite/build_app && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
            base,
            extra_flag,
            host_pkg_path if item_index <= 1 else bf3_pkg_path))
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


def host_run(ssh, program, thread_num, batch_size, payload_size, is_read, is_export, output_file, item_index,
             remote_item_index):
    read_str = ""
    if is_read:
        read_str = "--g_is_read"
    export_str = ""
    life_time = importer_live
    ip_str = machine_ips[item_index]
    if is_export:
        export_str = "--g_is_exported"
        life_time = exporter_live
        ip_str = machine_ips[remote_item_index]

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && ./{1} "
        "--g_thread_num {2} "
        "--g_batch_size {3} "
        "--g_payload {4} "
        "{5} "
        "{6} "
        "--g_network_address {7} "
        "--g_pci_address {8} "
        "--g_life_time {9} > {10}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
                                          ip_str + ":" + str(port), pcie_addrs[item_index], life_time,
                                          output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, thread_num, batch_size, payload_size, is_read, is_export, output_file, item_index,
            remote_item_index):
    read_str = ""
    if is_read:
        read_str = "--g_is_read"
    export_str = ""
    life_time = importer_live
    ip_str = machine_ips[item_index]
    if is_export:
        export_str = "--g_is_exported"
        life_time = exporter_live
        ip_str = machine_ips[remote_item_index]

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && ./{1} "
        "--g_thread_num {2} "
        "--g_batch_size {3} "
        "--g_payload {4} "
        "{5} "
        "{6} "
        "--g_network_address {7} "
        "--g_pci_address {8} "
        "--g_life_time {9} > {10}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
                                          ip_str + ":" + str(port), pcie_addrs[item_index], life_time,
                                          output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def ssh_connect(ip, user):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, 22, user, None, key_filename=key_file)
    return ssh


if __name__ == '__main__':
    for test_index in test_index_loop:
        ssh_host = ssh_connect(machine_ips[test_index[0]], user)
        ssh_bf3 = ssh_connect(machine_ips[test_index[1]], user)

        if init_build:
            t0 = threading.Thread(target=make_and_clean,
                                  args=(ssh_host, test_index[0]))
            m1 = threading.Thread(target=make_and_clean,
                                  args=(ssh_bf3, test_index[1]))
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
                        t1 = threading.Thread(target=host_run if test_index[0] <= 1 else bf3_run,
                                              args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, True,
                                                    output_file_format.format(
                                                        machine_names[test_index[0]] + "-" + machine_names[
                                                            test_index[1]],
                                                        p_i,
                                                        b_i, t_i,
                                                        "read" if is_read_test else "write",
                                                        "export"), test_index[0], test_index[1]))
                        t2 = threading.Thread(target=host_run if test_index[1] <= 1 else bf3_run,
                                              args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, False,
                                                    output_file_format.format(
                                                        machine_names[test_index[0]] + "-" + machine_names[
                                                            test_index[1]],
                                                        p_i,
                                                        b_i, t_i,
                                                        "read" if is_read_test else "write",
                                                        "import"), test_index[1], test_index[0]))

                        t2.start()
                        time.sleep(2)
                        t1.start()

                        t2.join()
                        t1.join()

                        # print("finish one side")

                        # t3 = threading.Thread(target=host_run if test_index[0] <= 1 else bf3_run,
                        #                       args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, False,
                        #                             output_file_format.format(
                        #                                 machine_names[test_index[1]] + "-" + machine_names[
                        #                                     test_index[0]],
                        #                                 p_i,
                        #                                 b_i, t_i,
                        #                                 "read" if is_read_test else "write",
                        #                                 "import"), test_index[0], test_index[1]))
                        # t4 = threading.Thread(target=host_run if test_index[1] <= 1 else bf3_run,
                        #                       args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, True,
                        #                             output_file_format.format(
                        #                                 machine_names[test_index[1]] + "-" + machine_names[
                        #                                     test_index[0]],
                        #                                 p_i,
                        #                                 b_i, t_i,
                        #                                 "read" if is_read_test else "write",
                        #                                 "export"), test_index[1], test_index[0]))

                        # t3.start()
                        # time.sleep(2)
                        # t4.start()

                        # t3.join()
                        # t4.join()

                        print("finish: thread {} batch {} payload {} read {}\n".format(t_i, b_i, p_i, is_read_test))
                        time.sleep(3)
        ssh_host.close()
        ssh_bf3.close()
