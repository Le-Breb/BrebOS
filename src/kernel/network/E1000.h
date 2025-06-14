#ifndef E100_H
#define E100_H

#include <stdint.h>

#include "Ethernet.h"
#include "../core/PCI.h"
#include "../core/interrupts.h"
#include "../core/interrupt_handler.h"

// Most of the code comes from https://wiki.osdev.org/Intel_Ethernet_i217

#define INTEL_VEND     0x8086  // Vendor ID for Intel
#define E1000_DEV      0x100E  // Device ID for the e1000 Qemu, Bochs, and VirtualBox emmulated NICs
#define E1000_I217     0x153A  // Device ID for Intel I217
#define E1000_82577LM  0x10EA  // Device ID for Intel 82577LM


// I have gathered those from different Hobby online operating systems instead of getting them one by one from the manual

#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_CTRL_EXT    0x0018
#define REG_IMASK       0x00D0
#define REG_RCTRL       0x0100
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818

#define REG_TCTRL       0x0400
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818


#define REG_RDTR         0x2820 // RX Delay Timer Register
#define REG_RXDCTL       0x2828 // RX Descriptor Control
#define REG_RADV         0x282C // RX Int. Absolute Delay Timer
#define REG_RSRPD        0x2C00 // RX Small Packet Detect Interrupt


#define REG_TIPG         0x0410      // Transmit Inter Packet Gap
#define ECTRL_SLU        0x40        //set link up


#define RCTL_EN                         (1 << 1)    // Receiver Enable
#define RCTL_SBP                        (1 << 2)    // Store Bad Packets
#define RCTL_UPE                        (1 << 3)    // Unicast Promiscuous Enabled
#define RCTL_MPE                        (1 << 4)    // Multicast Promiscuous Enabled
#define RCTL_LPE                        (1 << 5)    // Long Packet Reception Enable
#define RCTL_LBM_NONE                   (0 << 6)    // No Loopback
#define RCTL_LBM_PHY                    (3 << 6)    // PHY or external SerDesc loopback
#define RTCL_RDMTS_HALF                 (0 << 8)    // Free Buffer Threshold is 1/2 of RDLEN
#define RTCL_RDMTS_QUARTER              (1 << 8)    // Free Buffer Threshold is 1/4 of RDLEN
#define RTCL_RDMTS_EIGHTH               (2 << 8)    // Free Buffer Threshold is 1/8 of RDLEN
#define RCTL_MO_36                      (0 << 12)   // Multicast Offset - bits 47:36
#define RCTL_MO_35                      (1 << 12)   // Multicast Offset - bits 46:35
#define RCTL_MO_34                      (2 << 12)   // Multicast Offset - bits 45:34
#define RCTL_MO_32                      (3 << 12)   // Multicast Offset - bits 43:32
#define RCTL_BAM                        (1 << 15)   // Broadcast Accept Mode
#define RCTL_VFE                        (1 << 18)   // VLAN Filter Enable
#define RCTL_CFIEN                      (1 << 19)   // Canonical Form Indicator Enable
#define RCTL_CFI                        (1 << 20)   // Canonical Form Indicator Bit Value
#define RCTL_DPF                        (1 << 22)   // Discard Pause Frames
#define RCTL_PMCF                       (1 << 23)   // Pass MAC Control Frames
#define RCTL_SECRC                      (1 << 26)   // Strip Ethernet CRC

// Buffer Sizes
#define RCTL_BSIZE_256                  (3 << 16)
#define RCTL_BSIZE_512                  (2 << 16)
#define RCTL_BSIZE_1024                 (1 << 16)
#define RCTL_BSIZE_2048                 (0 << 16)
#define RCTL_BSIZE_4096                 ((3 << 16) | (1 << 25))
#define RCTL_BSIZE_8192                 ((2 << 16) | (1 << 25))
#define RCTL_BSIZE_16384                ((1 << 16) | (1 << 25))


// Transmit Command

#define CMD_EOP                         (1 << 0)    // End of Packet
#define CMD_IFCS                        (1 << 1)    // Insert FCS
#define CMD_IC                          (1 << 2)    // Insert Checksum
#define CMD_RS                          (1 << 3)    // Report Status
#define CMD_RPS                         (1 << 4)    // Report Packet Sent
#define CMD_VLE                         (1 << 6)    // VLAN Packet Enable
#define CMD_IDE                         (1 << 7)    // Interrupt Delay Enable


// TCTL Register

#define TCTL_EN                         (1 << 1)    // Transmit Enable
#define TCTL_PSP                        (1 << 3)    // Pad Short Packets
#define TCTL_CT_SHIFT                   4           // Collision Threshold
#define TCTL_COLD_SHIFT                 12          // Collision Distance
#define TCTL_SWXOFF                     (1 << 22)   // Software XOFF Transmission
#define TCTL_RTLC                       (1 << 24)   // Re-transmit on Late Collision

#define TSTA_DD                         (1 << 0)    // Descriptor Done
#define TSTA_EC                         (1 << 1)    // Excess Collisions
#define TSTA_LC                         (1 << 2)    // Late Collision
#define LSTA_TU                         (1 << 3)    // Transmit Underrun

class MMIOUtils
{
public:
    static uint8_t read8(uint32_t p_address);
    static uint16_t read16(uint32_t p_address);
    static uint32_t read32(uint32_t p_address);
    static uint32_t read64(uint32_t p_address);
    static void write8(uint32_t p_address, uint8_t p_value);
    static void write16(uint32_t p_address, uint16_t p_value);
    static void write32(uint32_t p_address, uint32_t p_value);
    //static void write64(uint64_t p_address, uint64_t p_value);
};


#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 8

struct e1000_rx_desc
{
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t status;
    volatile uint8_t errors;
    volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc
{
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t cso;
    volatile uint8_t cmd;
    volatile uint8_t status;
    volatile uint8_t css;
    volatile uint16_t special;
} __attribute__((packed));

// Todo: free memory (tx/rx init, start)
class E1000 : Interrupt_handler
{
private:
    PCI::Device device;

    uint8_t bar_type; // Type of BAR0
    uint16_t io_base; // IO Base Address
    uint32_t mem_base; // MMIO Base Address
    bool eerprom_exists; // A flag indicating if eeprom exists
    uint8_t mac[6]; // A buffer for storing the mack address
    struct e1000_rx_desc* rx_descs[E1000_NUM_RX_DESC]{}; // Receive Descriptor Buffers
    struct e1000_tx_desc* tx_descs[E1000_NUM_TX_DESC]{}; // Transmit Descriptor Buffers
    uint16_t rx_cur; // Current Receive Descriptor Buffer
    uint16_t tx_cur; // Current Transmit Descriptor Buffer
    uint8_t* rx_desc_virt_addresses[E1000_NUM_RX_DESC]{};
    uint8_t* tx_desc_virt_addresses[E1000_NUM_RX_DESC]{};


    // Send Commands and read results From NICs either using MMIO or IO Ports
    void writeCommand(uint16_t p_address, uint32_t p_value) const;
    [[nodiscard]] uint32_t readCommand(uint16_t p_address) const;


    bool detectEEProm(); // Return true if EEProm exist, else it returns false and set the eerprom_existsdata member
    [[nodiscard]] uint32_t eepromRead(uint8_t addr) const; // Read 4 bytes from a specific EEProm Address
    bool readMACAddress(); // Read MAC Address
    void startLink(); // Start up the network
    void rxinit(); // Initialize receive descriptors an buffers
    void txinit(); // Initialize transmit descriptors an buffers
    void pollRx(); // Handle a packet reception.
    void enableInterrupt() const; // Enable Interrupts
    void tx_free(); // Free processed tx descriptor buffers
public:
    explicit E1000(PCI::Device pci_device);
    // Constructor. takes as a parameter a pointer to an object that encapsulate all he PCI configuration data of the device
    bool start(); // perform initialization tasks and starts the driver
    void fire(cpu_state_t* cpu_state, stack_state_t* stack_state) override;
    // This method should be called by the interrupt handler
    uint8_t* getMacAddress(); // Returns the MAC address
    int sendPacket(const Ethernet::packet_info* packet); // Send a packet
    ~E1000() = default; // Default Destructor
};
#endif //E100_H
