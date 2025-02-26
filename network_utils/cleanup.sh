#!/bin/bash
set -e

TAP_IF="tap0"
DNSMASQ_PID="/var/run/dnsmasq_tap0.pid"

# Detect primary interface
PRIMARY_IFACE=$(ip route get 8.8.8.8 | grep -oP 'dev \K\S+')

echo "Cleaning up NAT rules"
sudo iptables -t nat -D POSTROUTING -o "$PRIMARY_IFACE" -j MASQUERADE || true
sudo iptables -D FORWARD -i "$TAP_IF" -o "$PRIMARY_IFACE" -j ACCEPT || true
sudo iptables -D FORWARD -i "$PRIMARY_IFACE" -o "$TAP_IF" -m state --state RELATED,ESTABLISHED -j ACCEPT || true

# Stop dnsmasq
if [ -f "$DNSMASQ_PID" ]; then
    echo "Stopping dnsmasq"
    sudo kill "$(cat "$DNSMASQ_PID")" || true
    sudo rm -f "$DNSMASQ_PID"
fi

# Remove TAP interface
if ip link show "$TAP_IF" > /dev/null 2>&1; then
    echo "Removing TAP interface: $TAP_IF"
    sudo ip link set "$TAP_IF" down
    sudo ip tuntap del dev "$TAP_IF" mode tap
fi

if [ -f /tmp/original_ip_forward ]; then
    ORIGINAL_FORWARD=$(cat /tmp/original_ip_forward)
    sudo sysctl -w net.ipv4.ip_forward="$ORIGINAL_FORWARD" > /dev/null
    sudo rm  -f /tmp/original_ip_forward
else
    sudo sysctl -w net.ipv4.ip_forward=0  > /dev/null # Fallback to default if unknown
fi

echo "Cleanup complete."
