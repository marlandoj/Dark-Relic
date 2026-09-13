import hashlib
import importlib.util
import json
import math
import struct
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location('geometry', Path(__file__).parents[1] / 'integration/widowfen_geometry.py')
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)


class WidowfenGeometryTests(unittest.TestCase):
    def test_meshes_are_valid_and_reproducible(self):
        with tempfile.TemporaryDirectory(prefix='zo-task-', dir='/tmp') as tmp:
            root = Path(tmp)
            counts = geometry.build_geometry(root)
            hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir()}
            self.assertEqual(set(counts), {'AlderWood', 'AlderLeaves', 'ReedClump', 'FernClump', 'SlatePatch'})
            for name, triangles in counts.items():
                data = (root / (name + '.glb')).read_bytes()
                self.assertEqual(struct.unpack_from('<III', data), (0x46546c67, 2, len(data)))
                json_size, chunk_type = struct.unpack_from('<II', data, 12)
                self.assertEqual(chunk_type, 0x4e4f534a)
                doc = json.loads(data[20:20 + json_size])
                binary_size, binary_type = struct.unpack_from('<II', data, 20 + json_size)
                self.assertEqual(binary_type, 0x004e4942)
                blob = data[28 + json_size:]
                self.assertEqual(binary_size, len(blob))
                self.assertGreater(triangles, 0)
                self.assertLess(triangles, 10000)
                for accessor, view in zip(doc['accessors'], doc['bufferViews']):
                    self.assertEqual(accessor['count'], triangles * 3)
                    self.assertLessEqual(view['byteOffset'] + view['byteLength'], len(blob))
                    values = struct.unpack_from('<' + 'f' * (view['byteLength'] // 4), blob, view['byteOffset'])
                    self.assertTrue(all(math.isfinite(x) for x in values))
                positions = struct.unpack_from('<' + 'f' * (triangles * 9), blob)
                for i in range(0, len(positions), 9):
                    a, b, c = positions[i:i+3], positions[i+3:i+6], positions[i+6:i+9]
                    area = geometry.cross(geometry.add(b, geometry.mul(a, -1)), geometry.add(c, geometry.mul(a, -1)))
                    self.assertGreater(sum(x*x for x in area), 1e-16)
            self.assertEqual(geometry.build_geometry(root), counts)
            self.assertEqual({p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in root.iterdir()}, hashes)

    def test_plantings_preserve_the_combat_lane(self):
        plan = geometry.planting_plan()
        self.assertEqual(plan, geometry.planting_plan())
        self.assertEqual(len(plan), 224)
        self.assertEqual(sum(p['kind'] == 'tree' for p in plan), 114)
        for plant in plan:
            x, y, _ = plant['position']
            self.assertGreater(plant['scale'], 0)
            if plant['kind'] == 'tree':
                self.assertGreater(math.hypot(x, y - 400), 3100)
            else:
                self.assertGreater(abs(x), 1500)


if __name__ == '__main__':
    unittest.main()
