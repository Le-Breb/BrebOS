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
It is designed to work under QEMU, but it can also be used on real computers ! (It properly boots, though driver errors are likely to occur cause i did not add support for the specific hardware of the machine used).<br>
BrebOS is written in C++ and ASM_x86.

### Creation process
I realized the setup of the kernel thanks to https://littleosbook.github.io/. Then, I used the developper's best friend, Google, and more
particularly the os developper's best friend, https://wiki.osdev.org/. Those two friends combined with passion and determination ended up creating *BrebOS* :D!

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

You also need a **Cross compiler**: Please refer to [this page](https://wiki.osdev.org/GCC_Cross-Compiler), and follow the instructionns on sections 2 and 3. <br>
The build takes several minutes, don't forget to enable parallelization: whenever you execute the `make` command, add `-j $(lscpu | grep 'Core(s) per socket' | grep -o '[0-9]+')` at the end of the command.


### Build

This step assumes you fulfilled all the requirements aforementioned.

You simply need to run:
```sh
export PATH="$HOME/opt/cross/bin:$PATH"
RELEASE=1 make
```

### Run ▶️


#### Using QEMU

```sh
RELEASE = 1 make run
```

#### On a real computer (Read carefully before proceeding)

In `BrebOS/`, run the following command. **⚠️ Replace /dev/sda with the USB stick you want to have BrebOS installed on. ⚠️**
```sh
sudo dd if=os.iso of=/dev/sda bs=4M status=progress && sync
```
⚠️ Use this command carefully. It overwrites whatever is on the USB stick. Make sure to backup all your data before proceeding.
After this command has been executed, the USB stick is no longer able to store files.
It is your responsability to put back a file system if you want to use the USB stick as before afterwards. (This is a very simple task for anyone that knows how to use Google...) ⚠️

To my knowledge, there is absolutely nothing which could harm your computer.
The worse than can happen is a crash, or the computer simply not booting. Anyway, **I am not responsible in any way for any damage potentially caused.**

## What can I do with BrebOS ❓

### Commands

Now that you have the kernel up and running, you can start executing commands. Here is the list of all commands available:

| Command | Description |
|---------|-------------|
| `p <program_path> [argv0] [argv1] ...` | Starts a program with provided argument list, default is empty. The kernel `$PATH` is filled with `/bin`, where all programs are. For example, to start `/bin/a_program`, simply write `p a_program`. |
| `q` | Quit (Shutdown). |
| `ls <path>` | Lists files and directories at `path`. |
| `mkdir <path/dir_name>` | Creates the directory `dir_name` at `path`. |
| `touch <path/file_name>` | Creates the file `dir_name` at `path`. |

### Running your own programs inside BrebOS (How cool is this !?)

BrebOS can run C++ programs that *you* write, provided they respect the following conditions:
- Your program cannot use `dynamic_cast`, nor exceptions. They require additional support which i did not setup.
- Your program contains `.h` and `.cpp` files, all within the same directory.
- You cannot make use of any library, even `libc`, as it is not ported on `BrebOS`. The only exception is my custom `libc`. The relevant headers are located under
`src/libc`. For example, if you want to use `printf`, simply write `#include <kstdio.h>`. You are likely to be interested in `ksyscalls.h` and `kstdio.h`.
- Any other C++ feature (supposedly) works!

To add your program, simply follow the following steps:
- Copy all `.h` and `.cpp` files under `BrebOS/src/programs/your_awesome_program_name` <br>
That's all! Yep, you've read well, there's nothing else you have to do! The Makefile will handle the compilation of your program and will add it at `/bin` in BrebOS' disk. <br>
You can now run your program using `p your_awesome_program_name` in BrebOS.

## Features ⚙️
### Core
- **Segmentation** (GDT setup)
- **Paging** (PDT, page tables)
- **Interrupts** (IDT)
- **Dynamic memory allocation** (malloc, free)
### Processes
- **Preemptive scheduling**
- **Syscalls**
- **ELF support**
  - ELF loading and execution (ELf processing, address space setup, global/local static variables)
  - Dynamic libraries support (relocations)
  - Lazy binding support (addresses of functions within dynamic libraries are resolved at runtime by `dynlk`, the OS' dynamic linker)
  - Custom libc (basic functions such as `strlen` and syscalls wrappers)
- **Virtual File System (VFS)**
  - ATA driver (taken from osdev.com, not implemented by myself)
  - FAT32 support
  - VFS to abstract hardware/file_system specific details
### Misc
- Keyboard driver

### Roadmap 🛣️

Those are things I'm willing to do. To not mistake it with things *I will* do.
- **Multithreading**
- **Multi core**
- **Bootloader**
- **C++ RTTI and exceptions support**
- **Network**<br>
  Complete network stack with every basic internet protocol (Ethernet, TCP, IP...). <br>
  Ability to send a mail.
  
- **Drivers** <br>
  Add various real-life hardware drivers to be able to fully use BrebOS on real machines.
  For now, the kernel only works with the specific hardware emulated by QEMU.


