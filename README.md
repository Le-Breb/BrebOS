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

Make sure you have the following packages installed on you machine:
- **make** | Build system
- **nasm** | ASM compiler
- **gcc** | C compiler
- **g++** | C++ compiler
- **mtools** | DOS disks utilities
- **xorriso** | ISO manipulator
- **qemu-system-i386** | Emulator
- **dnsmasq** | DHCP server
- **libgmp3-dev** | GCC build dependance
- **libmpc-dev** | GCC build dependance
- **libmpfr-dev** | GCC build dependance

The package names are the ones that I use on Ubuntu with `apt`. I guess most package names are similar on different package managers as those are very common packages, but it's up to you to find the correct package names to be used with your package manager. For GCC build dependances, you can find more information [here](https://wiki.osdev.org/GCC_Cross-Compiler#Installing_Dependencies)

### Build

Run (inside the repo)
```sh
./utils/gcc_setup.sh && ./utils/newlib_setup.sh && ./utils/brebos_gcc_setup.sh
```

⚠️This takes a long time (~18min on my machine). You only need to run this once.⚠️ <br>
ℹ️ This does not install anything globally on your machine, everything is downloaded and built in the repo. ℹ️<br>

This installs a cross compiler which targets 32bits architecture, then it installs the libc used by userland programs, and finally it configures then rebuild the compiler so that it becomes a hosted compiler, specifically set up for BrebOs (a hosted compiler knows the target OS, what libc to use and other nice stuff).

Now you can build the project itself:
```sh
RELEASE=1 make
```

### Run ▶️

```sh
RELEASE=1 make run
```

ℹ️ This will ask for elevated privileges, which are required for setting up NAT. ℹ️


Note: To date, running `git pull` may break stuff, because newlib is not included in the build system, thus you would update kernel code which could rely on non-updated newlib code. As for now, if u want the latest udpate, please re-clone the repo and rerun everything (i'm sorry, I'll fix that one day).

## What can I do with BrebOS ❓

### Commands

Now that you have the kernel up and running, you can start executing commands. The kernel uses a shell made as a school
project (42sh, made in groups of of 4 students), that ported in BrebOS. <br>
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

As 42sh follows the linux shell syntax, you can start a program symply by typing `<program_path> [argv1] [argv2] ...`.
The kernel `$PATH` is filled with `/bin`, where all programs are. Hence, to start `/bin/a_program`, simply write
`a_program`.

### Running your own programs inside BrebOS (How cool is this !? ◝(ᵔᗜᵔ)◜)

BrebOS can run C++ programs that *you* write, provided they respect the following conditions:

- Your program cannot use `dynamic_cast`, nor exceptions. They require additional support which i did not setup.
- Most of the standard libc is  available. If something is not available, you will have a compile error, indicating an undefined reference or undeclared function. You can also include `<ksyscalls.h>` in your programs, which you can find at `src/libc`. This is a small OS specific library which provides handy syscalls.
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
- **Shared pages 🤝** (Read-Only shared pages and Copy-On-Write)

### Processes </>

- **Userland 🙍🏻‍♂️** (processes run in ring 3, kernel in ring 0)
- **Preemptive scheduling ✋**
- **Syscalls 📞**
    - 'man page'-ish 🖹
      - execve
      - fork
      - malloc/free/realloc
      - open/read/write/close/lseek
      - stat
      - get_pid
      - wait_pid
      - mkdir/touch
    - custom 🎨
      - terminate_process
      - get_key
      - shutdown
      - runtime dynamic linker
      - ls
      - clear_screen
      - wget
      - feh
- **ELF support </>**
    - ELF loading and execution (ELf processing, address space setup, global/local static variables handling)
    - Dynamic loader (relocations)
    - Dynamic libraries support
    - Lazy binding support (addresses of functions within dynamic libraries are resolved at runtime by `dynlk`, the OS'
      dynamic linker)
    - OS specific library, which programs are dynamically linked to
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
    - the following features are implemented within 42sh but BrebOS lacks support for them to work:
        - variables
        - redirections
        - pipeline
        - command substitution

### Misc ✚

- PS2 Keyboard driver ⌨
- Hosted cross compiler
- Newlib integration

### Roadmap 🛣️

Those are things I'm willing to do. To not mistake it with things *I will* do.

- **Multithreading**
- **Multi core**
- **Bootloader**
- **C++ RTTI and exceptions support**

- **Drivers** <br>
  Add various real-life hardware drivers to be able to fully use BrebOS on real machines.
  For now, the kernel only works with the specific hardware emulated by QEMU.


