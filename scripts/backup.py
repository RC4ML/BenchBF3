import paramiko
import threading
import time

base = "/home/wyx/stupid-doca"

init_build = False

# index of test machine
# 0: host representer 0
# 1: host representer 1
# 2: bf3 representer 0
# 3: bf3 representer 1

test_index = [0, 2]

# use for Nexus connect
host_machine = "192.168.1.2"
bf3_machine = "192.168.1.3"

machine_ips = [host_machine, host_machine, bf3_machine, bf3_machine]
pcie_addrs = ["01:00.0", "01:00.1", "03:00.0", "03:00.1"]

machine_names = ["host0", "host1", "bf3_0", "bf3_1"]

port = 1234

program_name = "dma_copy_bench"

user = "wyx"
passwd = "badan"
output_file_format = "/home/wyx/result/dma_copy/{}_p{}_b{}_t{}_{}_{}"
payload = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576]
thread = [1]
batch = [1, 128]

is_read_test = True

exporter_live = 15
importer_live = 10

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(base,
                                                                                                     extra_flag,
                                                                                                     host_pkg_path if item_index <= 1 else bf3_pkg_path))
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def host_run(ssh, program, thread_num, batch_size, payload_size, is_read, is_export, output_file, item_index,
             remote_item_index):
    read_str = ""
    if is_read:
        read_str = "--read"
    export_str = ""
    life_time = importer_live
    ip_str = machine_ips[item_index]
    if is_export:
        export_str = "--export"
        life_time = exporter_live
        ip_str = machine_ips[remote_item_index]

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && ./{1} "
        "--thread {2} "
        "--batch {3} "
        "--payload {4} "
        "{5} "
        "{6} "
        "--addr {7} "
        "--pci-addr {8} "
        "--time {9} > {10}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
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
        read_str = "--read"
    export_str = ""
    life_time = importer_live
    ip_str = machine_ips[item_index]
    if is_export:
        export_str = "--export"
        life_time = exporter_live
        ip_str = machine_ips[remote_item_index]

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && ./{1} "
        "--thread {2} "
        "--batch {3} "
        "--payload {4} "
        "{5} "
        "{6} "
        "--addr {7} "
        "--pci-addr {8} "
        "--time {9} > {10}".format(base, program, thread_num, batch_size, payload_size, read_str, export_str,
                                   ip_str + ":" + str(port), pcie_addrs[item_index], life_time,
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
                              args=(ssh_host, test_index[0], "-DDPU=ON" if test_index[0] > 1 else ""))
        m1 = threading.Thread(target=make_and_clean,
                              args=(ssh_bf3, test_index[1], "-DDPU=ON" if test_index[1] > 1 else ""))
        # build one by one
        t0.start()
        t0.join()

        m1.start()
        m1.join()

    for t_i in thread:
        for b_i in batch:
            for index, p_i in enumerate(payload):
                t1 = threading.Thread(target=host_run if test_index[0] <= 1 else bf3_run,
                                      args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, True,
                                            output_file_format.format(
                                                machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
                                                b_i, t_i,
                                                "read" if is_read_test else "write",
                                                "export"), test_index[0], test_index[1]))
                t2 = threading.Thread(target=host_run if test_index[1] <= 1 else bf3_run,
                                      args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, False,
                                            output_file_format.format(
                                                machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
                                                b_i, t_i,
                                                "read" if is_read_test else "write",
                                                "import"), test_index[1], test_index[0]))

                t2.start()
                time.sleep(2)
                t1.start()

                t2.join()
                t1.join()

                print("finish one side")

                t3 = threading.Thread(target=host_run if test_index[0] <= 1 else bf3_run,
                                      args=(ssh_host, program_name, t_i, b_i, p_i, is_read_test, False,
                                            output_file_format.format(
                                                machine_names[test_index[1]] + "-" + machine_names[test_index[0]], p_i,
                                                b_i, t_i,
                                                "read" if is_read_test else "write",
                                                "import"), test_index[0], test_index[1]))
                t4 = threading.Thread(target=host_run if test_index[1] <= 1 else bf3_run,
                                      args=(ssh_bf3, program_name, t_i, b_i, p_i, is_read_test, True,
                                            output_file_format.format(
                                                machine_names[test_index[1]] + "-" + machine_names[test_index[0]], p_i,
                                                b_i, t_i,
                                                "read" if is_read_test else "write",
                                                "export"), test_index[1], test_index[0]))

                t3.start()
                time.sleep(2)
                t4.start()

                t3.join()
                t4.join()

                print("finish: thread {} batch {} payload {}\n".format(t_i, b_i, p_i))

    ssh_host.close()
    ssh_bf3.close()
