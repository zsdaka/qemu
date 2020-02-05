#!/usr/bin/env python
#
# VM testing base class
#
# Copyright 2017-2019 Red Hat Inc.
#
# Authors:
#  Fam Zheng <famz@redhat.com>
#  Gerd Hoffmann <kraxel@redhat.com>
#
# This code is licensed under the GPL version 2 or later.  See
# the COPYING file in the top-level directory.
#

from __future__ import print_function
import os
import re
import sys
import socket
import logging
import time
import datetime
sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'python'))
from qemu.accel import kvm_available
from qemu.machine import QEMUMachine
import subprocess
import hashlib
import optparse
import atexit
import tempfile
import shutil
import multiprocessing
import traceback
from socket_thread import SocketThread
import yaml

SSH_KEY_FILE = os.path.join(os.path.dirname(__file__),
               "..", "keys", "id_rsa")
SSH_PUB_KEY_FILE = os.path.join(os.path.dirname(__file__),
                   "..", "keys", "id_rsa.pub")

class ConsoleTimeoutException(Exception):
    """Raise this exception when read from console times out."""
    pass

# This is the standard configuration.
# Any or all of these can be overridden by
# passing in a config argument to the VM constructor.
DEFAULT_CONFIG = {
    'cpu'             : "max",
    'machine'         : 'pc',
    'guest_user'      : "qemu",
    'guest_pass'      : "qemupass",
    'root_pass'       : "qemupass",
    'ssh_key_file'    : SSH_KEY_FILE,
    'ssh_pub_key_file': SSH_PUB_KEY_FILE,
    'memory'          : "4G",
    'extra_args'      : [],
    'qemu_args'       : "",
    'dns'             : "",
    'ssh_port'        : 0,
    'install_cmds'    : "",
    'boot_dev_type'   : "block",
    'ssh_timeout'     : 1,
}
BOOT_DEVICE = {
    'block' :  "-drive file={},if=none,id=drive0,cache=writeback "\
               "-device virtio-blk,drive=drive0,bootindex=0",
    'scsi'  :  "-device virtio-scsi-device,id=scsi "\
               "-drive file={},format=raw,if=none,id=hd0 "\
               "-device scsi-hd,drive=hd0,bootindex=0",
}
class BaseVM(object):

    envvars = [
        "https_proxy",
        "http_proxy",
        "ftp_proxy",
        "no_proxy",
    ]

    # The script to run in the guest that builds QEMU
    BUILD_SCRIPT = ""
    # The guest name, to be overridden by subclasses
    name = "#base"
    # The guest architecture, to be overridden by subclasses
    arch = "#arch"
    # command to halt the guest, can be overridden by subclasses
    poweroff = "poweroff"
    # enable IPv6 networking
    ipv6 = True
    # This is the timeout on the wait for console bytes.
    socket_timeout = 120
    # Scale up some timeouts under TCG.
    # 4 is arbitrary, but greater than 2,
    # since we found we need to wait more than twice as long.
    tcg_ssh_timeout_multiplier = 4
    console_logfile = "console.log"
    def __init__(self, debug=False, vcpus=None, config=None,
                boot_console=None, build_path=None):
        self._guest = None
        self._console_fd = None
        self._build_path = build_path
        self._boot_console = boot_console
        self._socket_thread = None
        self._console_timeout_sec = self.socket_timeout
        # Allow input config to override defaults.
        self._config = DEFAULT_CONFIG.copy()
        if config != None:
            self._config.update(config)
        self.validate_ssh_keys()
        self._tmpdir = os.path.realpath(tempfile.mkdtemp(prefix="vm-test-",
                                                         suffix=".tmp",
                                                         dir="."))
        atexit.register(shutil.rmtree, self._tmpdir)
        # Copy the key files to a temporary directory.
        # Also chmod the key file to agree with ssh requirements.
        self._config['ssh_key'] = \
            open(self._config['ssh_key_file']).read().rstrip()
        self._config['ssh_pub_key'] = \
            open(self._config['ssh_pub_key_file']).read().rstrip()
        self._ssh_tmp_key_file = os.path.join(self._tmpdir, "id_rsa")
        open(self._ssh_tmp_key_file, "w").write(self._config['ssh_key'])
        subprocess.check_call(["chmod", "600", self._ssh_tmp_key_file])

        self._ssh_tmp_pub_key_file = os.path.join(self._tmpdir, "id_rsa.pub")
        open(self._ssh_tmp_pub_key_file,
             "w").write(self._config['ssh_pub_key'])

        self.debug = debug
        self._stderr = sys.stderr
        self._devnull = open(os.devnull, "w")
        if self.debug:
            self._stdout = sys.stdout
        else:
            self._stdout = self._devnull
        netdev = "user,id=vnet,hostfwd=:127.0.0.1:{}-:22"
        self._args = [ \
            "-nodefaults", "-m", self._config['memory'],
            "-cpu", self._config['cpu'],
            "-netdev",
            netdev.format(self._config['ssh_port']) +
            (",ipv6=no" if not self.ipv6 else "") +
            (",dns=" + self._config['dns'] if self._config['dns'] else ""),
            "-device", "virtio-net-pci,netdev=vnet",
            "-vnc", "127.0.0.1:0,to=20"]
        if vcpus and vcpus > 1:
            self._args += ["-smp", "%d" % vcpus]
        if kvm_available(self.arch):
            self._args += ["-enable-kvm"]
        else:
            logging.info("KVM not available, not using -enable-kvm")
        self._data_args = []

        if self._config['qemu_args'] != None:
            qemu_args = self._config['qemu_args']
            qemu_args = qemu_args.replace('\n',' ').replace('\r','')
            # Remove any empty strings from list.
            self._config['extra_args'] = [x for x in qemu_args.split(' ') if x]

    def validate_ssh_keys(self):
        """Check to see if the ssh key files exist."""
        if 'ssh_key_file' not in self._config or\
           not os.path.exists(self._config['ssh_key_file']):
            raise Exception("ssh key file not found.")
        if 'ssh_pub_key_file' not in self._config or\
           not os.path.exists(self._config['ssh_pub_key_file']):
               raise Exception("ssh pub key file not found.")

    def wait_boot(self, wait_string=None):
        """Wait for the standard string we expect
           on completion of a normal boot.
           The user can also choose to override with an
           alternate string to wait for."""
        if wait_string is None:
            if self.login_prompt is None:
                raise Exception("self.login_prompt not defined")
            wait_string = self.login_prompt
        # Intentionally bump up the default timeout under TCG,
        # since the console wait below takes longer.
        timeout = self.socket_timeout
        if not kvm_available(self.arch):
            timeout *= 8
        self.console_init(timeout=timeout)
        self.console_wait(wait_string)

    def __getattr__(self, name):
        # Support direct access to config by key.
        # for example, access self._config['cpu'] by self.cpu
        if name.lower() in self._config.keys():
            return self._config[name.lower()]
        return object.__getattribute__(self, name)

    def _download_with_cache(self, url, sha256sum=None, sha512sum=None):
        def check_sha256sum(fname):
            if not sha256sum:
                return True
            checksum = subprocess.check_output(["sha256sum", fname]).split()[0]
            return sha256sum == checksum.decode("utf-8")

        def check_sha512sum(fname):
            if not sha512sum:
                return True
            checksum = subprocess.check_output(["sha512sum", fname]).split()[0]
            return sha512sum == checksum.decode("utf-8")

        cache_dir = os.path.expanduser("~/.cache/qemu-vm/download")
        if not os.path.exists(cache_dir):
            os.makedirs(cache_dir)
        fname = os.path.join(cache_dir,
                             hashlib.sha1(url.encode("utf-8")).hexdigest())
        if os.path.exists(fname) and check_sha256sum(fname) and check_sha512sum(fname):
            return fname
        logging.debug("Downloading %s to %s...", url, fname)
        subprocess.check_call(["wget", "-c", url, "-O", fname + ".download"],
                              stdout=self._stdout, stderr=self._stderr)
        os.rename(fname + ".download", fname)
        return fname

    def _ssh_do(self, user, cmd, check):
        ssh_cmd = ["ssh",
                   "-t",
                   "-o", "StrictHostKeyChecking=no",
                   "-o", "UserKnownHostsFile=" + os.devnull,
                   "-o",
                   "ConnectTimeout={}".format(self._config["ssh_timeout"]),
                   "-p", self.ssh_port, "-i", self._ssh_tmp_key_file]
        # If not in debug mode, set ssh to quiet mode to
        # avoid printing the results of commands.
        if not self.debug:
            ssh_cmd.append("-q")
        for var in self.envvars:
            ssh_cmd += ['-o', "SendEnv=%s" % var ]
        assert not isinstance(cmd, str)
        ssh_cmd += ["%s@127.0.0.1" % user] + list(cmd)
        logging.debug("ssh_cmd: %s", " ".join(ssh_cmd))
        r = subprocess.call(ssh_cmd)
        if check and r != 0:
            raise Exception("SSH command failed: %s" % cmd)
        return r

    def ssh(self, *cmd):
        return self._ssh_do(self.GUEST_USER, cmd, False)

    def ssh_root(self, *cmd):
        return self._ssh_do("root", cmd, False)

    def ssh_check(self, *cmd):
        self._ssh_do(self.GUEST_USER, cmd, True)

    def ssh_root_check(self, *cmd):
        self._ssh_do("root", cmd, True)

    def build_image(self, img):
        raise NotImplementedError

    def exec_qemu_img(self, *args):
        cmd = [os.environ.get("QEMU_IMG", "qemu-img")]
        cmd.extend(list(args))
        subprocess.check_call(cmd)

    def add_source_dir(self, src_dir):
        name = "data-" + hashlib.sha1(src_dir.encode("utf-8")).hexdigest()[:5]
        tarfile = os.path.join(self._tmpdir, name + ".tar")
        logging.debug("Creating archive %s for src_dir dir: %s", tarfile, src_dir)
        subprocess.check_call(["./scripts/archive-source.sh", tarfile],
                              cwd=src_dir, stdin=self._devnull,
                              stdout=self._stdout, stderr=self._stderr)
        self._data_args += ["-drive",
                            "file=%s,if=none,id=%s,cache=writeback,format=raw" % \
                                    (tarfile, name),
                            "-device",
                            "virtio-blk,drive=%s,serial=%s,bootindex=1" % (name, name)]

    def init_console(self, socket):
        """Init the thread to dump console to a file.
           Also open the file descriptor we will use to
           read from the console."""
        self._socket_thread = SocketThread(socket, self.console_logfile)
        self._console_fd = open(self.console_logfile, "r")
        self._socket_thread.start()
        print("console logfile is: {}".format(self.console_logfile))

    def boot(self, img, extra_args=[]):
        boot_dev = BOOT_DEVICE[self._config['boot_dev_type']]
        boot_params = boot_dev.format(img)
        args = self._args + boot_params.split(' ')
        args += self._data_args + extra_args + self._config['extra_args']
        args += ["-device", "VGA"]
        logging.debug("QEMU args: %s", " ".join(args))
        qemu_path = get_qemu_path(self.arch, self._build_path)
        guest = QEMUMachine(binary=qemu_path, args=args)
        guest.set_machine(self._config['machine'])
        guest.set_console()
        try:
            guest.launch()
        except:
            logging.error("Failed to launch QEMU, command line:")
            logging.error(" ".join([qemu_bin] + args))
            logging.error("Log:")
            logging.error(guest.get_log())
            logging.error("QEMU version >= 2.10 is required")
            raise
        atexit.register(self.shutdown)
        self._guest = guest
        self.init_console(guest.console_socket)
        usernet_info = guest.qmp("human-monitor-command",
                                 command_line="info usernet")
        self.ssh_port = None
        for l in usernet_info["return"].splitlines():
            fields = l.split()
            if "TCP[HOST_FORWARD]" in fields and "22" in fields:
                self.ssh_port = l.split()[3]
        if not self.ssh_port:
            raise Exception("Cannot find ssh port from 'info usernet':\n%s" % \
                            usernet_info)

    def console_init(self, timeout = None):
        if timeout == None:
            timeout = self.socket_timeout
        self._console_timeout_sec = timeout

    def console_log(self, text):
        for line in re.split("[\r\n]", text):
            # filter out terminal escape sequences
            line = re.sub("\x1b\[[0-9;?]*[a-zA-Z]", "", line)
            line = re.sub("\x1b\([0-9;?]*[a-zA-Z]", "", line)
            # replace unprintable chars
            line = re.sub("\x1b", "<esc>", line)
            line = re.sub("[\x00-\x1f]", ".", line)
            line = re.sub("[\x80-\xff]", ".", line)
            if line == "":
                continue
            # log console line
            sys.stderr.write("con recv: %s\n" % line)

    def console_recv(self, n):
        """Read n chars from the console_logfile being dumped to
           by the socket thread we created earlier."""
        start_time = time.time()
        while True:
            data = self._console_fd.read(1)
            if data != "":
                break
            time.sleep(0.1)
            elapsed_sec = time.time() - start_time
            if elapsed_sec > self._console_timeout_sec:
                raise ConsoleTimeoutException
        return data.encode('latin1')

    def console_wait(self, expect, expectalt = None):
        vm = self._guest
        output = ""
        while True:
            try:
                chars = self.console_recv(1)
            except ConsoleTimeoutException:
                sys.stderr.write("console: *** read timeout ***\n")
                sys.stderr.write("console: waiting for: '%s'\n" % expect)
                if not expectalt is None:
                    sys.stderr.write("console: waiting for: '%s' (alt)\n" % expectalt)
                sys.stderr.write("console: line buffer:\n")
                sys.stderr.write("\n")
                self.console_log(output.rstrip())
                sys.stderr.write("\n")
                raise
            output += chars.decode("latin1")
            if expect in output:
                break
            if not expectalt is None and expectalt in output:
                break
            if "\r" in output or "\n" in output:
                lines = re.split("[\r\n]", output)
                output = lines.pop()
                if self.debug:
                    self.console_log("\n".join(lines))
        if self.debug:
            self.console_log(output)
        if not expectalt is None and expectalt in output:
            return False
        return True

    def console_consume(self):
        vm = self._guest
        output = ""
        vm.console_socket.setblocking(0)
        while True:
            try:
                chars = vm.console_socket.recv(1)
            except:
                break
            output += chars.decode("latin1")
            if "\r" in output or "\n" in output:
                lines = re.split("[\r\n]", output)
                output = lines.pop()
                if self.debug:
                    self.console_log("\n".join(lines))
        if self.debug:
            self.console_log(output)
        vm.console_socket.setblocking(1)

    def console_send(self, command):
        vm = self._guest
        if self.debug:
            logline = re.sub("\n", "<enter>", command)
            logline = re.sub("[\x00-\x1f]", ".", logline)
            sys.stderr.write("con send: %s\n" % logline)
        for char in list(command):
            vm.console_socket.send(char.encode("utf-8"))
            time.sleep(0.01)

    def console_wait_send(self, wait, command):
        self.console_wait(wait)
        self.console_send(command)

    def console_ssh_init(self, prompt, user, pw):
        sshkey_cmd = "echo '%s' > .ssh/authorized_keys\n" \
                     % self._config['ssh_pub_key'].rstrip()
        self.console_wait_send("login:",    "%s\n" % user)
        self.console_wait_send("Password:", "%s\n" % pw)
        self.console_wait_send(prompt,      "mkdir .ssh\n")
        self.console_wait_send(prompt,      sshkey_cmd)
        self.console_wait_send(prompt,      "chmod 755 .ssh\n")
        self.console_wait_send(prompt,      "chmod 644 .ssh/authorized_keys\n")

    def console_sshd_config(self, prompt):
        self.console_wait(prompt)
        self.console_send("echo 'PermitRootLogin yes' >> /etc/ssh/sshd_config\n")
        for var in self.envvars:
            self.console_wait(prompt)
            self.console_send("echo 'AcceptEnv %s' >> /etc/ssh/sshd_config\n" % var)

    def print_step(self, text):
        sys.stderr.write("### %s ...\n" % text)

    def wait_ssh(self, wait_root=False, seconds=300, cmd="exit 0"):
        # Allow more time for VM to boot under TCG.
        if not kvm_available(self.arch):
            seconds *= self.tcg_ssh_timeout_multiplier
        starttime = datetime.datetime.now()
        endtime = starttime + datetime.timedelta(seconds=seconds)
        cmd_success = False
        while datetime.datetime.now() < endtime:
            if wait_root and self.ssh_root(cmd) == 0:
                cmd_success = True
                break
            elif self.ssh(cmd) == 0:
                cmd_success = True
                break
            seconds = (endtime - datetime.datetime.now()).total_seconds()
            logging.debug("%ds before timeout", seconds)
            time.sleep(1)
        if not cmd_success:
            raise Exception("Timeout while waiting for guest ssh")

    def shutdown(self):
        self._socket_thread.join()
        self._console_fd.close()
        self._guest.shutdown()
    def wait(self):
        self._guest.wait()

    def graceful_shutdown(self):
        self.ssh_root(self.poweroff)
        self._guest.wait()

    def qmp(self, *args, **kwargs):
        return self._guest.qmp(*args, **kwargs)

    def gen_cloud_init_iso(self):
        cidir = self._tmpdir
        mdata = open(os.path.join(cidir, "meta-data"), "w")
        name = self.name.replace(".","-")
        mdata.writelines(["instance-id: {}-vm-0\n".format(name),
                          "local-hostname: {}-guest\n".format(name)])
        mdata.close()
        udata = open(os.path.join(cidir, "user-data"), "w")
        print("guest user:pw {}:{}".format(self._config['guest_user'],
                                           self._config['guest_pass']))
        udata.writelines(["#cloud-config\n",
                          "chpasswd:\n",
                          "  list: |\n",
                          "    root:%s\n" % self._config['root_pass'],
                          "    %s:%s\n" % (self._config['guest_user'],
                                           self._config['guest_pass']),
                          "  expire: False\n",
                          "users:\n",
                          "  - name: %s\n" % self._config['guest_user'],
                          "    sudo: ALL=(ALL) NOPASSWD:ALL\n",
                          "    ssh-authorized-keys:\n",
                          "    - %s\n" % self._config['ssh_pub_key'],
                          "  - name: root\n",
                          "    ssh-authorized-keys:\n",
                          "    - %s\n" % self._config['ssh_pub_key'],
                          "locale: en_US.UTF-8\n"])
        proxy = os.environ.get("http_proxy")
        if not proxy is None:
            udata.writelines(["apt:\n",
                              "  proxy: %s" % proxy])
        udata.close()
        subprocess.check_call(["genisoimage", "-output", "cloud-init.iso",
                               "-volid", "cidata", "-joliet", "-rock",
                               "user-data", "meta-data"],
                               cwd=cidir,
                               stdin=self._devnull, stdout=self._stdout,
                               stderr=self._stdout)
        return os.path.join(cidir, "cloud-init.iso")

def get_qemu_path(arch, build_path=None):
    """Fetch the path to the qemu binary."""
    qemu_local = os.environ.get("QEMU_LOCAL", 0)
    # If QEMU environment variable set, it takes precedence
    if "QEMU" in os.environ:
        qemu_path = os.environ["QEMU"]
    elif qemu_local:
        if not build_path:
            raise Exception("--build-path option required with QEMU_LOCAL")
        qemu_path = os.path.join(build_path, arch + "-softmmu")
        qemu_path = os.path.join(qemu_path, "qemu-system-" + arch)
    else:
        # Default is to use system path for qemu.
        qemu_path = "qemu-system-" + arch
    return qemu_path

def get_qemu_version(qemu_path):
    """Get the version number from the current QEMU,
       and return the major number."""
    output = subprocess.check_output([qemu_path, '--version'])
    version_line = output.decode("latin1")
    version_num = re.split(' |\(', version_line)[3].split('.')[0]
    return int(version_num)

def parse_config(config, args):
    """ Parse yaml config and populate our config structure.
        The yaml config allows the user to override the
        defaults for VM parameters.  In many cases these
        defaults can be overridden without rebuilding the VM."""
    if args.config:
        config_file = args.config
    elif 'QEMU_CONFIG' in os.environ:
        config_file = os.environ['QEMU_CONFIG']
    else:
        return config
    if not os.path.exists(config_file):
        raise Exception("config file {} does not exist".format(config_file))
    with open(config_file) as f:
        yaml_dict = yaml.safe_load(f)

    if 'qemu-conf' in yaml_dict:
        config.update(yaml_dict['qemu-conf'])
    else:
        raise Exception("config file {} is not valid"\
                        " missing qemu-conf".format(config_file))
    return config

def parse_args(vmcls):

    def get_default_jobs():
        if kvm_available(vmcls.arch):
            return multiprocessing.cpu_count() // 2
        else:
            return 1

    parser = optparse.OptionParser(
        description="VM test utility.  Exit codes: "
                    "0 = success, "
                    "1 = command line error, "
                    "2 = environment initialization failed, "
                    "3 = test command failed")
    parser.add_option("--debug", "-D", action="store_true",
                      help="enable debug output")
    parser.add_option("--image", "-i", default="%s.img" % vmcls.name,
                      help="image file name")
    parser.add_option("--force", "-f", action="store_true",
                      help="force build image even if image exists")
    parser.add_option("--jobs", type=int, default=get_default_jobs(),
                      help="number of virtual CPUs")
    parser.add_option("--verbose", "-V", action="store_true",
                      help="Pass V=1 to builds within the guest")
    parser.add_option("--build-image", "-b", action="store_true",
                      help="build image")
    parser.add_option("--build-qemu",
                      help="build QEMU from source in guest")
    parser.add_option("--build-target",
                      help="QEMU build target", default="check")
    parser.add_option("--interactive", "-I", action="store_true",
                      help="Interactively run command")
    parser.add_option("--snapshot", "-s", action="store_true",
                      help="run tests with a snapshot")
    parser.add_option("--config", "-c", default=None,
                      help="Provide config yaml for configuration. "\
                           "See config_example.yaml for example.")
    parser.add_option("--boot-console", action="store_true",
                      help="Show console during boot. ")
    parser.add_option("--build-path", default=None,
                      help="Path of build directory. ")
    parser.disable_interspersed_args()
    return parser.parse_args()

def main(vmcls, config=None):
    try:
        if config == None:
            config = {}
        args, argv = parse_args(vmcls)
        if not argv and not args.build_qemu and not args.build_image:
            print("Nothing to do?")
            return 1
        config = parse_config(config, args)
        logging.basicConfig(level=(logging.DEBUG if args.debug
                                   else logging.WARN))
        vm = vmcls(debug=args.debug, vcpus=args.jobs, config=config,
                   boot_console=args.boot_console,
                   build_path=args.build_path)
        if args.build_image:
            if os.path.exists(args.image) and not args.force:
                sys.stderr.writelines(["Image file exists: %s\n" % args.image,
                                      "Use --force option to overwrite\n"])
                return 1
            return vm.build_image(args.image)
        if args.build_qemu:
            vm.add_source_dir(args.build_qemu)
            cmd = [vm.BUILD_SCRIPT.format(
                   configure_opts = " ".join(argv),
                   jobs=int(args.jobs),
                   target=args.build_target,
                   verbose = "V=1" if args.verbose else "")]
        else:
            cmd = argv
        img = args.image
        if args.snapshot:
            img += ",snapshot=on"
        vm.boot(img)
        if vm._boot_console:
            vm.wait_boot()
        vm.wait_ssh()
    except Exception as e:
        if isinstance(e, SystemExit) and e.code == 0:
            return 0
        sys.stderr.write("Failed to prepare guest environment\n")
        traceback.print_exc()
        return 2

    exitcode = 0
    if vm.ssh(*cmd) != 0:
        exitcode = 3
    if args.interactive:
        vm.ssh()

    if not args.snapshot:
        vm.graceful_shutdown()

    return exitcode
