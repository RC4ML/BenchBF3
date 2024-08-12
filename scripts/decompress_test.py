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

host_machine = "192.168.1.2"
bf3_machine = "192.168.1.3"

machine_ips = [host_machine, bf3_machine]
pcie_addrs = ["01:00.0", "03:00.0"]

machine_names = ["host", "bf3"]

program_name = "compress_bench"
test_file_path = "/home/cxz/study/compress_source/"
test_file_prefix = "total_"

batch_size = [1, 2, 4, 8, 16, 32, 64]
payloads = [4096, 8192, 16384, 32768, 65536, 131072, 262144]

output_file_format = "/home/cxz/result/compress_result/{}_p{}_b{}"

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo compress_bench > {0}/test_suite/build_app  && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
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


def host_run(ssh, program, b_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--input_path {4} > {5}".format(base, program, b_size,
                                        pcie_addrs[0], test_file_path + test_file_prefix + str(payload),
                                        output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, b_size, payload, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--input_path {4} > {5}".format(base, program, b_size,
                                        pcie_addrs[1], test_file_path + test_file_prefix + str(payload),
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

    create_dir(ssh_host, output_file_format.format(1, 1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1, 1))

    print("create output file success")

    for p_i in payloads:
        for b_i in batch_size:
            t1 = threading.Thread(target=host_run,
                                  args=(ssh_host, program_name, b_i, p_i,
                                        output_file_format.format(
                                            machine_names[test_index[0]], p_i, b_i)))
            t2 = threading.Thread(target=bf3_run,
                                  args=(ssh_bf3, program_name, b_i, p_i,
                                        output_file_format.format(
                                            machine_names[test_index[1]], p_i, b_i)))

            t1.start()
            t1.join()

            print("finish host side")

            t2.start()
            t2.join()

            print("finish: payload {}  batch {}\n".format(p_i, b_i))

    ssh_host.close()
    ssh_bf3.close()
