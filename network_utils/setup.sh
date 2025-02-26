#!/bin/bash
set -e

# Network settings
TAP_IF="tap0"
SUBNET="192.168.100.0/24"
GATEWAY="192.168.100.1"
DHCP_RANGE="192.168.100.10,192.168.100.100,12h"
DNSMASQ_PID="/var/run/dnsmasq_tap0.pid"

# Detect primary interface (Wi-Fi or Ethernet)
PRIMARY_IFACE=$(ip route get 8.8.8.8 | grep -oP 'dev \K\S+')

echo "Using primary interface: $PRIMARY_IFACE"

# Create TAP interface
if ! ip link show "$TAP_IF" > /dev/null 2>&1; then
    echo "Creating TAP interface: $TAP_IF"
    sudo ip tuntap add dev "$TAP_IF" mode tap
fi

# Assign IP to TAP
echo "Configuring $TAP_IF with IP $GATEWAY"
sudo ip addr add "$GATEWAY/24" dev "$TAP_IF"
sudo ip link set "$TAP_IF" up

# Enable IP forwarding
echo "Enabling IP forwarding"
ORIGINAL_FORWARD=$(sysctl -n net.ipv4.ip_forward)
echo "$ORIGINAL_FORWARD" | sudo tee /tmp/original_ip_forward > /dev/null

# Configure NAT (Masquerading)
echo "Setting up NAT from $TAP_IF to $PRIMARY_IFACE"
sudo iptables -t nat -A POSTROUTING -o "$PRIMARY_IFACE" -j MASQUERADE
sudo iptables -A FORWARD -i "$TAP_IF" -o "$PRIMARY_IFACE" -j ACCEPT
sudo iptables -A FORWARD -i "$PRIMARY_IFACE" -o "$TAP_IF" -m state --state RELATED,ESTABLISHED -j ACCEPT

# Start dnsmasq for DHCP
echo "Starting dnsmasq for DHCP on $TAP_IF"
sudo dnsmasq --interface="$TAP_IF" --bind-interfaces \
    --dhcp-range="$DHCP_RANGE" --dhcp-option=3,"$GATEWAY" \
    --dhcp-option=6,8.8.8.8,8.8.4.4 --pid-file="$DNSMASQ_PID"

echo "Setup complete."
