.section .text

.global __sig_return
.type __sig_return, @function

__sig_return:
    mov $0x25, %eax
    int $0x80
    ud2
