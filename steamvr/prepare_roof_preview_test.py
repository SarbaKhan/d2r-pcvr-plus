"""Synthetic fixtures only; no Blizzard presets, meshes or saves."""
import copy
import json
from pathlib import Path
import tempfile
import unittest

import prepare_roof_preview as preview


def entity(model='act3_kurast_roof/roof_example.model'):
    return {'name': 'example', 'id': 7, 'components': [
        {'type': 'TransformDefinitionComponent', 'position': {'x': 1, 'y': 2, 'z': 3}},
        {'type': 'ModelDefinitionComponent', 'filename': preview.MODELS + model},
        {'type': 'FadeTargetComponent', 'targetId': '123'},
        {'type': 'PhysicsBodyDefinitionComponent', 'fixturedefs': ['kept']}]}


def preset(*entities):
    return {'type': 'Preset', 'entities': list(entities),
            'dependencies': ['kept'], 'terrain': {'example': True}}


class RoofSelectionTests(unittest.TestCase):
    def test_only_roof_fade_changes(self):
        roof = entity()
        wall = entity('walls/wall.model')
        wall['name'] = 'roof_decoration'  # Entity name is not a model classifier.
        source = preset(roof, wall)
        before = copy.deepcopy(source)
        result, count = preview.retain_roofs(source)
        self.assertEqual(count, 1)
        self.assertEqual(source, before)
        expected = copy.deepcopy(before)
        expected['entities'][0]['components'].pop(2)
        self.assertEqual(result, expected)
        self.assertEqual(preview.retain_roofs(result), (result, 0))

    def test_all_variations_must_be_roofs(self):
        roof = entity('act3_kurast_slums/slums_hut_roof_corner01.model')
        roof['components'][1] = {'type': 'ModelVariationDefinitionComponent',
            'variations': [{'filename': preview.MODELS + 'act3_kurast_roof/roof_example.model'},
                           {'filename': preview.MODELS + 'act3_kurast_slums/slums_roof01.model'}]}
        self.assertEqual(preview.retain_roofs(preset(roof))[1], 1)
        for bad in ('', preview.MODELS + 'walls/roof_picture.model', None):
            roof['components'][1]['variations'][1]['filename'] = bad
            self.assertEqual(preview.retain_roofs(preset(roof)), (preset(roof), 0))

    def test_other_model_families_and_missing_models_untouched(self):
        source = preset(entity('../act1/roof_example.model'), entity('walls/roof_example.model'))
        source['entities'].append({'components': [{'type': 'FadeTargetComponent'}]})
        self.assertEqual(preview.retain_roofs(source), (source, 0))
        self.assertFalse(preview.roof_model(preview.MODELS + 'act3_kurast_roof/../walls/roof_example.model'))
        self.assertFalse(preview.roof_model('X:' + preview.MODELS + 'act3_kurast_roof/roof_example.model'))

    def test_only_target_presets_generated(self):
        files = {preview.PRESETS + 'example.json': preset(entity()),
                 preview.PRESETS + 'ordinary.json': preset(entity('walls/wall.model')),
                 'data/hd/env/preset/act1/roof.json': preset(entity()),
                 preview.PRESETS + '../escaped.json': preset(entity())}
        class Assets:
            def read(self, name):
                return json.dumps(files[name]).encode()
        assets = Assets()
        assets.files = files
        result = preview.generate(assets)
        self.assertEqual(list(result), [preview.PRESETS + 'example.json'])
        data, info = next(iter(result.values()))
        self.assertEqual(info['source_sha256'], preview.digest(assets.read(next(iter(result)))))
        self.assertEqual(info['sha256'], preview.digest(data))


class PreviewLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.game = Path(self.temp.name).resolve()
        self.root = self.game / 'mods/pcvr-dev/pcvr-dev.mpq'
        self.root.mkdir(parents=True)
        (self.root / 'modinfo.json').write_bytes(preview.MOD_INFO)
        self.names = [preview.PRESETS + 'example.json', preview.PRESETS + 'second.json']
        self.generated = {name: (b'{}', {'sha256': preview.digest(b'{}'), 'roof_entities': 1})
                          for name in self.names}

    def test_install_remove_preserves_unrelated_files(self):
        unrelated = self.root / 'data/keep.json'
        unrelated.parent.mkdir()
        unrelated.write_bytes(b'keep')
        preview.install_preview(preview.checked_root(self.game), self.generated)
        self.assertTrue(all((self.root / name).read_bytes() == b'{}' for name in self.names))
        self.assertEqual(preview.remove_preview(self.root), 2)
        self.assertEqual(unrelated.read_bytes(), b'keep')
        self.assertEqual((self.root / 'modinfo.json').read_bytes(), preview.MOD_INFO)
        self.assertEqual(preview.remove_preview(self.root), 0)

    def test_conflicting_override_blocks_all_writes(self):
        conflict = self.root / self.names[1]
        conflict.parent.mkdir(parents=True)
        conflict.write_bytes(b'other mod')
        with self.assertRaises(ValueError):
            preview.install_preview(self.root, self.generated)
        self.assertEqual(conflict.read_bytes(), b'other mod')
        self.assertFalse((self.root / self.names[0]).exists())
        self.assertFalse((self.root / preview.RECEIPT).exists())

    def test_modified_override_blocks_all_removals(self):
        preview.install_preview(self.root, self.generated)
        (self.root / self.names[1]).write_bytes(b'modified')
        with self.assertRaises(ValueError):
            preview.remove_preview(self.root)
        self.assertTrue((self.root / self.names[0]).is_file())
        self.assertEqual((self.root / self.names[1]).read_bytes(), b'modified')

    def test_missing_file_after_interruption_can_be_removed(self):
        preview.install_preview(self.root, self.generated)
        (self.root / self.names[1]).unlink()
        self.assertEqual(preview.remove_preview(self.root), 1)

    def test_receipt_cannot_remove_outside_scope(self):
        for name in ('../modinfo.json', preview.PRESETS + '../../../../modinfo.json',
                     'data/hd/env/preset/act1/house.json', str(self.game / 'outside.json')):
            (self.root / preview.RECEIPT).write_text(json.dumps(
                {'format': 1, 'files': {name: {'sha256': preview.digest(b'keep')}}}))
            with self.assertRaises(ValueError):
                preview.remove_preview(self.root)
            self.assertEqual((self.root / 'modinfo.json').read_bytes(), preview.MOD_INFO)

    def test_refuses_normal_save_path(self):
        (self.root / 'modinfo.json').write_text('{"name":"pcvr-dev","savepath":"../"}')
        with self.assertRaises(ValueError):
            preview.checked_root(self.game)


if __name__ == '__main__':
    unittest.main()
