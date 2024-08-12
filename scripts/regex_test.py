# please warning don't use sudo to run this scripts

import paramiko
import threading

base = "/home/cxz/study/stupid-doca"
rule_path_src = base + "/test_suite/regex_bench/rules.txt"
rule_path_dst = "/tmp/rules"
rule_path_dst_file = rule_path_dst + ".rof2.binary"
input_path = base + "/test_suite/regex_bench/data.txt"

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

machine_names = ["host", "bf3"]

program_name = "regex_bench"
batch = [1, 2, 4, 8, 16, 32, 64]

output_file_format = "/home/cxz/result/regex_lat/{}_b{}"

host_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/x86_64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/x86_64-linux-gnu/pkgconfig"
bf3_pkg_path = "PKG_CONFIG_PATH=:/opt/mellanox/doca/lib/aarch64-linux-gnu/pkgconfig:/opt/mellanox/flexio/lib/pkgconfig:/opt/mellanox/grpc/lib/pkgconfig:/opt/mellanox/dpdk/lib/aarch64-linux-gnu/pkgconfig"


def make_and_clean(ssh, item_index, extra_flag=""):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}  && echo regex_bench > {0}/test_suite/build_app  && rm -rf ./build && mkdir build && cd build && {2} cmake .. {1} && make -j".format(
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


def compile(ssh, src, dst):
    stdin, stdout, stderr = ssh.exec_command(
        "rxpc -V bf3 -f {0} -p 0.001 -o {1}".format(src, dst)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def host_run(ssh, program, batch_size, rule, input, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_host && echo {7} | sudo -S ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--rule_path {4} "
        "--input_path {5} > {6}".format(base, program, batch_size,
                                        pcie_addrs[0], rule, input,
                                        output_file, passwd)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, program, batch_size, rule, input, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "cd {0}/bin_dpu && echo {7} | sudo -S ./{1} "
        "--g_batch_size {2} "
        "--g_pci_address {3} "
        "--rule_path {4} "
        "--input_path {5} > {6}".format(base, program, batch_size,
                                        pcie_addrs[1], rule, input,
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

    create_dir(ssh_host, output_file_format.format(1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1))

    print("create output file success")

    compile(ssh_host, rule_path_src, rule_path_dst)
    compile(ssh_bf3, rule_path_src, rule_path_dst)

    print("compile rule file success")

    for b_i in batch:
        t1 = threading.Thread(target=host_run,
                              args=(ssh_host, program_name, b_i, rule_path_dst_file, input_path,
                                    output_file_format.format(
                                        machine_names[test_index[0]], b_i)))
        t2 = threading.Thread(target=bf3_run,
                              args=(ssh_bf3, program_name, b_i, rule_path_dst_file, input_path,
                                    output_file_format.format(
                                        machine_names[test_index[1]], b_i)))

        t1.start()
        t1.join()

        print("finish one side")

        t2.start()
        t2.join()

        print("finish: batch {}\n".format(b_i))

    ssh_host.close()
    ssh_bf3.close()
