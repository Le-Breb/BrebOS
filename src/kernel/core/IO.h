#ifndef INCLUDE_IO_H
#define INCLUDE_IO_H

/** outb:
* Sends the given data to the given I/O port. Defined in io.s
*
* @param port The I/O port to send the data to
* @param data The data to send to the I/O port
25
*/
#include "kstddef.h"

extern "C" void outb(unsigned short port, unsigned char data);

/** inb:
* Read a byte from an I/O port.
*
* @param port The address of the I/O port
* @return
The read byte
*/
extern "C" unsigned char inb(unsigned short port);

extern "C" void io_wait(void);

class IO
{
public:
	/**Reads data from IO port in a buffer
	 *
	 * @param port port to read from
	 * @param buffer buffer to write to
	 * @param quads number of long words (aka quads) to read
	 */
	static void insl(uint port, void* buffer, uint quads);
};

#endif /* INCLUDE_IO_H */
