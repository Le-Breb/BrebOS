import netifaces
from scapy.all import Ether, ARP, srp, ICMP, IP, sr1, get_if_hwaddr, get_if_addr


# Function to get the default network interface
def get_default_interface():
    gateways = netifaces.gateways()
    default_gateway = gateways['default'][netifaces.AF_INET]
    return default_gateway[1]


# Get the default interface
iface = 'tap0'  # Change this if needed

# Fetch the host's MAC and IP addresses
host_mac = get_if_hwaddr(iface)  # Host's MAC address
host_ip = get_if_addr(iface)  # Host's IP address

# Target IP (the IP you're asking about)
target_ip = "192.168.100.96"  # Replace with the IP address you want to query


def send_arp_request():
    """Send an ARP request to get the MAC address of the target IP"""
    arp_request = Ether(dst="ff:ff:ff:ff:ff:ff") / ARP(
        op=1,  # ARP request
        hwsrc=host_mac,  # Host's MAC
        psrc=host_ip,  # Host's IP
        hwdst="00:00:00:00:00:00",  # Target MAC (unknown)
        pdst=target_ip  # Target IP
    )

    answered, _ = srp(arp_request, iface=iface, timeout=2, verbose=False)

    if answered:
        for sent, received in answered:
            print(f"Received ARP response: {received.psrc} is at {received.hwsrc}")
    else:
        print("No ARP response received")


def send_ping():
    print(f"Sending ping to {target_ip}")

    """Send an ICMP Echo Request (ping) to the target IP"""
    icmp_request = IP(dst=target_ip) / ICMP()

    response = sr1(icmp_request, timeout=2, verbose=False)

    if response:
        print(f"Ping response from {response.src}: TTL={response.ttl}")
    else:
        print("No ping response received")


# Run both functions
send_arp_request()
send_ping()
