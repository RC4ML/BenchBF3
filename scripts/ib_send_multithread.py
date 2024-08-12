import paramiko
import threading
import time



user = "cxz"
passwd = "cxz123"

output_file_format = "/home/cxz/result/ib_multi_result/{}_p{}_t{}_{}"

payloads = [32,64, 128]
threads_num = [8,16,32]

client_dev = "mlx5_0"
client_gid = "3"
client_timeout = 20

server_dev = "mlx5_0"
server_gid = "3"
server_ip = "10.0.0.200"
server_timeout = 30
port_start = 17000



def test(ssh, is_server, payload, total_thread, thread_index):
    print(is_server, thread_index)
    name="send2"
    if is_server:
        stdin, stdout, stderr = ssh.exec_command("taskset -c {0} timeout {1} ib_send_bw -d {2} -x {3} --report_gbits  -s {4} -p {5} --run_infinitely > {6}".format(thread_index, server_timeout, server_dev, server_gid, payload, port_start+thread_index, output_file_format.format(name,payload, total_thread, thread_index)))
    else:
        stdin, stdout, stderr = ssh.exec_command("taskset -c {0} timeout {1} ib_send_bw -d {2} -x {3} --report_gbits  -s {4} -p {5} --run_infinitely {6} > {7}".format(thread_index, client_timeout, client_dev, client_gid, payload, port_start+thread_index, server_ip, output_file_format.format(name,payload,total_thread, thread_index)))

    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)


def ssh_connect(ip, user, passwd):
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(ip, 22, user, passwd)
    return ssh

def create_dir(ssh, filename):
    stdin, stdout, stderr = ssh.exec_command(
        "mkdir -p $(dirname {0})".format(filename)
    )
    str1 = stdout.read().decode('utf-8')
    str2 = stderr.read().decode('utf-8')
    print(str1)
    print(str2)

if __name__ == '__main__':
    ssh_clients = []
    ssh_servers = []
    for i in range(32):
        ssh_clients.append(ssh_connect("172.25.2.23", user, passwd))
        ssh_servers.append(ssh_connect("172.25.2.24", user, passwd))

    create_dir(ssh_clients[0], output_file_format.format("send", 1, 1, 1))
    create_dir(ssh_servers[0], output_file_format.format("send", 1, 1, 1))

    for payload in payloads:
        for thread_num in threads_num:
            server_threads_list = []
            client_threads_list = []
            
            for thread_id in range(thread_num):
                server_threads_list.append(threading.Thread(target=test, args=(ssh_servers[thread_id], True, payload, thread_num, thread_id)))
                client_threads_list.append(threading.Thread(target=test, args=(ssh_clients[thread_id], False, payload, thread_num, thread_id)))
            time.sleep(2)
            for thread_id in range(thread_num):
                server_threads_list[thread_id].start()
            
            time.sleep(2)

            for thread_id in range(thread_num):
                client_threads_list[thread_id].start()
            
            for thread_id in range(thread_num):
                server_threads_list[thread_id].join()
                client_threads_list[thread_id].join()

            print("finish {} {}", payload, thread_num)

    for i in range(32):
        ssh_clients[i].close()
        ssh_servers[i].close()



