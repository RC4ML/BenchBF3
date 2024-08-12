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
# 1: host representer 1
# 2: bf3 representer 0
# 3: bf3 representer 1

test_index = [0, 1]

# use for Nexus connect
host_machine = "192.168.1.2"
bf3_machine = "192.168.1.3"

machine_ips = [host_machine, bf3_machine]
pcie_addrs = ["01:00.0", "03:00.0"]
pcie_rep_addrs = ["", "01:00.0"]

machine_names = ["host", "bf3"]

program_name = "channel_bench"

output_file_format = "/home/cxz/result/channel_lat/{}_p{}_b{}_{}"
payload = [16, 32, 64, 128, 256, 512, 1024]
batch = [1, 2, 4, 8, 16, 32, 64, 128]

server_live = 15
client_live = 10

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo channel_bench > {0}/test_suite/build_app && echo rtt > {0}/test_suite/channel_bench/build_app && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
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


def host_run(ssh, program, batch_size, payload_size, is_export, output_file):
    export_str = ""
    life_time = client_live
    if is_export:
        export_str = "--g_is_exported"

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && ./{1} "
        "--g_batch_size {2} "
        "--g_payload {3} "
        "{4} "
        "--g_pci_address {5} "
        "--g_life_time {6} > {7}".format(base, program, batch_size, payload_size, export_str,
                                         pcie_addrs[0], life_time,
                                         output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, batch_size, payload_size, is_export, output_file):
    export_str = ""
    life_time = server_live
    if is_export:
        export_str = "--g_is_exported"

    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && ./{1} "
        "--g_batch_size {2} "
        "--g_payload {3} "
        "--g_is_server "
        "{4} "
        "--g_pci_address {5} "
        "--g_rep_pci_address {6} "
        "--g_life_time {7} > {8}".format(base, program, batch_size, payload_size, export_str,
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

    create_dir(ssh_host, output_file_format.format(1, 1, 1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1, 1, 1))
    print("create output file success")

    for b_i in batch:
        for index, p_i in enumerate(payload):
            t1 = threading.Thread(target=host_run,
                                  args=(ssh_host, program_name, b_i, p_i, True,
                                        output_file_format.format(
                                            machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
                                            b_i, "export")))
            t2 = threading.Thread(target=bf3_run,
                                  args=(ssh_bf3, program_name, b_i, p_i, False,
                                        output_file_format.format(
                                            machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
                                            b_i, "import")))

            t2.start()
            time.sleep(2)
            t1.start()

            t2.join()
            t1.join()

            print("finish one side")

            # t3 = threading.Thread(target=host_run,
            #                       args=(ssh_host, program_name, b_i, p_i, False,
            #                             output_file_format.format(
            #                                 machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
            #                                 b_i, "import")))

            # t4 = threading.Thread(target=bf3_run,
            #                       args=(ssh_bf3, program_name, b_i, p_i, True,
            #                             output_file_format.format(
            #                                 machine_names[test_index[0]] + "-" + machine_names[test_index[1]], p_i,
            #                                 b_i, "export")))

            # t3.start()
            # time.sleep(2)
            # t4.start()

            # t3.join()
            # t4.join()

            print("finish: batch {} payload {}\n".format(b_i, p_i))

    ssh_host.close()
    ssh_bf3.close()
