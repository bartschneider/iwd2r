"""Tests for tools/gen_map.py, tools/check_evidence.py, tools/gen_fields.py,
tools/minischema.py, tools/shaders.py and the pure parts of tools/play.py, tools/realinput.py,
tools/winsandbox.py, tools/oracle.py and tools/ghidra/labels.py.

Run from the repository root: python3 -I tests/test_tools.py

Each test builds a throwaway git repository in a temporary directory, runs the
checker as a subprocess with cwd set to that repository, and removes the
directory afterwards.
"""

import contextlib
import datetime
import glob
import importlib.util
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import types
import unittest
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GEN_MAP = os.path.join(ROOT, 'tools', 'gen_map.py')
SHADERS_TOOL = os.path.join(ROOT, 'tools', 'shaders.py')


class GenMapTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = self._tmp.name
        self.addCleanup(self._tmp.cleanup)
        self.write('src/CAIAction.cpp', 'int a;\n// TODO: Incomplete.\n// 0x401000\n')
        self.write('src/CVidMode.cpp', '#include "platform/ddraw_shim.h"\nint v;\n')
        self.write('src/CGameSpy.cpp', 'int g;\n')
        self.write('src/CChitin3d.cpp', 'int c;\n')
        self.write('compat/x.h', 'int x;\n')
        self.write('platform/y.cpp', 'int y;\n')

    def write(self, rel, text, mode='w'):
        path = os.path.join(self.repo, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, mode) as f:
            f.write(text)

    def run_gen(self, *args):
        return subprocess.run([sys.executable, '-I', GEN_MAP, '--repo', self.repo, *args],
                              capture_output=True, text=True)

    def rows(self):
        with open(os.path.join(self.repo, 'docs', 'MAP.md')) as f:
            out = {}
            for line in f:
                if line.startswith('|'):
                    cells = [c.strip() for c in line.strip().strip('|').split('|')]
                    out[cells[0]] = cells
            return out

    def test_rows(self):
        self.assertEqual(self.run_gen().returncode, 0)
        rows = self.rows()
        self.assertEqual(rows['AI'][1:5], ['1', '3', '1', '1'])
        self.assertEqual(rows['Video'][1], '2')  # CVidMode + CChitin3d
        self.assertIn('ddraw_shim.h', rows['Video'][5])
        self.assertEqual(rows['Messaging'][1], '1')  # CGameSpy, not Game
        self.assertEqual(rows['Game'][1], '0')
        self.assertEqual(rows['Engine core'][1], '0')
        self.assertEqual(rows['compat'][1], '1')
        self.assertEqual(rows['platform'][1], '1')
        self.assertEqual(rows['AI'][5], '\u2014')

    def test_deterministic(self):
        self.run_gen()
        path = os.path.join(self.repo, 'docs', 'MAP.md')
        first = open(path, 'rb').read()
        self.run_gen()
        self.assertEqual(first, open(path, 'rb').read())

    def test_check(self):
        self.run_gen()
        self.assertEqual(self.run_gen('--check').returncode, 0)
        self.write('src/CAIAction.cpp', 'int more;\n', 'a')
        r = self.run_gen('--check')
        self.assertEqual(r.returncode, 1)
        self.assertTrue(r.stdout.strip() or r.stderr.strip())

    def test_check_without_map_fails(self):
        self.assertEqual(self.run_gen('--check').returncode, 1)


def load_tool(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(ROOT, 'tools', name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


play = load_tool('play')
realinput = load_tool('realinput')
dataview = load_tool('dataview')
winsandbox = load_tool('winsandbox')


def load_ghidra_tool(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(ROOT, 'tools', 'ghidra', name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


labels = load_ghidra_tool('labels')


def scenario_files():
    return sorted(glob.glob(os.path.join(ROOT, 'scenarios', '*.txt')))


class PlayArgsTest(unittest.TestCase):
    def overrides(self, *argv):
        a = play.parse_args(list(argv))
        return play.build_overrides(a.resolution, a.fullscreen, a.sets)

    def test_defaults(self):
        a = play.parse_args([])
        self.assertFalse(a.linux)
        self.assertFalse(a.headless)
        self.assertIsNone(a.script)
        self.assertIsNone(a.frames)
        self.assertEqual(self.overrides(), [
            'Program Options/Resolution=800',
            'Program Options/Full Screen=0'])

    def test_flags_map_to_overrides(self):
        self.assertEqual(self.overrides('--fullscreen', '--resolution', '1024'), [
            'Program Options/Resolution=1024',
            'Program Options/Full Screen=1'])
        self.assertEqual(self.overrides('--windowed')[1],
                         'Program Options/Full Screen=0')
        self.assertEqual(self.overrides('--set', 'Game Options/Gore=0',
                                        '--set', 'A/B=x=y')[2:],
                         ['Game Options/Gore=0', 'A/B=x=y'])

    def test_bad_flags_rejected(self):
        for argv in (['--resolution', 'wide'], ['--scale', '5'],
                     ['--vm', 'linux', '--script', 'x', '--resolution', '640'],
                     ['--vm', 'linux', '--script', 'x', '--resolution', '1920',
                      '--scale', '2'],
                     ['--set', 'NoSlash=1'],
                     ['--set', 'A/B'], ['--set', 'A/B=1;C/D=2'],
                     ['--frames', '0'], ['--fullscreen', '--windowed']):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit, msg=argv):
                    play.parse_args(argv)

    def test_resolution_and_scale_flags(self):
        a = play.parse_args(['--resolution', '1366', '--scale', 'auto'])
        self.assertEqual(play.build_overrides(a.resolution, a.fullscreen,
                                              a.sets, a.scale), [
            'Program Options/Resolution=1366',
            'Program Options/Full Screen=0',
            'Program Options/Scale=auto'])
        self.assertEqual(play.parse_args(['--resolution', 'Desktop'])
                         .resolution, 'desktop')

    def test_vm_overrides_carry_height_and_scale(self):
        def vm(*argv):
            return play.vm_overrides(play.parse_args(
                ['--vm', 'linux', '--script', 'x'] + list(argv)))
        overrides, g = vm()
        self.assertEqual(overrides, [
            'Program Options/Resolution=800',
            'Program Options/Full Screen=0',
            'Program Options/Scale=1',
            'Program Options/Height=600'])
        self.assertEqual((g['width'], g['height'], g['scale']), (800, 600, 1))
        overrides, g = vm('--resolution', '1366', '--set',
                          'Program Options/Height=768', '--set', 'A/B=1')
        self.assertEqual(overrides[2:], ['Program Options/Scale=1',
                                         'Program Options/Height=768',
                                         'A/B=1'])
        self.assertEqual((g['output_width'], g['output_height']), (1366, 768))
        overrides, g = vm('--resolution', '1920')
        self.assertEqual(overrides[3], 'Program Options/Height=1080')
        overrides, g = vm('--resolution', '2560', '--scale', '2')
        self.assertEqual((g['width'], g['height'], g['scale'],
                          g['output_width']), (1280, 720, 2, 2560))
        overrides, g = vm('--resolution', '3840', '--scale', 'auto')
        self.assertEqual((g['width'], g['height'], g['scale']), (1280, 720, 3))
        overrides, g = vm('--resolution', 'desktop')
        self.assertEqual((g['width'], g['height']), play.VM_DESKTOP)

    def test_desktop_refusal(self):
        self.assertIsNone(play.desktop_refusal(True, {}))
        self.assertIsNone(play.desktop_refusal(False, {'IWD2R_DESKTOP_OK': '1'}))
        self.assertIn('IWD2R_DESKTOP_OK', play.desktop_refusal(False, {}))
        self.assertIn('--vm windows', play.desktop_refusal(False, {'IWD2R_DESKTOP_OK': '0'}))

    def test_child_env(self):
        env = play.child_env({'PATH': '/bin'}, '/data', ['A/B=1', 'C/D=2'],
                             True, 300, '/s.txt', '/shots')
        self.assertEqual(env['IWD2R_INI_OVERRIDE'], 'A/B=1;C/D=2')
        self.assertEqual(env['SDL_VIDEO_DRIVER'], 'dummy')
        self.assertEqual(env['SDL_AUDIO_DRIVER'], 'dummy')
        self.assertEqual(env['IWD2R_SMOKE_FRAMES'], '300')
        self.assertEqual(env['IWD2R_SCRIPT'], '/s.txt')
        self.assertEqual(env['IWD2R_SHOT_DIR'], '/shots')
        self.assertEqual(env['IWD2_DATA'], '/data')
        self.assertEqual(env['PATH'], '/bin')
        env = play.child_env({}, '/data', [], False, None, None, '/shots')
        for name in ('SDL_VIDEO_DRIVER', 'IWD2R_SMOKE_FRAMES', 'IWD2R_SCRIPT'):
            self.assertNotIn(name, env)
        self.assertEqual(env['IWD2R_AUDIO'], 'null')
        env = play.child_env({}, '/data', [], False, None, None, '/shots',
                             sound=True)
        self.assertNotIn('IWD2R_AUDIO', env)
        self.assertNotIn('IWD2R_SHOT_PRESENTED', env)
        env = play.child_env({}, '/data', [], False, None, None, '/shots',
                             shot_presented=True)
        self.assertEqual(env['IWD2R_SHOT_PRESENTED'], '1')

    def test_shot_presented_flag(self):
        self.assertFalse(play.parse_args([]).shot_presented)
        self.assertTrue(play.parse_args(['--shot-presented']).shot_presented)

    def test_batch_set_escapes_percent(self):
        self.assertEqual(play.batch_set('X', 'a & b 100%'), 'set "X=a & b 100%%"')

    def test_sync_list_skips_build_and_runs(self):
        self.assertEqual(play.sync_list(['a.cpp', 'build/x', 'runs/1/log.txt',
                                         'tools/build/y', '']),
                         ['a.cpp', 'tools/build/y'])


class ShadersTest(unittest.TestCase):
    """tools/shaders.py --check against the committed blobs (it fetches the
    pinned glslang into $IWD2R_HOME/glslang when absent)."""

    def run_check(self, *args):
        return subprocess.run([sys.executable, '-I', SHADERS_TOOL, '--check', *args],
                              capture_output=True, text=True)

    def test_shaders_check_detects_stale_blob(self):
        real = self.run_check()
        self.assertEqual(real.returncode, 0, real.stdout + real.stderr)
        src = os.path.join(ROOT, 'platform', 'shaders')
        with tempfile.TemporaryDirectory() as tmp:
            for name in os.listdir(src):
                with open(os.path.join(src, name), 'rb') as a, \
                        open(os.path.join(tmp, name), 'wb') as b:
                    b.write(a.read())
            self.assertEqual(self.run_check('--dir', tmp).returncode, 0)
            blob = os.path.join(tmp, 'sharp.frag.spv')
            with open(blob, 'r+b') as f:
                f.seek(40)
                byte = f.read(1)
                f.seek(40)
                f.write(bytes([byte[0] ^ 0x01]))
            stale = self.run_check('--dir', tmp)
            self.assertEqual(stale.returncode, 1)
            self.assertIn('sharp.frag.spv does not match its source', stale.stdout)
            os.remove(blob)
            missing = self.run_check('--dir', tmp)
            self.assertEqual(missing.returncode, 1)
            self.assertIn('sharp.frag.spv is missing', missing.stdout)

    def test_shaders_check_covers_new_blobs(self):
        src = os.path.join(ROOT, 'platform', 'shaders')
        new = ('fsr_easu.frag', 'fsr_rcas.frag', 'nis.frag')
        for name in new:
            self.assertTrue(os.path.isfile(os.path.join(src, name + '.spv')), name)
        # Every shader is embedded by platform/CMakeLists.txt.
        with open(os.path.join(ROOT, 'platform', 'CMakeLists.txt')) as f:
            listed = re.search(r'set\(IWD2R_SHADERS ([^)]*)\)', f.read()).group(1).split()
        self.assertEqual(sorted(listed), sorted(n for n in os.listdir(src)
                                                if n.endswith(('.vert', '.frag'))))
        with tempfile.TemporaryDirectory() as tmp:
            for name in os.listdir(src):
                with open(os.path.join(src, name), 'rb') as a, \
                        open(os.path.join(tmp, name), 'wb') as b:
                    b.write(a.read())
            for name in new:
                with open(os.path.join(tmp, name + '.spv'), 'r+b') as f:
                    f.seek(40)
                    byte = f.read(1)
                    f.seek(40)
                    f.write(bytes([byte[0] ^ 0x01]))
            stale = self.run_check('--dir', tmp)
            self.assertEqual(stale.returncode, 1)
            for name in new:
                self.assertIn('%s.spv does not match its source' % name, stale.stdout)

    def test_pinned_release_has_checksums(self):
        with open(os.path.join(ROOT, 'tools', 'shaders_versions.json')) as f:
            pin = json.load(f)['glslang']
        self.assertTrue(pin['version'])
        for key in ('linux-x86_64', 'windows-x86_64'):
            self.assertIn(pin['version'], pin[key]['url'])
            self.assertRegex(pin[key]['sha256'], r'^[0-9a-f]{64}$')


class PlayRunFolderTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = self._tmp.name
        self.addCleanup(self._tmp.cleanup)

    def test_name(self):
        now = datetime.datetime(2026, 1, 2, 3, 4, 5)
        self.assertEqual(play.run_folder_name(now), '20260102-030405')

    def test_unique_folders(self):
        now = datetime.datetime(2026, 1, 2, 3, 4, 5)
        first = play.make_run_folder(self.dir, now)
        second = play.make_run_folder(self.dir, now)
        self.assertEqual(os.path.basename(first), '20260102-030405')
        self.assertEqual(os.path.basename(second), '20260102-030405-2')
        self.assertTrue(os.path.isdir(second))


class PlayIniBackupTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.install = os.path.join(self._tmp.name, 'install')
        self.backup = os.path.join(self._tmp.name, 'run', 'ini-backup')
        self.addCleanup(self._tmp.cleanup)
        os.makedirs(os.path.join(self.install, 'sub.ini'))  # a directory: skipped
        self.write('icewind2.ini', b'[Program Options]\r\nResolution=1366\r\n')
        self.write('Keymap.INI', b'\xef\xbb\xbfa=b\n')
        self.write('chitin.key', b'KEY')

    def write(self, name, data):
        with open(os.path.join(self.install, name), 'wb') as f:
            f.write(data)

    def read(self, name):
        with open(os.path.join(self.install, name), 'rb') as f:
            return f.read()

    def test_backup_and_restore_byte_identical(self):
        names = play.backup_inis(self.install, self.backup)
        self.assertEqual(names, ['Keymap.INI', 'icewind2.ini'])
        before = {n: self.read(n) for n in names}
        self.write('icewind2.ini', b'[Program Options]\nResolution=800\n')
        self.write('new.ini', b'created by the engine')
        changed = play.restore_inis(self.backup, self.install, names)
        self.assertEqual(changed, ['icewind2.ini'])
        for n in names:
            self.assertEqual(self.read(n), before[n])
        # Nothing in the install is deleted.
        self.assertEqual(self.read('new.ini'), b'created by the engine')
        self.assertEqual(self.read('chitin.key'), b'KEY')

    def test_restore_recreates_a_deleted_ini(self):
        names = play.backup_inis(self.install, self.backup)
        os.remove(os.path.join(self.install, 'icewind2.ini'))
        self.assertEqual(play.restore_inis(self.backup, self.install, names),
                         ['icewind2.ini'])
        self.assertEqual(self.read('icewind2.ini'),
                         b'[Program Options]\r\nResolution=1366\r\n')


class PlayVmArgsTest(unittest.TestCase):
    def test_vm_needs_a_scenario(self):
        for argv in (['--vm', 'linux'], ['--suite'],
                     ['--vm', 'linux', '--suite', '--script', 'x.txt'],
                     ['--vm', 'linux', '--suite', '--linux'],
                     ['--vm', 'linux', '--suite', '--headless'],
                     ['--vm', 'linux', '--suite', '--frames', '10'],
                     ['--vm', 'linux', '--suite', '--fullscreen'],
                     ['--vm', 'mac', '--suite'],
                     ['--vm', 'linux', '--suite', '--jobs', '0'],
                     ['--vm', 'linux', '--suite', '--timeout', '0']):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit, msg=argv):
                    play.parse_args(argv)

    def test_vm_defaults(self):
        a = play.parse_args(['--vm', 'all', '--suite'])
        self.assertEqual((a.vm, a.suite, a.jobs, a.timeout),
                         ('all', True, 1, 180))
        a = play.parse_args(['--vm', 'windows', '--script', 's.txt',
                             '--timeout', '60'])
        self.assertEqual((a.vm, a.script, a.timeout), ('windows', 's.txt', 60))

    def test_suite_files_are_the_scenarios(self):
        self.assertEqual(play.suite_files(), scenario_files())
        self.assertIn('start_screen',
                      [play.scenario_name(p) for p in play.suite_files()])

    def test_suite_ignores_pending(self):
        with tempfile.TemporaryDirectory() as tmp:
            for rel in ('a.txt', 'pending/x.txt', 'b.txt'):
                os.makedirs(os.path.dirname(os.path.join(tmp, rel)),
                            exist_ok=True)
                with open(os.path.join(tmp, rel), 'w') as f:
                    f.write('wait 1\n')
            self.assertEqual(play.suite_files(tmp),
                             [os.path.join(tmp, 'a.txt'),
                              os.path.join(tmp, 'b.txt')])
        self.assertNotIn('world_entry',
                         [play.scenario_name(p) for p in play.suite_files()])

    def test_table(self):
        text = play.format_table('linux', [('a', 0, 1.25, 3),
                                           ('bb', 124, 180.0, 0)])
        lines = text.splitlines()
        self.assertEqual(lines[0], 'linux')
        self.assertEqual(lines[1].split(),
                         ['scenario', 'exit', 'duration', 'shots'])
        self.assertEqual(lines[2].split(), ['a', '0', '1.2s', '3'])
        self.assertEqual(lines[3].split(), ['bb', '124', '180.0s', '0'])
        self.assertEqual(lines[4], '1 of 2 passed')


class ScenarioParseTest(unittest.TestCase):
    def test_every_verb(self):
        actions = realinput.parse_scenario(
            '# comment\n'
            '\n'
            'WAIT 60   # trailing\n'
            'click 10 20\r\n'
            'RClick 3 4\n'
            'move 0 0\n'
            'key escape\n'
            'key a\n'
            'type Hello  World 2\n'
            'shot 01_start\n'
            'Checkpoint start\n'
            'quit\n')
        self.assertEqual(actions, [
            {'op': 'wait', 'frames': 60, 'line': 3},
            {'op': 'click', 'x': 10, 'y': 20, 'ui': True, 'line': 4},
            {'op': 'rclick', 'x': 3, 'y': 4, 'ui': True, 'line': 5},
            {'op': 'move', 'x': 0, 'y': 0, 'ui': True, 'line': 6},
            {'op': 'key', 'key': 'ESCAPE', 'line': 7},
            {'op': 'key', 'key': 'A', 'line': 8},
            {'op': 'type', 'text': 'Hello  World 2', 'line': 9},
            {'op': 'shot', 'name': '01_start', 'line': 10},
            {'op': 'checkpoint', 'name': 'start', 'line': 11},
            {'op': 'quit', 'line': 12},
        ])

    def test_errors_name_the_line(self):
        cases = [
            ('wait\n', 'line 1: wait needs one frame count'),
            ('wait -1\n', 'line 1: wait needs one frame count'),
            ('wait 1 2\n', 'line 1: wait needs one frame count'),
            ('\nclick 1\n', 'line 2: click needs X Y (non-negative integers)'),
            ('rclick a b\n', 'line 1: rclick needs X Y (non-negative integers)'),
            ('move 1 -2\n', 'line 1: move needs X Y (non-negative integers)'),
            ('key\n', 'line 1: key needs one key name'),
            ('key NOPE\n', "line 1: unknown key 'NOPE'"),
            ('key !\n', "line 1: unknown key '!'"),
            ('type\n', 'line 1: type needs text'),
            ('type a-b\n', "line 1: type supports letters, digits and spaces, not '-'"),
            ('shot\n', 'line 1: shot needs one name'),
            ('shot a/b\n', 'line 1: shot name must not contain a path'),
            ('shot c:x\n', 'line 1: shot name must not contain a path'),
            ('quit now\n', 'line 1: quit takes no arguments'),
            ('checkpoint\n', 'line 1: checkpoint needs one name'),
            ('checkpoint a b\n', 'line 1: checkpoint needs one name'),
            ('checkpoint a/b\n',
             'line 1: checkpoint name must be letters, digits, _ or -'),
            ('checkpoint a.b\n',
             'line 1: checkpoint name must be letters, digits, _ or -'),
            ('checkpoint \u00e9\n',
             'line 1: checkpoint name must be letters, digits, _ or -'),
            ('checkpoint a\nwait 1\ncheckpoint a\n',
             "line 3: duplicate checkpoint 'a' (first on line 1)"),
            ('checkpoint Chargen\ncheckpoint CHARGEN\n',
             "line 2: duplicate checkpoint 'CHARGEN' (first on line 1)"),
            ('origin\n', 'line 1: origin needs ui or screen'),
            ('origin top\n', 'line 1: origin needs ui or screen'),
            ('wait 1\n\n# x\njump 1\n', "line 4: unknown command 'jump'"),
        ]
        for text, message in cases:
            with self.assertRaises(realinput.ScenarioError, msg=text) as cm:
                realinput.parse_scenario(text)
            self.assertEqual(str(cm.exception), message)

    def test_shipped_scenarios_parse(self):
        pending = glob.glob(os.path.join(ROOT, 'scenarios', 'pending',
                                         '*.txt'))
        for path in scenario_files() + pending:
            self.assertTrue(realinput.load_scenario(path), path)

    def test_world_entry_checkpoints(self):
        actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'pending', 'world_entry.txt'))
        names = [a['name'] for a in actions if a['op'] == 'checkpoint']
        chargen = [a['name'] for a in realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'chargen_replay.txt')) if a['op'] == 'checkpoint']
        self.assertEqual(names, chargen + [
            'skills_done', 'feats_done', 'named', 'party_formation',
            'chapter', 'world', 'world_settled'])
        self.assertEqual(actions[-1]['op'], 'quit')

    def test_keysym_table_covers_every_key_name(self):
        self.assertEqual(set(realinput.X_KEYSYM), set(realinput.KEY_VK))
        used = set()
        for path in scenario_files():
            used |= {a['key'] for a in realinput.load_scenario(path)
                     if a['op'] == 'key'}
        self.assertIn('ESCAPE', used)
        for name in used:
            self.assertTrue(realinput.keysym_for_name(name), name)
        self.assertEqual(realinput.keysym_for_name('ESCAPE'), 'Escape')
        self.assertEqual(realinput.keysym_for_name('return'), 'Return')
        self.assertEqual(realinput.keysym_for_name('SPACE'), 'space')
        self.assertEqual(realinput.keysym_for_name('BACK'), 'BackSpace')
        self.assertEqual(realinput.keysym_for_name('F12'), 'F12')
        self.assertEqual(realinput.keysym_for_name('q'), 'q')
        self.assertEqual(realinput.keysym_for_name('7'), '7')
        self.assertIsNone(realinput.keysym_for_name('?'))

    def test_vk_names_match_script_core_cpp(self):
        # Every name in platform/script_core.cpp's kNamedKeys, and no other.
        with open(os.path.join(ROOT, 'platform', 'script_core.cpp')) as f:
            text = f.read()
        names = set(re.findall(r'\{ "([A-Z0-9_]+)", VK_', text))
        self.assertEqual(names, set(realinput.KEY_VK))
        self.assertEqual(realinput.vk_for_name('escape'), 0x1B)
        self.assertEqual(realinput.vk_for_name('z'), ord('Z'))
        self.assertEqual(realinput.vk_for_name('NOPE'), 0)

    def test_windows_actions(self):
        actions = realinput.parse_scenario('key LEFT\ntype aB 1\nclick 5 6\n')
        out = realinput.windows_actions(actions)
        self.assertEqual(out[0], {'op': 'key', 'line': 1, 'key': 'LEFT',
                                  'vk': 0x25, 'extended': True,
                                  'shift': False})
        self.assertEqual([(a['vk'], a['shift']) for a in out[1:5]],
                         [(0x41, False), (0x42, True), (0x20, False),
                          (0x31, False)])
        self.assertEqual(out[5], {'op': 'click', 'x': 5, 'y': 6, 'line': 3})
        json.dumps(out)  # serialisable
        # Window pixels: the centred UI, times the scale; origin dropped.
        g = realinput.resolve_screen(2560, 1440, 2, (2560, 1440))
        out = realinput.windows_actions(realinput.parse_scenario(
            'click 5 6\norigin screen\nclick 5 6\n'), g)
        self.assertEqual(out, [
            {'op': 'click', 'x': (240 + 5) * 2, 'y': (60 + 6) * 2, 'line': 1},
            {'op': 'click', 'x': 10, 'y': 12, 'line': 3}])
        # driver.ps1 knows no checkpoint; it is dropped like origin.
        out = realinput.windows_actions(realinput.parse_scenario(
            'checkpoint a\nwait 2\n'))
        self.assertEqual(out, [{'op': 'wait', 'frames': 2, 'line': 2}])
        self.assertEqual(realinput.describe(
            {'op': 'checkpoint', 'name': 'a'}), 'checkpoint a')

    def test_origin_and_window_points(self):
        actions = realinput.parse_scenario(
            'origin screen\nclick 1 2\norigin UI\nmove 3 4\n')
        self.assertEqual(actions[0], {'op': 'origin', 'ui': False, 'line': 1})
        self.assertFalse(actions[1]['ui'])
        self.assertTrue(actions[3]['ui'])
        click = {'op': 'click', 'x': 645, 'y': 410, 'ui': True}
        self.assertEqual(realinput.window_point(click, None), (645, 410))
        for size, scale, want in (
                ((800, 600), 1, (645, 410)),
                ((1024, 768), 1, (757, 494)),
                ((1366, 768), 1, (928, 494)),
                ((1920, 1080), 1, (1205, 650)),
                ((1280, 720), 2, (1770, 940)),
                ((1280, 720), 3, (2655, 1410)),
                ((1600, 1200), 1, (1290, 820)),
                ((2048, 1536), 1, (1514, 988))):
            g = {'width': size[0], 'height': size[1], 'scale': scale}
            self.assertEqual(realinput.window_point(click, g), want,
                             (size, scale))
        screen = dict(click, ui=False)
        self.assertEqual(realinput.window_point(
            screen, {'width': 1280, 'height': 720, 'scale': 2}), (1290, 820))

    def test_resolve_screen_matches_screen_layout(self):
        r = realinput.resolve_screen
        self.assertEqual(r(800, None, 1, (1920, 1080))['height'], 600)
        self.assertEqual(r(2048, None, 1, (1920, 1080))['height'], 1536)
        self.assertEqual(r(1366, None, 1, (1920, 1080))['height'], 768)
        self.assertEqual(r('desktop', None, 1, (1920, 1080))['width'], 1920)
        self.assertEqual(r(1920, 1080, 'auto', (1920, 1080))['scale'], 1)
        self.assertEqual(r(2560, 1440, 'auto', (1920, 1080))['scale'], 2)
        self.assertEqual(r(3840, 2160, 'auto', (1920, 1080))['scale'], 3)
        self.assertEqual(r(1366, 768, 'auto', (1920, 1080))['scale'], 1)
        with self.assertRaises(ValueError):
            r(640, 480, 1, (1920, 1080))
        with self.assertRaises(ValueError):
            r(1920, 1080, 2, (1920, 1080))

    def test_world_scale_geometry(self):
        r = realinput.resolve_screen
        # World Scale absent: as ever.
        g = r(2560, 1440, 2, (2560, 1440))
        self.assertEqual((g['scale'], g['ui_scale'], g['world_scale'],
                          g['world_ui_scale'], g['full_ui_scale']),
                         (2, 2, 2, 2, 2))
        # Scale 2, World Scale 1 at 1920x1080: the engine sees the output;
        # the world screen's panels at 2, the 800x600 screens at 1 (600
        # rows do not fit 1080 at 2).
        g = r(1920, 1080, 2, (1920, 1080), '1')
        self.assertEqual((g['width'], g['height'], g['scale'],
                          g['ui_scale'], g['world_ui_scale'],
                          g['full_ui_scale'], g['output_width']),
                         (1920, 1080, 1, 2, 2, 1, 1920))
        self.assertEqual(r(2560, 1440, 'auto', (2560, 1440),
                           'auto')['world_scale'], 1)
        self.assertEqual(r(1920, 1080, 1, (1920, 1080), 'auto')['scale'], 1)
        self.assertEqual(r(3840, 2160, 2, (3840, 2160), '')['scale'], 2)
        # World Scale above Scale is clamped.
        self.assertEqual(r(1920, 1080, 1, (1920, 1080), '2')['scale'], 1)
        with self.assertRaises(ValueError):
            r(1920, 1080, 4, (1920, 1080), '2')
        with self.assertRaises(ValueError):
            r(1920, 1080, 2, (1920, 1080), '5')
        # The clamps: 1920x1080 at 3/1 shows the world panels at 2;
        # 1366x768 at 2/1 everything at 1.
        g31 = r(1920, 1080, 3, (1920, 1080), '1')
        self.assertEqual((g31['world_ui_scale'], g31['full_ui_scale']),
                         (2, 1))
        g1366 = r(1366, 768, 2, (1920, 1080), '1')
        self.assertEqual((g1366['world_ui_scale'], g1366['full_ui_scale']),
                         (1, 1))
        click = {'op': 'click', 'x': 645, 'y': 410, 'ui': True}
        # At 1920x1080 2/1 the 800x600 screens are shown at 1, centred.
        self.assertEqual(realinput.window_point(click, g),
                         (560 + 645, 240 + 410))
        self.assertEqual(realinput.window_point(click, g1366),
                         ((1366 - 800) // 2 + 645, (768 - 600) // 2 + 410))
        # 2560x1440 at 2/1: centred at 2x.
        g2 = r(2560, 1440, 2, (2560, 1440), '1')
        self.assertEqual(g2['full_ui_scale'], 2)
        self.assertEqual(realinput.window_point(click, g2),
                         (480 + 645 * 2, 120 + 410 * 2))
        # 1600x1200 at 2/1 (the original height): the UI fills the window.
        g16 = r(1600, None, 2, (1920, 1080), '1')
        self.assertEqual((g16['height'], g16['full_ui_scale']), (1200, 2))
        self.assertEqual(realinput.window_point(click, g16),
                         (645 * 2, 410 * 2))
        # A crop: a 1100-high window at 2x shows 550 of the 600 rows.
        gc = r(1920, 1100, 2, (1920, 1100), '1')
        self.assertEqual(gc['full_ui_scale'], 1)
        gc = dict(gc, full_ui_scale=2)
        with self.assertRaises(ValueError):
            realinput.window_point(dict(click, y=10), gc)
        # Screen points are world points.
        self.assertEqual(realinput.window_point(
            dict(click, ui=False), g), (645, 410))
        # play.py takes Scale and World Scale from --set.
        g = play.vm_geometry(1920, 1, ['Program Options/Scale=2',
                                       'Program Options/World Scale=1'])
        self.assertEqual((g['ui_scale'], g['world_scale'], g['width']),
                         (2, 1, 1920))
        self.assertEqual(play.vm_geometry(2560, 2, [])['ui_scale'], 2)

    def test_describe_round_trips(self):
        text = ('wait 5\nclick 1 2\norigin screen\nmove 3 4\nkey TAB\n'
                'type ab\nshot x\nquit\n')
        actions = realinput.parse_scenario(text)
        self.assertEqual('\n'.join(realinput.describe(a) for a in actions)
                         + '\n', text)


class XvfbDisplayTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.x11 = os.path.join(self._tmp.name, '.X11-unix')
        self.locks = self._tmp.name
        os.makedirs(self.x11)

    def pick(self, **kw):
        return realinput.pick_display(x11_dir=self.x11, lock_dir=self.locks,
                                      abstract=kw.pop('abstract', set()),
                                      **kw)

    def touch(self, path):
        open(path, 'w').close()

    def test_first_free_from_90(self):
        self.assertEqual(self.pick(), 90)

    def test_skips_sockets_locks_taken_and_abstract(self):
        self.touch(os.path.join(self.x11, 'X90'))
        self.touch(os.path.join(self.locks, '.X91-lock'))
        self.assertEqual(self.pick(), 92)
        self.assertEqual(self.pick(taken={92}), 93)
        self.assertEqual(self.pick(taken={92}, abstract={93}), 94)
        self.assertEqual(self.pick(start=10), 10)

    def test_none_free(self):
        with self.assertRaises(RuntimeError):
            self.pick(limit=2, taken={90, 91})

    def test_abstract_sockets_from_proc_net_unix(self):
        path = os.path.join(self._tmp.name, 'unix')
        with open(path, 'w') as f:
            f.write('Num RefCount Protocol Flags Type St Inode Path\n'
                    '0000: 00000002 00000000 00010000 0001 01 1 @/tmp/.X11-unix/X91\n'
                    '0000: 00000002 00000000 00010000 0001 01 2 /tmp/.X11-unix/X0\n'
                    '0000: 00000002 00000000 00010000 0001 01 3 @/tmp/.X11-unix/X100\n')
        self.assertEqual(realinput.abstract_x_sockets(path), {91, 100})
        self.assertEqual(realinput.abstract_x_sockets(path + '-missing'), set())

    def test_screen_and_cpu(self):
        self.assertEqual(realinput.job_cpu(0, {3, 1}), 1)
        self.assertEqual(realinput.job_cpu(3, {3, 1}), 3)


class InstallViewTest(unittest.TestCase):
    def test_view_links_the_install_and_keeps_writes_private(self):
        with tempfile.TemporaryDirectory() as tmp:
            install = os.path.join(tmp, 'install')
            for d in ('Data', 'cache/data', 'temp', 'TempSave'):
                os.makedirs(os.path.join(install, d))
            for f, data in (('CHITIN.KEY', 'k'), ('icewind2.ini', 'a=1'),
                            ('cache/data/x.bif', 'b'), ('temp/t', 't')):
                with open(os.path.join(install, f), 'w') as fh:
                    fh.write(data)
            view = realinput.make_install_view(install,
                                               os.path.join(tmp, 'view'))
            self.assertTrue(os.path.islink(os.path.join(view, 'Data')))
            self.assertTrue(os.path.islink(os.path.join(view, 'CHITIN.KEY')))
            for d in ('cache', 'temp', 'TempSave'):
                path = os.path.join(view, d)
                self.assertFalse(os.path.islink(path), d)
                self.assertEqual(os.listdir(path), [])
            ini = os.path.join(view, 'icewind2.ini')
            self.assertFalse(os.path.islink(ini))
            with open(ini, 'w') as fh:
                fh.write('changed')
            with open(os.path.join(install, 'icewind2.ini')) as fh:
                self.assertEqual(fh.read(), 'a=1')
            realinput.remove_install_view(view)
            self.assertFalse(os.path.exists(view))
            self.assertTrue(os.path.isfile(
                os.path.join(install, 'cache', 'data', 'x.bif')))
            self.assertEqual(sorted(os.listdir(install)),
                             ['CHITIN.KEY', 'Data', 'TempSave', 'cache',
                              'icewind2.ini', 'temp'])


class FakeGame:
    pid = 4242

    def __init__(self):
        self.code = None

    def poll(self):
        return self.code


class XDriverTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.calls = []
        self.lines = []
        self.game = FakeGame()
        self.driver = realinput.XDriver(
            7, self.game, self._tmp.name, self._tmp.name, self.lines.append,
            deadline=1e12, run=self.fake_run, sleep=lambda s: None)
        self.driver.window = 0x50

    def fake_run(self, cmd, timeout=30):
        self.calls.append(cmd)
        if cmd[:2] == ['xdotool', 'search']:
            return 0, '80\n'
        return 0, ''

    def xdo(self):
        return [c[1:] for c in self.calls if c[0] == 'xdotool'
                and c[1] != 'search']

    def test_click_is_real_pointer_input(self):
        self.driver.do({'op': 'click', 'x': 645, 'y': 262, 'line': 1})
        self.assertEqual(self.xdo(), [
            ['mousemove', '--window', '80', '645', '262'],
            ['mousedown', '1'], ['mouseup', '1']])

    def test_click_lands_on_the_centred_scaled_ui(self):
        self.driver.geometry = {'width': 1280, 'height': 720, 'scale': 2}
        self.driver.do({'op': 'click', 'x': 645, 'y': 262, 'ui': True,
                        'line': 1})
        self.assertEqual(self.xdo()[0],
                         ['mousemove', '--window', '80', '1770', '644'])

    def test_rclick_and_key(self):
        self.driver.window = 80
        self.driver.do({'op': 'rclick', 'x': 1, 'y': 2, 'line': 1})
        self.driver.do({'op': 'key', 'key': 'ESCAPE', 'line': 2})
        self.assertEqual(self.xdo()[1:], [
            ['mousedown', '3'], ['mouseup', '3'],
            ['keydown', 'Escape'], ['keyup', 'Escape']])

    def test_game_exit_ends_the_run(self):
        self.game.code = 0
        self.assertEqual(self.driver.run([{'op': 'wait', 'frames': 1,
                                           'line': 1}]), 'game exited')


class SandboxWsbTest(unittest.TestCase):
    def test_wsb_is_well_formed_with_the_mapped_folders(self):
        folders = [(r'C:\b', winsandbox.BOX_BIN, True),
                   (r'C:\Users\thier\iwd2r-vm\game', winsandbox.BOX_GAME,
                    False),
                   (r'C:\r & s', winsandbox.BOX_RESULTS, False),
                   (r'C:\t', winsandbox.BOX_TOOLS, True)]
        root = ET.fromstring(winsandbox.make_wsb(folders,
                                                 winsandbox.logon_command()))
        self.assertEqual(root.tag, 'Configuration')
        self.assertEqual(root.findtext('Networking'), 'Disable')
        self.assertEqual(root.findtext('vGPU'), 'Enable')
        self.assertEqual(root.findtext('MemoryInMB'), '4096')
        mapped = [(m.findtext('HostFolder'), m.findtext('SandboxFolder'),
                   m.findtext('ReadOnly'))
                  for m in root.find('MappedFolders')]
        self.assertEqual(mapped, [
            (r'C:\b', r'C:\iwd2r\bin', 'true'),
            (r'C:\Users\thier\iwd2r-vm\game', r'C:\iwd2r\game', 'false'),
            (r'C:\r & s', r'C:\iwd2r\results', 'false'),
            (r'C:\t', r'C:\iwd2r\tools', 'true')])
        command = root.find('LogonCommand').findtext('Command')
        self.assertIn(r'C:\iwd2r\tools\driver.ps1', command)
        self.assertIn('-ExecutionPolicy Bypass', command)

    def test_driver_config_and_inputs(self):
        cfg = winsandbox.driver_config(['A/B=1', 'C/D=2'], 180, 30, 'x')
        self.assertEqual(cfg['ini_override'], 'A/B=1;C/D=2')
        self.assertEqual(cfg['exe'], r'C:\iwd2r\bin\iwd2r.exe')
        self.assertEqual(cfg['actions'], r'C:\iwd2r\tools\actions.json')
        with tempfile.TemporaryDirectory() as tmp:
            winsandbox.write_inputs(tmp, cfg, [{'op': 'quit', 'line': 1}])
            self.assertEqual(sorted(os.listdir(tmp)),
                             ['actions.json', 'config.json', 'driver.ps1'])
            with open(os.path.join(tmp, 'driver.ps1')) as f, \
                    open(winsandbox.DRIVER) as src:
                self.assertEqual(f.read(), src.read())

    def test_start_batch(self):
        self.assertEqual(winsandbox.start_batch(r'C:\r\run.wsb'),
                         '@echo off\nstart "" '
                         '"C:\\Windows\\System32\\WindowsSandbox.exe" '
                         '"C:\\r\\run.wsb"\n')

    def test_available(self):
        self.assertFalse(winsandbox.available('/nonexistent/WindowsSandbox.exe'))
        self.assertIn('Containers-DisposableClientVM', winsandbox.ENABLE_HINT)


class SandboxCacheTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.src = os.path.join(self._tmp.name, 'install')
        self.cache = os.path.join(self._tmp.name, 'cache')
        os.makedirs(self.src)
        self.write(self.src, 'CHITIN.KEY', b'KEY1', 1000)

    def write(self, directory, name, data, mtime=None):
        os.makedirs(directory, exist_ok=True)
        path = os.path.join(directory, name)
        with open(path, 'wb') as f:
            f.write(data)
        if mtime is not None:
            os.utime(path, (mtime, mtime))

    def test_refresh_decision(self):
        self.assertTrue(winsandbox.cache_needs_refresh(self.src, self.cache))
        self.write(self.cache, 'chitin.key', b'KEY1', 1000)
        self.assertFalse(winsandbox.cache_needs_refresh(self.src, self.cache))
        self.write(self.cache, 'chitin.key', b'KEY1', 1001)
        self.assertTrue(winsandbox.cache_needs_refresh(self.src, self.cache))
        self.write(self.cache, 'chitin.key', b'KEY12', 1000)
        self.assertTrue(winsandbox.cache_needs_refresh(self.src, self.cache))

    def test_source_without_key_is_an_error(self):
        with self.assertRaises(FileNotFoundError):
            winsandbox.cache_needs_refresh(self.cache, self.src)

    def test_reset_inis_copies_from_the_install(self):
        self.write(self.src, 'icewind2.ini', b'orig')
        self.write(self.cache, 'icewind2.ini', b'changed by a run')
        self.assertEqual(winsandbox.reset_cache_inis(self.src, self.cache),
                         ['icewind2.ini'])
        with open(os.path.join(self.cache, 'icewind2.ini'), 'rb') as f:
            self.assertEqual(f.read(), b'orig')


def tiny_pe(dlls):
    """A minimal PE32+ image whose import directory names dlls."""
    data = bytearray(0x600)
    data[0:2] = b'MZ'
    struct.pack_into('<I', data, 0x3C, 0x40)
    data[0x40:0x44] = b'PE\0\0'
    struct.pack_into('<HH12xHH', data, 0x44, 0x8664, 1, 0xF0, 0x22)
    opt = 0x58
    struct.pack_into('<H', data, opt, 0x20B)
    struct.pack_into('<II', data, opt + 112 + 8, 0x1000, 20 * (len(dlls) + 1))
    section = opt + 0xF0
    data[section:section + 8] = b'.idata\0\0'
    struct.pack_into('<IIII', data, section + 8, 0x200, 0x1000, 0x200, 0x400)
    name_rva = 0x1100
    for i, dll in enumerate(dlls):
        struct.pack_into('<IIIII', data, 0x400 + 20 * i, 0, 0, 0, name_rva, 0)
        at = 0x400 + name_rva - 0x1000
        data[at:at + len(dll) + 1] = dll.encode() + b'\0'
        name_rva += 0x20
    return bytes(data)


class SandboxRuntimeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = self._tmp.name

    def test_pe_imports(self):
        path = os.path.join(self.dir, 'a.exe')
        with open(path, 'wb') as f:
            f.write(tiny_pe(['KERNEL32.dll', 'VCRUNTIME140.dll']))
        self.assertEqual(winsandbox.pe_imports(path),
                         ['KERNEL32.dll', 'VCRUNTIME140.dll'])
        with open(path, 'wb') as f:
            f.write(b'not a PE')
        with self.assertRaises(ValueError):
            winsandbox.pe_imports(path)

    def test_deploy_copies_only_the_imported_runtime(self):
        crt = os.path.join(self.dir, 'Redist', '14.40.1', 'x64',
                           'Microsoft.VC143.CRT')
        newer = os.path.join(self.dir, 'Redist', '14.44.2', 'x64',
                             'Microsoft.VC143.CRT')
        for d in (crt, newer):
            os.makedirs(d)
            for name in ('vcruntime140.dll', 'vcruntime140_1.dll',
                         'msvcp140.dll', 'concrt140.dll'):
                with open(os.path.join(d, name), 'w') as f:
                    f.write(d)
        self.assertEqual(winsandbox.find_redist(
            os.path.join(self.dir, 'Redist')), newer)
        out = os.path.join(self.dir, 'bin')
        os.makedirs(out)
        exe = os.path.join(out, 'iwd2r.exe')
        with open(exe, 'wb') as f:
            f.write(tiny_pe(['KERNEL32.dll', 'MSVCP140.dll',
                             'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll']))
        names = winsandbox.deploy_runtime(exe, lambda m: None,
                                          os.path.join(self.dir, 'Redist'))
        self.assertEqual(names, ['msvcp140.dll', 'vcruntime140.dll',
                                 'vcruntime140_1.dll'])
        self.assertEqual(sorted(os.listdir(out)),
                         ['iwd2r.exe', 'msvcp140.dll', 'vcruntime140.dll',
                          'vcruntime140_1.dll'])
        with open(exe, 'wb') as f:
            f.write(tiny_pe(['KERNEL32.dll']))
        self.assertEqual(winsandbox.deploy_runtime(exe, lambda m: None,
                                                   self.dir), [])


class LabelsTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.dir = self._tmp.name
        self.addCleanup(self._tmp.cleanup)

    def write(self, rel, text):
        path = os.path.join(self.dir, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(text)
        return path

    def test_ours_reads_method_after_address(self):
        self.write('src/IcewindMisc.cpp',
                   '#include "IcewindMisc.h"\n\n'
                   '// 0x5845D0\n'
                   'INT IcewindMisc::Roll(INT nRolls, INT nSides)\n'
                   '{\n    return 0;\n}\n')
        self.assertEqual(labels.ours(os.path.join(self.dir, 'src')),
                         [(0x5845D0, 'IcewindMisc::Roll')])

    def test_ours_skips_address_without_definition(self):
        self.write('src/a.cpp',
                   'void f()\n{\n'
                   '// 0x401000\n'
                   '\n'
                   '}\n'
                   'int g()\n{\n}\n')
        self.assertEqual(labels.ours(os.path.join(self.dir, 'src')), [])

    def test_ours_names(self):
        self.write('src/sub/b.cpp',
                   '// 0x401010\n'
                   '// NOTE: Inlined.\n'
                   'static int Helper(char* s)\n{\n}\n'
                   '// 0x401020\n'
                   'CFoo::~CFoo()\n{\n}\n'
                   '// 0x401030\n'
                   'BOOL CResRef::operator!=(const char* pName) const\n{\n}\n'
                   '// 0x8CF6E0\n'
                   'static const CString ScopeStr("::");\n'
                   '// 0x8BA28C\n'
                   'int CFoo::PRIME[] = {\n    1,\n};\n')
        self.assertEqual(labels.ours(os.path.join(self.dir, 'src')),
                         [(0x401010, 'Helper'), (0x401020, 'CFoo::~CFoo'),
                          (0x401030, 'CResRef::operator!=')])

    def test_ieex_skips_placeholders(self):
        path = self.write('function_names.db',
                          '0x401000 FUN_00401000\r\n'
                          '0x401090 CResWED_Construct\r\n')
        self.assertEqual(labels.ieex(path), [(0x401090, 'CResWED_Construct')])

    def test_extra_reads_names_and_skips_comments(self):
        path = self.write('extra_labels.txt',
                          '# how 0x547020 was identified\n'
                          '\n'
                          '0x547020 CRuleTables::GetIterativeAttackBonus\n'
                          '  0x4a42f0   CGameEffect::CheckSave  \n')
        self.assertEqual(labels.extra(path),
                         [(0x547020, 'CRuleTables::GetIterativeAttackBonus'),
                          (0x4A42F0, 'CGameEffect::CheckSave')])
        for bad in ('547020 NoPrefix\n', '0x547020\n', '0x1 a b\n'):
            path = self.write('extra_labels.txt', bad)
            with self.assertRaisesRegex(ValueError, 'extra_labels.txt:1'):
                labels.extra(path)

    def test_merge_ranks_ours_then_extra_then_ieex(self):
        ours = [(0x5845D0, 'IcewindMisc::Roll')]
        extra = [(0x5845D0, 'Extra::Loses'), (0x4A42F0, 'CGameEffect::CheckSave'),
                 (0x547020, 'Bar')]
        ieex = [(0x4A42F0, 'CGameEffect_CheckSave'), (0x403000, 'Bar'),
                (0x404000, 'Baz')]
        self.assertEqual(labels.merge(ours, ieex, extra),
                         [(0x403000, 'ieex_Bar'), (0x404000, 'Baz'),
                          (0x4A42F0, 'CGameEffect::CheckSave'),
                          (0x547020, 'Bar'),
                          (0x5845D0, 'IcewindMisc::Roll')])

    def test_committed_extra_labels_parse_and_are_not_ours(self):
        path = os.path.join(ROOT, 'tools', 'ghidra', 'extra_labels.txt')
        extra = labels.extra(path)
        self.assertTrue(extra)
        ours = {a for a, _ in labels.ours(os.path.join(ROOT, 'src'))}
        self.assertEqual([a for a, _ in extra if a in ours], [])
        self.assertEqual(len({a for a, _ in extra}), len(extra))

    def test_merge_prefers_ours_and_prefixes_collisions(self):
        ours = [(0x5845D0, 'IcewindMisc::Roll'), (0x401000, 'Foo')]
        ieex = [(0x5845D0, 'IcewindMisc_Roll'), (0x402000, 'Foo'),
                (0x403000, 'Bar')]
        self.assertEqual(labels.merge(ours, ieex),
                         [(0x401000, 'Foo'), (0x402000, 'ieex_Foo'),
                          (0x403000, 'Bar'), (0x5845D0, 'IcewindMisc::Roll')])


EVIDENCE_CHECKER = os.path.join(ROOT, 'tools', 'check_evidence.py')
TWO_FUNCTIONS = ('// test fixture\n'
                 '#include "a.h"\n'
                 '\n'
                 '// 0x401000\n'
                 'int A::f(int x)\n'
                 '{\n'
                 '    return x;\n'
                 '}\n'
                 '\n'
                 '// 0x401020\n'
                 'int A::g(int x)\n'
                 '{\n'
                 '    return x + 1;\n'
                 '}\n')


class CheckEvidenceTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = self._tmp.name
        self.addCleanup(self._tmp.cleanup)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Test')
        self.git('config', 'user.email', 'test@example.com')
        self.git('config', 'commit.gpgsign', 'false')
        self.write('src/a.cpp', TWO_FUNCTIONS)
        self.commit('base')
        self.base = self.git('rev-parse', 'HEAD').strip()

    def git(self, *args):
        return subprocess.run(['git', *args], cwd=self.repo, check=True,
                              capture_output=True, text=True).stdout

    def write(self, rel, text):
        path = os.path.join(self.repo, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(text)

    def commit(self, message='change'):
        self.git('add', '-A')
        self.git('commit', '-q', '--allow-empty', '-m', message)

    def check(self):
        return subprocess.run([sys.executable, '-I', EVIDENCE_CHECKER,
                               '--base', self.base],
                              cwd=self.repo, capture_output=True, text=True)

    def evidence(self, addr, extra):
        self.write(f'docs/evidence/{addr}.md',
                   '---\n'
                   f'address: 0x{addr}\n'
                   'name: A::f\n'
                   f'{extra}\n'
                   'ghidra_version: 12.1.4\n'
                   '---\n\n## Notes\n')

    def test_unchanged_passes(self):
        self.commit()
        self.assertEqual(self.check().returncode, 0)

    def test_body_change_without_evidence_fails(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('0x401000', r.stdout)
        self.assertNotIn('0x401020', r.stdout)

    def test_body_change_with_exemption_passes(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'exemption: "pure data"')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_evidence_without_case_or_exemption_fails(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'oracle_case:')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('0x401000', r.stdout)
        self.assertIn('oracle_case', r.stdout)

    def test_evidence_with_oracle_case_passes(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'oracle_case: rand/roll')
        self.commit()
        self.assertEqual(self.check().returncode, 0)

    def test_pure_deletion_inside_body_counts(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('    return x + 1;\n',
                                                      ''))
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('0x401020', r.stdout)

    def test_change_outside_functions_passes(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace(
            '#include "a.h"\n', '#include "a.h"\n#include "b.h"\n\n'
            'static int counter;\n').replace(
            '}\n\n// 0x401020', '}\n\nint between = 3;\n\n// 0x401020'))
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_header_change_passes(self):
        self.write('src/a.h', '// 0x401000\nint f()\n{\n    return 1;\n}\n')
        self.commit()
        self.assertEqual(self.check().returncode, 0)

    def test_deleted_function_ignored(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace(
            '\n// 0x401020\nint A::g(int x)\n{\n    return x + 1;\n}\n', ''))
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)

    def test_deleted_file_ignored(self):
        os.remove(os.path.join(self.repo, 'src/a.cpp'))
        self.commit()
        self.assertEqual(self.check().returncode, 0)

    def test_new_file_included(self):
        self.write('src/b.cpp', '// 0x402000\nvoid B::h()\n{\n}\n')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('0x402000', r.stdout)
        self.assertIn('src/b.cpp', r.stdout)

    def check_base(self, base):
        return subprocess.run([sys.executable, '-I', EVIDENCE_CHECKER,
                               '--base', base],
                              cwd=self.repo, capture_output=True, text=True)

    def test_unknown_base_skips(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.commit()
        r = self.check_base('1234567890abcdef1234567890abcdef12345678')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('cannot diff against', r.stderr)

    def test_unrelated_base_skips(self):
        branch = self.git('symbolic-ref', '--short', 'HEAD').strip()
        self.git('checkout', '-q', '--orphan', 'other')
        self.git('rm', '-rq', '--cached', '.')
        self.write('other.txt', 'x\n')
        self.git('add', 'other.txt')
        self.git('commit', '-q', '-m', 'unrelated')
        other = self.git('rev-parse', 'HEAD').strip()
        self.git('checkout', '-q', '-f', branch)
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.commit()
        r = self.check_base(other)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('cannot diff against', r.stderr)

    def test_path_with_space_and_non_ascii(self):
        self.write('src/sub dir/\u00e9t\u00e9.cpp',
                   '// 0x402000\nvoid B::h()\n{\n}\n')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('src/sub dir/\u00e9t\u00e9.cpp: 0x402000', r.stdout)

    def test_uncommitted_change_ignored(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.assertEqual(self.check().returncode, 0)


check_evidence = load_tool('check_evidence')
oracle = load_tool('oracle')

SCRIPT_OUTPUT = ('function\t0x5845D0\tIcewindMisc::Roll\n'
                 'caller\t0x45A000\tCGameSprite::Foo\t0x45A123\n'
                 'caller\t<none>\t<data>\t0x84E000\n'
                 'callee\t0x7B2A10\trand\n'
                 'code\n'
                 '\n'
                 'int __cdecl IcewindMisc::Roll(int param_1,int param_2)\n'
                 '\n'
                 '{\n'
                 '  return 0;\n'
                 '}\n'
                 '\n')

TWO_QUERIES = ('QUERY\tIcewindMisc::Roll\n' + SCRIPT_OUTPUT
               + 'QUERY\t0x4B1000\n'
               'function\t0x4B1000\tCRuleTables::GetXPCap\n'
               'caller\t0x4B2000\tCRuleTables::Bar\t0x4B2010\n'
               'code\n'
               'int CRuleTables::GetXPCap(void)\n'
               '{}\n')


class EvidenceFormatTest(unittest.TestCase):
    def test_front_matter(self):
        text = ('---\naddress: 0x5845D0\nname: IcewindMisc::Roll\n'
                'oracle_case:\nexemption: "pure data"\n'
                "quoted: 'x'\n---\n\n## Notes\nkey: not front matter\n")
        self.assertEqual(check_evidence.front_matter(text),
                         {'address': '0x5845D0', 'name': 'IcewindMisc::Roll',
                          'oracle_case': '', 'exemption': 'pure data',
                          'quoted': 'x'})

    def test_front_matter_missing(self):
        self.assertEqual(check_evidence.front_matter('# Title\n'), {})
        self.assertEqual(check_evidence.front_matter('---\na: 1\n'), {})

    def test_evidence_name(self):
        self.assertEqual(check_evidence.evidence_name(0x5845D0), '5845D0.md')

    def test_parse_script_output(self):
        parsed = oracle.parse_decompile(SCRIPT_OUTPUT)[0]
        self.assertEqual(parsed['matches'], [])
        self.assertEqual(parsed['function'], (0x5845D0, 'IcewindMisc::Roll'))
        self.assertEqual(parsed['callers'],
                         [('0x45A000', 'CGameSprite::Foo', '0x45A123'),
                          ('<none>', '<data>', '0x84E000')])
        self.assertEqual(parsed['callees'], [('0x7B2A10', 'rand')])
        self.assertEqual(parsed['code'],
                         'int __cdecl IcewindMisc::Roll(int param_1,'
                         'int param_2)\n\n{\n  return 0;\n}')

    def test_parse_ambiguous(self):
        parsed = oracle.parse_decompile(
            'match\t0x401000\tA::f\nmatch\t0x401020\tA::f\n')[0]
        self.assertEqual(parsed['matches'],
                         [('0x401000', 'A::f'), ('0x401020', 'A::f')])
        self.assertIsNone(parsed['function'])

    def test_render(self):
        text = oracle.render_evidence(
            oracle.parse_decompile(SCRIPT_OUTPUT)[0], '12.1.4')
        self.assertTrue(text.startswith(
            '---\naddress: 0x5845D0\nname: IcewindMisc::Roll\n'
            'oracle_case:\nghidra_version: 12.1.4\n---\n'))
        self.assertEqual(check_evidence.front_matter(text)['address'],
                         '0x5845D0')
        for heading in ('## Decompiled (Ghidra)', '## Callers',
                        '## Callees', '## Notes'):
            self.assertIn('\n' + heading + '\n', text)
        self.assertIn('```c\nint __cdecl IcewindMisc::Roll(int param_1,'
                      'int param_2)\n\n{\n  return 0;\n}\n```\n', text)
        self.assertIn('- 0x45A000 CGameSprite::Foo (call at 0x45A123)\n',
                      text)
        self.assertIn('- 0x84E000 (not in a function)\n', text)
        self.assertIn('- 0x7B2A10 rand\n', text)
        self.assertLess(text.index('## Callers'), text.index('## Callees'))

    def test_render_empty_lists(self):
        parsed = oracle.parse_decompile(
            'function\t0x401000\tf\ncode\n{}\n')[0]
        text = oracle.render_evidence(parsed, '12.1.4')
        self.assertIn('## Callers\n\nNone.\n', text)
        self.assertIn('## Callees\n\nNone.\n', text)

    def test_ghidra_version(self):
        self.assertEqual(oracle.ghidra_version(), '12.1.4')

    def test_parse_decompile_several_queries(self):
        entries = oracle.parse_decompile(TWO_QUERIES)
        self.assertEqual([e['query'] for e in entries],
                         ['IcewindMisc::Roll', '0x4B1000'])
        roll, cap = entries
        self.assertEqual(roll['function'], (0x5845D0, 'IcewindMisc::Roll'))
        self.assertEqual(roll['callers'],
                         [('0x45A000', 'CGameSprite::Foo', '0x45A123'),
                          ('<none>', '<data>', '0x84E000')])
        self.assertEqual(roll['callees'], [('0x7B2A10', 'rand')])
        self.assertEqual(roll['code'],
                         'int __cdecl IcewindMisc::Roll(int param_1,'
                         'int param_2)\n\n{\n  return 0;\n}')
        self.assertEqual(cap['function'], (0x4B1000, 'CRuleTables::GetXPCap'))
        self.assertEqual(cap['callers'],
                         [('0x4B2000', 'CRuleTables::Bar', '0x4B2010')])
        self.assertEqual(cap['callees'], [])
        self.assertEqual(cap['code'], 'int CRuleTables::GetXPCap(void)\n{}')

    def test_parse_decompile_query_kinds(self):
        entries = oracle.parse_decompile(
            'QUERY\tA::f\nmatch\t0x401000\tA::f\nmatch\t0x401020\tA::f\n'
            'QUERY\tNope::g\n')
        self.assertEqual(len(entries), 2)
        self.assertEqual(len(entries[0]['matches']), 2)
        self.assertIsNone(entries[0]['function'])
        self.assertEqual(entries[1]['query'], 'Nope::g')
        self.assertIsNone(entries[1]['function'])
        self.assertEqual(entries[1]['matches'], [])


class EvidenceCommandTest(unittest.TestCase):
    """oracle.py evidence FUNC...: one stubbed Ghidra run, a file each."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.dir = os.path.join(self._tmp.name, 'docs', 'evidence')
        self.calls = []

        def fake_decompile(queries):
            self.calls.append(list(queries))
            return self.output

        self.output = TWO_QUERIES
        for name, value in (('EVIDENCE_DIR', self.dir),
                            ('ROOT', self._tmp.name),
                            ('decompile', fake_decompile),
                            ('ghidra_version', lambda: '12.1.4')):
            old = getattr(oracle, name)
            setattr(oracle, name, value)
            self.addCleanup(setattr, oracle, name, old)

    def run_evidence(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = oracle.main(['evidence', *argv])
        return code, out.getvalue()

    def path(self, address):
        return os.path.join(self.dir, address + '.md')

    def test_evidence_writes_each_function(self):
        code, out = self.run_evidence('IcewindMisc::Roll', '0x4B1000')
        self.assertEqual(code, 0, out)
        self.assertEqual(self.calls, [['IcewindMisc::Roll', '0x4B1000']])
        with open(self.path('5845D0')) as f:
            self.assertIn('name: IcewindMisc::Roll\n', f.read())
        with open(self.path('4B1000')) as f:
            self.assertIn('name: CRuleTables::GetXPCap\n', f.read())

        # Without --force an existing file is kept and the run fails; with
        # it, each file is replaced.
        with open(self.path('5845D0'), 'w') as f:
            f.write('kept\n')
        os.remove(self.path('4B1000'))
        code, out = self.run_evidence('IcewindMisc::Roll', '0x4B1000')
        self.assertEqual(code, 1)
        self.assertIn('5845D0.md exists; --force replaces it', out)
        with open(self.path('5845D0')) as f:
            self.assertEqual(f.read(), 'kept\n')
        self.assertTrue(os.path.exists(self.path('4B1000')))
        code, out = self.run_evidence('IcewindMisc::Roll', '0x4B1000',
                                      '--force')
        self.assertEqual(code, 0, out)
        with open(self.path('5845D0')) as f:
            self.assertIn('name: IcewindMisc::Roll\n', f.read())

    def test_evidence_lists_ambiguous_and_missing(self):
        self.output = (TWO_QUERIES.split('QUERY\t0x4B1000')[0]
                       + 'QUERY\tA::f\nmatch\t0x401000\tA::f\n'
                       'match\t0x401020\tA::f\nQUERY\tNope::g\n')
        code, out = self.run_evidence('IcewindMisc::Roll', 'A::f', 'Nope::g')
        self.assertEqual(code, 1)
        self.assertIn('A::f names 2 functions; pass one address:', out)
        self.assertIn('  0x401020 A::f', out)
        self.assertIn('no function Nope::g in the Ghidra project', out)
        self.assertTrue(os.path.exists(self.path('5845D0')))
        self.assertEqual(sorted(os.listdir(self.dir)), ['5845D0.md'])


filelock = load_tool('filelock')


class FileLockTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.path = os.path.join(self._tmp.name, 'sub', 'x.lock')

    def test_hold_blocks_second_holder(self):
        import threading
        held = threading.Event()
        release = threading.Event()

        def holder():
            with filelock.hold(self.path, 'x'):
                held.set()
                release.wait(10)

        t = threading.Thread(target=holder)
        t.start()
        try:
            self.assertTrue(held.wait(10))
            with self.assertRaises(TimeoutError):
                with filelock.hold(self.path, 'x', timeout=0.2):
                    pass
        finally:
            release.set()
            t.join(10)
        with filelock.hold(self.path, 'x', timeout=0.2):
            pass

    def test_hold_reports_waiting(self):
        import threading
        held = threading.Event()
        release = threading.Event()

        def holder():
            with filelock.hold(self.path, 'ghidra'):
                held.set()
                release.wait(10)

        t = threading.Thread(target=holder)
        t.start()
        old = filelock.WAIT_NOTICE
        filelock.WAIT_NOTICE = 0.05
        err = io.StringIO()
        try:
            self.assertTrue(held.wait(10))
            with contextlib.redirect_stderr(err):
                with self.assertRaises(TimeoutError):
                    with filelock.hold(self.path, 'ghidra', timeout=0.3):
                        pass
        finally:
            filelock.WAIT_NOTICE = old
            release.set()
            t.join(10)
        self.assertEqual(err.getvalue(),
                         f'ghidra: waiting for {self.path}\n')

    def test_headless_holds_ghidra_lock(self):
        make_project = load_ghidra_tool('make_project')
        self.assertEqual(os.path.normpath(make_project.LOCK_PATH),
                         os.path.join(os.path.dirname(
                             make_project.PROJECT_DIR), 'iwd2.lock'))
        project = os.path.join(self._tmp.name, 'projects', 'iwd2')
        make_project.PROJECT_DIR = project
        make_project.LOCK_PATH = self.path
        seen = []

        def fake_run(cmd, **kwargs):
            try:
                with filelock.hold(self.path, 'test', timeout=0):
                    seen.append('free')
            except TimeoutError:
                seen.append('held')
            return subprocess.CompletedProcess(cmd, 0, '', '')

        make_project.subprocess = types.SimpleNamespace(
            run=fake_run, CompletedProcess=subprocess.CompletedProcess)
        make_project.headless(['-x'], 'log')
        self.assertEqual(seen, ['held'])
        with filelock.hold(self.path, 'test', timeout=0):
            pass


def load_test_script(name):
    spec = importlib.util.spec_from_file_location(
        name, os.path.join(ROOT, 'tests', name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


make_test_pe = load_test_script('make_test_pe')


class OracleSelftestTest(unittest.TestCase):
    """tools/oracle.py run --selftest: the checks on the runner's output,
    against the PE that tests/make_test_pe.py writes (base 0x10000000)."""

    def setUp(self):
        self.pe = make_test_pe.build()

    def test_file_offset(self):
        self.assertEqual(oracle.file_offset(self.pe, 0x10001000), 0x200)
        self.assertEqual(oracle.file_offset(self.pe, 0x10002048), 0x448)
        # Past .text's virtual size, and outside every section.
        self.assertIsNone(oracle.file_offset(self.pe, 0x10001010))
        self.assertIsNone(oracle.file_offset(self.pe, 0x10005000))
        self.assertEqual(oracle.file_dword(self.pe, 0x10001000), 0xC3)

    def selftest(self, lines):
        # check_selftest requires the 0x847000 line; the fixture has no such
        # address, so it is reported and filtered out here.
        results = oracle.check_selftest('\n'.join(lines) + '\n', self.pe)
        missing = [r for r in results if 'no mem line' in r[1]]
        self.assertEqual(len(missing), 1)
        return [r for r in results if r not in missing]

    def test_matching_output_passes(self):
        results = self.selftest([
            'mem 10001000 000000c3',
            'mem 10002048 77001234',
            'import 10002048 KERNEL32.dll!GetTickCount 77001234',
            'rand 41'])
        self.assertEqual([ok for ok, _ in results], [True, True, True])

    def test_wrong_bytes_fail(self):
        results = self.selftest(['mem 10001000 000000c4', 'rand 41'])
        self.assertEqual([ok for ok, _ in results], [False, True])

    def test_iat_slot_must_match_getprocaddress_and_name(self):
        for mem, imp in (
                # Resolved elsewhere than GetProcAddress says.
                ('77001234', 'KERNEL32.dll!GetTickCount 77009999'),
                # The file's slot names another function.
                ('77001234', 'KERNEL32.dll!Sleep 77001234'),
                # Unresolved (GetProcAddress gave nothing).
                ('00000000', 'KERNEL32.dll!GetTickCount 00000000'),
                # Never patched: still the file's hint/name RVA.
                ('000020a0', 'KERNEL32.dll!GetTickCount 77001234')):
            results = self.selftest(['mem 10002048 ' + mem,
                                     'import 10002048 ' + imp, 'rand 41'])
            self.assertFalse(results[0][0], (mem, imp))

    def test_rand_must_be_41(self):
        for lines in (['rand 42'], []):
            results = self.selftest(lines)
            self.assertEqual(results[-1][0], False)

    def test_missing_rdata_line_fails(self):
        results = oracle.check_selftest('rand 41\n', self.pe)
        self.assertFalse(all(ok for ok, _ in results))

    def test_evaluate_run(self):
        self.assertEqual(oracle.evaluate_run(0, None, 0, ''),
                         (True, 'exit code 0'))
        self.assertFalse(oracle.evaluate_run(0, None, 4, '')[0])
        self.assertFalse(oracle.evaluate_run(6, 'refusing', 6, 'other')[0])
        self.assertTrue(oracle.evaluate_run(6, 'refusing', 6,
                                            'oracle: refusing to patch')[0])

    def test_run_patterns_match_runner_messages(self):
        runs = {name: pattern for name, _, _, _, pattern
                in oracle.SELFTEST_RUNS}
        self.assertEqual(list(runs), ['selftest', 'wrong WinMain call site',
                                      'wrong WinMain target', 'timeout',
                                      'range pre-occupied', 'wrong MD5'])
        messages = {
            'wrong WinMain call site':
                'oracle: refusing to patch the WinMain call: byte at '
                '0x007E9B44 is 0xA8, not a call (0xE8)',
            'timeout':
                'oracle: timed out after 3 s; the runner child was killed',
            'wrong WinMain target':
                'oracle: refusing to patch the WinMain call: call at '
                '0x007E9B43 targets 0x004218F0, expected WinMain at '
                '0x00421900',
            'range pre-occupied':
                'oracle: cannot reserve 0x00400000-0x00A21000 for iwd2.exe: '
                'occupied by private memory at 0x00400000-0x00401000 '
                '(allocation base 0x00400000)',
            'wrong MD5':
                'oracle: C:\\x\\iwd2r_oracle.exe has MD5 '
                '396ba0b7eaf32b3081f6d5d3de720d39; the oracle needs the '
                'vanilla iwd2.exe (200f038bec3ed50a964d68b3f0f1f0e7)',
        }
        for name, message in messages.items():
            self.assertRegex(message, runs[name])

    def test_vanilla_json(self):
        with open(oracle.VANILLA_JSON) as f:
            vanilla = json.load(f)
        self.assertEqual(vanilla['md5'], oracle.vanilla_md5())
        self.assertRegex(vanilla['md5'], r'^[0-9a-f]{32}$')
        for key in ('winmain_call_site', 'winmain', 'srand', 'rand',
                    'getptd', 'crt_heap'):
            self.assertRegex(vanilla[key], r'^0x[0-9A-F]+$', key)
            self.assertIn(key, vanilla['notes'])
        ranges = [(int(a, 16), int(b, 16)) for a, b in vanilla['crt_ranges']]
        self.assertEqual(ranges, sorted(ranges))
        for (_, end), (next_start, _) in zip(ranges, ranges[1:]):
            self.assertLessEqual(end, next_start)
        for start, end in ranges:
            # Inside .data (0x8A5000-0xA1AE81).
            self.assertLess(start, end)
            self.assertGreaterEqual(start, 0x8A5000)
            self.assertLessEqual(end, 0xA1AE81)
        self.assertEqual(len(vanilla['notes']['crt_ranges']), len(ranges))
        # _holdrand: offset 0x14 of _tiddata (0x74 bytes).
        self.assertEqual(vanilla['tiddata_rand'], ['0x14', '0x18'])
        self.assertIn('tiddata_rand', vanilla['notes'])
        # _crtheap lies inside a CRT range, so restores never touch it.
        heap = int(vanilla['crt_heap'], 16)
        self.assertTrue(any(start <= heap and heap + 4 <= end
                            for start, end in ranges))


def oracle_case(name, expect='pass', failures=0, crashed=False,
                kind='fault'):
    return {'case': name, 'expect': expect, 'iterations': 10,
            'passed': not failures and not crashed,
            'failure_count': failures,
            'failures': [{'inputs': '1', 'original': '41', 'ours': '42'}]
            * min(failures, 1),
            'crashed': crashed,
            'crash': {'inputs': 'x', 'exception': '0xC0000005',
                      'address': '0x00000010', 'kind': kind, 'type': ''}
            if crashed else None}


def oracle_report(cases, complete=True, running=None):
    return {'file': 'f', 'complete': complete, 'running': running,
            'seed': 1, 'iterations': 100, 'cases': cases}


class OracleReportTest(unittest.TestCase):
    """tools/oracle.py run: merging the runner's per-file reports."""

    SELFTEST = [oracle_case('selftest/isolation'),
                oracle_case('selftest/crash', 'crash', crashed=True),
                oracle_case('selftest/must_fail', 'fail', failures=100)]

    def merge(self, *runs):
        return oracle.merge_reports(
            1, 100, [run if len(run) == 4 else run + (None,) for run in runs])

    def test_case_ok(self):
        self.assertTrue(oracle.case_ok(oracle_case('a')))
        self.assertFalse(oracle.case_ok(oracle_case('a', failures=1)))
        self.assertFalse(oracle.case_ok(oracle_case('a', crashed=True)))
        self.assertTrue(oracle.case_ok(oracle_case('a', 'fail', failures=1)))
        self.assertFalse(oracle.case_ok(oracle_case('a', 'fail')))
        self.assertFalse(oracle.case_ok(oracle_case('a', 'fail', failures=1,
                                                    crashed=True)))
        self.assertTrue(oracle.case_ok(oracle_case('a', 'crash',
                                                   crashed=True)))
        # A declared crash with a mismatch beside it, or one that left the
        # heap corrupt, is not what was declared.
        self.assertFalse(oracle.case_ok(oracle_case('a', 'crash', failures=1,
                                                    crashed=True)))
        self.assertFalse(oracle.case_ok(oracle_case(
            'a', 'crash', crashed=True, kind=oracle.HEAP_CORRUPT)))
        self.assertTrue(oracle.case_ok(oracle_case(
            'a', 'crash', crashed=True, kind='original C++ throw')))
        self.assertFalse(oracle.case_ok(oracle_case('a', 'crash')))

    def test_selftest_passes_with_expected_lists(self):
        merged = self.merge(('selftest', 0, oracle_report(self.SELFTEST)))
        self.assertTrue(merged['passed'])
        self.assertEqual(merged['expected_failures'], ['selftest/must_fail'])
        self.assertEqual(merged['expected_crashes'], ['selftest/crash'])
        self.assertEqual(merged['unexpected'], [])
        self.assertEqual([c['file'] for c in merged['cases']],
                         ['selftest'] * 3)
        self.assertIn('run PASSED', oracle.summary_lines(merged)[-1])

    def test_must_fail_passing_fails_the_run(self):
        cases = self.SELFTEST[:2] + [oracle_case('selftest/must_fail',
                                                 'fail')]
        merged = self.merge(('selftest', 7, oracle_report(cases)))
        self.assertFalse(merged['passed'])
        self.assertEqual(merged['unexpected'], ['selftest/must_fail'])
        self.assertEqual(merged['expected_failures'], [])
        lines = oracle.summary_lines(merged)
        self.assertIn('PASSED (expected fail)', '\n'.join(lines))
        self.assertIn('run FAILED', lines[-1])

    def test_unexpected_failure_or_crash_fails_the_run(self):
        for case in (oracle_case('rand/x', failures=3),
                     oracle_case('rand/x', crashed=True)):
            merged = self.merge(('rand', 7, oracle_report([case])))
            self.assertFalse(merged['passed'])
            self.assertEqual(merged['unexpected'], ['rand/x'])

    def test_missing_report_or_timeout_fails_the_run(self):
        merged = self.merge(('selftest', 0, oracle_report(self.SELFTEST)),
                            ('rand', 3221225477, None), ('ui', None, None))
        self.assertFalse(merged['passed'])
        self.assertEqual(merged['unexpected'], ['rand', 'ui'])
        self.assertEqual([f['ok'] for f in merged['files']],
                         [True, False, False])
        self.assertEqual(merged['files'][2]['error'], 'no report (timed out)')

    def test_incomplete_report_keeps_cases_and_names_the_running_one(self):
        report = oracle_report(self.SELFTEST[:1], complete=False,
                               running='selftest/crash')
        for code, died, ended in (
                (11, '0xC0000374 (heap corruption)',
                 'the process died with 0xC0000374 (heap corruption)'),
                (10, None, 'timed out')):
            merged = self.merge(('selftest', code, report, died))
            self.assertFalse(merged['passed'])
            self.assertEqual([c['case'] for c in merged['cases']],
                             ['selftest/isolation', 'selftest/crash'])
            self.assertEqual(merged['unexpected'], ['selftest/crash',
                                                    'selftest'])
            self.assertEqual(merged['files'][0]['error'],
                             'incomplete report (%s)' % ended)
            self.assertIn(ended, merged['cases'][1]['crash']['kind'])

    def test_read_report_rejects_bad_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'r.json')
            self.assertIsNone(oracle.read_report(path))
            for text in ('{"cases": [', '[]', '{"cases": 3}'):
                with open(path, 'w') as f:
                    f.write(text)
                self.assertIsNone(oracle.read_report(path), text)
            with open(path, 'w') as f:
                json.dump(oracle_report([]), f)
            self.assertEqual(oracle.read_report(path)['cases'], [])

    def test_pending_cases_are_listed_without_failing(self):
        cases = [oracle_case('tables/getat'),
                 dict(oracle_case('tables/load'), iterations=0,
                      pending='needs engine state'),
                 dict(oracle_case('pending/save'),
                      pending='ours not implemented')]
        merged = self.merge(('tables', 0, oracle_report(cases)))
        self.assertTrue(merged['passed'])
        self.assertEqual(merged['pending'],
                         ['tables/load: needs engine state',
                          'pending/save: ours not implemented'])
        lines = oracle.summary_lines(merged)
        self.assertIn('tables/load                      pending: needs engine '
                      'state, 0 inputs', lines[1])
        self.assertIn('passed, pending: ours not implemented', lines[2])
        self.assertEqual(lines[-1], '1 passed, 0 expected failures or '
                         'crashes, 2 pending, 0 unexpected; run PASSED')

    def test_a_pending_case_whose_body_ran_nothing_fails_the_run(self):
        listed = dict(oracle_case('pending/hit'), iterations=0,
                      has_body=False, pending='needs engine state')
        empty = dict(oracle_case('pending/roll', failures=1), iterations=0,
                     has_body=True, pending='ours not implemented')
        # Even without the runner's own failure, 0 inputs is not a pass.
        quiet = dict(oracle_case('pending/quiet'), iterations=0,
                     has_body=True, pending='ours not implemented')
        merged = self.merge(('pending', 7, oracle_report([listed, empty,
                                                          quiet])))
        self.assertFalse(merged['passed'])
        self.assertEqual(merged['pending'],
                         ['pending/hit: needs engine state'])
        self.assertEqual(merged['unexpected'], ['pending/roll',
                                                'pending/quiet'])
        lines = oracle.summary_lines(merged)
        self.assertIn('pending/hit                      pending: needs '
                      'engine state, 0 inputs', lines[0])
        text = '\n'.join(lines)
        self.assertIn('pending/roll                     PENDING (ran 0 '
                      'inputs): ours not implemented, 0 inputs, 1 failures',
                      text)
        self.assertIn('pending/quiet                    PENDING (ran 0 '
                      'inputs)', text)
        self.assertEqual(lines[-1], '0 passed, 0 expected failures or '
                         'crashes, 1 pending, 2 unexpected; run FAILED')

    def test_a_failing_pending_case_fails_the_run(self):
        case = dict(oracle_case('pending/save', failures=1),
                    pending='ours not implemented')
        merged = self.merge(('pending', 7, oracle_report([case])))
        self.assertFalse(merged['passed'])
        self.assertEqual(merged['pending'], [])
        self.assertEqual(merged['unexpected'], ['pending/save'])

    def test_extract_tables_exits_with_the_reason(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(SystemExit) as e:
                oracle.extract_tables(tmp, os.path.join(tmp, 'out'))
            self.assertIn('no CHITIN.KEY', str(e.exception))

    def test_exit_code_must_agree(self):
        merged = self.merge(('selftest', 7, oracle_report(self.SELFTEST)))
        self.assertFalse(merged['passed'])
        self.assertIn('exit code 7', merged['files'][0]['error'])


VANILLA_BYTES = b'MZ vanilla iwd2.exe'
VANILLA_TEST_MD5 = 'fcc0314ffee2d07c22e20be35c5b7023'
WEIDU_LOG = ('// Log of Currently Installed WeiDU Mods\r\n'
             '// The top of the file is the \'oldest\' mod\r\n'
             '~IWD2EE/IWD2EE.TP2~ #0 #1000 // IWD2EE Core\r\n'
             '~IWD2EE/IWD2EE.TP2~ #0 #0 // Class Revisions\r\n'
             '~IWD2EE/IWD2EE.TP2~ #0 #96 // Add PnP Wizard Spell Learning\r\n')
INI = ('[Alias]\r\n'
       'HD0:=C:\\Games\\Icewind Dale II\\\r\n'
       'CD1:=C:\\Games\\Icewind Dale II\\Data\\\r\n'
       'CD2:=c:\\games\\icewind dale ii\\CD2\\\r\n'
       '[Program Options]\r\n'
       'Resolution=1366\r\n'
       'Last Resolution=1366x768\r\n')


class OracleVanillaTest(unittest.TestCase):
    """tools/oracle.py vanilla: the checks on the vanilla copy."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.game = self.tmp.name
        locks = tempfile.TemporaryDirectory()
        self.addCleanup(locks.cleanup)
        old = oracle.VANILLA_LOCK
        oracle.VANILLA_LOCK = os.path.join(locks.name, 'vanilla.lock')
        self.addCleanup(setattr, oracle, 'VANILLA_LOCK', old)
        self.write('CHITIN.KEY', b'KEY V1  ')
        self.write('iwd2.exe', VANILLA_BYTES)
        self.write('WeiDU.log', b'// Log of Currently Installed WeiDU Mods\r\n')
        self.write('icewind2.ini', oracle.rewrite_ini(
            INI, oracle.INSTALL_WIN, 'C:\\copy').encode())
        os.makedirs(os.path.join(self.game, 'Override'))

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, rel, data):
        path = os.path.join(self.game, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)

    def verify(self):
        return oracle.verify_vanilla(self.game, VANILLA_TEST_MD5)

    def test_md5_constant(self):
        import hashlib
        self.assertEqual(hashlib.md5(VANILLA_BYTES).hexdigest(),
                         VANILLA_TEST_MD5)

    def test_clean_copy_passes(self):
        self.assertEqual(self.verify(), [])

    def test_no_weidu_log_passes(self):
        os.remove(os.path.join(self.game, 'WeiDU.log'))
        self.assertEqual(self.verify(), [])

    def test_ieex_dll_fails(self):
        self.write('IEex.dll', b'x')
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('IEex.dll', failures[0])

    def test_ieex_anywhere_fails(self):
        self.write('Override/IEex_Core.lua', b'x')
        self.write('Override/ieexhelper.DLL', b'x')
        failures = self.verify()
        self.assertEqual(len(failures), 2)
        self.assertTrue(any('IEex_Core.lua' in f for f in failures))
        self.assertTrue(any('ieexhelper.DLL' in f for f in failures))

    def test_weidu_log_listing_iwd2ee_fails(self):
        self.write('WeiDU.log', WEIDU_LOG.encode())
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('WeiDU.log', failures[0])
        self.assertIn('IWD2EE.TP2', failures[0])

    def test_wrong_exe_md5_fails(self):
        os.remove(os.path.join(self.game, 'iwd2.exe'))
        self.write('IWD2.exe', b'MZ patched by IWD2EE')
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('IWD2.exe', failures[0])
        self.assertIn('MD5', failures[0])

    def test_exe_name_is_case_insensitive(self):
        os.remove(os.path.join(self.game, 'iwd2.exe'))
        self.write('IWD2.EXE', VANILLA_BYTES)
        self.assertEqual(self.verify(), [])

    def test_missing_exe_fails(self):
        os.remove(os.path.join(self.game, 'iwd2.exe'))
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('iwd2.exe', failures[0])

    def test_missing_chitin_key_fails(self):
        os.remove(os.path.join(self.game, 'CHITIN.KEY'))
        self.assertEqual(len(self.verify()), 1)

    def test_ini_pointing_at_install_fails(self):
        self.write('icewind2.ini', INI.encode())
        failures = self.verify()
        self.assertTrue(failures)
        self.assertTrue(all('icewind2.ini' in f for f in failures))

    def test_any_ini_pointing_at_install_fails(self):
        self.write('ddraw.ini', b'path=c:\\GAMES\\Icewind Dale II\\x\r\n')
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('ddraw.ini', failures[0])

    def test_resolution_must_be_800(self):
        self.write('icewind2.ini', INI.replace(
            'C:\\Games\\Icewind Dale II', 'C:\\copy').replace(
            'c:\\games\\icewind dale ii', 'C:\\copy').encode())
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('Resolution', failures[0])

    def test_iwd2ee_leftovers_fail(self):
        for rel in ('IWD2EE.exe', 'Setup-iwd2ee.exe', 'SETUP-IWD2EE.DEBUG',
                    'iwd2ee/backup/1000/iwd2.exe'):
            self.write(rel, b'x')
        failures = self.verify()
        for rel in ('IWD2EE.exe', 'Setup-iwd2ee.exe', 'SETUP-IWD2EE.DEBUG',
                    'iwd2ee'):
            self.assertTrue(any(rel in f for f in failures), rel)

    def test_rewrite_ini(self):
        text = oracle.rewrite_ini(INI, oracle.INSTALL_WIN,
                                  'C:\\Users\\thier\\iwd2r-oracle\\game')
        self.assertEqual(text, (
            '[Alias]\r\n'
            'HD0:=C:\\Users\\thier\\iwd2r-oracle\\game\\\r\n'
            'CD1:=C:\\Users\\thier\\iwd2r-oracle\\game\\Data\\\r\n'
            'CD2:=C:\\Users\\thier\\iwd2r-oracle\\game\\CD2\\\r\n'
            '[Program Options]\r\n'
            'Resolution=800\r\n'
            'Last Resolution=1366x768\r\n'))

    def test_weidu_components(self):
        self.assertEqual(oracle.weidu_components(WEIDU_LOG),
                         [('IWD2EE/IWD2EE.TP2', 0, 1000),
                          ('IWD2EE/IWD2EE.TP2', 0, 0),
                          ('IWD2EE/IWD2EE.TP2', 0, 96)])

    def test_uninstall_bat(self):
        bat = oracle.uninstall_bat(
            'C:\\copy', 'Setup-iwd2ee.exe',
            oracle.weidu_components(WEIDU_LOG), 'C:\\log.txt')
        self.assertEqual(bat.splitlines(), [
            '@echo off',
            'cd /d "C:\\copy" || exit /b 1',
            '"Setup-iwd2ee.exe" --noautoupdate --no-exit-pause --skip-at-view '
            '--language 0 --log "C:\\log.txt" --force-uninstall-list '
            '1000 0 96 < nul',
            'exit /b %errorlevel%'])

    def test_leftovers(self):
        for rel in ('IEex.dll', 'IEexHelper.dll', 'IEexHelperGUI.dll',
                    'IEex.ini', 'IEex.log', 'IWD2EE.exe', 'Setup-iwd2ee.exe',
                    'Setup-iwd2ee_mod_compatibility_patch.exe',
                    'SETUP-IWD2EE.DEBUG', 'Override/IEex_Core.lua',
                    'Override/IEex_INI.lua', 'Override/AR1000.ARE',
                    'iwd2ee/iwd2ee.tp2',
                    'iwd2ee_mod_compatibility_patch/x.tp2', 'binkw32.dll',
                    'Shaders/readme.txt'):
            self.write(rel, b'x')
        self.assertEqual(oracle.leftovers(self.game), sorted([
            'IEex.dll', 'IEexHelper.dll', 'IEexHelperGUI.dll', 'IEex.ini',
            'IEex.log', 'IWD2EE.exe', 'Setup-iwd2ee.exe',
            'Setup-iwd2ee_mod_compatibility_patch.exe', 'SETUP-IWD2EE.DEBUG',
            'Override/IEex_Core.lua', 'Override/IEex_INI.lua', 'iwd2ee',
            'iwd2ee_mod_compatibility_patch', 'Shaders']))

    def mark(self, text=None):
        with open(os.path.join(self.game, oracle.MARKER), 'w') as f:
            f.write(json.dumps({'md5': oracle.vanilla_md5()})
                    if text is None else text)

    def test_ensure_keeps_a_marked_copy(self):
        self.mark()
        built = []
        self.assertEqual(oracle.ensure_vanilla(game=self.game,
                                               build=built.append),
                         self.game)
        self.assertEqual(built, [])

    def test_ensure_rebuilds_without_marker(self):
        built = []
        oracle.ensure_vanilla(game=self.game, build=built.append)
        self.assertEqual(built, [self.game])

    def test_ensure_rebuilds_on_request(self):
        self.mark()
        built = []
        oracle.ensure_vanilla(rebuild=True, game=self.game,
                              build=built.append)
        self.assertEqual(built, [self.game])

    def test_ensure_rebuilds_on_a_bad_marker(self):
        for text in ('', '{"md5": ', '[]', '{}', '{"md5": "0"}'):
            self.mark(text)
            built = []
            oracle.ensure_vanilla(game=self.game, build=built.append)
            self.assertEqual(built, [self.game], text)

    def test_ensure_builds_under_the_vanilla_lock(self):
        held = []

        def build(game):
            try:
                with filelock.hold(oracle.VANILLA_LOCK, 'test', timeout=0):
                    held.append(False)
            except TimeoutError:
                held.append(True)
        oracle.ensure_vanilla(game=self.game, build=build)
        self.assertEqual(held, [True])

    def test_ensure_rechecks_the_marker_under_the_lock(self):
        # Another worktree finished the copy while this one waited.
        built = []
        locks = LockRecorder(on_enter=lambda path: self.mark())
        with LockRecorder.patch(oracle, locks):
            oracle.ensure_vanilla(game=self.game, build=built.append)
        self.assertEqual(locks.paths, [(oracle.VANILLA_LOCK, 'vanilla')])
        self.assertEqual(built, [])

    def test_write_marker_is_atomic(self):
        oracle.write_marker(self.game, {'md5': oracle.vanilla_md5()})
        self.assertTrue(oracle.marker_ok(self.game))
        self.assertFalse(os.path.exists(
            os.path.join(self.game, oracle.MARKER + '.tmp')))

    def test_ieex_folder_fails(self):
        os.makedirs(os.path.join(self.game, 'Override', 'IEex_stuff'))
        failures = self.verify()
        self.assertEqual(len(failures), 1)
        self.assertIn('IEex_stuff', failures[0])

    def test_links_in(self):
        os.symlink(os.path.join(self.game, 'Override'),
                   os.path.join(self.game, 'Data'))
        os.symlink(os.path.join(self.game, 'iwd2.exe'),
                   os.path.join(self.game, 'Override', 'x.exe'))
        self.assertEqual(oracle.links_in(self.game),
                         ['Data', os.path.join('Override', 'x.exe')])

    def test_marker_is_ignored_by_verification(self):
        with open(os.path.join(self.game, oracle.MARKER), 'w') as f:
            f.write('{}')
        self.assertEqual(self.verify(), [])


class OracleSafeTargetTest(unittest.TestCase):
    """tools/oracle.py vanilla: the guard before deleting or writing the
    copy."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = os.path.realpath(self.tmp.name)
        self.oracle_dir = os.path.join(base, 'oracle')
        self.install = os.path.join(base, 'Games', 'Icewind Dale II')
        self.game = os.path.join(self.oracle_dir, 'game')
        for path in (self.oracle_dir, self.install, self.game):
            os.makedirs(path, exist_ok=True)
        self.reparse = set()

    def tearDown(self):
        self.tmp.cleanup()

    def why(self, game, install=None, oracle_dir=None):
        return oracle.unsafe_target(
            game, install or self.install, oracle_dir or self.oracle_dir,
            reparse=lambda p: ('%s is a reparse point' % p
                               if p.rstrip('/') in self.reparse else None))

    def test_safe(self):
        self.assertIsNone(self.why(self.game))
        self.assertIsNone(self.why(self.game + '/'))

    def test_equal_paths(self):
        self.assertIn('is the install', self.why(self.install))

    def test_child_of_install(self):
        self.assertIn('inside the install',
                      self.why(os.path.join(self.install, 'game')))

    def test_parent_of_install(self):
        self.assertIn('the install', self.why(os.path.dirname(self.install)))

    def test_case_and_trailing_slash_variants(self):
        self.assertIn('is the install', self.why(self.install + '/'))
        self.assertIn('is the install', self.why(
            self.install, install=self.install.upper() + '//'))
        self.assertIn('inside the install', self.why(
            os.path.join(self.install.swapcase(), 'Data'),
            oracle_dir=self.install.swapcase()))
        self.assertIn('the install', self.why(
            os.path.dirname(self.install).upper() + '/',
            install=self.install))

    def test_symlinked_game_dir(self):
        os.rmdir(self.game)
        os.symlink(self.install, self.game)
        self.assertIn('is the install', self.why(self.game))
        os.remove(self.game)
        elsewhere = os.path.join(self.oracle_dir, 'elsewhere')
        os.makedirs(elsewhere)
        os.symlink(elsewhere, self.game)
        self.assertIn('symlink', self.why(self.game))

    def test_symlinked_oracle_dir(self):
        real = self.oracle_dir + '-real'
        os.rename(self.oracle_dir, real)
        os.symlink(real, self.oracle_dir)
        self.assertIn('symlink', self.why(
            os.path.join(self.oracle_dir, 'game'),
            oracle_dir=self.oracle_dir))

    def test_reparse_point(self):
        self.reparse.add(self.game)
        self.assertIn('reparse point', self.why(self.game))
        self.reparse = {self.oracle_dir}
        self.assertIn('reparse point', self.why(self.game))

    def test_game_outside_oracle_dir(self):
        other = os.path.join(os.path.dirname(self.oracle_dir), 'game')
        self.assertIn('not directly in', self.why(other))
        self.assertIn('not directly in', self.why(
            os.path.join(self.game, 'deeper')))

    def test_missing_game_is_fine(self):
        os.rmdir(self.game)
        self.assertIsNone(self.why(self.game))


def fake_cmd(returncode=0, stdout='', stderr='', raises=None):
    calls = []

    def run(argv):
        calls.append(argv)
        if raises:
            raise raises
        return types.SimpleNamespace(returncode=returncode, stdout=stdout,
                                     stderr=stderr)
    run.calls = calls
    return run


class OracleWindowsReparseTest(unittest.TestCase):
    """tools/oracle.py: windows_reparse fails closed."""

    GAME = '/mnt/c/Users/thier/iwd2r-oracle/game'

    def check(self, run, path=GAME):
        return oracle.windows_reparse(path, run=run)

    def test_asks_dir_on_the_parent(self):
        run = fake_cmd(0, 'other\r\n')
        self.assertIsNone(self.check(run))
        self.assertEqual(run.calls, [['dir', '/AL', '/B',
                                      'C:\\Users\\thier\\iwd2r-oracle']])

    def test_exit_0_naming_the_target(self):
        reason = self.check(fake_cmd(0, 'junk\r\nGAME\r\n'))
        self.assertIn('is a reparse point', reason)

    def test_exit_0_without_the_target(self):
        self.assertIsNone(self.check(fake_cmd(0, 'game2\r\nout\r\n')))

    def test_exit_1_file_not_found(self):
        self.assertIsNone(self.check(fake_cmd(1, '', 'File Not Found\r\n')))

    def test_exit_1_other_stderr(self):
        reason = self.check(fake_cmd(
            1, '', 'The system cannot find the file specified.\r\n'))
        self.assertIn('cannot tell', reason)
        self.assertIn('cannot find the file', reason)

    def test_nonzero_exit_with_stdout(self):
        self.assertIn('cannot tell', self.check(fake_cmd(1, 'game\r\n',
                                                         'File Not Found')))
        self.assertIn('exit 2', self.check(fake_cmd(2, 'x', '')))

    def test_missing_cmd_exe(self):
        reason = self.check(fake_cmd(raises=FileNotFoundError(
            2, 'No such file', '/mnt/c/Windows/System32/cmd.exe')))
        self.assertIn('cannot run cmd.exe', reason)

    def test_timeout(self):
        reason = self.check(fake_cmd(raises=subprocess.TimeoutExpired(
            'cmd.exe', 60)))
        self.assertIn('timed out', reason)

    def test_path_off_drvfs(self):
        run = fake_cmd(0, '')
        for path in ('/home/thier/game', '/tmp/x', '/mnt/c', 'relative/game',
                     '/mnt/wsl/game'):
            self.assertIn('not on a Windows drive', self.check(run, path),
                          path)
        self.assertEqual(run.calls, [])

    def test_drvfs_win(self):
        self.assertEqual(oracle.drvfs_win('/mnt/c/Users/thier/'),
                         'C:\\Users\\thier')
        self.assertEqual(oracle.drvfs_win('/mnt/d'), 'D:\\')
        self.assertIsNone(oracle.drvfs_win('/home/thier'))


class OracleBuildOrderTest(unittest.TestCase):
    """tools/oracle.py vanilla: build_vanilla runs the guard before every
    destructive step, and a refusing guard stops it before any."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = os.path.realpath(self.tmp.name)
        self.oracle_dir = os.path.join(base, 'oracle')
        self.game = os.path.join(self.oracle_dir, 'game')
        self.install = os.path.join(base, 'install')
        files = {'CHITIN.KEY': b'KEY', 'IWD2.exe': b'MZ patched',
                 'WeiDU.log': WEIDU_LOG.encode(), 'icewind2.ini': INI.encode(),
                 'Setup-iwd2ee.exe': b'MZ', 'IEex.ini': b'x',
                 'Shaders/readme.txt': b'x', 'Override/a.itm': b'x'}
        for rel, data in files.items():
            path = os.path.join(self.install, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as f:
                f.write(data)
        os.makedirs(self.game)
        with open(os.path.join(self.game, 'old.txt'), 'w') as f:
            f.write('old copy')
        self.events = []
        self.guard_raises = False

    def tearDown(self):
        self.tmp.cleanup()

    def guard(self, game, install):
        self.events.append('guard')
        if self.guard_raises:
            raise SystemExit('oracle: refusing to touch the copy: test')

    def remove_tree(self, path):
        self.events.append('rmtree')
        import shutil
        shutil.rmtree(path)

    def rsync(self, install, game, log):
        self.events.append('rsync')
        import shutil
        shutil.copytree(install, game, dirs_exist_ok=True)
        return 0

    def weidu(self, bat, game, log):
        self.events.append('weidu')
        os.remove(os.path.join(game, 'IWD2.exe'))
        with open(os.path.join(game, 'iwd2.exe'), 'wb') as f:
            f.write(VANILLA_BYTES)
        with open(os.path.join(game, 'WeiDU.log'), 'w') as f:
            f.write('// Log of Currently Installed WeiDU Mods\n')
        return 0

    def delete(self, path):
        self.events.append('delete ' + os.path.basename(path))
        os.remove(path) if os.path.isfile(path) else \
            __import__('shutil').rmtree(path)

    def build(self):
        from unittest import mock
        patches = [
            mock.patch.object(oracle, 'ORACLE_DIR', self.oracle_dir),
            mock.patch.object(oracle, 'RUNS_DIR',
                              os.path.join(self.tmp.name, 'runs')),
            mock.patch.object(oracle, 'assert_safe_target', self.guard),
            mock.patch.object(oracle, 'remove_tree', self.remove_tree),
            mock.patch.object(oracle, 'rsync_copy', self.rsync),
            mock.patch.object(oracle, 'run_weidu', self.weidu),
            mock.patch.object(oracle, 'delete_leftover', self.delete),
            mock.patch.object(oracle, 'prove_separate',
                              lambda game, install, log: None),
            mock.patch.object(oracle, 'win_path',
                              lambda p: 'X:' + p.replace('/', '\\')),
        ]
        with contextlib.ExitStack() as stack:
            for p in patches:
                stack.enter_context(p)
            stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
            return oracle.build_vanilla(self.game, self.install,
                                        md5=VANILLA_TEST_MD5)

    def test_guard_before_each_destructive_step(self):
        self.assertEqual(self.build(), self.game)
        self.assertEqual(self.events, [
            'guard', 'rmtree', 'guard', 'rsync', 'guard', 'weidu',
            'guard', 'delete IEex.ini', 'guard', 'delete Setup-iwd2ee.exe',
            'guard', 'delete Shaders'])
        self.assertTrue(oracle.marker_ok(self.game, VANILLA_TEST_MD5))
        self.assertFalse(os.path.exists(os.path.join(self.game, 'old.txt')))

    def test_refusing_guard_stops_before_anything(self):
        self.guard_raises = True
        with open(os.path.join(self.game, oracle.MARKER), 'w') as f:
            f.write('{"md5": "kept"}')
        with self.assertRaises(SystemExit):
            self.build()
        self.assertEqual(self.events, ['guard'])
        self.assertEqual(sorted(os.listdir(self.game)),
                         sorted([oracle.MARKER, 'old.txt']))


class PlayBuildWindowsDefaultsTest(unittest.TestCase):
    def test_defaults_build_the_game(self):
        import inspect
        params = inspect.signature(play.build_windows).parameters
        self.assertEqual(params['preset'].default, 'windows-msvc-x64')
        self.assertEqual(params['target'].default, 'iwd2r')
        self.assertEqual(params['exe_rel'].default, play.WIN_EXE_REL)
        self.assertEqual(params['bat_name'].default, 'play-build.bat')


bif = load_tool('bif')


def make_key(bifs, resources):
    """A KEY V1: bifs is [name], resources [(resref, type, bif, file)]."""
    header = 24
    names = b''.join(n.encode() + b'\0' for n in bifs)
    obif = header
    oname = obif + 12 * len(bifs)
    ores = oname + len(names)
    out = b'KEY V1  ' + struct.pack('<4I', len(bifs), len(resources), obif,
                                    ores)
    pos = oname
    for n in bifs:
        out += struct.pack('<IIHH', 0, pos, len(n) + 1, 1)
        pos += len(n) + 1
    out += names
    for resref, rtype, b, f in resources:
        out += struct.pack('<8sHI', resref.encode(), rtype, (b << 20) | f)
    return out


def make_biff(files):
    """A BIFF V1 holding [(file index, bytes)]."""
    offset = 20
    data_start = offset + 16 * len(files)
    out = b'BIFFV1  ' + struct.pack('<3I', len(files), 0, offset)
    body = b''
    for index, data in files:
        out += struct.pack('<IIIHH', index, data_start + len(body),
                           len(data), 0x3F4, 0)
        body += data
    return out + body


class BifTest(unittest.TestCase):
    """tools/bif.py on a synthetic CHITIN.KEY and BIFF."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.game = self._tmp.name
        os.makedirs(os.path.join(self.game, 'Data'))
        os.makedirs(os.path.join(self.game, 'CD2', 'Data'))
        os.makedirs(os.path.join(self.game, 'Override'))
        key = make_key(['data\\Main.bif', 'data\\Extra.bif'],
                       [('XPLEVEL', 0x3F4, 0, 1), ('SKILLS', 0x3F4, 1, 0),
                        ('XPLEVEL', 0x3F0, 0, 0), ('MISSING', 0x3F4, 5, 0),
                        ('00SWDL01', 0x3ED, 0, 2)])
        self.write('CHITIN.KEY', key)
        self.write('Data/MAIN.BIF', make_biff([(0, b'ids'), (1, b'2DA xp'),
                                               (2, b'ITM V2.0')]))
        self.write('CD2/Data/Extra.bif', make_biff([(0, b'2DA skills')]))

    def write(self, rel, data):
        with open(os.path.join(self.game, rel), 'wb') as f:
            f.write(data)

    def test_parse_key(self):
        with open(os.path.join(self.game, 'CHITIN.KEY'), 'rb') as f:
            bifs, resources = bif.parse_key(f.read())
        self.assertEqual(bifs, ['data/Main.bif', 'data/Extra.bif'])
        self.assertEqual(resources[('XPLEVEL', 0x3F4)], (0, 1))
        self.assertEqual(resources[('XPLEVEL', 0x3F0)], (0, 0))

    def test_extract_from_biff_by_type_and_case(self):
        self.assertEqual(bif.extract(self.game, 'xplevel.2da'), b'2DA xp')
        self.assertEqual(bif.extract(self.game, 'XPLEVEL.IDS'), b'ids')
        self.assertEqual(bif.extract(self.game, '00swdl01.itm'), b'ITM V2.0')

    def test_extract_from_a_cd_folder(self):
        self.assertEqual(bif.extract(self.game, 'SKILLS.2DA'), b'2DA skills')

    def test_override_wins(self):
        self.write('Override/XPLevel.2da', b'override')
        self.assertEqual(bif.extract(self.game, 'XPLEVEL.2DA'), b'override')

    def test_errors(self):
        for name, message in (('NOPE.2DA', 'not in CHITIN.KEY'),
                              ('MISSING.2DA', 'archive 5 of 2'),
                              ('XPLEVEL.WAV', 'unknown resource type')):
            with self.assertRaisesRegex(bif.BifError, message):
                bif.extract(self.game, name)
        self.write('Data/MAIN.BIF', b'BIFCV1.0' + bytes(16))
        with self.assertRaisesRegex(bif.BifError, 'not a BIFF V1'):
            bif.extract(self.game, 'XPLEVEL.2DA')
        with self.assertRaisesRegex(bif.BifError, 'not a KEY V1'):
            bif.parse_key(b'KEY V2  ' + bytes(16))
        with self.assertRaisesRegex(bif.BifError, 'past the end'):
            bif.parse_key(b'KEY V1  ' + struct.pack('<4I', 9, 0, 24, 24))
        with self.assertRaisesRegex(bif.BifError, 'no file 7'):
            bif.read_biff(make_biff([(0, b'x')]), 7)
        with self.assertRaisesRegex(bif.BifError, 'past the end'):
            bif.read_biff(make_biff([(0, b'xyz')])[:-1], 0)

    def test_truncated_biff_header(self):
        for data in (b'BIFFV1  ', b'BIFFV1  ' + bytes(11)):
            with self.assertRaisesRegex(bif.BifError, 'BIFF header truncated'):
                bif.read_biff(data, 0)

    def test_truncated_biff_table(self):
        whole = make_biff([(0, b'x'), (1, b'y')])
        with self.assertRaisesRegex(bif.BifError,
                                    'file entries run past the end'):
            bif.read_biff(whole[:20 + 16 + 8], 1)
        # Through extract: a BifError, never a struct.error.
        self.write('Data/MAIN.BIF', whole[:20 + 16])
        with self.assertRaisesRegex(bif.BifError, 'run past the end'):
            bif.extract(self.game, 'XPLEVEL.2DA')

    def test_bad_key_name_offset(self):
        key = bytearray(make_key(['data\\Main.bif'], []))
        struct.pack_into('<I', key, 24 + 4, len(key) + 100)
        with self.assertRaisesRegex(bif.BifError,
                                    'bif 0 name .* past the end'):
            bif.parse_key(bytes(key))
        key = bytearray(make_key(['data\\Main.bif'], []))
        struct.pack_into('<H', key, 24 + 8, 0xFFFF)
        with self.assertRaisesRegex(bif.BifError, 'length 65535'):
            bif.parse_key(bytes(key))

    def test_extract_to_writes_upper_case_names(self):
        out = os.path.join(self.game, 'out')
        paths = bif.extract_to(self.game, out, ['xplevel.2da'])
        self.assertEqual(paths, [os.path.join(out, 'XPLEVEL.2DA')])
        with open(paths[0], 'rb') as f:
            self.assertEqual(f.read(), b'2DA xp')
        self.assertEqual(os.listdir(out), ['XPLEVEL.2DA'])


gen_fields = load_tool('gen_fields')
minischema = load_tool('minischema')

FIXTURE_HEADERS = {
    'CThing.h': """#define THING_SLOTS 3

class CPart;

class CPart {
public:
    /* 0000 */ CString m_sName;
    /* 0004 */ BYTE m_nKind;
    /* 0006 */ SHORT m_nCount;
};

class CThing : public CBase {
public:
    CThing();
    /* 0000 */ virtual void Update(); // a method: ignored
    /* 0x0010 */ CPart m_part;
    /* 0020 */ CThing* m_pNext; // a #guess comment
    /* 0024 */ DWORD m_flags[THING_SLOTS];
    /* 0030 */ RESREF m_portrait;
    /* 0038 */ BOOLEAN m_bOn;
    int m_noComment;
    /* 003C */ INT m_nValue;
    /* 0040 */ union {
        struct {
            /* 0040 */ BYTE m_low;
            /* 0041 */ BYTE m_high;
        };
        WORD m_word;
    };
    /* 0044 */ CResRef m_area;
    /* 004C */ CPoint m_pos;
    /* 0054 */ CPoint m_corners[2];
};

class CBigThing : public CThing {
public:
    /* 0010 */ SHORT m_nKindOverride; // shadows nothing: a new name
    /* 0064 */ BYTE m_extra;
};
""",
    'CEntry.h': """class CEntry {
public:
    /* 0000 */ BYTE m_counts[3];
    /* 0006 */ SHORT m_id;
    /* 0008 */ CThing* m_ptr;
};
class CThingHolder { // CThing is not CThingHolder
public:
    /* 0000 */ int m_unused;
};
""",
}


class GenFieldsTest(unittest.TestCase):
    """tools/gen_fields.py on a fixture header."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.src = os.path.join(self.tmp.name, 'src')
        os.makedirs(self.src)
        for name, text in FIXTURE_HEADERS.items():
            with open(os.path.join(self.src, name), 'w') as f:
                f.write(text)
        self.headers = gen_fields.Headers(self.src)

    def generate(self, fields, globals_=None):
        return gen_fields.generate({'x-fields': fields}, self.headers,
                                   globals_ or {})

    def test_members_types_and_offsets(self):
        out = self.generate({
            'next': {'class': 'CThing', 'member': 'm_pNext'},
            'flags': {'class': 'CThing', 'member': 'm_flags'},
            'portrait': {'class': 'CThing', 'member': 'm_portrait'},
            'on': {'class': 'CThing', 'member': 'm_bOn'},
            'value': {'class': 'CThing', 'member': 'm_nValue'},
            'high': {'class': 'CThing', 'member': 'm_high'},
            'kind': {'class': 'CThing', 'member': 'm_part.m_nKind'},
            'count': {'class': 'CThing', 'member': 'm_part.m_nCount'},
        })
        self.assertEqual(out, {
            'next': {'base': 'CThing', 'offset': '0x0020', 'type': 'ptr'},
            'flags': {'base': 'CThing', 'offset': '0x0024', 'type': 'u32',
                      'count': 3},
            'portrait': {'base': 'CThing', 'offset': '0x0030',
                         'type': 'resref'},
            'on': {'base': 'CThing', 'offset': '0x0038', 'type': 'u8'},
            'value': {'base': 'CThing', 'offset': '0x003C', 'type': 'i32'},
            'high': {'base': 'CThing', 'offset': '0x0041', 'type': 'u8'},
            'kind': {'base': 'CThing', 'offset': '0x0014', 'type': 'u8'},
            'count': {'base': 'CThing', 'offset': '0x0016', 'type': 'i16'},
        })

    def test_value_classes_and_bases(self):
        out = self.generate({
            'area': {'class': 'CThing', 'member': 'm_area'},
            'pos': {'class': 'CThing', 'member': 'm_pos'},
            'corners': {'class': 'CThing', 'member': 'm_corners'},
            'extra': {'class': 'CBigThing', 'member': 'm_extra'},
            'inherited': {'class': 'CBigThing', 'member': 'm_bOn'},
            'through': {'class': 'CBigThing', 'member': 'm_part.m_nCount'},
        })
        self.assertEqual(out, {
            'area': {'base': 'CThing', 'offset': '0x0044', 'type': 'resref'},
            'pos': {'base': 'CThing', 'offset': '0x004C', 'type': 'i32',
                    'count': 2},
            'corners': {'base': 'CThing', 'offset': '0x0054', 'type': 'i32',
                        'count': 4},
            'extra': {'base': 'CBigThing', 'offset': '0x0064', 'type': 'u8'},
            'inherited': {'base': 'CBigThing', 'offset': '0x0038',
                          'type': 'u8'},
            'through': {'base': 'CBigThing', 'offset': '0x0016',
                        'type': 'i16'},
        })

    def test_global_and_sizeof(self):
        out = self.generate({
            'thing': {'global': 'g_pThing', 'type': 'ptr'},
            'entry_size': {'class': 'CEntry', 'sizeof': True},
        }, {'g_pThing': '0x8cf6dc'})
        self.assertEqual(out, {
            'thing': {'base': '0x8CF6DC', 'offset': '0x0000', 'type': 'ptr'},
            'entry_size': {'base': 'CEntry', 'offset': '0x000C',
                           'type': 'size'}})

    def test_errors_name_the_field(self):
        cases = [
            ({'class': 'CNope', 'member': 'm_x'}, 'no class CNope'),
            ({'class': 'CThing', 'member': 'm_noComment'},
             'no commented member m_noComment'),
            ({'class': 'CThing', 'member': 'Update'},
             'no commented member Update'),
            ({'class': 'CThing', 'member': 'm_part'},
             'has type CPart, not a scalar'),
            ({'class': 'CThing', 'member': 'm_pNext.m_x'},
             'is not a class member'),
            ({'global': 'g_missing', 'type': 'ptr'}, 'no address for global'),
            ({'global': 'g_pThing'}, 'needs a "type"'),
            ({'class': 'CThing', 'sizeof': True}, 'has a class type'),
            ({'member': 'm_x'}, 'malformed spec'),
        ]
        for spec, message in cases:
            with self.assertRaises(gen_fields.FieldError, msg=spec) as cm:
                self.generate({'f': spec}, {'g_pThing': '0x1'})
            self.assertIn('field f: ', str(cm.exception))
            self.assertIn(message, str(cm.exception))

    def test_check_and_write(self):
        schema = os.path.join(self.tmp.name, 'schema.json')
        vanilla = os.path.join(self.tmp.name, 'vanilla.json')
        out = os.path.join(self.tmp.name, 'fields.json')
        with open(schema, 'w') as f:
            json.dump({'x-fields': {'on': {'class': 'CThing',
                                           'member': 'm_bOn'}}}, f)
        with open(vanilla, 'w') as f:
            json.dump({}, f)
        args = ['--schema', schema, '--src', self.src, '--vanilla', vanilla,
                '--out', out]
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(gen_fields.main(args + ['--check']), 1)
            self.assertEqual(gen_fields.main(args), 0)
            self.assertEqual(gen_fields.main(args + ['--check']), 0)
        with open(out) as f:
            self.assertEqual(json.load(f), {'on': {
                'base': 'CThing', 'offset': '0x0038', 'type': 'u8'}})
        with open(out, 'a') as f:
            f.write(' ')
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(gen_fields.main(args + ['--check']), 1)
        self.assertIn('out of date', text.getvalue())

    def test_repository_fields_are_current(self):
        # oracle/replay/fields.json matches the schema and src/ headers.
        with contextlib.redirect_stdout(io.StringIO()) as text:
            code = gen_fields.main(['--check'])
        self.assertEqual(code, 0, text.getvalue())

    def test_repository_offsets(self):
        # Offsets checked against the vanilla exe's disassembly
        # (vanilla.json notes, oracle/replay/recorder.cpp).
        with open(os.path.join(ROOT, 'oracle', 'replay', 'fields.json')) as f:
            fields = json.load(f)
        self.assertEqual(fields['baldur_chitin']['base'], '0x8CF6DC')
        self.assertEqual(fields['active_engine']['offset'], '0x03C4')
        self.assertEqual(fields['engine_connection']['offset'], '0x1C98')
        self.assertEqual(fields['engine_start']['offset'], '0x1C84')
        self.assertEqual(fields['objects_array']['offset'], '0x374C')
        self.assertEqual(fields['object_entry_size']['offset'], '0x000C')
        self.assertEqual(fields['object_entry_id']['offset'], '0x0006')
        self.assertEqual(fields['object_entry_ptr']['offset'], '0x0008')


class MiniSchemaTest(unittest.TestCase):
    SCHEMA = {
        '$schema': 'https://json-schema.org/draft/2020-12/schema',
        'title': 't', 'x-fields': {'ignored': True},
        'type': 'object', 'required': ['n', 'tags'],
        'additionalProperties': False,
        'properties': {
            'n': {'type': 'integer', 'minimum': 0, 'maximum': 9},
            'name': {'type': ['string', 'null'], 'maxLength': 3},
            'kind': {'type': 'string', 'enum': ['a', 'b']},
            'tags': {'type': 'array', 'items': {'type': 'integer'},
                     'minItems': 1, 'maxItems': 2},
            'free': {'type': 'object'},
        },
    }

    def errors(self, value):
        return minischema.validate(value, self.SCHEMA)

    def test_valid(self):
        self.assertEqual(self.errors({'n': 0, 'tags': [1]}), [])
        self.assertEqual(self.errors({'n': 9, 'name': None, 'kind': 'b',
                                      'tags': [1, 2], 'free': {'x': 1}}), [])

    def test_each_keyword(self):
        cases = [
            ({'tags': [1]}, "$: missing required property 'n'"),
            ({'n': 1, 'tags': [1], 'more': 1}, '$.more: unexpected property'),
            ({'n': True, 'tags': [1]}, '$.n: True is not of type integer'),
            ({'n': 1.0, 'tags': [1]}, '$.n: 1.0 is not of type integer'),
            ({'n': -1, 'tags': [1]}, '$.n: -1 is below the minimum 0'),
            ({'n': 10, 'tags': [1]}, '$.n: 10 is above the maximum 9'),
            ({'n': 1, 'tags': [1], 'name': 5},
             '$.name: 5 is not of type string or null'),
            ({'n': 1, 'tags': [1], 'name': 'long'},
             '$.name: longer than 3 characters'),
            ({'n': 1, 'tags': [1], 'kind': 'c'},
             "$.kind: 'c' is not one of ['a', 'b']"),
            ({'n': 1, 'tags': []}, '$.tags: 0 items, fewer than 1'),
            ({'n': 1, 'tags': [1, 2, 3]}, '$.tags: 3 items, more than 2'),
            ({'n': 1, 'tags': [1, 'x']},
             "$.tags[1]: 'x' is not of type integer"),
            ([], '$: [] is not of type object'),
        ]
        for value, message in cases:
            self.assertEqual(self.errors(value), [message], value)

    def test_unsupported_keyword_is_an_error(self):
        self.assertEqual(minischema.validate(1, {'type': 'integer',
                                                 'multipleOf': 2}),
                         ['$: unsupported schema keywords multipleOf'])

    def test_replay_schema(self):
        with open(os.path.join(ROOT, 'oracle', 'replay', 'schema.json')) as f:
            schema = json.load(f)
        dump = {'frame': 61, 'checkpoint': 'start', 'screen': 'connection',
                'rng_state': 1,
                'globals': {'au_counter': 61, 'fullscreen': 0,
                            'in_async_update': 0, 'ai_stalled_frames': 0,
                            'reinitializing': 0, 'loading': 0, 'loads': 0,
                            'held_frames': 0, 'ai_rng_state': None},
                'chargen': None, 'world': None}
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['chargen'] = {'race': 1, 'subrace': 0, 'class': 5,
                           'abilities': [10, 11, 12, 13, 14, 15],
                           'rolls': [0, 0, 0, 0, 0, 0, 0], 'skill_points': 4,
                           'feats': [0, 1, 4294967295], 'portrait': 'MAN1L'}
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['chargen']['abilities'] = [10]
        dump['screen'] = 'nowhere'
        self.assertEqual(minischema.validate(dump, schema), [
            "$.screen: 'nowhere' is not one of %s"
            % schema['properties']['screen']['enum'],
            '$.chargen.abilities: 1 items, fewer than 6'])

    def test_replay_schema_world(self):
        with open(os.path.join(ROOT, 'oracle', 'replay', 'schema.json')) as f:
            schema = json.load(f)
        member = {'id': 65536, 'x': 1046, 'y': 1258, 'hp': 9, 'max_hp': 12,
                  'skills': [0] * 64}
        dump = {'frame': 1500, 'checkpoint': 'world', 'screen': 'world',
                'rng_state': 1,
                'globals': {'au_counter': 1498, 'fullscreen': 0,
                            'in_async_update': 0, 'ai_stalled_frames': 0,
                            'reinitializing': 0, 'loading': 0, 'loads': 1,
                            'held_frames': 13, 'ai_rng_state': 7},
                'chargen': None,
                'world': {'area': 'AR1000', 'party': [member]}}
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['world']['party'].append(
            {'id': 131073, 'x': None, 'y': None, 'hp': None, 'max_hp': None,
             'skills': None})
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['world']['party'][0] = dict(member, hp='9')
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.party[0].hp: '9' is not of type integer or null"])
        del dump['world']
        self.assertEqual(minischema.validate(dump, schema),
                         ["$: missing required property 'world'"])

    def test_every_engine_field_is_a_screen(self):
        with open(os.path.join(ROOT, 'oracle', 'replay', 'schema.json')) as f:
            schema = json.load(f)
        screens = {f[len('engine_'):] for f in schema['x-fields']
                   if f.startswith('engine_')}
        self.assertEqual(set(schema['properties']['screen']['enum']),
                         screens | {'none', 'unknown'})


class LockRecorder:
    """A stand-in for filelock.hold that records (path, what) and whether
    a lock is held at a given moment."""

    def __init__(self, on_enter=None):
        self.paths = []
        self.held = []
        self.on_enter = on_enter

    @contextlib.contextmanager
    def hold(self, path, what, timeout=None):
        self.paths.append((path, what))
        self.held.append(path)
        if self.on_enter:
            self.on_enter(path)
        try:
            yield
        finally:
            self.held.remove(path)

    @staticmethod
    def patch(module, recorder):
        from unittest import mock
        return mock.patch.object(module, 'filelock',
                                 types.SimpleNamespace(hold=recorder.hold))


class WinCheckoutTest(unittest.TestCase):
    def test_win_checkout_defaults(self):
        base = '/mnt/c/Users/thier/iwd2r-build'
        self.assertEqual(play.win_checkout('/home/u/iwd2-re-worktrees/first',
                                           {}), base)
        self.assertEqual(play.win_checkout('/home/u/iwd2-re-worktrees/first/',
                                           {}), base)
        self.assertEqual(play.win_checkout(
            '/home/u/iwd2-re-worktrees/p3-chargen', {}),
            base + '-p3-chargen')
        self.assertEqual(play.win_checkout('/home/u/iwd2-re', {}),
                         base + '-iwd2-re')
        self.assertEqual(play.win_checkout(
            '/home/u/iwd2-re-worktrees/first',
            {'IWD2R_WIN_CHECKOUT': '/mnt/c/x'}), '/mnt/c/x')
        self.assertEqual(play.win_checkout(
            '/home/u/iwd2-re-worktrees/p3-x',
            {'IWD2R_WIN_CHECKOUT': ''}), base + '-p3-x')


class WorktreeScriptTest(unittest.TestCase):
    """tools/worktree.sh in a throwaway repository with a first branch."""

    SCRIPT = os.path.join(ROOT, 'tools', 'worktree.sh')

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.repo = os.path.join(self._tmp.name, 'repo')
        self.base = os.path.join(self._tmp.name, 'worktrees')
        os.makedirs(self.repo)
        self.git('init', '-q', '-b', 'first')
        self.commit(self.repo, 'a.txt', 'one')

    def git(self, *args, cwd=None):
        env = dict(os.environ, GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                   GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t',
                   GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')
        return subprocess.run(['git', *args], cwd=cwd or self.repo, env=env,
                              check=True, capture_output=True, text=True)

    def commit(self, where, name, text):
        with open(os.path.join(where, name), 'w') as f:
            f.write(text)
        self.git('add', name, cwd=where)
        self.git('commit', '-qm', name, cwd=where)

    def script(self, *args):
        env = dict(os.environ, IWD2R_WORKTREES=self.base,
                   GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                   GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t',
                   GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')
        return subprocess.run(['bash', self.SCRIPT, *args], cwd=self.repo,
                              env=env, capture_output=True, text=True)

    def branches(self):
        return self.git('branch', '--format=%(refname:short)').stdout.split()

    def test_worktree_script_new_and_done(self):
        r = self.script('new', 'chargen')
        self.assertEqual(r.returncode, 0, r.stderr)
        path = os.path.join(self.base, 'p3-chargen')
        self.assertEqual(r.stdout, path + '\n')
        self.assertTrue(os.path.isfile(os.path.join(path, 'a.txt')))
        self.assertIn('p3/chargen', self.branches())
        self.assertEqual(
            self.git('rev-parse', 'HEAD', cwd=path).stdout,
            self.git('rev-parse', 'first').stdout)
        self.assertNotEqual(self.script('new', 'chargen').returncode, 0)

        self.commit(path, 'b.txt', 'two')
        r = self.script('done', 'chargen')
        self.assertEqual(r.returncode, 1)
        self.assertIn('not merged into first', r.stderr)
        self.assertTrue(os.path.isdir(path))
        self.assertIn('p3/chargen', self.branches())

        self.git('merge', '-q', '--no-edit', 'p3/chargen')
        r = self.script('done', 'chargen')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(os.path.exists(path))
        self.assertNotIn('p3/chargen', self.branches())

    def test_bad_stream_name(self):
        self.assertEqual(self.script('new', '../x').returncode, 2)
        self.assertEqual(self.script('nope', 'x').returncode, 2)


class SharedLockTest(unittest.TestCase):
    """replay record/compare hold ORACLE_DIR/replay.lock; each --vm windows
    scenario holds VM_ROOT/sandbox.lock."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

    def test_replay_holds_lock(self):
        from unittest import mock
        locks = LockRecorder()
        seen = []

        def fake_record(script, actions, *rest, **kw):
            seen.append((os.path.basename(script), list(locks.held)))
            return 0

        with LockRecorder.patch(oracle, locks), \
                mock.patch.object(oracle, 'record_original', fake_record), \
                mock.patch.object(oracle, 'RUNS_DIR', self.tmp):
            code = oracle.main(['replay', 'record', 'chargen_replay',
                                '--seed', '1'])
        self.assertEqual(code, 0)
        self.assertEqual(locks.paths, [(oracle.REPLAY_LOCK, 'replay')])
        self.assertEqual(oracle.REPLAY_LOCK,
                         oracle.ORACLE_DIR + '/replay.lock')
        self.assertEqual(seen, [('chargen_replay.txt', [oracle.REPLAY_LOCK])])
        self.assertEqual(locks.held, [])

    def test_replay_compare_holds_lock(self):
        from unittest import mock
        locks = LockRecorder()
        seen = []

        def fake_compare(args):
            seen.append((args.scenario, list(locks.held)))
            return 0

        with LockRecorder.patch(oracle, locks), \
                mock.patch.object(oracle, 'replay_compare', fake_compare):
            code = oracle.main(['replay', 'compare', 'chargen_replay'])
        self.assertEqual(code, 0)
        self.assertEqual(seen, [('chargen_replay', [oracle.REPLAY_LOCK])])
        self.assertEqual(locks.paths, [(oracle.REPLAY_LOCK, 'replay')])

    def test_sandbox_run_holds_lock(self):
        from unittest import mock
        locks = LockRecorder()
        events = []
        vm_root = os.path.join(self.tmp, 'vm')
        lock = vm_root + '/sandbox.lock'

        def run_scenario(exe, cache, actions, overrides, name, run_dir,
                         host_run, timeout, rate, log):
            events.append(('run', name, list(locks.held)))
            return {'code': 0, 'seconds': 1.0, 'error': None,
                    'screen_shots': 0, 'engine_shots': 0}

        sandbox = types.SimpleNamespace(
            available=lambda: True, VM_ROOT=vm_root,
            GAME_CACHE=vm_root + '/game', SANDBOX_LOCK=lock,
            ENABLE_HINT='',
            refresh_cache=lambda d, c, log: events.append(
                ('refresh', list(locks.held))),
            reset_cache_inis=lambda d, c: events.append(
                ('reset', list(locks.held))),
            run_scenario=run_scenario)
        self.assertEqual(winsandbox.SANDBOX_LOCK,
                         winsandbox.VM_ROOT + '/sandbox.lock')
        scenarios = [(path, realinput.load_scenario(path))
                     for path in scenario_files()[:2]]
        base_dir = os.path.join(self.tmp, 'runs', 'stamp')
        args = types.SimpleNamespace(timeout=10, no_build=True)
        with LockRecorder.patch(play, locks), \
                mock.patch.object(play, 'winsandbox', sandbox), \
                mock.patch.object(play, 'ROOT', self.tmp), \
                mock.patch.object(play, 'build_windows_vm',
                                  lambda a: 'iwd2r.exe'), \
                contextlib.redirect_stdout(io.StringIO()):
            rows = play.run_windows_vm(args, [], scenarios, '/data', {},
                                       base_dir, True, None)
        self.assertEqual(len(rows), 2)
        names = [play.scenario_name(p) for p, _ in scenarios]
        self.assertEqual(events, [
            ('refresh', [lock]),
            ('reset', [lock]), ('run', names[0], [lock]),
            ('reset', [lock]), ('run', names[1], [lock])])
        self.assertEqual(locks.paths, [(lock, 'sandbox')] * 3)


class SandboxReplayTest(unittest.TestCase):
    """replay record/compare in Windows Sandbox (winsandbox.run_replay,
    tools/sandbox/replay-driver.ps1), with the sandbox stubbed."""

    GAME = r'C:\Users\thier\iwd2r-oracle\game'

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

    def test_wsb_maps_the_replay_folders(self):
        folders = winsandbox.replay_folders(r'C:\b\oracle', self.GAME,
                                            r'C:\vm\results',
                                            r'C:\vm\tools')
        root = ET.fromstring(winsandbox.make_wsb(
            folders, winsandbox.replay_logon_command()))
        mapped = [(m.findtext('HostFolder'), m.findtext('SandboxFolder'),
                   m.findtext('ReadOnly'))
                  for m in root.find('MappedFolders')]
        self.assertEqual(mapped, [
            (r'C:\b\oracle', r'C:\iwd2r\oracle', 'true'),
            # Its own path: icewind2.ini's [Alias] paths name it.
            (self.GAME, self.GAME, 'false'),
            (r'C:\vm\results', r'C:\iwd2r\results', 'false'),
            (r'C:\vm\tools', r'C:\iwd2r\tools', 'true')])
        self.assertEqual(root.findtext('Networking'), 'Disable')
        self.assertEqual(
            root.find('LogonCommand').findtext('Command'),
            r'powershell.exe -NoProfile -ExecutionPolicy Bypass -File '
            r'C:\iwd2r\tools\replay-driver.ps1 -Config '
            r'C:\iwd2r\tools\config.json')

    def test_driver_config(self):
        env = oracle.recorder_env(
            winsandbox.BOX_REPLAY_DIR + '\\scenario.txt',
            winsandbox.BOX_REPLAY_DIR, 1, 30)
        self.assertEqual(env, {
            'IWD2R_SCRIPT': r'C:\iwd2r\results\replay\scenario.txt',
            'IWD2R_REPLAY_DIR': r'C:\iwd2r\results\replay',
            'IWD2R_DUMP_EVERY': '30', 'IWD2R_SEED': '1'})
        self.assertEqual(oracle.recorder_env('s', 'd', 7, 0, 40, True), {
            'IWD2R_SCRIPT': 's', 'IWD2R_REPLAY_DIR': 'd',
            'IWD2R_DUMP_EVERY': '0', 'IWD2R_SEED': '7',
            'IWD2R_TEST_STEAL_FOCUS': '40',
            'IWD2R_TEST_NO_FOCUS_GUARD': '1'})
        self.assertEqual(oracle.recorder_env('s', 'd', 7, 0,
                                             slow_reads=20)
                         ['IWD2R_TEST_SLOW_READS'], '20')
        cfg = winsandbox.replay_driver_config(self.GAME, env, 180)
        self.assertEqual(cfg, {
            'injector': r'C:\iwd2r\oracle\injector.exe',
            'recorder': r'C:\iwd2r\oracle\recorder.dll',
            'exe': self.GAME + r'\iwd2.exe',
            'workdir': self.GAME,
            'replay_dir': r'C:\iwd2r\results\replay',
            'results': r'C:\iwd2r\results',
            'env': env,
            'timeout': 180,
            'backstop': 240,
            'exclusions': [r'C:\iwd2r\oracle', self.GAME,
                           r'C:\iwd2r\results'],
            'stop_audio': False,
            'directplay': r'C:\iwd2r\tools\directplay',
            'shutdown': True})
        self.assertTrue(os.path.isfile(winsandbox.REPLAY_DRIVER))

    def test_stage_directplay(self):
        source = os.path.join(self.tmp, 'SysWOW64')
        os.makedirs(source)
        for name in winsandbox.DIRECTPLAY_FILES:
            with open(os.path.join(source, name.upper()), 'w') as f:
                f.write(name)
        target = os.path.join(self.tmp, 'tools', 'directplay')
        self.assertEqual(winsandbox.stage_directplay(target, source),
                         list(winsandbox.DIRECTPLAY_FILES))
        self.assertEqual(sorted(os.listdir(target)),
                         sorted(winsandbox.DIRECTPLAY_FILES))
        os.remove(os.path.join(source, 'DPLAYX.DLL'))
        with self.assertRaisesRegex(RuntimeError, 'dplayx.dll'):
            winsandbox.stage_directplay(target, source)

    def test_results_copy_plan(self):
        plan = winsandbox.replay_copy_plan(
            '/r', '/run/replay', ['driver.log', 'done.txt'],
            ['status.txt', 'checkpoint-start.json'])
        self.assertEqual(plan, [
            ('/r/replay/checkpoint-start.json',
             '/run/replay/checkpoint-start.json'),
            ('/r/replay/status.txt', '/run/replay/status.txt'),
            ('/r/done.txt', '/run/replay/sandbox/done.txt'),
            ('/r/driver.log', '/run/replay/sandbox/driver.log')])

    def record(self, argv, replace_run=None):
        """Runs oracle.main(argv) with everything outside stubbed; returns
        (exit code, events, locks)."""
        from unittest import mock
        locks = LockRecorder()
        events = []
        build = os.path.join(self.tmp, 'win')
        out = os.path.join(build, oracle.REPLAY_DIR_REL)
        os.makedirs(out)
        for name in (oracle.INJECTOR, oracle.RECORDER):
            open(os.path.join(out, name), 'w').close()
        game = os.path.join(self.tmp, 'game')
        os.makedirs(game)
        dump = {'frame': 10, 'globals': {'reinitializing': 0,
                                         'ai_stalled_frames': 0}}

        def run_replay(build_dir, game_dir, script, env, timeout, host_run,
                       out_dir, log):
            events.append(('sandbox', list(locks.held), env['IWD2R_SEED'],
                           build_dir == out))
            os.makedirs(out_dir)
            with open(os.path.join(out_dir, 'status.txt'), 'w') as f:
                f.write('frame=10\nstate=finished\n')
            with open(os.path.join(out_dir, 'checkpoint-start.json'),
                      'w') as f:
                json.dump(dump, f)
            return {'code': 0, 'stdout': 'injector: pid 1', 'stderr': '',
                    'error': None, 'seconds': 31.0}

        def record_on_host(*a, **kw):
            events.append(('host', list(locks.held)))
            out_dir = a[8]
            os.makedirs(out_dir)
            with open(os.path.join(out_dir, 'checkpoint-start.json'),
                      'w') as f:
                json.dump(dump, f)
            return 0, '', ''

        # The play module oracle.py imports (tools/ is on its sys.path).
        oracle_play = __import__('play')
        sandbox = types.SimpleNamespace(
            available=lambda: True, ENABLE_HINT='play: off',
            VM_ROOT=os.path.join(self.tmp, 'vm'),
            SANDBOX_LOCK=os.path.join(self.tmp, 'vm', 'sandbox.lock'),
            BOX_REPLAY_DIR=winsandbox.BOX_REPLAY_DIR,
            deploy_runtime=lambda exe, log, arch: events.append(
                ('runtime', os.path.basename(exe), arch)),
            run_replay=replace_run or run_replay)
        with LockRecorder.patch(oracle, locks), \
                mock.patch.dict(sys.modules, {'winsandbox': sandbox}), \
                mock.patch.object(oracle_play, 'WIN_CHECKOUT', build), \
                mock.patch.object(oracle, 'ensure_vanilla', lambda: game), \
                mock.patch.object(oracle, 'prepare_replay_ini',
                                  lambda g: None), \
                mock.patch.object(oracle, 'record_on_host', record_on_host), \
                mock.patch.object(oracle, 'RUNS_DIR',
                                  os.path.join(self.tmp, 'runs')), \
                mock.patch.object(oracle, 'check_dumps', lambda d, schema: {
                    n: [] for n in os.listdir(d)
                    if n.startswith('checkpoint-')}), \
                contextlib.redirect_stdout(io.StringIO()):
            code = oracle.main(argv)
        return code, events, locks, sandbox

    def test_record_in_sandbox_takes_both_locks_in_order(self):
        code, events, locks, sandbox = self.record(
            ['replay', 'record', 'start_screen', '--seed', '3'])
        self.assertEqual(code, 0)
        self.assertEqual(locks.paths, [(oracle.REPLAY_LOCK, 'replay'),
                                       (sandbox.SANDBOX_LOCK, 'sandbox')])
        self.assertEqual(events, [
            ('runtime', 'injector.exe', 'x86'),
            ('sandbox', [oracle.REPLAY_LOCK, sandbox.SANDBOX_LOCK], '3',
             True)])
        self.assertEqual(locks.held, [])
        run = glob.glob(os.path.join(self.tmp, 'runs', '*', 'replay'))[0]
        with open(os.path.join(run, 'replay.json')) as f:
            report = json.load(f)
        self.assertEqual(report['mode'], 'sandbox')
        self.assertEqual(report['sandbox_seconds'], 31.0)
        self.assertEqual(report['missing_checkpoints'], [])

    def test_slow_reads_reach_the_recorder(self):
        seen = []

        def run(build_dir, game_dir, script, env, timeout, host_run,
                out_dir, log):
            seen.append(env.get('IWD2R_TEST_SLOW_READS'))
            os.makedirs(out_dir)
            return {'code': None, 'stdout': '', 'stderr': '',
                    'error': 'stop', 'seconds': 1.0}
        self.record(['replay', 'record', 'start_screen',
                     '--test-slow-reads', '15'], run)
        self.assertEqual(seen, ['15'])

    def test_host_switch(self):
        code, events, locks, _ = self.record(
            ['replay', 'record', 'start_screen', '--host'])
        self.assertEqual(code, 0)
        self.assertEqual(events, [('host', [oracle.REPLAY_LOCK])])
        self.assertEqual(locks.paths, [(oracle.REPLAY_LOCK, 'replay')])
        run = glob.glob(os.path.join(self.tmp, 'runs', '*', 'replay'))[0]
        with open(os.path.join(run, 'replay.json')) as f:
            self.assertEqual(json.load(f)['mode'], 'host')

    def test_compare_passes_the_host_switch(self):
        from unittest import mock
        seen = []
        with mock.patch.object(oracle, 'replay_compare',
                               lambda a: seen.append(a.host) or 0), \
                LockRecorder.patch(oracle, LockRecorder()):
            oracle.main(['replay', 'compare', 'chargen_replay'])
            oracle.main(['replay', 'compare', 'chargen_replay', '--host'])
        self.assertEqual(seen, [False, True])

    def test_sandbox_error_fails_the_run(self):
        def failing(build_dir, game_dir, script, env, timeout, host_run,
                    out_dir, log):
            os.makedirs(out_dir)
            return {'code': None, 'stdout': '', 'stderr': '',
                    'error': 'sandbox timeout: no done.txt after 420 s',
                    'seconds': 420.0}
        code, _, _, _ = self.record(['replay', 'record', 'start_screen'],
                                    failing)
        self.assertEqual(code, oracle.INJECTOR_TIMEOUT)
        run = glob.glob(os.path.join(self.tmp, 'runs', '*', 'replay'))[0]
        with open(os.path.join(run, 'replay.json')) as f:
            report = json.load(f)
        self.assertEqual(report['sandbox_error'],
                         'sandbox timeout: no done.txt after 420 s')
        self.assertTrue(report['outcome'].startswith(
            'sandbox: sandbox timeout'))
        self.assertEqual(report['missing_checkpoints'], ['start'])


class OracleReplayTest(unittest.TestCase):
    """The pure parts of tools/oracle.py replay record."""

    def test_mute_ini_zeroes_every_volume_key(self):
        text = ('Volume Movie=85\r\nVolume Music=60\r\nVolume Voices=100\r\n'
                'Volume Ambients=70\r\nVolume SFX=80\r\nOther=1\r\n')
        self.assertEqual(oracle.mute_ini(text),
                         'Volume Movie=0\r\nVolume Music=0\r\nVolume Voices=0\r\n'
                         'Volume Ambients=0\r\nVolume SFX=0\r\nOther=1\r\n')
        self.assertEqual(oracle.mute_ini('Resolution=800\r\n'), 'Resolution=800\r\n')
        self.assertIn('Volume SFX=0', oracle.replay_ini(
            'Full Screen=1\r\nResolution=1366\r\nVolume SFX=80\r\n'))

    def test_replay_ini(self):
        text = '[Program Options]\r\nFull Screen=1\r\nResolution=1366\r\n' \
            'Last Full Screen=1\r\n'
        self.assertEqual(oracle.replay_ini(text),
                         '[Program Options]\r\nFull Screen=0\r\n'
                         'Resolution=800\r\nLast Full Screen=1\r\n')
        self.assertEqual(oracle.replay_ini(oracle.replay_ini(text)),
                         oracle.replay_ini(text))
        for broken in ('Resolution=800\n', 'Full Screen=1\n'):
            with self.assertRaises(ValueError):
                oracle.replay_ini(broken)

    def test_prepare_replay_ini_guards_and_writes_the_copy(self):
        from unittest import mock
        with tempfile.TemporaryDirectory() as tmp:
            game = os.path.join(tmp, 'game')
            os.makedirs(game)
            ini = os.path.join(game, 'Icewind2.ini')
            with open(ini, 'wb') as f:
                f.write(b'Full Screen=1\r\nResolution=800\r\n')
            calls = []
            with mock.patch.object(oracle, 'assert_safe_target',
                                   lambda g, i: calls.append((g, i))), \
                    contextlib.redirect_stdout(io.StringIO()):
                oracle.prepare_replay_ini(game, '/install')
            self.assertEqual(calls, [(game, '/install')])
            with open(ini, 'rb') as f:
                self.assertEqual(f.read(),
                                 b'Full Screen=0\r\nResolution=800\r\n')

            def refuse(g, i):
                raise SystemExit('refused')
            with open(ini, 'wb') as f:
                f.write(b'Full Screen=1\r\nResolution=800\r\n')
            with mock.patch.object(oracle, 'assert_safe_target', refuse):
                with self.assertRaises(SystemExit):
                    oracle.prepare_replay_ini(game, '/install')
            with open(ini, 'rb') as f:
                self.assertEqual(f.read(),
                                 b'Full Screen=1\r\nResolution=800\r\n')

    def test_parse_status(self):
        self.assertEqual(oracle.parse_status(
            'frame=21046\nscript_line=5\nstate=running\n'),
            {'frame': 21046, 'script_line': 5, 'state': 'running'})
        self.assertEqual(oracle.parse_status(''), {})

    def test_replay_outcome(self):
        running = {'frame': 21046, 'script_line': 5, 'state': 'running'}
        self.assertEqual(oracle.replay_outcome(10, running, 20), (
            10, 'timed out after 20 s; the game was killed (last frame '
            '21046, scenario line 5)'))
        self.assertEqual(oracle.replay_outcome(None, {}, 20)[0], 10)
        self.assertIn('last frame ?', oracle.replay_outcome(None, {}, 20)[1])
        done = {'frame': 61, 'script_line': 0, 'state': 'finished'}
        self.assertEqual(oracle.replay_outcome(0, done, 180), (
            0, 'the game exited with code 0 (last frame 61, scenario '
            'finished)'))
        self.assertEqual(oracle.replay_outcome(0, running, 180)[0], 1)
        self.assertEqual(oracle.replay_outcome(11, done, 180)[0], 1)
        self.assertEqual(oracle.replay_outcome(12, {}, 180), (
            1, 'the recorder did not load (see recorder.log)'))
        self.assertEqual(oracle.replay_outcome(3, done, 180)[0], 1)

    def test_check_dumps(self):
        schema = {'type': 'object', 'required': ['frame']}
        with tempfile.TemporaryDirectory() as tmp:
            for name, text in (('frame-30.json', '{"frame": 30}'),
                               ('checkpoint-a.json', '{}'),
                               ('checkpoint-b.json', '{'),
                               ('replay.json', '{}'),
                               ('status.txt', 'frame=1')):
                with open(os.path.join(tmp, name), 'w') as f:
                    f.write(text)
            out = oracle.check_dumps(tmp, schema)
        self.assertEqual(sorted(out), ['checkpoint-a.json',
                                       'checkpoint-b.json', 'frame-30.json'])
        self.assertEqual(out['frame-30.json'], [])
        self.assertEqual(out['checkpoint-a.json'],
                         ["$: missing required property 'frame'"])
        self.assertTrue(out['checkpoint-b.json'][0].startswith('not JSON'))

    def test_scenario_path(self):
        self.assertEqual(oracle.scenario_path('start_screen'),
                         os.path.join(ROOT, 'scenarios', 'start_screen.txt'))
        hang = os.path.join(ROOT, 'oracle', 'replay', 'hang.txt')
        self.assertEqual(oracle.scenario_path(hang), hang)
        self.assertEqual(realinput.load_scenario(hang), [
            {'op': 'wait', 'frames': 100000, 'line': 5},
            {'op': 'quit', 'line': 6}])
        with self.assertRaises(SystemExit):
            oracle.scenario_path('no_such_scenario')

    def test_invariant(self):
        def dump(frame, au, stalled=0, reinit=0):
            return {'frame': frame, 'globals': {
                'au_counter': au, 'ai_stalled_frames': stalled,
                'reinitializing': reinit}}
        self.assertIsNone(oracle.invariant_problem('c', dump(61, 61)))
        # The calls a load draws from inside an AI update are held, not
        # counted (FrameGate): au_counter equals frame, and held_frames
        # counts them apart.
        held = dump(156, 156)
        held['globals'].update(held_frames=15, loads=1)
        self.assertIsNone(oracle.invariant_problem('c', held))
        self.assertEqual(oracle.invariant_problem('c', dump(61, 40, 21)),
                         'c: 21 frames by frame 61 had no AI update '
                         '(au_counter 40): the AI thread stalled')
        self.assertIn('None frames', oracle.invariant_problem(
            'c', {'frame': 1, 'globals': {}}))
        self.assertEqual(oracle.invariant_problem('c', dump(61, 61, 0, 1)),
                         'c: reinitializing is 1 at frame 61: the game '
                         'stopped updating (a focus loss?)')
        # A checkpoint inside a load: the progress bar up, or the AI thread
        # inside an update.
        loading = dump(61, 61)
        loading['globals'].update(loading=1, in_async_update=0)
        self.assertEqual(oracle.invariant_problem('c', loading),
                         'c: a load is in progress at frame 61 (loading 1, '
                         'in_async_update 0): the checkpoint lands inside it')
        loading['globals'].update(loading=0, in_async_update=1)
        self.assertIn('a load is in progress',
                      oracle.invariant_problem('c', loading))
        loading['globals'].update(in_async_update=0, loads=3)
        self.assertIsNone(oracle.invariant_problem('c', loading))
        with tempfile.TemporaryDirectory() as tmp:
            for name, d in (('checkpoint-a.json', dump(5, 5)),
                            ('checkpoint-b.json', dump(9, 3, 6)),
                            ('frame-30.json', dump(30, 1, 29))):
                with open(os.path.join(tmp, name), 'w') as f:
                    json.dump(d, f)
            dumps = {'checkpoint-a.json': [], 'checkpoint-b.json': [],
                     'frame-30.json': []}
            self.assertEqual(oracle.invariant_problems(tmp, dumps), [
                'checkpoint-b.json: 6 frames by frame 9 had no AI update '
                '(au_counter 3): the AI thread stalled'])
            # A dump that failed the schema is reported there, not here.
            dumps['checkpoint-b.json'] = ['bad']
            self.assertEqual(oracle.invariant_problems(tmp, dumps), [])

    def test_missing_checkpoints(self):
        dumps = {'checkpoint-menu.json': [], 'frame-30.json': []}
        self.assertEqual(oracle.missing_checkpoints(['menu', 'chargen'],
                                                    dumps), ['chargen'])
        self.assertEqual(oracle.missing_checkpoints([], dumps), [])

    def test_record_arguments(self):
        for args in (['--timeout', '0'], ['--timeout', '-5'],
                     ['--seed', '-1'], ['--seed', '4294967296'],
                     ['--dump-every', '-1'], ['--test-steal-focus', '0']):
            with self.assertRaises(SystemExit, msg=args), \
                    contextlib.redirect_stderr(io.StringIO()):
                oracle.main(['replay', 'record', 'start_screen'] + args)
        self.assertEqual(oracle.seed_arg('4294967295'), 4294967295)
        self.assertEqual(oracle.positive_int('20'), 20)
        self.assertEqual(oracle.non_negative_int('0'), 0)

    def test_scenario_checkpoint_names(self):
        self.assertEqual(realinput.parse_scenario(
            'checkpoint Party_formation-2\n'),
            [{'op': 'checkpoint', 'name': 'Party_formation-2', 'line': 1}])
        actions = realinput.load_scenario(
            os.path.join(ROOT, 'scenarios', 'replay_click.txt'))
        self.assertEqual([a['name'] for a in actions
                          if a['op'] == 'checkpoint'],
                         ['menu', 'select_party', 'party_formation',
                          'chargen'])

    def test_input_settings_shared(self):
        # Our engine (platform/window.cpp) and the recorder answer the
        # input settings from the one header, never with literals.
        with open(os.path.join(ROOT, 'platform', 'input_settings.h')) as f:
            names = re.findall(r'constexpr \w+ (k\w+) = ', f.read())
        self.assertEqual(names, [
            'kDoubleClickTime', 'kDoubleClickWidth', 'kDoubleClickHeight',
            'kSwapButton', 'kKeyboardDelay', 'kKeyboardSpeed',
            'kWheelScrollLines'])
        for rel in ('platform/window.cpp', 'oracle/replay/recorder.cpp'):
            with open(os.path.join(ROOT, rel)) as f:
                text = f.read()
            self.assertIn('#include "platform/input_settings.h"', text, rel)
            for name in names:
                self.assertIn('platform::input_settings::%s' % name, text,
                              '%s does not use %s' % (rel, name))
        with open(os.path.join(ROOT, 'platform', 'window.cpp')) as f:
            text = f.read()
        self.assertIn('UINT GetDoubleClickTime() { return '
                      'platform::input_settings::kDoubleClickTime; }', text)

    def test_start_screen_has_its_checkpoint(self):
        actions = realinput.load_scenario(
            os.path.join(ROOT, 'scenarios', 'start_screen.txt'))
        self.assertIn({'op': 'checkpoint', 'name': 'start', 'line': 4},
                      actions)


class OracleReplayCompareTest(unittest.TestCase):
    """Our engine's state dump and tools/oracle.py replay compare."""

    def schema(self):
        with open(os.path.join(ROOT, 'oracle', 'replay', 'schema.json')) as f:
            return json.load(f)

    def test_golden_dumps_validate(self):
        # tests/test_state_dump.cpp requires the writer to produce exactly
        # these files; here they must satisfy the recorder's schema.
        schema = self.schema()
        paths = sorted(glob.glob(os.path.join(ROOT, 'tests', 'fixtures',
                                              'state_dump_*.json')))
        self.assertEqual([os.path.basename(p) for p in paths], [
            'state_dump_chargen.json', 'state_dump_menu.json',
            'state_dump_periodic.json', 'state_dump_world.json'])
        for path in paths:
            with open(path) as f:
                dump = json.load(f)
            self.assertEqual(minischema.validate(dump, schema), [], path)

    def test_screens(self):
        self.assertEqual(oracle.load_screens(),
                         ['connection', 'create_char', 'world'])
        enum = self.schema()['properties']['screen']['enum']
        for screen in oracle.load_screens():
            self.assertIn(screen, enum)

    def test_known_differences_file(self):
        self.assertEqual(oracle.load_known(), [])
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'k.json')
            good = [{'field': 'globals.ai_rng_state', 'checkpoint': '*',
                     'reason': 'r', 'evidence': '0x72DE60'}]
            with open(path, 'w') as f:
                json.dump(good, f)
            self.assertEqual(oracle.load_known(path), good)
            for bad in ({}, [{'field': 'x'}],
                        [dict(good[0], reason='')],
                        [dict(good[0], extra='x')]):
                with open(path, 'w') as f:
                    json.dump(bad, f)
                with self.assertRaises(SystemExit), \
                        contextlib.redirect_stderr(io.StringIO()):
                    oracle.load_known(path)

    def test_flatten_and_differences(self):
        dump = {'frame': 5, 'checkpoint': 'a', 'screen': 'create_char',
                'globals': {'au_counter': 4, 'ai_stalled_frames': 0,
                            'in_async_update': 0, 'ai_rng_state': 7},
                'chargen': {'abilities': [1, 2], 'portrait': None}}
        self.assertEqual(oracle.flatten(dump), {
            'frame': 5, 'checkpoint': 'a', 'screen': 'create_char',
            'globals.au_counter': 4, 'globals.ai_stalled_frames': 0,
            'globals.in_async_update': 0, 'globals.ai_rng_state': 7,
            'chargen.abilities[0]': 1, 'chargen.abilities[1]': 2,
            'chargen.portrait': None})
        other = json.loads(json.dumps(dump))
        # Timing is reported, never compared (ruling R13c).
        other.update(frame=9, checkpoint='b')
        other['globals'].update(au_counter=1, ai_stalled_frames=3,
                                in_async_update=1, held_frames=4)
        self.assertEqual(oracle.field_differences(dump, other), [])
        other['globals']['ai_rng_state'] = 8
        other['chargen']['abilities'] = [1, 3, 4]
        other['chargen'] = dict(other['chargen'], portrait='X')
        self.assertEqual(oracle.field_differences(dump, other), [
            ('chargen.abilities[1]', 2, 3),
            ('chargen.abilities[2]', '<missing>', 4),
            ('chargen.portrait', None, 'X'),
            ('globals.ai_rng_state', 7, 8)])

    def write_dumps(self, folder, dumps):
        os.makedirs(folder)
        for name, dump in dumps.items():
            with open(os.path.join(folder, 'checkpoint-%s.json' % name),
                      'w') as f:
                json.dump(dump, f)

    def dump(self, screen, rng=1, au=10, frame=10):
        return {'frame': frame, 'screen': screen, 'rng_state': rng,
                'globals': {'au_counter': au, 'ai_stalled_frames': 0,
                            'in_async_update': 0, 'reinitializing': 0,
                            'fullscreen': 0, 'loading': 0, 'loads': 1,
                            'held_frames': 2, 'ai_rng_state': 3},
                'chargen': None}

    def test_compare_checkpoints(self):
        screens = ['connection', 'create_char']
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'original')
            b = os.path.join(tmp, 'ours')
            self.write_dumps(a, {'menu': self.dump('connection'),
                                 'party': self.dump('single_player', rng=1),
                                 'chargen': self.dump('create_char', rng=5),
                                 'known': self.dump('create_char', rng=6),
                                 'gone': self.dump('connection')})
            self.write_dumps(b, {'menu': self.dump('connection', au=9),
                                 'party': self.dump('single_player', rng=2),
                                 'chargen': self.dump('create_char', rng=7),
                                 'known': self.dump('create_char', rng=8)})
            known = [{'field': 'rng_state', 'checkpoint': 'known',
                      'reason': 'project 3', 'evidence': '0x72DE60'}]
            lines, failures, hits = oracle.compare_checkpoints(
                a, b, ['menu', 'party', 'chargen', 'known', 'gone'], screens,
                known)
        self.assertEqual(failures, 2)
        self.assertEqual([h[:4] for h in hits], [('known', 'rng_state', 6,
                                                  8)])
        self.assertEqual(lines, [
            'checkpoint menu (connection): match (frame 10/10, au_counter '
            '10/9, ai_stalled_frames 0/0, in_async_update 0/0, held_frames '
            '2/2; original/ours)',
            'checkpoint party (single_player): different, screen not '
            'enabled (frame 10/10, au_counter 10/10, ai_stalled_frames 0/0, '
            'in_async_update 0/0, held_frames 2/2; original/ours)',
            '  diff rng_state: original 1, ours 2',
            'checkpoint chargen (create_char): DIFFERENT (frame 10/10, '
            'au_counter 10/10, ai_stalled_frames 0/0, in_async_update 0/0, '
            'held_frames 2/2; original/ours)',
            '  DIFF rng_state: original 5, ours 7',
            'checkpoint known (create_char): match (frame 10/10, au_counter '
            '10/10, ai_stalled_frames 0/0, in_async_update 0/0, held_frames '
            '2/2; original/ours)',
            '  KNOWN rng_state: original 6, ours 8 (project 3; evidence '
            '0x72DE60)',
            'checkpoint gone: MISSING on our side'])

    def test_known_entry_wildcard(self):
        known = [{'field': 'f', 'checkpoint': '*', 'reason': 'r',
                  'evidence': 'e'}]
        self.assertIs(oracle.known_entry(known, 'any', 'f'), known[0])
        self.assertIsNone(oracle.known_entry(known, 'any', 'g'))

    def test_our_env(self):
        base = {'PATH': '/bin', 'IWD2R_SMOKE_FRAMES': '5',
                'IWD2R_CONTROL_DIR': '/x'}
        env = oracle.our_env(base, '/game', 'scenarios/s.txt', 'runs/r/ours',
                             7, 30)
        self.assertNotIn('IWD2R_SMOKE_FRAMES', env)
        self.assertNotIn('IWD2R_CONTROL_DIR', env)
        self.assertEqual(env['IWD2_DATA'], '/game')
        self.assertEqual(env['SDL_VIDEO_DRIVER'], 'dummy')
        self.assertEqual(env['IWD2R_SCRIPT'],
                         os.path.abspath('scenarios/s.txt'))
        self.assertEqual(env['IWD2R_STATE_DUMP'],
                         os.path.abspath('runs/r/ours'))
        self.assertEqual(env['IWD2R_SHOT_DIR'], env['IWD2R_STATE_DUMP'])
        self.assertEqual(env['IWD2R_SEED'], '7')
        self.assertEqual(env['IWD2R_DUMP_EVERY'], '30')
        self.assertEqual(env['IWD2R_INI_OVERRIDE'],
                         'Program Options/Resolution=800;'
                         'Program Options/Full Screen=0')
        self.assertEqual(base['IWD2R_SMOKE_FRAMES'], '5')

    def test_our_outcome(self):
        done = {'frame': 951, 'script_line': 0, 'state': 'finished'}
        self.assertEqual(oracle.our_outcome(0, done, 180), (
            0, 'our engine exited with code 0 (last frame 951, scenario '
            'finished)'))
        running = {'frame': 40, 'script_line': 9, 'state': 'running'}
        self.assertEqual(oracle.our_outcome(None, running, 20)[0], 10)
        self.assertEqual(oracle.our_outcome(0, running, 20)[0], 1)
        self.assertEqual(oracle.our_outcome(5, {}, 20), (
            1, 'our engine exited with code 5 (last frame ?)'))

    def test_compare_counts(self):
        screens = ['create_char']
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'original')
            b = os.path.join(tmp, 'ours')
            self.write_dumps(a, {'menu': self.dump('connection', rng=1),
                                 'party': self.dump('single_player', rng=1),
                                 'same': self.dump('single_player'),
                                 'chargen': self.dump('create_char')})
            self.write_dumps(b, {'menu': self.dump('connection', rng=2),
                                 'party': self.dump('single_player', rng=2),
                                 'same': self.dump('single_player'),
                                 'chargen': self.dump('create_char')})
            counts = {}
            lines, failures, hits = oracle.compare_checkpoints(
                a, b, ['menu', 'party', 'same', 'chargen'], screens, [],
                counts)
        self.assertEqual(failures, 0)
        self.assertEqual(counts, {'enabled': 1, 'outside': 2})

    def test_compare_verdict(self):
        self.assertEqual(oracle.compare_verdict(
            0, 0, 0, [], {'enabled': 18, 'outside': 0}),
            ['result: MATCH at 18 of 18 checkpoints'])
        self.assertEqual(oracle.compare_verdict(
            0, 0, 0, [], {'enabled': 18, 'outside': 2}),
            ['result: MATCH at 18 of 18 checkpoints',
             'note: 2 checkpoints differed outside the enabled screens'])
        self.assertEqual(oracle.compare_verdict(
            0, 0, 0, [('cp', 'f', 1, 2, {})], {'enabled': 3, 'outside': 1}),
            ['result: MATCH at 3 of 3 checkpoints, with 1 known difference',
             'note: 1 checkpoint differed outside the enabled screens'])
        self.assertEqual(oracle.compare_verdict(
            2, 0, 1, [], {'enabled': 3, 'outside': 0}),
            ['result: DIFFERENT: 2 differences; a run failed (original 0, '
             'ours 1)'])

    def test_run_arguments(self):
        for args in (['--seed', '-1'], ['--seed', '4294967296'],
                     ['--seed', 'x'], ['--iterations', '0'],
                     ['--iterations', '-5'], ['--iterations', '1.5']):
            with self.assertRaises(SystemExit, msg=args), \
                    contextlib.redirect_stderr(io.StringIO()):
                oracle.main(['run'] + args)

    def test_compare_arguments(self):
        for args in (['--timeout', '0'], ['--seed', '-1'],
                     ['--dump-every', 'x']):
            with self.assertRaises(SystemExit, msg=args), \
                    contextlib.redirect_stderr(io.StringIO()):
                oracle.main(['replay', 'compare', 'chargen_replay'] + args)

    def test_chargen_scenario(self):
        actions = realinput.load_scenario(
            os.path.join(ROOT, 'scenarios', 'chargen_replay.txt'))
        names = [a['name'] for a in actions if a['op'] == 'checkpoint']
        self.assertEqual(names[:2], ['menu', 'chargen'])
        self.assertEqual(names[-1], 'abilities_done')
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(actions[-1], {'op': 'quit', 'line':
                                       actions[-1]['line']})

    def test_replay_clock_shared(self):
        # Our replay mode (platform/replay.cpp, platform/clock.cpp) and the
        # recorder run the one clock, from the one header.
        with open(os.path.join(ROOT, 'platform', 'replay_clock.h')) as f:
            header = f.read()
        values = dict(re.findall(r'constexpr \w+ (k\w+) = (\w+);', header))
        self.assertEqual(values, {
            'kClockBase': '0x01000000', 'kFramesPerSecond': '30',
            'kWallClockYear': '2002', 'kWallClockMonth': '8',
            'kWallClockDay': '27', 'kWallClockEpoch': '1030406400'})
        import calendar
        self.assertEqual(calendar.timegm((2002, 8, 27, 0, 0, 0)),
                         int(values['kWallClockEpoch']))
        uses = {
            'oracle/replay/recorder.cpp': [
                'replay_clock::kClockBase', 'replay_clock::kFramesPerSecond',
                'replay_clock::TickAfterFrame', 'replay_clock::kWallClockYear',
                'replay_clock::kWallClockMonth',
                'replay_clock::kWallClockDay'],
            'platform/replay.cpp': [
                'clock::kClockBase', 'clock::TickAfterFrame',
                'clock::WallClockSeconds'],
        }
        for rel, names in uses.items():
            with open(os.path.join(ROOT, rel)) as f:
                text = f.read()
            self.assertIn('#include "platform/replay_clock.h"', text, rel)
            for name in names:
                self.assertIn(name, text, '%s does not use %s' % (rel, name))
            self.assertNotIn('0x01000000', text, rel)
            self.assertIsNone(re.search(r'wYear = \d', text), rel)
        with open(os.path.join(ROOT, 'platform', 'clock.cpp')) as f:
            text = f.read()
        self.assertIn('platform::replay::NowMs()', text)
        self.assertIn('platform::replay::SetTimer(', text)


class KnownDifferencesTest(unittest.TestCase):
    """tools/oracle.py replay compare: known_differences.json checks."""

    def write(self, entries):
        folder = tempfile.mkdtemp()
        self.addCleanup(__import__('shutil').rmtree, folder)
        path = os.path.join(folder, 'known.json')
        with open(path, 'w') as f:
            json.dump(entries, f)
        return path

    def entry(self, evidence='docs/evidence/README.md', field='a.b',
              checkpoint='*'):
        return {'field': field, 'checkpoint': checkpoint, 'reason': 'r',
                'evidence': evidence}

    def test_existing_evidence_loads(self):
        known = oracle.load_known(self.write([self.entry()]))
        self.assertEqual(len(known), 1)

    def test_missing_evidence_file_is_refused(self):
        path = self.write([self.entry(evidence='docs/evidence/NOPE.md')])
        with self.assertRaises(SystemExit) as raised:
            oracle.load_known(path)
        self.assertIn('NOPE.md', str(raised.exception))

    def test_evidence_by_address(self):
        oracle.load_known(self.write([self.entry(evidence='0x72DE60')]))
        with self.assertRaises(SystemExit):
            oracle.load_known(self.write([self.entry(evidence='0x1')]))

    def test_committed_file_loads(self):
        oracle.load_known()

    def test_stale_entries_are_the_unmatched_ones(self):
        used = self.entry(field='x')
        unused = self.entry(field='y')
        hits = [('cp', 'x', 1, 2, used)]
        self.assertEqual(oracle.stale_known([used, unused], hits), [unused])
        self.assertEqual(oracle.stale_known([used], hits), [])
        self.assertEqual(oracle.stale_known([], []), [])

    def test_known_hit_is_not_stale_end_to_end(self):
        folder = tempfile.mkdtemp()
        self.addCleanup(__import__('shutil').rmtree, folder)
        dirs = []
        for side, value in (('o', 1), ('u', 2)):
            d = os.path.join(folder, side)
            os.mkdir(d)
            with open(os.path.join(d, 'checkpoint-cp.json'), 'w') as f:
                json.dump({'screen': 'connection', 'v': value}, f)
            dirs.append(d)
        known = [self.entry(field='v', checkpoint='cp'),
                 self.entry(field='w', checkpoint='cp')]
        lines, failures, hits = oracle.compare_checkpoints(
            dirs[0], dirs[1], ['cp'], ['connection'], known)
        self.assertEqual(failures, 0)
        self.assertEqual(oracle.stale_known(known, hits), [known[1]])


class DataViewTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.data = os.path.join(self.tmp, 'install')
        for rel, text in (('CHITIN.KEY', 'k'), ('Data/a.bif', 'b'),
                          ('icewind2.ini', 'a=1'), ('Keymap.INI', 'k=1'),
                          ('Temp/stale', 's'), ('cache/c', 'c'),
                          ('mpsave/default/s.sav', 'sv'),
                          ('Characters/A.CHR', 'chr'),
                          ('Sounds/x.wav', 'w'), ('Scripts/x.bs', 's')):
            path = os.path.join(self.data, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w') as f:
                f.write(text)

    def read(self, *parts):
        with open(os.path.join(*parts)) as f:
            return f.read()

    def test_rules_file_lists_the_writable_entries(self):
        rules = dataview.load_rules()
        self.assertEqual(rules['empty_dirs'], [
            'temp', 'tempsave', 'cache', 'save', 'ScrnShot'])
        self.assertEqual(rules['copied_dirs'], ['mpsave', 'Characters'])
        self.assertEqual(rules['copied_files'], ['*.ini'])

    def test_read_only_entries_are_links_writable_ones_are_real(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        for name in ('CHITIN.KEY', 'Data', 'Sounds', 'Scripts'):
            self.assertTrue(os.path.islink(os.path.join(view, name)), name)
        for name in ('Temp', 'cache', 'tempsave', 'save', 'ScrnShot'):
            path = os.path.join(view, name)
            self.assertTrue(os.path.isdir(path) and not os.path.islink(path),
                            name)
            self.assertEqual(os.listdir(path), [], name)
        for name in ('mpsave', 'Characters'):
            self.assertFalse(os.path.islink(os.path.join(view, name)), name)
        self.assertEqual(self.read(view, 'mpsave/default/s.sav'), 'sv')
        self.assertEqual(self.read(view, 'Characters/A.CHR'), 'chr')

    def test_inis_are_copied_not_linked(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        for name in ('icewind2.ini', 'Keymap.INI'):
            self.assertFalse(os.path.islink(os.path.join(view, name)))
        with open(os.path.join(view, 'icewind2.ini'), 'w') as f:
            f.write('changed')
        self.assertEqual(self.read(self.data, 'icewind2.ini'), 'a=1')

    def test_engine_writes_stay_in_the_view(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        with open(os.path.join(view, 'Temp', 't'), 'w') as f:
            f.write('t')
        os.remove(os.path.join(view, 'mpsave/default/s.sav'))
        self.assertEqual(os.listdir(os.path.join(self.data, 'Temp')),
                         ['stale'])
        self.assertTrue(os.path.exists(
            os.path.join(self.data, 'mpsave/default/s.sav')))

    def test_make_view_is_idempotent(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        with open(os.path.join(view, 'icewind2.ini'), 'w') as f:
            f.write('changed')
        with open(os.path.join(view, 'Temp', 't'), 'w') as f:
            f.write('t')
        again = dataview.make_view(self.data, view)
        self.assertEqual(again, view)
        self.assertEqual(self.read(view, 'icewind2.ini'), 'changed')
        self.assertEqual(os.listdir(os.path.join(view, 'Temp')), ['t'])
        self.assertTrue(os.path.islink(os.path.join(view, 'Data')))

    def test_missing_writable_folders_are_created(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        for name in ('tempsave', 'save', 'ScrnShot'):
            self.assertTrue(os.path.isdir(os.path.join(view, name)), name)

    def test_missing_copied_folders_are_created_empty(self):
        shutil.rmtree(os.path.join(self.data, 'mpsave'))
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        path = os.path.join(view, 'mpsave')
        self.assertTrue(os.path.isdir(path) and not os.path.islink(path))
        self.assertEqual(os.listdir(path), [])
        self.assertFalse(os.path.exists(os.path.join(self.data, 'mpsave')))

    def test_refuses_a_view_inside_the_data_folder(self):
        with self.assertRaises(ValueError):
            dataview.make_view(self.data, os.path.join(self.data, 'view'))
        with self.assertRaises(ValueError):
            dataview.make_view(self.data, self.tmp)
        self.assertFalse(os.path.exists(os.path.join(self.data, 'view')))

    def test_remove_view_leaves_the_install_alone(self):
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'))
        dataview.remove_view(view)
        self.assertFalse(os.path.exists(view))
        self.assertEqual(self.read(self.data, 'Data/a.bif'), 'b')
        self.assertEqual(self.read(self.data, 'cache/c'), 'c')
        self.assertEqual(self.read(self.data, 'mpsave/default/s.sav'), 'sv')
        dataview.remove_view(view)

    def test_windows_drive_detection(self):
        self.assertTrue(dataview.on_windows_drive('/mnt/c/x/y'))
        self.assertFalse(dataview.on_windows_drive('/mnt/wsl/x'))
        self.assertFalse(dataview.on_windows_drive('/tmp/x'))

    def test_windows_links_are_junctions_and_hard_links(self):
        calls = []
        orig = dataview._junction
        dataview._junction = lambda target, link: (
            calls.append((target, link)), os.symlink(target, link))
        self.addCleanup(setattr, dataview, '_junction', orig)
        view = dataview.make_view(self.data, os.path.join(self.tmp, 'view'),
                                  windows=True)
        self.assertEqual(sorted(os.path.basename(c[1]) for c in calls),
                         ['Data', 'Scripts', 'Sounds'])
        key = os.path.join(view, 'CHITIN.KEY')
        self.assertFalse(os.path.islink(key))
        self.assertTrue(os.path.samefile(
            key, os.path.join(self.data, 'CHITIN.KEY')))


if __name__ == '__main__':
    unittest.main()
