#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

/** outb:
* Sends the given data to the given I/O port. Defined in io.s
*
* @param port The I/O port to send the data to
* @param data The data to send to the I/O port
25
*/
void outb(unsigned short port, unsigned char data);

/** inb:
* Read a byte from an I/O port.
*
* @param port The address of the I/O port
* @return
The read byte
*/
unsigned char inb(unsigned short port);

void io_wait(void);

#endif /* INCLUDE_IO_H */
