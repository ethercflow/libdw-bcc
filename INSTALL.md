# Installing libdw-bpf

* [Kernel Configuration](#kernel-configuration)
* [Source](#source)
  - [Centos](#centos---source)

## Kernel Configuration

In general, to use these features, a Linux kernel version 4.1 or newer is
required. In addition, the kernel should have been compiled with the following
flags set:

``` sh
CONFIG_BPF=y
CONFIG_BPF_SYSCALL=y
# [optional, for tc filters]
CONFIG_NET_CLS_BPF=m
# [optional, for tc actions]
CONFIG_NET_ACT_BPF=m
CONFIG_BPF_JIT=y
# [for Linux kernel versions 4.1 through 4.6]
CONFIG_HAVE_BPF_JIT=y
# [for Linux kernel versions 4.7 and later]
CONFIG_HAVE_EBPF_JIT=y
# [optional, for kprobes]
CONFIG_BPF_EVENTS=y
```

# Source

## Centos - Source

For Centos 7.6 only

### Compile Centos kernel with [bpf.patch](patches/bpf.patch)
libwd-bpf needs to read user process's stack address space in eBPF virtual
machine, but the BPF_CALLs `bpf_probe_read` and `bpf_probe_read_str` can't meet
the demand, so I implement a new BPF_CALL `bpf_probe_read_stack` to make it
work. To add a new BPF_CALL will change the kernel's data structure, so it
cann't be made to a livepatch module with
[kpatch](https://github.com/dynup/kpatch)  (Maybe use shadow variables can solve
this, I've not tried yet).
To custom a Centos kernel please refer the
[Custom_Kernel](https://wiki.centos.org/HowTos/Custom_Kernel).


### Install Build Dependencies

``` sh
sudo yum install cmake -y
sudo yum install libunwind-devel libunwind -y
```

The examples use `libbcc` to get symbol name and highlevel eBPF program
interface. Because ``libbcc`maintain the `libbpf`, we need to patch them with
[bcc.patch](patches/bcc.patch) and [libbpf.patch](patches/libbpf.patch). After
patch please refer the [BCC INSTALL
MANUAL](https://github.com/iovisor/bcc/blob/master/INSTALL.md#centos---source)
to compile and install it.

