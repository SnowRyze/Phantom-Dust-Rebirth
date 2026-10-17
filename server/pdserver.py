#!/usr/bin/env python3
"""pdserver: relay server for Phantom Dust (pdnet).

Players' pdnet DLLs register here over UDP. When the game on player A connects to player B, the server
gives each side its own relay port. Each game sends its traffic to its relay port, and the server forwards
it to the other player from the other player's relay port. Players only ever send outbound traffic to the
server, so no port forwarding is needed on their side.

Only the server needs open ports: UDP <control> (default 43300) and the relay range (default 43310-43509).

    python3 pdserver.py [--host 0.0.0.0] [--control 43300] [--relay-ports 43310-43509] [--no-strict-ip]
"""
import argparse
import asyncio
import logging
import socket
import struct
import time

MAGIC = b"PDS1"
HEADER = struct.Struct("<4sBI")  # magic, type, nonce

REGISTER, REGISTERED, CONNECT, OFFER, OFFER_OK, CONNECTED, FAILED, RELEASE, CLOSED, PING, PEERPING, PEERPONG, PONG = range(1, 14)
FAIL_NOT_REGISTERED, FAIL_NO_RESPONSE, FAIL_NO_PORTS = 1, 2, 3

DEVICE_TIMEOUT = 60        # seconds without REGISTER before a device is forgotten
PAIR_IDLE_TIMEOUT = 180    # seconds without relayed traffic before a pair is freed
OFFER_TIMEOUT = 5.0
QUEUE_LIMIT = 64           # packets held for a side whose address isn't known yet

log = logging.getLogger("pdserver")


def pack(msg_type, nonce, body=b""):
    return HEADER.pack(MAGIC, msg_type, nonce) + body


def short_id(device_id):
    return f"{device_id:016X}"[-6:]


class Device:
    def __init__(self, device_id, addr):
        self.id = device_id
        self.addr = addr
        self.last_seen = time.monotonic()


class RelaySide:
    """One player's end of a pair: the server port they send to, and where their game socket was seen."""

    def __init__(self, device_id, port):
        self.device_id = device_id
        self.port = port
        self.transport = None
        self.addr = None
        self.queue = []


class Pair:
    def __init__(self, a, b):
        self.sides = {a.device_id: a, b.device_id: b}
        self.last_activity = time.monotonic()

    def other(self, device_id):
        return next(s for d, s in self.sides.items() if d != device_id)


class RelayPort(asyncio.DatagramProtocol):
    def __init__(self, server, pair, side):
        self.server, self.pair, self.side = server, pair, side

    def connection_made(self, transport):
        self.side.transport = transport

    def datagram_received(self, data, addr):
        side, pair = self.side, self.pair
        if side.addr != addr:
            dev = self.server.devices.get(side.device_id)
            if self.server.strict_ip and dev and dev.addr[0] != addr[0]:
                log.warning("relay %d: ignoring %s (device %s registered from %s)", side.port, addr, short_id(side.device_id), dev.addr[0])
                return
            log.info("relay %d: device %s game traffic from %s", side.port, short_id(side.device_id), addr)
            side.addr = addr
        pair.last_activity = time.monotonic()
        other = pair.other(side.device_id)
        if other.addr is None:
            if len(other.queue) < QUEUE_LIMIT:
                other.queue.append(data)
            return
        other.transport.sendto(data, other.addr)
        # The other side just became reachable through this port: deliver anything queued for this side.
        if side.queue:
            for queued in side.queue:
                side.transport.sendto(queued, side.addr)
            side.queue.clear()


class ControlProtocol(asyncio.DatagramProtocol):
    def __init__(self, server):
        self.server = server

    def connection_made(self, transport):
        self.server.control = transport

    def datagram_received(self, data, addr):
        if len(data) < HEADER.size:
            return
        magic, msg_type, nonce = HEADER.unpack_from(data)
        if magic != MAGIC:
            return
        try:
            self.server.handle(msg_type, nonce, data[HEADER.size:], addr)
        except Exception:
            log.exception("bad control message %d from %s", msg_type, addr)


class Server:
    def __init__(self, host, relay_ports, strict_ip):
        self.host = host
        self.free_ports = list(range(relay_ports[0], relay_ports[1] + 1))
        self.strict_ip = strict_ip
        self.devices = {}
        self.pairs = {}               # frozenset({idA, idB}) -> Pair
        self.offers = {}              # nonce -> Future (OFFER_OK) / Future (PEERPONG)
        self.control = None
        self.next_nonce = 1

    def nonce(self):
        self.next_nonce = (self.next_nonce + 1) & 0xFFFFFFFF or 1
        return self.next_nonce

    def send(self, addr, msg_type, nonce, body=b""):
        self.control.sendto(pack(msg_type, nonce, body), addr)

    # ------------------------------------------------------------------ control messages

    def handle(self, msg_type, nonce, body, addr):
        if msg_type == REGISTER:
            (device_id,) = struct.unpack_from("<Q", body)
            dev = self.devices.get(device_id)
            if dev is None:
                log.info("device %s registered from %s", short_id(device_id), addr)
                self.devices[device_id] = Device(device_id, addr)
            else:
                dev.addr, dev.last_seen = addr, time.monotonic()
            self.send(addr, REGISTERED, nonce)
        elif msg_type == CONNECT:
            asyncio.ensure_future(self.connect(nonce, body, addr))
        elif msg_type in (OFFER_OK, PEERPONG):
            fut = self.offers.pop(nonce, None)
            if fut and not fut.done():
                fut.set_result(body)
        elif msg_type == RELEASE:
            (peer_id,) = struct.unpack_from("<Q", body)
            sender = self.device_by_addr(addr)
            if sender:
                self.release(sender.id, peer_id, notify=True)
        elif msg_type == PING:
            asyncio.ensure_future(self.ping(nonce, body, addr))

    def device_by_addr(self, addr):
        return next((d for d in self.devices.values() if d.addr == addr), None)

    async def ask(self, addr, msg_type, body, timeout=OFFER_TIMEOUT, retry=0.5):
        """Sends a message to a device until it answers (OFFER_OK / PEERPONG) or the timeout passes."""
        nonce = self.nonce()
        fut = asyncio.get_running_loop().create_future()
        self.offers[nonce] = fut
        deadline = time.monotonic() + timeout