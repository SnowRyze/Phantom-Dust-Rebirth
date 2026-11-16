"""Reads a Theatre film (.pdfilm, Conceptual branch) and decodes its match traffic.

    py -I tools\\filmread.py <film> [--stream NAME] [--limit N]

Film layout: see src\\film.cpp. A recorded datagram is the game's mesh packet:
    7-byte mesh header: u16 mesh sequence, u8 type (0x40 game data, 0x01 heartbeat / ack), u32 length of the whole packet.
    8-byte message header: u16 length, u16 0x1200, u16 message kind, u16 the sender's slot (DAT_140594e80).
    Kind 8 is the game's per-peer block (FUN_14027abe0), below; kinds 4 and 10 and a kind-8 block whose mask has its top
    bit set are short end-of-match messages (FUN_1401912e0 and the others that call FUN_140125430), not decoded.
    The per-peer block:
        u64 mask    bit N < 26: stream N is in this packet; bit 26 + K: an ack for the K-th sequenced stream
        u32 slot    the sender's slot (DAT_1404bbe00)
        u32 half    the sender's game frame / 2
        u32 lag     the sender's frame-lag estimate (FUN_14027ba90)
        u32 extra   FUN_140184730()
        then each stream in the mask, in order: for a sequenced stream a u32 sequence number first, then its data;
        then a u32 per ack bit; whatever is left over is voice data (GameChat), not decoded.
Everything is bounds-checked: a damaged or foreign film stops with a message, never reads past its data.
"""
import argparse
import collections
import struct
import sys

# The stream table at 0x1403fb4d0 in PDUWP.exe: (name, size, sequenced).
STREAMS = [
    ('PlayerPos', 40, False), ('CardKind', 8, False), ('FireStartReq', 28, True), ('FireReq', 96, True),
    ('SkillActionCtrl', 28, True), ('SkillActionEnd', 96, True), ('Stage', 28, True), ('Stage2', 32, True),
    ('CapsuleData', 12, True), ('EnvCapture', 28, True), ('EnvCapture2', 12, True), ('CapsuleSteal', 4, True),
    ('CapsuleMove', 16, True), ('PowSteal', 4, True), ('Condition', 6, True), ('DrawTimer', 4, True),
    ('PlayerCaptureReq', 2, True), ('PlayerCaptureAns', 3, True), ('PlayerRevive', 1, True), ('NoDmgTimer', 4, True),
    ('DmgEffect', 6, True), ('Warp', 28, True), ('ReadyStart', 8, True), ('KilledCount', 4, True),
    ('SkillHit', 6, True), ('SkillHit2', 4, True),
]
SEQUENCED = [i for i, s in enumerate(STREAMS) if s[2]]
KINDS = {1: 'in', 2: 'out', 3: 'peer', 4: 'state', 5: 'snapshot', 255: 'end'}


class FilmError(Exception):
    pass


def read_film(path):
    data = open(path, 'rb').read()
    if len(data) < 30 or data[:8] != b'PDFILM\0\0':
        raise FilmError('not a film (bad magic)')
    fmt, stamp, image, start = struct.unpack_from('<IIIQ', data, 8)
    if fmt != 1:
        raise FilmError('film format %d, this reader knows 1' % fmt)
    n = struct.unpack_from('<H', data, 28)[0]
    if 30 + n > len(data):
        raise FilmError('header cut off')
    header = {'exe_stamp': stamp, 'exe_size': image, 'start': start, 'kit': data[30:30 + n].decode('utf-8', 'replace')}
    records = []
    o = 30 + n
    while o + 12 <= len(data):
        kind, peer, length, ms, frame = struct.unpack_from('<BBHIi', data, o)
        if o + 12 + length > len(data):
            header['cut_off_at'] = o  # the game stopped mid-write: what came before is still good
            break
        records.append((kind, peer, ms, frame, data[o + 12:o + 12 + length]))
        o += 12 + length
    return header, records


def decode_packet(p):
    """The game block of one datagram: a dict, or None for a heartbeat / something else."""
    if len(p) < 7 or p[2] != 0x40:
        return None
    total = struct.unpack_from('<I', p, 3)[0]
    if total != len(p) or len(p) < 15:
        return {'error': 'length %d, header says %d' % (len(p), total)}
    msg_len, magic, kind, sender = struct.unpack_from('<HHHH', p, 7)
    if kind != 8 or len(p) < 15 + 24:
        return {'message': kind}
    o = 15
    mask, slot, half, lag, extra = struct.unpack_from('<QIIII', p, o)
    if mask >> 63:
        return {'message': 'end of match'}
    o += 24
    out = {'slot': slot, 'half': half, 'lag': lag, 'sender': sender, 'streams': [], 'acks': []}
    for i, (name, size, seq) in enumerate(STREAMS):
        if not (mask >> i) & 1:
            continue
        need = size + (4 if seq else 0)
        if o + need > len(p):
            return {'error': 'stream %s runs past the packet' % name}
        number = struct.unpack_from('<I', p, o + size)[0] if seq else None  # the number comes after the data
        body = p[o:o + size]
        out['streams'].append((i, number, body))
        o += need
    for k, i in enumerate(SEQUENCED):
        if (mask >> (26 + k)) & 1:
            if o + 4 > len(p):
                return {'error': 'ack runs past the packet'}
            out['acks'].append((i, struct.unpack_from('<I', p, o)[0]))
            o += 4
    out['voice'] = len(p) - o
    return out


def describe(i, body):
    name = STREAMS[i][0]
    if name == 'PlayerPos':
        bits, x, y, z, target, angle = struct.unpack_from('<QfffiH', body, 0)
        return 'pos %.1f %.1f %.1f facing %d target %d' % (x, y, z, angle, target)
    if name == 'CardKind':
        return 'hand %s' % list(struct.unpack_from('<4h', body, 0))
    return body.hex(' ')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('film')
    ap.add_argument('--stream', help='print every record of this stream (e.g. FireReq)')
    ap.add_argument('--limit', type=int, default=40)
    a = ap.parse_args()
    try:
        header, records = read_film(a.film)
    except (OSError, FilmError, struct.error) as e:
        print('cannot read %s: %s' % (a.film, e))
        return 1
    print('kit %s, game exe %08x / %x' % (header['kit'], header['exe_stamp'], header['exe_size']))
    if 'cut_off_at' in header:
        print('the film is cut off at byte %d (the game stopped while writing); the rest is read' % header['cut_off_at'])
    kinds = collections.Counter(KINDS.get(r[0], r[0]) for r in records)
    print('records:', dict(kinds))
    peers = {r[1]: r[4].decode('ascii', 'replace') for r in records if r[0] == 3}
    print('peers:', peers)
    per = collections.defaultdict(collections.Counter)   # (direction, slot) -> stream counts
    errors = collections.Counter()
    shown = 0
    for kind, peer, ms, frame, payload in records:
        if kind not in (1, 2):
            continue
        d = decode_packet(payload)
        if d is None:
            per[(KINDS[kind], 'mesh')]['heartbeat/other'] += 1
            continue
        if 'error' in d:
            errors[d['error']] += 1
            continue
        if 'message' in d:
            per[(KINDS[kind], 'messages')]['kind %s' % d['message']] += 1
            continue
        key = (KINDS[kind], 'slot %d' % d['slot'])
        per[key]['packets'] += 1
        per[key]['voice bytes'] += d['voice']
        for i, number, body in d['streams']:
            per[key][STREAMS[i][0]] += 1
            if a.stream and STREAMS[i][0] == a.stream and shown < a.limit:
                shown += 1
                print('%6.1f s %-3s slot %d #%s %s' % (ms / 1000, KINDS[kind], d['slot'], number, describe(i, body)))
    for key in sorted(per):
        print('%s %s:' % key, ', '.join('%s %d' % kv for kv in sorted(per[key].items())))
    if errors:
        print('undecoded packets:', dict(errors))
    return 0


if __name__ == '__main__':
    sys.exit(main())
