<h2> <center> BrebOS </center> </h2>

## Table of contents

<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#introduction-🗒️">Introduction</a>
    </li>
    <li>
      <a href="#installation-🔨">Installation</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#build">Build</a></li>
        <li><a href="#run-▶️">Run</a></li>
      </ul>
    </li>
    <li><a href="#what-can-i-do-with-brebos-❓">What can i do with BrebOS ?</a></li>
    <li><a href="#features-⚙️">Features</a></li>
    <li><a href="#roadmap-🛣️">Roadmap</a></li>

  </ol>
</details>

<!-- INTRODUCTION -->

## Introduction 🗒️

### What is it ?

BrebOS is a hand made x86 32 bits OS made for learning and fun!
It is designed to work under QEMU, and hopefully i'll manage to have an USB driver working so that it can work on real
machines!.<br>
BrebOS is written in C++ and ASM_x86.

### Creation process

I realized the setup of the kernel thanks to https://littleosbook.github.io/. Then, I used the developper's best friend,
Google, and more
particularly the os developper's best friend, https://wiki.osdev.org/. Those two friends combined with passion and
determination ended up creating *BrebOS* :D!

## Installation 🔨

### Prerequisites

Make sure you have the following packages installed on you machine:
- **make** | Build system
- **ninja-build** | Build system
- **nasm** | ASM compiler
- **gcc** | C compiler
- **g++** | C++ compiler
- **mtools** | DOS disks utilities
- **xorriso** | ISO manipulator
- **qemu-system-i386** | Emulator
- **dnsmasq** | DHCP server
- **libgmp3-dev** | GCC build dependence
- **libmpc-dev** | GCC build dependence
- **libmpfr-dev** | GCC build dependence

The package names are the ones that I use on Ubuntu with `apt`. I guess most package names are similar on different package managers as those are very common packages, but it's up to you to find the correct package names to be used with your package manager. For GCC build dependances, you can find more information [here](https://wiki.osdev.org/GCC_Cross-Compiler#Installing_Dependencies)

### Build

Run (inside the repo)
```sh
./utils/toolchain_setup.sh
```

⚠️This takes a while (~8min on my machine). You only need to run this once.⚠️ <br>
ℹ️ This does not install anything globally on your machine, everything is downloaded and built in the repo. ℹ️<br>

That script builds installs:
- `mlibc`: a c standard library
- `libstdc++-v3` : a c++ standard library
- `i686-brebos-gcc`: a hosted compiler, aware of Brebos, mlibc and libstdc++ internals
- `binutils` and `autoconf` : required for building gcc and libstdc++-v3  

Now you can build the project itself:
```sh
RELEASE=1 make
```

### Run ▶️

```sh
RELEASE=1 make run
```

ℹ️ This will ask for elevated privileges, which are required for setting up NAT. ℹ️

## What can I do with BrebOS ❓

### Commands

Now that you have the kernel up and running, you can start executing commands. The kernel uses a shell made as a school
project (42sh, made in groups of of 4 students), that ported in BrebOS. <br>
The shell language is a subset of the Linux one. You should be able to use every basic shell syntax that exists in
Linux.<br>
Here is the list of the main commands (you can view the entire list by browsing at `BrebOS/src/programs`):

| Command                      | Description                                                                                    |
|------------------------------|------------------------------------------------------------------------------------------------|
| `q`                          | Quit (Shutdown).                                                                               |
| `ls <path>`                  | Lists files and directories at `path`.                                                         |
| `mkdir <path/dir_name>`      | Creates the directory `dir_name` at `path`.                                                    |
| `touch <dir_path/file_name>` | Creates the file `file_name` at `dir_path`.                                                    |
| `wget [OPTIONS] <endpoint>`                 | Downloads stuff with `HTTP GET` |
| `cat <path>`                 | Prints the content of the file at `path`.  |
| `cls`                | Clears the screen.                                                                              |
| `feh <path>`                | Displays an image on the screen, before clearing it.                                                                              |

As 42sh follows the Linux shell syntax, you can start a program simply by typing `<program_path> [argv1] [argv2] ...`.
The kernel `$PATH` is filled with `/bin`, where all programs are. Hence, to start `/bin/a_program`, simply write
`a_program`.

> ⚠️ 42sh does NOT run in interactive mode (yet ?). This means each command you make is independent of one
> another. Here's an example:
> ```shell
> $ pwd # /bin
> $ cd /a_dir
> $ pwd #/bin
> $ cd /a_dir; pwd # a_dir <- this works as 42sh is invocated only once
> ```

### Running your own programs inside BrebOS (How cool is this !? ◝(ᵔᗜᵔ)◜)

BrebOS can run C++ programs that *you* write, provided they respect the following conditions:

- Most of the standard libc is available. If something is not available, you will have a link error, indicating an undefined reference or undeclared function. You can also include `<ksyscalls.h>` in your programs, which you can find at `src/libc`. This is a small OS specific library which provides handy syscalls.
- The STL (C++ standard library) is available.
- Any C++ feature (normally) works, including RTTI and exceptions!

To add your program, simply follow the following steps:
- Copy your source files under `BrebOS/src/programs/your_awesome_program_name` <br>
  That's all! Yep, you've read well, there's nothing else you have to do! The Makefile will handle the compilation of
  your program and will add it at `/bin` in BrebOS' disk. <br>
  You can now run your program using `your_awesome_program_name` in BrebOS.

### Network

If you have knowledge in networking, you can play with network stuff. VM uses NAT to talk with the outer world. For
host-VM communication, host PC has IP 192.168.100.1, and the VM dynamically gets its IP from dnsmasq, a DHCP server
running locally, which is automatically started when executing `make run`.

## Features ⚙️

### Core ♥️

- **VBE Framebuffer 🖥️**
    - Manual pixel plotting
    - Double buffering
    - Font rendering
- **File System 📁**
    - ATA driver (taken from osdev.com, not implemented by me)
    - FAT32 support
    - VFS which abstracts hardware/file_system specific details and provide a unified interface within the kernel.
- **Network stack 🖧**
    - E1000 Ethernet card driver (mainly taken from [here](https://wiki.osdev.org/Intel_Ethernet_i217))
    - Ethernet
    - IPV4
    - UDP
    - ICMP ping reply
    - ARP
    - DHCP
    - TCP (weak for now)
    - HTTP GET
- **Interrupts 🔔** (IDT)

### Memory 🧠
- **Segmentation ✂️** (GDT setup)
- **Paging 🔀** (PDT, page tables)
- **Dynamic memory allocation 📦** (malloc, free)
- **Lazy memory allocation 🥱** (actually allocate memory only when processes try to access it)
- **Shared pages 🤝** (Read-Only shared pages and Copy-On-Write)

### Processes </>

- **Userland 🙍🏻‍♂️** (processes run in ring 3, kernel in ring 0)
- **Preemptive scheduling ✋**
- **Syscalls 📞**
    - 'man page'-ish 🖹
      - execve/fork
      - malloc/free/realloc/mmap/mprotect
      - open/read/write/close/lseek/fcntl/dup/dup2/pipe
      - stat/fstat
      - getcwd/chdir
      - kill/sigaction/sigprocmask
      - get_pid/wait_pid
      - mkdir/touch
      - tcbset
    - custom 🎨
      - terminate_process
      - get_screen_dimensions
      - get_key
      - shutdown
      - runtime dynamic linker for lazy symbol resolution
      - ls
      - clear_screen
      - wget
      - feh
- **ELF support </>**
    - ELF loading and execution (ELF parsing, address space setup)
    - ABI conforming stack layout
    - mlibc (libc)
    - dynamically linked programs support
    - libstdc++-v3 (c++ standard library)
    - OS specific library (not mandatory) that provides handy syscalls wrappers
- **42sh (Shell) 👨🏻‍💻**
    - command lists/compound lists
    - if/else
    - single quotes
    - comments
    - while/until/for
    - and or (&& ||)
    - double quotes and escape character
    - subshells
    - functions
    - builtins
        - true/false
        - echo
        - continue
        - break
        - pwd
    - redirections
    - pipeline
    - command substitution
    - variables
        
### Bootloader
Hand made bootloader, that:
- enables A20 line
- switches from VGA to VBE for wider screen resolution and manual pixel plotting
- sets up a stack
- sets up a GDT
- loads a stage2 bootloader
- switches to protected mode
- jumps in stage2 bootloader
- stage2 then loads the kernel and jumps in it

### Misc ✚

- PS2 Keyboard driver ⌨
- Hosted cross-compiler

### Roadmap 🛣️

Those are things I'm willing to do. Do not mistake it with things *I will* do.

- **add a driver for a faster drive type**, as the majority of program lifetimes are occupied by IO operations
- **busybox port**
- **robust TCP stack**
- **Multithreading**
- **Multi core**
- **Drivers** <br>
  Add various real-life hardware drivers to be able to fully use BrebOS on real machines.
  For now, the kernel only works with the specific hardware emulated by QEMU. Especially, a USB driver would be
particularly handy.


