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
It is designed to work under QEMU, but it can also be used on real computers ! (It properly boots, though driver errors
are likely to occur because i did not add support for the specific hardware of the machine used, so do not try it).<br>
BrebOS is written in C++ and ASM_x86.

### Creation process

I realized the setup of the kernel thanks to https://littleosbook.github.io/. Then, I used the developper's best friend,
Google, and more
particularly the os developper's best friend, https://wiki.osdev.org/. Those two friends combined with passion and
determination ended up creating *BrebOS* :D!

## Installation 🔨

### Prerequisites

Make sure you have installed the following packages, using `sudo apt install package_name`:

- **make** | build system
- **nasm** | ASM compiler
- **gcc** | C compiler
- **g++** | C++ compiler
- **mtools** | DOS disks utilities
- **xorriso** | ISO manipulator
- **qemu-system-i386** | Emulator
- **dnsmasq** | DHCP server

You also need a **Cross compiler**: Please refer to [this page](https://wiki.osdev.org/GCC_Cross-Compiler), and follow
the instructionns on sections 2 and 3. <br>
The build takes several minutes, don't forget to enable parallelization: whenever you execute the `make` command, add
`-j $(lscpu | grep 'Core(s) per socket' | grep -o '[0-9]+')` at the end of the command.



### Build

This step assumes you fulfilled all the requirements aforementioned.

Add your cross compiler yto your path and add some links (mdify according to where you installed your cross compiler):
```sh
export PATH="$HOME/opt/cross/bin:$PATH"
ln i686-elf-ar i686-brebos-ar
ln i686-elf-as i686-brebos-as
ln i686-elf-gcc i686-brebos-gcc
ln i686-elf-gcc i686-brebos-cc
ln i686-elf-ranlib i686-brebos-ranlib
```

You need to install newlib, the libc used by *BrebOS*. One has to be very cautious when doing that, so I proivded a script which does everything for you. Run
```sh
./utils/newlib_setup.sh
```
ℹ️ This does not install anything globally on your machine, everything is downloaded and built in the repo. ℹ️

Eventually, run:

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
project (42sh, made in groups of of 4 students), that I adapted to make it work in BrebOS. <br>
The shell language is a subset of the linux one. You should be able to use every basic shell syntax that exists in
linux, provided that it does not require heavy kernel support that i did not add yet (redirections for example)<br>
Here is the list of the main commands (you can view the entire list by browsing at `BrebOS/src/programs`):

| Command                      | Description                                                                                    |
|------------------------------|------------------------------------------------------------------------------------------------|
| `q`                          | Quit (Shutdown).                                                                               |
| `ls <path>`                  | Lists files and directories at `path`.                                                         |
| `mkdir <path/dir_name>`      | Creates the directory `dir_name` at `path`.                                                    |
| `touch <dir_path/file_name>` | Creates the file `file_name` at `dir_path`.                                                    |
| `wget [OPTIONS] <endpoint>`                 | Downloads stuff with `HTTP GET` |
| `cat <path>`                 | Prints the content of the file at `path`. Output is formatted according to the file extension. |
| `cls`                | Clears the screen.                                                                              |
| `feh <path>`                | Displays an image on the screen, before clearing it.                                                                              |

As 42sh follows the linux shell syntax, you can start a program symply by typing `<program_path> [argv0] [argv1] ...`.
The kernel `$PATH` is filled with `/bin`, where all programs are. Hence, to start `/bin/a_program`, simply write
`a_program`.

### Running your own programs inside BrebOS (How cool is this !? ◝(ᵔᗜᵔ)◜)

BrebOS can run C++ programs that *you* write, provided they respect the following conditions:

- Your program cannot use `dynamic_cast`, nor exceptions. They require additional support which i did not setup.
- Most of the standard libc is  available. If something is not available, you will have a compile error, indicating an undefined reference or undeclared function. You can also include `<ksyscalls.h>` in your programs, which you can find at `src/libk`. This is a small OS specific library which provides handy syscalls.
- The STL is not available.
- Any other C++ feature (supposedly) works!

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
    - E1000 ethernet card driver (mainly taken from [here](https://wiki.osdev.org/Intel_Ethernet_i217))
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

### Processes </>

- **Userland 🙍🏻‍♂️** (processes run in ring 3, kernel in ring 0)
- **Preemptive scheduling ✋**
- **Syscalls 📞**
- **ELF support </>**
    - ELF loading and execution (ELf processing, address space setup, global/local static variables handling)
    - Dynamic loader (relocations)
    - Dynamic libraries support
    - Lazy binding support (addresses of functions within dynamic libraries are resolved at runtime by `dynlk`, the OS'
      dynamic linker)
    - OS specific library, whici programs are dynamically linked to.
- **42sh (Shell) 👨🏻‍💻**
    - command lists/compound lists
    - if/else
    - single quotes
    - comments
    - while/until/for
    - and or (&& ||)
    - double quotes and escape character
    - variables
    - builtins
        - true/false
        - echo
        - continue
        - break
    - the following features are implemented within 42sh but BrebOS lacks support for them to work:
        - redirections
        - pipeline
        - subshells and command substitution

### Misc ✚

- PS2 Keyboard driver ⌨

### Roadmap 🛣️

Those are things I'm willing to do. To not mistake it with things *I will* do.

- **Multithreading**
- **Multi core**
- **Bootloader**
- **C++ RTTI and exceptions support**

- **Drivers** <br>
  Add various real-life hardware drivers to be able to fully use BrebOS on real machines.
  For now, the kernel only works with the specific hardware emulated by QEMU.


