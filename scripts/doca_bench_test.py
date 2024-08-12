# please warning don't use sudo to run this scripts

import paramiko
import threading
import time

user = "cxz"
key_file = "/home/cxz/.ssh/id_ed25519"

host_machine = "172.25.2.23"
bf3_machine = "172.25.2.133"

host_pcie_addr = "c1:00.0"
bf3_pcie_addr = "03:00.0"

host_machine_name = "host0"
bf3_machine_name = "bf3_3"

port = 1234

program_name = "doca_dma"

doca_bench_path = "/opt/mellanox/doca/tools/doca_bench"

output_file_format = "/home/cxz/result/doca_bench_2.7.0/{}_p{}_b{}_t{}_{}"

is_read_test_loop = [True, False]

payload = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072]
thread = [1,2,3,4,5,6,7,8,9,10]
batch = [1,2,4,8,16,32,64]

run_seconds = 10
host_core_list = "16-31"
bf3_core_list = "0-11"


def ssh_connect(ip, user):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, 22, user, None, key_filename=key_file)
    return ssh

def create_dir(ssh, filename):
    stdin, stdout, stderr = ssh.exec_command(
        "mkdir -p $(dirname {0})".format(filename)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)

def host_run(ssh, thread_num, batch_size, payload_size, is_read, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "{0} --device {1} --pipeline-steps {2} --data-provider random-data "
        "--uniform-job-size {3} --job-output-buffer-size {3} --data-provider-job-count {4} "
        "--companion-connection-string proto=tcp,user={5},port={6},addr={7},dev={8} {9} "
        "--core-count {10} --core-list {11} --run-limit-seconds {12} |  tail -n 2  > {13}".format(doca_bench_path, host_pcie_addr, program_name, payload_size, batch_size,
                                                                        user, port, bf3_machine, bf3_pcie_addr, "--use-remote-input-buffers" if is_read else "--use-remote-output-buffers",
                                                                        thread_num, host_core_list,run_seconds, output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def bf3_run(ssh, thread_num, batch_size, payload_size, is_read, output_file):
    stdin, stdout, stderr = ssh.exec_command(
        "{0} --device {1} --representor {1} --pipeline-steps {2} --data-provider random-data "
        "--uniform-job-size {3} --job-output-buffer-size {3} --data-provider-job-count {4} "
        "--companion-connection-string proto=tcp,user={5},port={6},addr={7},dev={8} {9} "
        "--core-count {10} --core-list {11} --run-limit-seconds {12} |  tail -n 2  > {13}".format(doca_bench_path, bf3_pcie_addr, program_name, payload_size, batch_size,
                                                                        user, port, host_machine, host_pcie_addr, "--use-remote-input-buffers" if is_read else "--use-remote-output-buffers",
                                                                        thread_num, bf3_core_list,run_seconds, output_file)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


if __name__ == '__main__':
    ssh_host = ssh_connect(host_machine, user)
    ssh_bf3 = ssh_connect(bf3_machine, user)

    create_dir(ssh_host, output_file_format.format(1, 1, 1, 1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1, 1, 1, 1))
    print("create output file success")

    for t_i in thread:
        for b_i in batch:
            for index, p_i in enumerate(payload):
                for is_read_test in is_read_test_loop:
                    # t1 = threading.Thread(target=host_run,
                    #                         args=(ssh_host, t_i, b_i, p_i, is_read_test,
                    #                             output_file_format.format(
                    #                                 host_machine_name,
                    #                                 p_i,
                    #                                 b_i, t_i,
                    #                                 "read" if is_read_test else "write")))
                    # t1.start()
                    # t1.join()

                    # print("finish one side")

                    t2 = threading.Thread(target=bf3_run,
                                            args=(ssh_bf3, t_i, b_i, p_i, is_read_test,
                                                output_file_format.format(
                                                    bf3_machine_name,
                                                    p_i,
                                                    b_i, t_i,
                                                    "read" if is_read_test else "write")))
                    t2.start()
                    t2.join()

                    print("finish: thread {} batch {} payload {} read {}\n".format(t_i, b_i, p_i, is_read_test))
                    time.sleep(2)
    ssh_host.close()
    ssh_bf3.close()


