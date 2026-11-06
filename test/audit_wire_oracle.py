#!/usr/bin/env python3
"""Independent Python struct oracle for the supplied link_message1/2 layouts."""
import json
from pathlib import Path
import struct
import sys


def require(condition, text):
    if not condition:
        raise RuntimeError(text)


def fixed(text, count):
    data = text.encode('utf-8')
    require(len(data) <= count, 'fixed string overflow')
    return data.ljust(count, b'\0')


def source_wire(value):
    header = value['header']
    body = value['body']
    points = body['points']
    data = bytes(body['data']['buffer'])
    require(body['data']['length'] == len(data), 'source manual length mismatch')
    # Header size 12; MsgData.length at 44; buffer at 48; all offsets are explicit.
    prefix = struct.pack(
        '>H2xIHBxhhf8sHHHhhhh2xI',
        header['msgcode'], header['id'], header['version'], header['state'],
        body['position']['x'], body['position']['y'], body['temperature'], fixed(body['name'], 8),
        *body['samples'],
        points[0]['x'], points[0]['y'], points[1]['x'], points[1]['y'], len(data)
    )
    require(len(prefix) == 48, 'source reference prefix')
    wire = prefix + data
    return wire + bytes((-len(wire)) % 4)


def target_wire(value):
    header = value['header']
    body = value['body']
    points = body['points']
    data = bytes(body['data']['buffer'])
    require(body['data']['length'] == len(data), 'target manual length mismatch')
    prefix = struct.pack(
        '<HIBB16siidIIIiiiiI',
        header['msgcode'], header['sequence'], header['revision'], header['state'],
        fixed(body['label'], 16), body['coordinates']['east'], body['coordinates']['north'],
        body['celsius'], *body['measurements'],
        points[0]['x'], points[0]['y'], points[1]['x'], points[1]['y'], len(data)
    )
    require(len(prefix) == 72, 'target reference prefix')
    return prefix + data


def main():
    path = Path(sys.argv[1] if len(sys.argv) > 1 else 'audit_output/oracle_cases.json')
    cases = json.loads(path.read_text())
    require(len(cases) == 190, 'all integration cases exported')
    for index, case in enumerate(cases):
        source = case['source_values']
        target = case['target_values']
        require(source_wire(source) == bytes.fromhex(case['source_hex']), f'source byte mismatch at case {index}')
        require(target_wire(target) == bytes.fromhex(case['target_hex']), f'target byte mismatch at case {index}')
        require(source['header']['msgcode'] == 1001 and target['header']['msgcode'] == 2001, 'message codes')
        require(target['header']['sequence'] == source['header']['id'], 'mapped identity')
        require(target['header']['revision'] == source['header']['version'], 'mapped revision')
        require(target['header']['state'] == source['header']['state'], 'mapped state')
        require(target['body']['label'] == source['body']['name'], 'mapped string')
        require(target['body']['celsius'] == source['body']['temperature'], 'mapped float precision')
        require(target['body']['coordinates']['east'] == source['body']['position']['x'], 'mapped east')
        require(target['body']['coordinates']['north'] == source['body']['position']['y'], 'mapped north')
        require(target['body']['measurements'] == source['body']['samples'], 'mapped samples')
        require(target['body']['points'] == source['body']['points'], 'mapped struct arrays')
        require(target['body']['data'] == source['body']['data'], 'mapped buffer and manual length')
    print(f'Python struct oracle: {len(cases)} cases / {2 * len(cases)} frames matched byte-for-byte')


if __name__ == '__main__':
    main()
