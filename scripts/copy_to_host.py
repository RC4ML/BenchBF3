import paramiko

src_ip = "127.0.0.1"
dst_ips = ["hostbf3"]

base = "/app/stupid-doca/bin"
dst_base = "/home/cxz/study/stupid-doca/bin"
user = "cxz"
passwd = "cxz123"


def copy_file(ssh, dst):
    stdin, stdout, stderr = ssh.exec_command(
        "sshpass -p {0} scp -prq -o StrictHostKeyChecking=no {1} {2}@{3}:{4}".format(passwd, base, user, dst, dst_base)
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
    ssh_client = ssh_connect(src_ip, "root", "cxz123")
    for ip in dst_ips:
        copy_file(ssh_client, ip)
        print("copy to {} done".format(ip))
    ssh_client.close()
