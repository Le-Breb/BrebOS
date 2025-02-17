#include "ports.h"

/* void Ports::outportb (uint16_t p_port,uint8_t p_data)
 *
 * This method outputs a byte to a hardware port.
 * It uses an inline asm with the volatile keyword
 * to disable compiler optimization.
 *
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 *
 * Notice the input constraint
 *      "dN" (port) : indicates using the DX register to store the
 *                  value of port in it
 *      "a"  (data) : store the value of data into
 *
 * The above constraint will instruct the compiler to generate assembly
 * code that looks like that
 *      mov    %edi,%edx
 *      mov    %esi,%eax
 *      out    %eax,(%dx)
 *
 * According the ABI, the edi will have the value of p_port and esi will have
 * the value of the p_data
 *
 */
void Ports::outportb (uint16_t p_port,uint8_t p_data)
{
    asm volatile ("outb %1, %0" : : "dN" (p_port), "a" (p_data));
}

/* void Ports::outportw (uint16_t p_port,uint16_t p_data)
 *
 * This method outputs a word to a hardware port.
 *
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 *
 */


void Ports::outportw (uint16_t p_port,uint16_t p_data)
{
    asm volatile ("outw %1, %0" : : "dN" (p_port), "a" (p_data));
}

/* void Ports::outportl (uint16_t p_port,uint32_t p_data)
 *
 * This method outputs a double word to a hardware port.
 *
 *  p_port: the port number to output the byte p_data to.
 *  p_data: the byte to to output to the port p_port.
 *
 */


void Ports::outportl (uint16_t p_port,uint32_t p_data)
{
    asm volatile ("outl %1, %0" : : "dN" (p_port), "a" (p_data));
}

/* uint8_t Ports::inportb( uint16_t p_port)
 *
 * This method reads a byte from a hardware port.
 *
 *  p_port: the port number to read the byte from.
 *  return value : a byte read from the port p_port.
 *
 * Notice the output constraint "=a", this tells the compiler
 * to expect the save the value of register AX into the variable l_ret
 * The register AX should contain the result of the inb instruction.
 *
 *
 */

uint8_t Ports::inportb( uint16_t p_port)
{
    uint8_t l_ret;
    asm volatile("inb %1, %0" : "=a" (l_ret) : "dN" (p_port));
    return l_ret;
}

/* uint16_t Ports::inportw( uint16_t p_port)
 *
 * This method reads a word from a hardware port.
 *
 *  p_port: the port number to read the word from.
 *  return value : a word read from the port p_port.
 *
 */


uint16_t Ports::inportw( uint16_t p_port)
{
    uint16_t l_ret;
    asm volatile ("inw %1, %0" : "=a" (l_ret) : "dN" (p_port));
    return l_ret;
}


/* uint16_t Ports::inportl( uint16_t p_port)
 *
 * This method reads a double word from a hardware port.
 *
 *  p_port: the port number to read the double word from.
 *  return value : a double word read from the port p_port.
 *
 */

uint32_t Ports::inportl( uint16_t p_port)
{
    uint32_t l_ret;
    asm volatile ("inl %1, %0" : "=a" (l_ret) : "dN" (p_port));
    return l_ret;
}