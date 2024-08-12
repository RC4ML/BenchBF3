import paramiko
import threading
import time



user = "cxz"
passwd = "cxz123"

output_file_format = "/home/cxz/result/ib_result/{}_p{}_c{}"

payloads = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072]
types = ["ib_write_bw", "ib_read_bw", "ib_write_lat", "ib_read_lat"]

def test(ssh, server, ip_addr, ttype, payload, iters, dev, gid):
    name=""
    if ip_addr == "10.0.0.101":
        name = "host"
    else:
        name = "bf3"
    if "bw" in ttype:
        if server:
            stdin, stdout, stderr = ssh.exec_command("timeout 40 {0} -d {1} -x {2} -s {3} --qp 8 --run_infinitely --report_gbits > {4}".format(ttype, dev, gid, payload, output_file_format.format(ttype, payload,name)))
        else:
            stdin, stdout, stderr = ssh.exec_command("timeout 40 {0} -d {1} -x {2} -s {3} --qp 8 --run_infinitely --report_gbits {4} > {5}".format(ttype, dev, gid, payload, ip_addr, output_file_format.format(ttype, payload,name)))
    else:
        if server:
            stdin, stdout, stderr = ssh.exec_command("{0} -d {1} -x {2} -s {3} -n {4} >> {5}".format(ttype, dev, gid, payload, iters, output_file_format.format(ttype, payload,name)))
        else:
            stdin, stdout, stderr = ssh.exec_command("{0} -d {1} -x {2} -s {3} -n {4} {5} >> {6}".format(ttype, dev, gid, payload, iters, ip_addr, output_file_format.format(ttype, payload,name)))


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
    ssh_host = ssh_connect("172.25.2.23", user, passwd)
    ssh_bf3 = ssh_connect("172.25.2.13", user, passwd)

    create_dir(ssh_host, output_file_format.format(1, 1, 1))
    create_dir(ssh_bf3, output_file_format.format(1, 1, 1))

    for ttype in types:
        for payload in payloads:
            if payload <= 262144:
                iters = 10000000
            else:
                iters = 1000

            print(ttype + " " + str(payload))

            t1 = threading.Thread(target=test, args=(ssh_host, True, "10.0.0.101",ttype, payload, iters, "mlx5_0", "3"))
            t2 = threading.Thread(target=test, args=(ssh_bf3, False, "10.0.0.100", ttype, payload, iters, "mlx5_2", "1"))

            t1.start()
            time.sleep(2)
            t2.start()

            # time.sleep(10)

            t2.join()
            t1.join()

            print("finish on one side")

            t3 = threading.Thread(target=test, args=(ssh_host, False, "10.0.0.101",ttype, payload, iters, "mlx5_0", "3"))
            t4 = threading.Thread(target=test, args=(ssh_bf3, True, "10.0.0.100", ttype, payload, iters,  "mlx5_2", "1"))
            
            t4.start()
            time.sleep(2)
            t3.start()

            t3.join()
            t4.join()

            print("finish on both sides")

    ssh_host.close()
    ssh_bf3.close()


