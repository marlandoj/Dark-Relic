"""Deterministic, original environment geometry; lengths are in metres."""
import argparse
import json
import math
import random
import struct
from pathlib import Path


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def mul(a, s):
    return tuple(x * s for x in a)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def unit(a):
    return mul(a, 1 / max(1e-9, math.sqrt(sum(x*x for x in a))))


class Mesh:
    def __init__(self):
        self.vertices = []
        self.normals = []
        self.uvs = []

    def tri(self, a, b, c, uv=((0, 0), (1, 0), (0.5, 1))):
        normal = unit(cross(add(b, mul(a, -1)), add(c, mul(a, -1))))
        self.vertices.extend((a, b, c))
        self.normals.extend((normal, normal, normal))
        self.uvs.extend(uv)

    def branch(self, start, end, r0, r1, sides=9):
        axis = unit(add(end, mul(start, -1)))
        tangent = unit(cross(axis, (1, 0, 0) if abs(axis[2]) > 0.9 else (0, 0, 1)))
        bitangent = cross(axis, tangent)
        rings = []
        for p, r in ((start, r0), (end, r1)):
            rings.append([add(p, mul(add(mul(tangent, math.cos(i*math.tau/sides)), mul(bitangent, math.sin(i*math.tau/sides))), r)) for i in range(sides)])
        for i in range(sides):
            j = (i+1) % sides
            self.tri(rings[0][i], rings[0][j], rings[1][i])
            self.tri(rings[1][i], rings[0][j], rings[1][j])

    def leaf(self, center, angle, length, width, droop=0.0):
        axis = (math.cos(angle), math.sin(angle), droop)
        side = (-math.sin(angle)*width, math.cos(angle)*width, 0)
        tip = add(center, mul(axis, length))
        mid = add(center, mul(axis, length*0.45))
        ridge = add(mid, (0, 0, width*0.22))
        left, right = add(mid, side), add(mid, mul(side, -1))
        for a, b, c in ((center, left, ridge), (left, tip, ridge), (tip, right, ridge), (right, center, ridge)):
            self.tri(a, b, c)

    def save(self, path):
        if not self.vertices:
            raise ValueError('Empty geometry')
        positions = [(x, z, -y) for x, y, z in self.vertices]
        normals = [(x, z, -y) for x, y, z in self.normals]
        arrays = (positions, normals, self.uvs)
        blob = bytearray()
        views, accessors = [], []
        for i, arr in enumerate(arrays):
            flat = [v for row in arr for v in row]
            data = struct.pack('<'+'f'*len(flat), *flat)
            views.append({'buffer': 0, 'byteOffset': len(blob), 'byteLength': len(data), 'target': 34962})
            blob.extend(data)
            accessor = {'bufferView': i, 'componentType': 5126, 'count': len(arr), 'type': 'VEC2' if i == 2 else 'VEC3'}
            if i == 0:
                accessor.update(min=[min(p[j] for p in arr) for j in range(3)], max=[max(p[j] for p in arr) for j in range(3)])
            accessors.append(accessor)
        doc = {'asset': {'version': '2.0', 'generator': 'Dark Relic Widowfen procedural geometry'}, 'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0}], 'meshes': [{'name': path.stem, 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}}]}], 'buffers': [{'byteLength': len(blob)}], 'bufferViews': views, 'accessors': accessors}
        raw = json.dumps(doc, separators=(',', ':')).encode()
        raw += b' ' * (-len(raw) % 4)
        blob += b'\x00' * (-len(blob) % 4)
        path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28+len(raw)+len(blob)) + struct.pack('<II', len(raw), 0x4e4f534a) + raw + struct.pack('<II', len(blob), 0x004e4942) + blob)


def build_geometry(destination):
    destination.mkdir(parents=True, exist_ok=True)
    rng = random.Random(1666)
    wood, leaves, reeds, fern, slab = (Mesh() for _ in range(5))
    trunk = [(0, 0, 0), (0.12, -0.08, 1.7), (-0.10, 0.12, 3.5), (0.25, 0.2, 5.1), (0.1, 0.35, 6.7), (0.3, 0.15, 8.1)]
    for i in range(len(trunk)-1):
        wood.branch(trunk[i], trunk[i+1], 0.36-i*0.055, 0.31-i*0.055, 12)
    for i in range(9):
        angle = i*math.tau/9
        wood.branch((0, 0, 0.42), (math.cos(angle)*1.2, math.sin(angle)*1.2, 0), 0.17, 0.035)
    for i in range(23):
        angle = i*2.4
        z = 2.8+i*0.20
        reach = rng.uniform(1.6, 3.3)*(1-0.35*i/23)
        mid = (math.cos(angle)*reach*0.55, math.sin(angle)*reach*0.55, z+0.6)
        end = (math.cos(angle)*reach, math.sin(angle)*reach, z+rng.uniform(0.8, 1.7))
        wood.branch((0.05, 0.12, z), mid, 0.12-i*0.003, 0.055)
        wood.branch(mid, end, 0.055, 0.018)
        for j in range(4):
            t = 0.35+j*0.18
            base = add(mul(mid, 1-t), mul(end, t))
            tip = add(base, (rng.uniform(-0.8, 0.8), rng.uniform(-0.8, 0.8), rng.uniform(-1.2, 0.2)))
            wood.branch(base, tip, 0.022, 0.006, 6)
            for k in range(20):
                center = add(tip, (rng.uniform(-0.6, 0.6), rng.uniform(-0.6, 0.6), rng.uniform(-0.45, 0.4)))
                leaves.leaf(center, rng.uniform(0, math.tau), rng.uniform(0.18, 0.40), rng.uniform(0.035, 0.08), -0.5)
    for i in range(34):
        base = (rng.uniform(-0.48, 0.48), rng.uniform(-0.48, 0.48), 0)
        height = rng.uniform(0.45, 1.25)
        angle = rng.uniform(0, math.tau)
        mid = add(base, (math.cos(angle)*0.1, math.sin(angle)*0.1, height*0.7))
        tip = add(base, (math.cos(angle)*0.35, math.sin(angle)*0.35, height))
        width = (-math.sin(angle)*0.018, math.cos(angle)*0.018, 0)
        reeds.tri(add(base, width), add(base, mul(width, -1)), mid)
        reeds.tri(add(mid, width), add(mid, mul(width, -1)), tip)
    for i in range(11):
        angle = i*math.tau/11
        for j in range(9):
            t = (j+1)/10
            p = (math.cos(angle)*t*0.8, math.sin(angle)*t*0.8, math.sin(t*math.pi)*0.42)
            for side in (-1, 1):
                fern.leaf(p, angle+side*0.8, 0.25*(1-t)+0.035, 0.025*(1-t)+0.01)
    ring = [(math.cos(i*math.tau/9)*rng.uniform(0.75, 1), math.sin(i*math.tau/9)*rng.uniform(0.6, 0.9), rng.uniform(0.008, 0.025)) for i in range(9)]
    for i in range(9):
        slab.tri((0, 0, 0.035), ring[i], ring[(i+1)%9])
    meshes = {'AlderWood': wood, 'AlderLeaves': leaves, 'ReedClump': reeds, 'FernClump': fern, 'SlatePatch': slab}
    for name, mesh in meshes.items():
        mesh.save(destination/(name+'.glb'))
    return {name: len(mesh.vertices)//3 for name, mesh in meshes.items()}


def planting_plan():
    rng = random.Random(1666)
    result = []
    for ring, count, radius in ((0, 34, 3300), (1, 42, 4800), (2, 38, 6600)):
        for i in range(count):
            angle = i*math.tau/count + rng.uniform(-0.04, 0.04)
            r = radius+rng.uniform(-180, 180)
            result.append({'kind': 'tree', 'position': [math.cos(angle)*r, 400+math.sin(angle)*r, -42], 'yaw': rng.uniform(0, 360), 'scale': rng.uniform(0.8, 1.4), 'ring': ring})
    for i in range(110):
        side = -1 if i%2 else 1
        result.append({'kind': 'reed' if i%3 else 'fern', 'position': [side*rng.uniform(1540, 2800), rng.uniform(-2300, 2800), -2], 'yaw': rng.uniform(0, 360), 'scale': rng.uniform(0.65, 1.2)})
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build_geometry(args.output), indent=2))
