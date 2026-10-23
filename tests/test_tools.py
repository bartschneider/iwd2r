"""Tests for tools/gen_map.py, tools/check_evidence.py, tools/gen_fields.py,
tools/minischema.py, tools/shaders.py, tools/xrec.py, tools/wso_fetch.py,
tools/wave.py and the pure parts of tools/play.py, tools/realinput.py,
tools/winsandbox.py, tools/oracle.py, tools/ghidra/labels.py and the project
bookkeeping of tools/ghidra/make_dll_project.py.

Run from the repository root: python3 -I tests/test_tools.py

Each test builds a throwaway git repository in a temporary directory, runs the
checker as a subprocess with cwd set to that repository, and removes the
directory afterwards.
"""

import contextlib
import datetime
import fcntl
import glob
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import signal
import struct
import subprocess
from unittest import mock
import sys
import tempfile
import threading
import time
import types
import unittest
import zipfile
import xml.etree.ElementTree as ET
import zlib

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
winhome = load_tool('winhome')
xrec = load_tool('xrec')
wso_fetch = load_tool('wso_fetch')
fn_digest = load_tool('fn_digest')
src_find = load_tool('src_find')
assemble_context = load_tool('assemble_context')
token_audit = load_tool('token_audit')
bg2_find = load_tool('bg2_find')
gltrace = load_tool('gltrace')
truth = load_tool('truth')
triage = load_tool('triage')
driver = load_tool('driver')


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
        # No --timeout: each scenario's own (scenario_timeout).
        a = play.parse_args(['--vm', 'all', '--suite'])
        self.assertEqual((a.vm, a.suite, a.jobs, a.timeout),
                         ('all', True, 1, None))
        a = play.parse_args(['--vm', 'windows', '--script', 's.txt',
                             '--timeout', '60'])
        self.assertEqual((a.vm, a.script, a.timeout), ('windows', 's.txt', 60))

    def test_scenario_timeout(self):
        with tempfile.TemporaryDirectory() as tmp:
            own = os.path.join(tmp, 'own.txt')
            with open(own, 'w') as f:
                f.write('# A long one.\n# timeout: 420\nwait 1\n')
            plain = os.path.join(tmp, 'plain.txt')
            with open(plain, 'w') as f:
                f.write('# timeout comes later\nwait 1\n# timeout: 9\n')
            # --timeout wins; else the header's; else the default.
            self.assertEqual(play.scenario_timeout(None, own), 420)
            self.assertEqual(play.scenario_timeout(60, own), 60)
            self.assertEqual(play.scenario_timeout(None, plain),
                             realinput.DEFAULT_TIMEOUT)
        # The scenarios that run longer than the default say so.
        self.assertGreaterEqual(play.scenario_timeout(None, os.path.join(
            ROOT, 'scenarios', 'walk_and_talk.txt')), 400)

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
        self.assertFalse([p for p in play.suite_files()
                          if os.sep + 'pending' + os.sep in p])

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

    def test_header_timeout(self):
        t = realinput.header_timeout
        self.assertEqual(t('# timeout: 420\nwait 1\n'), 420)
        self.assertEqual(t('# About it.\n\n#  Timeout:  300 \r\nwait 1\n'),
                         300)
        self.assertIsNone(t('# About it.\nwait 1\n'))
        # Only the header (the comment lines before the first command).
        self.assertIsNone(t('wait 1\n# timeout: 420\n'))
        self.assertIsNone(t('# the timeout is long\nwait 1\n'))
        for text, message in (
                ('# x\n# timeout: 0\n',
                 'line 2: timeout needs a positive number of seconds'),
                ('# timeout: soon\n',
                 'line 1: timeout needs a positive number of seconds'),
                ('# timeout: 1\n# timeout: 2\n',
                 'line 2: duplicate timeout (first on line 1)')):
            with self.assertRaises(realinput.ScenarioError, msg=text) as cm:
                t(text)
            self.assertEqual(str(cm.exception), message)
        # The scenario parsers take the header for a comment.
        self.assertEqual(realinput.parse_scenario('# timeout: 420\nquit\n'),
                         [{'op': 'quit', 'line': 2}])

    def test_shipped_scenarios_parse(self):
        pending = glob.glob(os.path.join(ROOT, 'scenarios', 'pending',
                                         '*.txt'))
        for path in scenario_files() + pending:
            self.assertTrue(realinput.load_scenario(path), path)
            # A header line that reads like "# timeout: S" must be one.
            realinput.load_header_timeout(path)

    def test_world_entry_checkpoints(self):
        actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'world_entry.txt'))
        names = [a['name'] for a in actions if a['op'] == 'checkpoint']
        chargen = [a['name'] for a in realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'chargen_replay.txt')) if a['op'] == 'checkpoint']
        self.assertEqual(names, chargen + [
            'skills_done', 'feats_done', 'named', 'party_formation',
            'chapter', 'world', 'world_settled'])
        self.assertEqual(actions[-1]['op'], 'quit')

    def test_walk_and_talk_checkpoints(self):
        actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'walk_and_talk.txt'))
        names = [a['name'] for a in actions if a['op'] == 'checkpoint']
        world = [a['name'] for a in realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'world_entry.txt'))
                 if a['op'] == 'checkpoint']
        self.assertEqual(names, world + [
            'opening_dialog', 'opening_done', 'moved', 'dialog',
            'dialog_end', 'reig_dialog', 'jon_dialog', 'reig_done',
            'door_open'])
        self.assertEqual(len(names), len(set(names)))
        # M2 Task 7: a shot of Hedron's dialog panel and of the door.
        self.assertEqual([a['name'] for a in actions if a['op'] == 'shot'],
                         ['dialog', 'door_open'])
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
        # driver.ps1 asks for a state dump at a checkpoint; origin is
        # dropped.
        out = realinput.windows_actions(realinput.parse_scenario(
            'checkpoint a\norigin screen\nwait 2\n'))
        self.assertEqual(out, [{'op': 'checkpoint', 'name': 'a', 'line': 1},
                               {'op': 'wait', 'frames': 2, 'line': 3}])
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
        self.frames = 0
        self.driver = realinput.XDriver(
            7, self.game, self._tmp.name, self._tmp.name, self.lines.append,
            deadline=time.time() + 60, run=self.fake_run,
            sleep=self.fast_engine)
        self.driver.window = 0x50

    def fast_engine(self, seconds):
        """The engine's control channel: a frame and a mouse-button read
        per call; frames.req and input.req answered with the counts."""
        self.frames += 1
        for name, answer in (('frames', 'frames.txt'), ('input', 'input.txt')):
            req = os.path.join(self._tmp.name, name + '.req')
            if os.path.exists(req):
                with open(os.path.join(self._tmp.name, answer), 'w') as f:
                    f.write('%d\n' % self.frames)
                os.remove(req)

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

    def test_type_presses_each_key_with_shift_for_capitals(self):
        # As driver.ps1 does: one key press per character, shift around a
        # capital, each phase waiting for the engine's input reads.
        self.driver.do({'op': 'type', 'text': 'Pr 1', 'line': 1})
        self.assertEqual(self.xdo(), [
            ['keydown', 'Shift_L'], ['keydown', 'p'], ['keyup', 'p'],
            ['keyup', 'Shift_L'],
            ['keydown', 'r'], ['keyup', 'r'],
            ['keydown', 'space'], ['keyup', 'space'],
            ['keydown', '1'], ['keyup', '1']])

    def test_game_exit_ends_the_run(self):
        self.game.code = 0
        self.assertEqual(self.driver.run([{'op': 'wait', 'frames': 1,
                                           'line': 1}]), 'game exited')

    def test_press_and_release_each_reach_a_slow_engine(self):
        # The engine polls input once per AI update and reports only "down"
        # and "pressed since the last poll" (GetAsyncKeyState), so a release
        # and the next press with no poll between them read as one long
        # press: a double click becomes one click. In Windows Sandbox the
        # gaps between polls reached 165 ms, and the portrait double click
        # that centres the view was sometimes lost. Each phase of a click
        # now lasts until the engine has read the mouse buttons again.
        control = os.path.join(self._tmp.name, 'control')
        os.makedirs(control)
        engine = {'polls': 0, 'slept': 0.0}

        def slow_engine(seconds):
            # 10 reads a second: one per 0.1 s of sleeping.
            time.sleep(seconds)
            engine['slept'] += seconds
            if not engine.get('frozen'):
                engine['polls'] = int(engine['slept'] * 10)
            req = os.path.join(control, 'input.req')
            if os.path.exists(req):
                with open(os.path.join(control, 'input.txt'), 'w') as f:
                    f.write('%d\n' % engine['polls'])
                os.remove(req)

        events = []

        def run(cmd, timeout=30):
            if cmd[:2] == ['xdotool', 'search']:
                return 0, '80\n'
            events.append((cmd[1], engine['polls']))
            return 0, ''

        self.driver = realinput.XDriver(
            7, self.game, self._tmp.name, control, self.lines.append,
            deadline=time.time() + 60, run=run, sleep=slow_engine)
        self.driver.window = 80
        self.driver.do({'op': 'click', 'x': 1, 'y': 2, 'line': 1})
        self.driver.do({'op': 'click', 'x': 1, 'y': 2, 'line': 2})
        self.driver.do({'op': 'key', 'key': 'ESCAPE', 'line': 3})
        presses = [e for e in events if e[0] in ('mousedown', 'mouseup',
                                                 'keydown', 'keyup')]
        self.assertEqual([e[0] for e in presses],
                         ['mousedown', 'mouseup'] * 2 + ['keydown', 'keyup'])
        # A read between every two transitions.
        for (_, before), (_, after) in zip(presses, presses[1:]):
            self.assertGreaterEqual(after - before, 1)
        # An engine that never reads the buttons delays a phase by at most
        # POLL_TIMEOUT.
        engine['frozen'] = True
        started = time.time()
        self.driver.do({'op': 'click', 'x': 1, 'y': 2, 'line': 4})
        self.assertLess(time.time() - started,
                        3 * realinput.POLL_TIMEOUT + 1)
        self.assertIn('  the engine did not read the mouse buttons in 1 s',
                      self.lines)

    def test_checkpoint_asks_the_engine_for_a_state_dump(self):
        control = os.path.join(self._tmp.name, 'control')
        os.makedirs(control)
        answered = []

        def engine(seconds):
            # The engine's PollControl: answers state-NAME.req with
            # checkpoint-NAME.json and removes the request.
            for name in os.listdir(control):
                if name.startswith('state-') and name.endswith('.req'):
                    point = name[len('state-'):-len('.req')]
                    with open(os.path.join(self._tmp.name, 'checkpoint-%s.json'
                                           % point), 'w') as f:
                        f.write('{"checkpoint": "%s"}\n' % point)
                    os.remove(os.path.join(control, name))
                    answered.append(point)

        self.driver = realinput.XDriver(
            7, self.game, self._tmp.name, control, self.lines.append,
            deadline=1e12, run=self.fake_run, sleep=engine)
        self.driver.do({'op': 'checkpoint', 'name': 'elf', 'line': 3})
        self.assertEqual(answered, ['elf'])
        self.assertEqual(self.driver.state_dumps, 1)
        self.assertEqual(self.xdo(), [])  # no input
        self.assertIn('  wrote checkpoint-elf.json', self.lines)


class CheckpointExpectationTest(unittest.TestCase):
    """A real-input run's checkpoint dumps against what the original
    reached there (scenarios/<name>.expect.json)."""

    SCENARIO = 'click 1 2\nwait 5\ncheckpoint gender\nwait 5\ncheckpoint w\n'

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.actions = realinput.parse_scenario(self.SCENARIO)

    def dump(self, folder, name, screen='create_char', race=2, world=None):
        os.makedirs(folder, exist_ok=True)
        chargen = None if world else {
            'race': race, 'subrace': 0, 'class': 5,
            'abilities': [13, 12, 9, 9, 10, 9], 'rolls': [0] * 7,
            'skill_points': 0, 'feats': [8, 66068416, 0],
            'portrait': '2MELF1_L'}
        with open(os.path.join(folder, 'checkpoint-%s.json' % name),
                  'w') as f:
            json.dump({'frame': 986, 'checkpoint': name, 'screen': screen,
                       'rng_state': 1, 'globals': {'au_counter': 986},
                       'chargen': chargen, 'world': world}, f)

    WORLD = {'area': 'AR1000', 'party': [{'id': 1, 'x': 2414, 'y': 897,
                                          'hp': 9, 'max_hp': 9,
                                          'facing': 4, 'morale': 10}],
             'doors': [{'name': 'AR1000_Door5', 'open': 0, 'locked': 0}],
             'dialog': {'active': 1, 'speaker': 1048592, 'entry': 34,
                        'responses': 5},
             'explored_count': 560, 'creatures': 36,
             'enemy_creatures': [{'id': 7, 'hp': 4, 'max_hp': 7,
                                  'timed_effects': 2,
                                  'equiped_effects': 1}]}

    def test_expected_fields_leave_out_timing_and_position(self):
        original = os.path.join(self.tmp, 'original')
        self.dump(original, 'gender')
        self.dump(original, 'w', screen='world', world=self.WORLD)
        expect = realinput.make_expectations(self.actions, original, 'test')
        self.assertEqual(expect['source'], 'test')
        self.assertEqual(expect['checkpoints']['gender'], {
            'screen': 'create_char', 'chargen.race': 2,
            'chargen.subrace': 0, 'chargen.class': 5,
            'chargen.abilities[0]': 13, 'chargen.abilities[1]': 12,
            'chargen.abilities[2]': 9, 'chargen.abilities[3]': 9,
            'chargen.abilities[4]': 10, 'chargen.abilities[5]': 9,
            'chargen.portrait': '2MELF1_L'})
        self.assertEqual(expect['checkpoints']['w'], {
            'screen': 'world', 'world.area': 'AR1000',
            'world.dialog.active': 1, 'world.dialog.speaker': 1048592,
            'world.dialog.entry': 34, 'world.dialog.responses': 5,
            'world.doors[0].name': 'AR1000_Door5',
            'world.doors[0].open': 0, 'world.doors[0].locked': 0,
            'world.party_count': 1, 'world.party[0].hp': 9,
            'world.party[0].max_hp': 9, 'world.party[0].morale': 10,
            'world.enemy_creatures[id=7].hp': 4,
            'world.enemy_creatures[id=7].max_hp': 7,
            'world.enemy_creatures[id=7].timed_effects': 2,
            'world.enemy_creatures[id=7].equiped_effects': 1})
        os.remove(os.path.join(original, 'checkpoint-w.json'))
        with self.assertRaisesRegex(ValueError, 'checkpoint-w.json'):
            realinput.make_expectations(self.actions, original, 'test')

    def test_check_finds_a_lost_click(self):
        original = os.path.join(self.tmp, 'original')
        self.dump(original, 'gender')
        self.dump(original, 'w', screen='world', world=self.WORLD)
        expect = realinput.make_expectations(self.actions, original, 'test')
        run = os.path.join(self.tmp, 'run')
        self.dump(run, 'gender')
        moved = dict(self.WORLD, party=[dict(self.WORLD['party'][0],
                                             x=2000, y=900, facing=2)])
        self.dump(run, 'w', screen='world', world=moved)
        self.assertEqual(realinput.check_checkpoints(self.actions, run,
                                                     expect), [])
        # A party member hurt, or one more member, is not what the
        # original reached.
        hurt = dict(self.WORLD, party=[dict(self.WORLD['party'][0], hp=4),
                                       dict(self.WORLD['party'][0], id=2)])
        self.dump(run, 'w', screen='world', world=hurt)
        self.assertEqual(
            realinput.check_checkpoints(self.actions, run, expect),
            ['checkpoint w: world.party[0].hp is 4, expected 9',
             'checkpoint w: world.party_count is 2, expected 1'])
        # An enemy hurt, or one more effect on it, is not either.
        hurtEnemy = dict(self.WORLD, enemy_creatures=[
            dict(self.WORLD['enemy_creatures'][0], hp=1, timed_effects=3)])
        self.dump(run, 'w', screen='world', world=hurtEnemy)
        self.assertEqual(
            realinput.check_checkpoints(self.actions, run, expect),
            ['checkpoint w: world.enemy_creatures[id=7].hp is 1, expected 4',
             'checkpoint w: world.enemy_creatures[id=7].timed_effects is 3, '
             'expected 2'])
        # The same enemy at another list index is: the render sort reorders
        # the list every frame (an id pin is order-free); another creature
        # around it says nothing about it.
        reordered = dict(self.WORLD, enemy_creatures=[
            {'id': 8, 'hp': 3, 'max_hp': 3, 'timed_effects': 0,
             'equiped_effects': 0},
            self.WORLD['enemy_creatures'][0]])
        self.dump(run, 'w', screen='world', world=reordered)
        self.assertEqual(realinput.check_checkpoints(self.actions, run,
                                                     expect), [])
        # A pinned enemy gone (or its state changed under its id) is not.
        gone = dict(self.WORLD, enemy_creatures=reordered['enemy_creatures'][
            :1])
        self.dump(run, 'w', screen='world', world=gone)
        self.assertEqual(
            realinput.check_checkpoints(self.actions, run, expect),
            ['checkpoint w: world.enemy_creatures[id=7].equiped_effects is '
             '"<missing>", expected 1',
             'checkpoint w: world.enemy_creatures[id=7].hp is "<missing>", '
             'expected 4',
             'checkpoint w: world.enemy_creatures[id=7].max_hp is '
             '"<missing>", expected 7',
             'checkpoint w: world.enemy_creatures[id=7].timed_effects is '
             '"<missing>", expected 2'])
        # The Gender click was lost: no race, and the run is elsewhere.
        self.dump(run, 'gender', race=0)
        self.dump(run, 'w', screen='connection')
        self.assertEqual(
            realinput.check_checkpoints(self.actions, run, expect),
            ['checkpoint gender: chargen.race is 0, expected 2',
             'checkpoint w: screen is "connection", expected "world"'])
        os.remove(os.path.join(run, 'checkpoint-w.json'))
        self.assertEqual(
            realinput.check_checkpoints(self.actions, run, None),
            ['no state dump at checkpoint w'])

    def test_expect_command_writes_the_file_beside_the_scenario(self):
        scenario = os.path.join(self.tmp, 'x.txt')
        with open(scenario, 'w') as f:
            f.write(self.SCENARIO)
        original = os.path.join(self.tmp, 'original')
        self.dump(original, 'gender')
        self.dump(original, 'w', screen='world', world=self.WORLD)
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(realinput.main(['expect', scenario, original]),
                             0)
        path = os.path.join(self.tmp, 'x.expect.json')
        self.assertEqual(realinput.expectation_path(scenario), path)
        loaded = realinput.load_expectations(scenario)
        self.assertEqual(sorted(loaded['checkpoints']), ['gender', 'w'])
        self.assertIn(original, loaded['source'])
        self.assertIsNone(realinput.load_expectations(
            os.path.join(self.tmp, 'none.txt')))

    def test_play_fails_a_run_that_missed_a_checkpoint(self):
        scenario = os.path.join(self.tmp, 'x.txt')
        with open(scenario, 'w') as f:
            f.write(self.SCENARIO)
        original = os.path.join(self.tmp, 'original')
        self.dump(original, 'gender')
        self.dump(original, 'w', screen='world', world=self.WORLD)
        with open(realinput.expectation_path(scenario), 'w') as f:
            json.dump(realinput.make_expectations(self.actions, original,
                                                  't'), f)
        run = os.path.join(self.tmp, 'run')
        self.dump(run, 'gender', race=0)
        self.dump(run, 'w', screen='world', world=self.WORLD)
        result = play.check_checkpoints(
            {'code': 0, 'error': None}, run, self.actions, scenario)
        self.assertEqual(result['code'], 1)
        self.assertEqual(result['error'],
                         'checkpoint gender: chargen.race is 0, expected 2')
        # A failed run keeps its own code and error.
        result = play.check_checkpoints(
            {'code': 124, 'error': 'scenario timeout'}, run, self.actions,
            scenario)
        self.assertEqual((result['code'], result['error']),
                         (124, 'scenario timeout'))

    def test_shipped_expectations_name_the_scenario_checkpoints(self):
        pending = glob.glob(os.path.join(ROOT, 'scenarios', 'pending',
                                         '*.txt'))
        found = []
        for path in scenario_files() + pending:
            expect = realinput.load_expectations(path)
            if expect is None:
                continue
            found.append(play.scenario_name(path))
            names = [a['name'] for a in realinput.load_scenario(path)
                     if a['op'] == 'checkpoint']
            self.assertEqual(sorted(expect['checkpoints']), sorted(names),
                             path)
            for fields in expect['checkpoints'].values():
                self.assertIn('screen', fields)
        # The scenario whose lost Gender click the old pass test missed,
        # and M1's and M2's exit scenarios.
        self.assertIn('chargen_replay', found)
        self.assertIn('world_entry', found)
        self.assertIn('walk_and_talk', found)


class EngineMemoryCapTest(unittest.TestCase):
    """The engine runs of play.py --vm linux (realinput.run_scenario) and
    of oracle.py replay compare (run_ours) go under the memory cap engine
    tests run with (M1/M2 review item 11)."""

    def which(self, name):
        return '/usr/bin/' + name

    def test_memory_capped(self):
        from unittest import mock
        with mock.patch.object(realinput.shutil, 'which', self.which):
            self.assertEqual(realinput.memory_capped(['/b/iwd2r'], {}), [
                'systemd-run', '--user', '--scope', '-q', '-p',
                'MemoryMax=8G', '-p', 'MemorySwapMax=0', '--', '/b/iwd2r'])
            self.assertIn('MemoryMax=4G', realinput.memory_capped(
                ['/b/iwd2r'], {'IWD2R_TEST_MEMORY_MAX': '4G'}))
        with mock.patch.object(realinput.shutil, 'which', lambda n: None):
            with self.assertRaisesRegex(RuntimeError, 'memory cap'):
                realinput.memory_capped(['/b/iwd2r'], {})

    def test_run_ours_runs_under_the_cap(self):
        from unittest import mock
        seen = []

        class Proc:
            def __init__(self, argv, **kw):
                seen.append((argv, kw['env']['IWD2R_SEED']))

            def wait(self, timeout=None):
                return 0

        script = os.path.join(ROOT, 'scenarios', 'chargen_replay.txt')
        with tempfile.TemporaryDirectory() as tmp, \
                mock.patch.object(oracle.subprocess, 'Popen', Proc), \
                mock.patch.object(oracle.dataview, 'make_view',
                                  lambda game, path: path), \
                mock.patch.object(oracle.dataview, 'remove_view',
                                  lambda view: None), \
                mock.patch.object(oracle, 'finish_replay',
                                  lambda *a: 0), \
                mock.patch.object(realinput.shutil, 'which', self.which), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(oracle.run_ours('/b/iwd2r', tmp, script, [],
                                             7, 5, 30, tmp, 'ours'), 0)
        limit = os.environ.get('IWD2R_TEST_MEMORY_MAX') or '8G'
        self.assertEqual(seen, [(['systemd-run', '--user', '--scope', '-q',
                                  '-p', 'MemoryMax=' + limit, '-p',
                                  'MemorySwapMax=0', '--', '/b/iwd2r'],
                                 '7')])


class SandboxWsbTest(unittest.TestCase):
    def test_wsb_is_well_formed_with_the_mapped_folders(self):
        folders = [(r'C:\b', winsandbox.BOX_BIN, True),
                   (r'C:\Users\alice\iwd2r-vm\game', winsandbox.BOX_GAME,
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
            (r'C:\Users\alice\iwd2r-vm\game', r'C:\iwd2r\game', 'false'),
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

    def test_run_waits_for_the_scenario_timeout_and_the_sandbox(self):
        # The host waits for done.txt as long as the driver's own timeout
        # plus the sandbox's boot, not a fixed 300 s that a 420 s
        # scenario would outlast.
        from unittest import mock
        waited = []
        with tempfile.TemporaryDirectory() as tmp:
            host_run = os.path.join(tmp, 'host')
            run_dir = os.path.join(tmp, 'run')
            os.makedirs(run_dir)

            def done(results, timeout):
                waited.append(timeout)
                for name, text in (('exitcode.txt', '0'), ('done.txt', 'done')):
                    with open(os.path.join(results, name), 'w') as f:
                        f.write(text)

            with mock.patch.object(winsandbox, 'wslpath_w', lambda p: p), \
                    mock.patch.object(winsandbox, 'start_sandbox',
                                      lambda h, w, log: None), \
                    mock.patch.object(winsandbox, 'wait_done', done), \
                    mock.patch.object(winsandbox, 'close_sandbox',
                                      lambda log: None):
                result = winsandbox.run_scenario(
                    os.path.join(tmp, 'bin', 'iwd2r.exe'), '/game', [], [],
                    'x', run_dir, host_run, 420, 30, lambda m: None)
        self.assertEqual(result['code'], 0)
        self.assertEqual(waited, [420 + winsandbox.SANDBOX_ALLOWANCE])
        self.assertGreaterEqual(winsandbox.SANDBOX_ALLOWANCE, 60)

    def test_driver_dry_run(self):
        # driver.ps1 -DryRun on the host compiles its C# and walks the
        # actions without starting anything; it catches a script that would
        # only fail inside the sandbox.
        powershell = ('/mnt/c/Windows/System32/WindowsPowerShell/v1.0/'
                      'powershell.exe')
        if not os.path.isfile(powershell) or \
                not os.path.isdir(winsandbox.VM_ROOT):
            self.skipTest('no Windows PowerShell or %s' % winsandbox.VM_ROOT)
        os.makedirs(os.path.join(winsandbox.VM_ROOT, 'dryrun'), exist_ok=True)
        with tempfile.TemporaryDirectory(
                dir=os.path.join(winsandbox.VM_ROOT, 'dryrun')) as tmp:
            actions = realinput.windows_actions(realinput.parse_scenario(
                'checkpoint menu\nclick 10 20\nkey ESCAPE\nwait 3\n'
                'shot s\nquit\n'))
            winsandbox.write_inputs(tmp, winsandbox.driver_config(
                ['A/B=1'], 60, 30, 'dry'), actions)
            win = subprocess.run(['wslpath', '-w', tmp], capture_output=True,
                                 text=True, check=True).stdout.strip()
            r = subprocess.run(
                [powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass',
                 '-File', win + '\\driver.ps1', '-Config',
                 win + '\\config.json', '-DryRun'],
                capture_output=True, text=True, timeout=120, cwd=tmp)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        for line in ('line 1: checkpoint menu',
                     'would request checkpoint-menu.json',
                     'line 2: click 10 20', 'line 3: key ESCAPE',
                     'line 6: quit', 'scenario finished'):
            self.assertIn(line, r.stdout)

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
        # The same CHITIN.KEY but nothing records the source: rebuilt.
        self.assertEqual(winsandbox.cache_state(self.src, self.cache),
                         'foreign')
        winsandbox.write_source_record(self.src, self.cache)
        self.assertFalse(winsandbox.cache_needs_refresh(self.src, self.cache))
        self.write(self.cache, 'chitin.key', b'KEY1', 1001)
        self.assertTrue(winsandbox.cache_needs_refresh(self.src, self.cache))
        self.write(self.cache, 'chitin.key', b'KEY12', 1000)
        self.assertTrue(winsandbox.cache_needs_refresh(self.src, self.cache))

    def refresh(self, source, lock=None):
        lines = []
        copied = winsandbox.refresh_cache(source, self.cache, lines.append,
                                          lock=lock)
        return copied, lines

    def test_cache_rebuilt_when_the_source_marker_changes(self):
        # A cache copied from the IWD2EE install (same CHITIN.KEY as the
        # vanilla copy, other files) is deleted and rebuilt, not reused.
        install = os.path.join(self._tmp.name, 'iwd2ee')
        self.write(install, 'CHITIN.KEY', b'KEY1', 1000)
        self.write(install, 'IEex.dll', b'mod')
        self.write(self.src, 'vanilla.ok', b'{"md5": "a"}')
        self.assertEqual(self.refresh(install)[0], True)
        self.assertTrue(os.path.exists(os.path.join(self.cache, 'IEex.dll')))
        held = []

        @contextlib.contextmanager
        def lock():
            held.append('vanilla')
            yield

        copied, lines = self.refresh(self.src, lock)
        self.assertTrue(copied)
        self.assertEqual(held, ['vanilla'])
        self.assertTrue(any('deleting it' in l for l in lines), lines)
        self.assertTrue(any('rebuilding the game cache' in l and self.src in l
                            for l in lines), lines)
        self.assertFalse(os.path.exists(os.path.join(self.cache, 'IEex.dll')))
        self.assertEqual(winsandbox.read_source_record(self.cache),
                         {'source': self.src,
                          'marker_sha256': winsandbox.source_record(
                              self.src)['marker_sha256']})
        self.assertIsNotNone(
            winsandbox.read_source_record(self.cache)['marker_sha256'])
        # Current now: no copy and no lock.
        self.assertEqual(self.refresh(self.src, lock)[0], False)
        self.assertEqual(held, ['vanilla'])
        # A rebuilt vanilla copy (new marker) is copied again from scratch.
        self.write(self.cache, 'stray.txt', b'x')
        self.write(self.src, 'vanilla.ok', b'{"md5": "a", "built": "later"}')
        self.assertEqual(winsandbox.cache_state(self.src, self.cache),
                         'foreign')
        self.assertEqual(self.refresh(self.src)[0], True)
        self.assertFalse(os.path.exists(os.path.join(self.cache,
                                                     'stray.txt')))
        # The same source with a changed CHITIN.KEY is only resynced.
        self.write(self.src, 'CHITIN.KEY', b'KEY2', 2000)
        self.assertEqual(winsandbox.cache_state(self.src, self.cache),
                         'stale')
        copied, lines = self.refresh(self.src)
        self.assertTrue(copied)
        self.assertFalse(any('deleting it' in l for l in lines), lines)

    def test_cache_is_never_its_own_source(self):
        with self.assertRaises(ValueError):
            winsandbox.refresh_cache(self.src, self.src, lambda m: None)

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

    def runtime_fixture(self):
        """A redist folder with a read-only msvcp140.dll (as under Program
        Files) and an exe importing it; returns (redist root, exe, the
        redist DLL)."""
        crt = os.path.join(self.dir, 'Redist', '14.44.2', 'x64',
                           'Microsoft.VC143.CRT')
        os.makedirs(crt)
        src = os.path.join(crt, 'msvcp140.dll')
        self.rewrite(src, 'runtime v1')
        out = os.path.join(self.dir, 'bin')
        os.makedirs(out)
        exe = os.path.join(out, 'iwd2r.exe')
        with open(exe, 'wb') as f:
            f.write(tiny_pe(['KERNEL32.dll', 'MSVCP140.dll']))
        return os.path.join(self.dir, 'Redist'), exe, src

    def rewrite(self, path, text, mtime=None):
        """Writes text into path and leaves it read-only (mtime: seconds,
        else now)."""
        if os.path.exists(path):
            os.chmod(path, 0o644)
        with open(path, 'w') as f:
            f.write(text)
        if mtime is not None:
            os.utime(path, (mtime, mtime))
        os.chmod(path, 0o444)

    def test_deploy_ignores_sub_second_mtimes(self):
        # On /mnt/c a copied file's mtime reads back in whole seconds: a
        # copy whose mtime is the source's, truncated, is current.
        redist, exe, src = self.runtime_fixture()
        os.utime(src, (1000.75, 1000.75))
        dst = os.path.join(os.path.dirname(exe), 'msvcp140.dll')
        with open(dst, 'w') as f:
            f.write('runtime vX')  # same size, other content
        os.utime(dst, (1000.0, 1000.0))
        winsandbox.deploy_runtime(exe, lambda m: None, redist)
        with open(dst) as f:
            self.assertEqual(f.read(), 'runtime vX')

    def test_deploy_twice_over_a_read_only_source(self):
        # copy2 carried the source's read-only bit over, so the next copy
        # could not open the file: PermissionError on msvcp140.dll.
        redist, exe, src = self.runtime_fixture()
        dst = os.path.join(os.path.dirname(exe), 'msvcp140.dll')
        winsandbox.deploy_runtime(exe, lambda m: None, redist)
        self.assertTrue(os.stat(dst).st_mode & 0o200, 'the copy is writable')
        self.assertEqual(int(os.path.getmtime(dst)),
                         int(os.path.getmtime(src)))
        # A newer runtime (an update) replaces the first copy.
        self.rewrite(src, 'runtime v2 (longer)', os.path.getmtime(src) + 10)
        winsandbox.deploy_runtime(exe, lambda m: None, redist)
        with open(dst) as f:
            self.assertEqual(f.read(), 'runtime v2 (longer)')
        # An older deployment left a read-only copy (copy2): replaced too.
        os.chmod(dst, 0o444)
        self.rewrite(src, 'runtime v3, longer still')
        winsandbox.deploy_runtime(exe, lambda m: None, redist)
        with open(dst) as f:
            self.assertEqual(f.read(), 'runtime v3, longer still')


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
        # The file may be empty: its last name (CGameSprite::Shatter,
        # 0x70A810) left when src/ came to define it. What it holds must
        # parse, be unique and not be ours.
        self.assertIsInstance(extra, list)
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

    def test_large_insertions_touch_only_changed_functions(self):
        # git's default diff gives up on the shortest edit script for big
        # insertions that repeat their neighbours' lines (as the effect
        # ports in src/CGameEffect.cpp did) and reports untouched functions
        # as changed; check_evidence diffs with --minimal.
        def source(new):
            lines = ['// fixture']
            for i in range(300):
                if new and i % 20 == 0 and i > 0:
                    lines += ['', f'int A::n{i}(int x)', '{']
                    for k in range(50):
                        lines += ['    if (x == 0) {', '        return x;',
                                  '    }', f'    return x + {k % 3};']
                    lines += ['}']
                changed = ' // changed' if new and i == 150 else ''
                lines += ['', f'// 0x{0x401000 + i * 0x20:X}',
                          f'int A::f{i}(int x)', '{', '    if (x == 0) {',
                          '        return x;', '    }',
                          f'    return x + {i % 3};{changed}', '}']
            return '\n'.join(lines) + '\n'

        self.write('src/a.cpp', source(False))
        self.commit('base')
        self.base = self.git('rev-parse', 'HEAD').strip()
        self.write('src/a.cpp', source(True))
        self.evidence(f'{0x401000 + 150 * 0x20:X}', 'exemption: "fixture"')
        self.commit()

        default = self.git('diff', '--unified=0', self.base, '--', 'src/')
        self.assertGreater(sum(1 for line in default.splitlines()
                               if line.startswith('-') and
                               not line.startswith('---')), 1)

        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('1 touched function(s)', r.stdout)

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

    def rules_address(self):
        """A rules-math address that cannot collide with the fixture's."""
        for address in sorted(check_evidence.RULES_MATH):
            if address not in (0x401000, 0x401020):
                return address
        self.fail('no rules-math address outside the fixture addresses')

    def touch_rules_math(self, extra):
        """Change a rules-math function's body; extra goes in its evidence."""
        address = self.rules_address()
        source = TWO_FUNCTIONS.replace('0x401000', f'0x{address:X}')
        self.write('src/a.cpp', source.replace('return x;', 'return x * 2;'))
        if extra is not None:
            self.evidence(f'{address:X}', extra)
        self.commit()
        return address

    def test_rules_math_read_only_fails(self):
        address = self.touch_rules_math('exemption: "pure data"\n'
                                        'kind: read-only')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'0x{address:X} A::f', r.stdout)
        self.assertIn('kind: read-only', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)
        self.assertNotIn('0x401020', r.stdout)

    def test_rules_math_replay_fails(self):
        address = self.touch_rules_math('exemption: "replay covers it"\n'
                                        'kind: replay')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'0x{address:X} A::f', r.stdout)
        self.assertIn('kind: replay', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)

    def test_rules_math_untagged_fails(self):
        address = self.touch_rules_math('exemption: "pure data"')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'0x{address:X} A::f', r.stdout)
        self.assertIn('no kind', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)

    def test_rules_math_missing_doc_fails(self):
        address = self.touch_rules_math(None)
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'docs/evidence/{address:X}.md', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)

    def test_rules_math_oracle_passes(self):
        self.touch_rules_math('oracle_case: rand/roll\n'
                              'kind: oracle')
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('rules math: oracle 1, replay 0, read-only 0, trace 0 '
                      '(1 of 1 touched function(s)', r.stdout)

    def test_rules_math_counted_in_evidence_kinds(self):
        self.touch_rules_math('exemption: "pure data"\nkind: read-only')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('evidence kinds: oracle 0, replay 0, read-only 1, '
                      'trace 0 (1 of 1 touched function(s))', r.stdout)

    def test_rules_math_note_on_missing_evidence(self):
        # A pure deletion inside the body still touches the function, and
        # the plain missing-evidence error carries the rules-math note.
        address = self.rules_address()
        self.write('src/a.cpp', TWO_FUNCTIONS.replace(
            '0x401000', f'0x{address:X}').replace('    return x;\n', ''))
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'docs/evidence/{address:X}.md', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)

    def test_non_rules_math_untagged_still_warns_only(self):
        # Outside the set an untagged document keeps the warn-only behaviour
        # (0x401000 is not in RULES_MATH).
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'oracle_case: rand/roll')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn('warning:', r.stdout)
        self.assertNotIn('rules math:', r.stdout)

    def trace_sources(self):
        """The two trace_source citation forms ruling 6 accepts: the fork's
        recovery note and a frida capture of our own binary."""
        return ['WillScarlettOhara/iwd2-re@e8dd16a7'
                ' docs/wso/recovery/CScreenMap.md',
                'frida sendmsg_probe orig:%s ours:%s compare=0'
                % ('a' * 64, 'b' * 64)]

    def test_trace_kind_accepted_with_source(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        for source in self.trace_sources():
            with self.subTest(source=source):
                self.evidence('401000', 'oracle_case: rand/roll\n'
                                        'kind: trace\n'
                                        f'trace_source: "{source}"')
                self.commit()
                r = self.check()
                self.assertEqual(r.returncode, 0, r.stdout)
                self.assertIn('trace 1 (1 of 1 touched function(s))',
                              r.stdout)

    def test_trace_kind_without_source_fails(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'oracle_case: rand/roll\nkind: trace')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('docs/evidence/401000.md', r.stdout)
        self.assertIn('trace_source', r.stdout)

    def test_trace_source_of_unknown_form_fails(self):
        self.write('src/a.cpp', TWO_FUNCTIONS.replace('return x;',
                                                      'return x * 2;'))
        self.evidence('401000', 'oracle_case: rand/roll\nkind: trace\n'
                                'trace_source: "checked by eye"')
        self.commit()
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('docs/evidence/401000.md', r.stdout)
        self.assertIn('trace_source', r.stdout)

    def test_rules_math_rejects_trace(self):
        address = self.touch_rules_math('oracle_case: rand/roll\n'
                                        'kind: trace\n'
                                        'trace_source: "%s"'
                                        % self.trace_sources()[0])
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'0x{address:X} A::f', r.stdout)
        self.assertIn('kind: trace', r.stdout)
        self.assertIn('requires kind: oracle', r.stdout)


class RulesMathSetTest(unittest.TestCase):
    """The curated rules-math set stays in step with the repository."""

    def test_set_is_curated(self):
        self.assertGreaterEqual(len(check_evidence.RULES_MATH), 10)
        for address in check_evidence.RULES_MATH:
            self.assertIsInstance(address, int)
            self.assertGreaterEqual(address, 0x401000)

    def test_every_address_has_an_evidence_doc(self):
        for address in sorted(check_evidence.RULES_MATH):
            path = os.path.join(ROOT, 'docs', 'evidence', f'{address:X}.md')
            self.assertTrue(os.path.isfile(path), f'missing {path}')

    def test_triage_doc_covers_the_set(self):
        path = os.path.join(ROOT, 'docs', 'evidence',
                            'rules-math-triage.md')
        with open(path) as f:
            text = f.read()
        for address in sorted(check_evidence.RULES_MATH):
            self.assertIn(f'0x{address:X}', text, f'unlisted 0x{address:X}')


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
            'oracle_case:\nkind:\nghidra_version: 12.1.4\n---\n'))
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
                                  'C:\\Users\\alice\\iwd2r-oracle\\game')
        self.assertEqual(text, (
            '[Alias]\r\n'
            'HD0:=C:\\Users\\alice\\iwd2r-oracle\\game\\\r\n'
            'CD1:=C:\\Users\\alice\\iwd2r-oracle\\game\\Data\\\r\n'
            'CD2:=C:\\Users\\alice\\iwd2r-oracle\\game\\CD2\\\r\n'
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


class OracleMirrorTest(unittest.TestCase):
    """tools/oracle.py linux_mirror: our side's copy of the vanilla install
    on the Linux disk."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.game = os.path.join(self.tmp.name, 'game')
        self.mirror = os.path.join(self.tmp.name, 'mirror')
        for rel, data in (('vanilla.ok', b'{"md5": "1"}'),
                          ('CHITIN.KEY', b'KEY V1  '),
                          ('icewind2.ini', b'[Program Options]\r\n'),
                          ('Data/AR1000.bif', b'BIFF'),
                          ('Characters/a.chr', b'CHR'),
                          ('temp/junk', b'x')):
            path = os.path.join(self.game, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as f:
                f.write(data)

    def read(self, rel):
        with open(os.path.join(self.mirror, rel), 'rb') as f:
            return f.read()

    def test_a_linux_install_is_used_as_it_is(self):
        self.assertEqual(oracle.linux_mirror(self.game, self.mirror),
                         self.game)
        self.assertFalse(os.path.exists(self.mirror))

    def test_copy_refresh_and_rebuild(self):
        self.assertEqual(
            oracle.linux_mirror(self.game, self.mirror, force=True),
            self.mirror)
        self.assertEqual(self.read('Data/AR1000.bif'), b'BIFF')
        self.assertEqual(self.read('icewind2.ini'), b'[Program Options]\r\n')
        # The engine's own folders are not copied.
        self.assertFalse(os.path.exists(os.path.join(self.mirror, 'temp')))
        # Same marker: the INIs and the copied folders follow the install,
        # the rest stays.
        with open(os.path.join(self.game, 'icewind2.ini'), 'wb') as f:
            f.write(b'changed')
        with open(os.path.join(self.game, 'Characters', 'b.chr'), 'wb') as f:
            f.write(b'B')
        with open(os.path.join(self.game, 'Data', 'AR1000.bif'), 'wb') as f:
            f.write(b'NEW!')
        oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertEqual(self.read('icewind2.ini'), b'changed')
        self.assertEqual(self.read('Characters/b.chr'), b'B')
        self.assertEqual(self.read('Data/AR1000.bif'), b'BIFF')
        # A new marker: copied again.
        with open(os.path.join(self.game, 'vanilla.ok'), 'wb') as f:
            f.write(b'{"md5": "2"}')
        oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertEqual(self.read('Data/AR1000.bif'), b'NEW!')
        self.assertFalse(os.path.exists(self.mirror + '.tmp'))

    def test_a_copy_cut_short_is_made_again(self):
        # A copy that stopped part way (left beside the mirror) is not
        # taken for the mirror and is replaced by a whole one.
        from unittest import mock
        partial = self.mirror + '.tmp'
        os.makedirs(partial)
        with open(os.path.join(partial, 'vanilla.ok'), 'wb') as f:
            f.write(b'{"md5": "1"}')
        real_copytree = shutil.copytree
        calls = []

        def failing_copytree(src, dst, *args, **kwargs):
            calls.append(dst)
            if len(calls) == 1:
                raise OSError('disk full')
            return real_copytree(src, dst, *args, **kwargs)

        with mock.patch.object(oracle.shutil, 'copytree', failing_copytree):
            with self.assertRaises(OSError):
                oracle.linux_mirror(self.game, self.mirror, force=True)
            self.assertFalse(os.path.exists(self.mirror))
            oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertEqual(calls[:2], [partial, partial])
        self.assertEqual(self.read('Data/AR1000.bif'), b'BIFF')
        self.assertEqual(self.read('vanilla.ok'), b'{"md5": "1"}')
        self.assertFalse(os.path.exists(partial))

    def test_an_ini_the_install_lost_goes_from_the_mirror(self):
        with open(os.path.join(self.game, 'Party.ini'), 'wb') as f:
            f.write(b'[Party]\r\n')
        oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertEqual(self.read('Party.ini'), b'[Party]\r\n')
        os.remove(os.path.join(self.game, 'Party.ini'))
        oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertFalse(os.path.exists(os.path.join(self.mirror, 'Party.ini')))
        # Other files the install lost stay until the next marker.
        os.remove(os.path.join(self.game, 'CHITIN.KEY'))
        oracle.linux_mirror(self.game, self.mirror, force=True)
        self.assertEqual(self.read('CHITIN.KEY'), b'KEY V1  ')
        self.assertEqual(self.read('icewind2.ini'), b'[Program Options]\r\n')


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

    GAME = '/mnt/c/Users/alice/iwd2r-oracle/game'

    def check(self, run, path=GAME):
        return oracle.windows_reparse(path, run=run)

    def test_asks_dir_on_the_parent(self):
        run = fake_cmd(0, 'other\r\n')
        self.assertIsNone(self.check(run))
        self.assertEqual(run.calls, [['dir', '/AL', '/B',
                                      'C:\\Users\\alice\\iwd2r-oracle']])

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
        for path in ('/home/alice/game', '/tmp/x', '/mnt/c', 'relative/game',
                     '/mnt/wsl/game'):
            self.assertIn('not on a Windows drive', self.check(run, path),
                          path)
        self.assertEqual(run.calls, [])

    def test_drvfs_win(self):
        self.assertEqual(oracle.drvfs_win('/mnt/c/Users/alice/'),
                         'C:\\Users\\alice')
        self.assertEqual(oracle.drvfs_win('/mnt/d'), 'D:\\')
        self.assertIsNone(oracle.drvfs_win('/home/alice'))


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
        self.write('Data/MAIN.BIF', b'BIF V1.0' + bytes(16))
        with self.assertRaisesRegex(bif.BifError, 'not a BIFF V1'):
            bif.extract(self.game, 'XPLEVEL.2DA')
        self.write('Data/MAIN.BIF', b'BIFCV1.0' + struct.pack('<I', 9))
        with self.assertRaisesRegex(bif.BifError, 'BIFC block runs past'):
            bif.extract(self.game, 'XPLEVEL.2DA')
        self.write('Data/MAIN.BIF', b'BIFCV1.0' + struct.pack('<3I', 9, 9, 4)
                   + b'junk')
        with self.assertRaisesRegex(bif.BifError, 'does not inflate'):
            bif.extract(self.game, 'XPLEVEL.2DA')
        with self.assertRaisesRegex(bif.BifError, 'not a KEY V1'):
            bif.parse_key(b'KEY V2  ' + bytes(16))
        with self.assertRaisesRegex(bif.BifError, 'past the end'):
            bif.parse_key(b'KEY V1  ' + struct.pack('<4I', 9, 0, 24, 24))
        with self.assertRaisesRegex(bif.BifError, 'no file 7'):
            bif.read_biff(make_biff([(0, b'x')]), 7)
        with self.assertRaisesRegex(bif.BifError, 'past the end'):
            bif.read_biff(make_biff([(0, b'xyz')])[:-1], 0)

    def test_extract_from_a_compressed_archive(self):
        biff = make_biff([(0, b'ids'), (1, b'2DA xp'), (2, b'ITM V2.0')])
        half = len(biff) // 2
        blocks = b''
        for part in (biff[:half], biff[half:]):
            packed = zlib.compress(part)
            blocks += struct.pack('<II', len(part), len(packed)) + packed
        self.write('Data/MAIN.BIF', b'BIFCV1.0' + struct.pack('<I', len(biff))
                   + blocks)
        self.assertEqual(bif.extract(self.game, 'xplevel.2da'), b'2DA xp')
        self.assertEqual(bif.extract(self.game, '00swdl01.itm'), b'ITM V2.0')

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
    'CHolder.h': """class CReply {
public:
    /* 0000 */ DWORD m_flags;
    /* 0004 */ POSITION m_displayPosition;
};

class CReplies : public CTypedPtrArray<CPtrArray, CReply*> {
public:
    /* 0014 */ STRREF m_text;
};

template <class T, int nType>
class CResHolder {
public:
    /* 0000 */ BOOL bLoading;
    /* 0004 */ T* pRes;
    /* 0008 */ CResRef cResRef;
};

// The original has a vtable at 0: the base starts at 4.
class CHeld : public CResHolder<CResX, 5> /* base 0x0004 */ {
public:
    /* 0014 */ INT m_nAbilities;
};

class CHeldToo : public CObject, public CResHolder<CResX, 6> /* base 0x10 */ {
public:
    /* 0020 */ WORD m_count;
};

class CNested : public CTypedPtrList<CPtrList, CPair<int, CResX*>*>, public CResHolder<CResX, 7> /* base 0x0010 */ {
public:
    /* 0030 */ BYTE m_last;
};

class CHolderNamed {
public:
    /* 0004 */ int m_n;
    /* 0008 */ CString m_sName;
};

class CHolder {
public:
    /* 0000 */ SCRIPTNAME m_name;
    /* 0020 */ CTypedPtrList<CPtrList, int*> m_list;
    /* 003C */ CTypedPtrArray<CPtrArray, CReplies*> m_entries;
    /* 0050 */ CTypedPtrList<CPtrList, int*>* m_pList;
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

    def test_mfc_containers_and_names(self):
        out = self.generate({
            'name': {'class': 'CHolder', 'member': 'm_name'},
            'head': {'class': 'CHolder', 'member': 'm_list.m_pNodeHead'},
            'count': {'class': 'CHolder', 'member': 'm_list.m_nCount'},
            'list_ptr': {'class': 'CHolder', 'member': 'm_pList'},
            'next': {'class': 'CPtrList::CNode', 'member': 'pNext'},
            'data': {'class': 'CPtrList::CNode', 'member': 'data'},
            'entries': {'class': 'CHolder', 'member': 'm_entries.m_pData'},
            'entries_size': {'class': 'CHolder',
                             'member': 'm_entries.m_nSize'},
            'replies': {'class': 'CReplies', 'member': 'm_pData'},
            'replies_size': {'class': 'CReplies', 'member': 'm_nSize'},
            'text': {'class': 'CReplies', 'member': 'm_text'},
            'shown': {'class': 'CReply', 'member': 'm_displayPosition'},
        })
        self.assertEqual(out, {
            'name': {'base': 'CHolder', 'offset': '0x0000', 'type': 'i8',
                     'count': 32},
            'head': {'base': 'CHolder', 'offset': '0x0024', 'type': 'ptr'},
            'count': {'base': 'CHolder', 'offset': '0x002C', 'type': 'i32'},
            'list_ptr': {'base': 'CHolder', 'offset': '0x0050',
                         'type': 'ptr'},
            'next': {'base': 'CPtrList::CNode', 'offset': '0x0000',
                     'type': 'ptr'},
            'data': {'base': 'CPtrList::CNode', 'offset': '0x0008',
                     'type': 'ptr'},
            'entries': {'base': 'CHolder', 'offset': '0x0040',
                        'type': 'ptr'},
            'entries_size': {'base': 'CHolder', 'offset': '0x0044',
                             'type': 'i32'},
            'replies': {'base': 'CReplies', 'offset': '0x0004',
                        'type': 'ptr'},
            'replies_size': {'base': 'CReplies', 'offset': '0x0008',
                             'type': 'i32'},
            'text': {'base': 'CReplies', 'offset': '0x0014', 'type': 'u32'},
            'shown': {'base': 'CReply', 'offset': '0x0004', 'type': 'ptr'},
        })
        with self.assertRaises(gen_fields.FieldError) as e:
            self.generate({'x': {'class': 'CHolder',
                                 'member': 'm_list.m_nSize'}})
        self.assertIn('no commented member m_nSize', str(e.exception))

    def test_base_offset_comment(self):
        # `/* base 0xNNNN */` after a base class moves its members.
        out = self.generate({
            'ref': {'class': 'CHeld', 'member': 'cResRef'},
            'loading': {'class': 'CHeld', 'member': 'bLoading'},
            'own': {'class': 'CHeld', 'member': 'm_nAbilities'},
            'plain': {'class': 'CResHolder', 'member': 'cResRef'},
            'second': {'class': 'CHeldToo', 'member': 'pRes'},
            'count': {'class': 'CHeldToo', 'member': 'm_count'},
        })
        self.assertEqual(out, {
            'ref': {'base': 'CHeld', 'offset': '0x000C', 'type': 'resref'},
            'loading': {'base': 'CHeld', 'offset': '0x0004', 'type': 'i32'},
            'own': {'base': 'CHeld', 'offset': '0x0014', 'type': 'i32'},
            'plain': {'base': 'CResHolder', 'offset': '0x0008',
                      'type': 'resref'},
            'second': {'base': 'CHeldToo', 'offset': '0x0014',
                       'type': 'ptr'},
            'count': {'base': 'CHeldToo', 'offset': '0x0020',
                      'type': 'u16'},
        })
        # Nested template arguments: the bases split at the commas outside
        # the brackets.
        out = self.generate({
            'head': {'class': 'CNested', 'member': 'm_pNodeHead'},
            'ref': {'class': 'CNested', 'member': 'cResRef'},
        })
        self.assertEqual(out, {
            'head': {'base': 'CNested', 'offset': '0x0004', 'type': 'ptr'},
            'ref': {'base': 'CNested', 'offset': '0x0018',
                    'type': 'resref'}})
        self.assertEqual(gen_fields.bases(
            'class C : public A<B, C<D, E>*>, protected F /* base 0x8 */ {'),
            [('A', 0), ('F', 8)])
        self.assertEqual(gen_fields.bases('class C {'), [])
        self.assertEqual(gen_fields.bases('struct S : T {'), [('T', 0)])
        self.assertEqual(gen_fields.bases(
            'class C : public ns::B<int>, private ::top :: D /* base 4 */ {'),
            [('B', 0), ('D', 4)])
        # CString: its m_pchData, the pointer to its characters.
        out = self.generate({'name': {'class': 'CHolderNamed',
                                      'member': 'm_sName.m_pchData'}})
        self.assertEqual(out, {'name': {'base': 'CHolderNamed',
                                        'offset': '0x0008', 'type': 'ptr'}})
        # The repository's CItem (src/CItem.h): its resref is at 0x0C in the
        # original (0x4E785C lea ebx, [esi+0xC]).
        with open(os.path.join(ROOT, 'oracle', 'replay', 'fields.json')) as f:
            fields = json.load(f)
        self.assertEqual(fields['item_resref'], {
            'base': 'CItem', 'offset': '0x000C', 'type': 'resref'})

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
        # The game block: CInfGame::AddPartyGold 0x5BF68B (add [ecx+0x4238]),
        # CGameSprite::Marshal 0x70B4E0 (lea ecx, [ebp+0x4AD8], the
        # equipment), CDerivedStats::Reload 0x4443D0/0x4443D9 (the base
        # stats' levels at 0x83 and 0x82), CItem::CItem 0x4E7840 (resref
        # 0x0C, use counts 0x18-0x1C, flags 0x20).
        self.assertEqual(fields['party_gold']['offset'], '0x4238')
        self.assertEqual(fields['sprite_items']['offset'], '0x4AD8')
        self.assertEqual(fields['sprite_level']['offset'], '0x0626')
        self.assertEqual(fields['sprite_class_levels']['offset'], '0x0627')
        self.assertEqual(fields['item_resref']['offset'], '0x000C')
        self.assertEqual(fields['item_use_count1']['offset'], '0x0018')
        self.assertEqual(fields['item_flags']['offset'], '0x0020')
        # The slots: CScreenLoad::RefreshGameSlots 0x63CA0A (m_nNumGameSlots
        # [edi+0x1CA]), 0x63CAAE (m_aGameSlots.m_pData [edi+0x1D6]);
        # CScreenSave::RefreshGameSlots 0x65BDBB ([edi+0x458]), 0x65BE5F
        # ([edi+0x464]); a slot's 0x63D354 (add ecx, 0x304), 0x63D36C
        # ([ecx+0x308]), 0x63D37F ([ecx+0x30C]), 0x63D393 (add ecx, 0x310).
        self.assertEqual(fields['load_slots_count']['offset'], '0x01CA')
        self.assertEqual(fields['load_slots']['offset'], '0x01D6')
        self.assertEqual(fields['save_slots_count']['offset'], '0x0458')
        self.assertEqual(fields['save_slots']['offset'], '0x0464')
        self.assertEqual(fields['load_slots_size']['offset'], '0x01DA')
        self.assertEqual(fields['save_slots_size']['offset'], '0x0468')
        for kind in ('load', 'save'):
            self.assertEqual(fields[kind + '_slot_character']['offset'],
                             '0x0304')
            self.assertEqual(fields[kind + '_slot_time']['offset'], '0x0308')
            self.assertEqual(fields[kind + '_slot_chapter']['offset'],
                             '0x030C')
            self.assertEqual(fields[kind + '_slot_chapter_text']['offset'],
                             '0x0310')


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
                            'ai_overlap_frames': 0,
                            'reinitializing': 0, 'loading': 0, 'loads': 0,
                            'held_frames': 0, 'ai_rng_state': None},
                'chargen': None, 'world': None,
                'game': {'party_gold': 0, 'members': []}, 'slots': None}
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
                  'skills': [0] * 64, 'action': 0, 'modal': 0, 'facing': 4,
                  'morale': 10}
        enemy = {'id': 196613, 'hp': 4, 'max_hp': 7, 'timed_effects': 2,
                 'equiped_effects': 1}
        dump = {'frame': 1500, 'checkpoint': 'world', 'screen': 'world',
                'rng_state': 1,
                'globals': {'au_counter': 1498, 'fullscreen': 0,
                            'in_async_update': 0, 'ai_stalled_frames': 0,
                            'ai_overlap_frames': 0,
                            'reinitializing': 0, 'loading': 0, 'loads': 1,
                            'held_frames': 13, 'ai_rng_state': 7},
                'chargen': None,
                'world': {'area': 'AR1000', 'party': [member],
                          'doors': [{'name': 'DOOR01', 'open': 0,
                                     'locked': 1}],
                          'dialog': {'active': 1, 'speaker': 3, 'entry': 0,
                                     'responses': 2},
                          'explored_count': 12, 'creatures': 3,
                          'enemy_creatures': [enemy]},
                'game': None, 'slots': None}
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['world']['party'].append(
            {'id': 131073, 'x': None, 'y': None, 'hp': None, 'max_hp': None,
             'skills': None, 'action': None, 'modal': None, 'facing': None,
             'morale': None})
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['world']['party'][0] = dict(member, hp='9')
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.party[0].hp: '9' is not of type integer or null"])
        dump['world']['party'][0] = dict(member)
        del dump['world']['party'][0]['action']
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.party[0]: missing required property 'action'"])
        dump['world']['party'][0] = member
        dump['world']['doors'][0]['open'] = '0'
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.doors[0].open: '0' is not of type integer"])
        dump['world']['doors'][0]['open'] = 0
        dump['world']['dialog'] = {'active': 0, 'speaker': -1, 'entry': -1,
                                   'responses': 0}
        self.assertEqual(minischema.validate(dump, schema), [])
        dump['world']['enemy_creatures'][0]['hp'] = '4'
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.enemy_creatures[0].hp: '4' is not of type integer "
            "or null"])
        dump['world']['enemy_creatures'][0]['hp'] = 4
        dump['world']['enemy_creatures'].append(dict(enemy, id=196614,
                                                     hp=None, max_hp=None,
                                                     timed_effects=None,
                                                     equiped_effects=None))
        self.assertEqual(minischema.validate(dump, schema), [])
        del dump['world']['enemy_creatures'][1]['timed_effects']
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world.enemy_creatures[1]: missing required property "
            "'timed_effects'"])
        dump['world']['enemy_creatures'][1] = enemy
        del dump['world']['creatures']
        self.assertEqual(minischema.validate(dump, schema), [
            "$.world: missing required property 'creatures'"])
        del dump['world']
        self.assertEqual(minischema.validate(dump, schema),
                         ["$: missing required property 'world'"])

    def test_replay_schema_game(self):
        with open(os.path.join(ROOT, 'oracle', 'replay', 'schema.json')) as f:
            schema = json.load(f)
        inventory = [None] * 51
        inventory[0] = {'resref': '00HELM01', 'uses': [0, 0, 0], 'flags': 1}
        inventory[16] = {'resref': '00ARRO01', 'uses': [40, 0, 0],
                         'flags': 0}
        member = {'id': 65536, 'gold': 0, 'xp': 300, 'level': 1,
                  'class_levels': [0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0],
                  'inventory': inventory}
        gone = {'id': 131073, 'gold': None, 'xp': None, 'level': None,
                'class_levels': None, 'inventory': None}
        dump = {'frame': 1500, 'checkpoint': 'loaded', 'screen': 'inventory',
                'rng_state': 1,
                'globals': {'au_counter': 1498, 'fullscreen': 0,
                            'in_async_update': 0, 'ai_stalled_frames': 0,
                            'ai_overlap_frames': 0,
                            'reinitializing': 0, 'loading': 0, 'loads': 1,
                            'held_frames': 13, 'ai_rng_state': 7},
                'chargen': None, 'world': None,
                'game': {'party_gold': 75, 'members': [member, gone]},
                'slots': [{'file': '000000004-Probe Save',
                           'name': 'Probe Save', 'character': 'Probe',
                           'time': 25200000, 'chapter': 0,
                           'chapter_text': 'Prologue'},
                          {'file': None, 'name': None, 'character': None,
                           'time': 0, 'chapter': -1, 'chapter_text': None}]}
        self.assertEqual(minischema.validate(dump, schema), [])
        member['inventory'] = inventory[:50]
        member['class_levels'] = [0] * 12
        inventory[16] = dict(inventory[16], uses=[70000, 0, 0])
        member['inventory'] = inventory
        self.assertEqual(minischema.validate(dump, schema), [
            '$.game.members[0].class_levels: 12 items, more than 11',
            '$.game.members[0].inventory[16].uses[0]: 70000 is above the '
            'maximum 65535'])
        del dump['game']['party_gold']
        member['class_levels'] = [0] * 11
        inventory[16]['uses'] = [40, 0, 0]
        del dump['slots'][1]['time']
        self.assertEqual(minischema.validate(dump, schema), [
            "$.game: missing required property 'party_gold'",
            "$.slots[1]: missing required property 'time'"])

    def test_game_fields_are_compared(self):
        """The game block's fields are compared, an item's by slot."""
        slots = [None] * 51
        original = {'game': {'party_gold': 75, 'members': [
            {'id': 1, 'gold': 0, 'xp': 300, 'level': 1,
             'class_levels': [0] * 11,
             'inventory': slots[:16] + [{'resref': 'A', 'uses': [40, 0, 0],
                                         'flags': 0}] + slots[17:]}]}}
        ours = json.loads(json.dumps(original))
        ours['game']['party_gold'] = 80
        ours['game']['members'][0]['xp'] = 0
        ours['game']['members'][0]['inventory'][16]['uses'][0] = 39
        ours['game']['members'][0]['inventory'][17] = {'resref': 'B',
                                                       'uses': [1, 0, 0],
                                                       'flags': 1}
        self.assertEqual(oracle.field_differences(original, ours), [
            ('game.members[0].inventory[16].uses[0]', 40, 39),
            ('game.members[0].inventory[17]', None, '<missing>'),
            ('game.members[0].inventory[17].flags', '<missing>', 1),
            ('game.members[0].inventory[17].resref', '<missing>', 'B'),
            ('game.members[0].inventory[17].uses[0]', '<missing>', 1),
            ('game.members[0].inventory[17].uses[1]', '<missing>', 0),
            ('game.members[0].inventory[17].uses[2]', '<missing>', 0),
            ('game.members[0].xp', 300, 0),
            ('game.party_gold', 75, 80)])

    def test_world_fields_are_compared(self):
        """The world block's fields are compared, none of them timing."""
        original = {'world': {'doors': [{'name': 'DOOR01', 'open': 0,
                                         'locked': 1}],
                              'dialog': {'active': 0, 'speaker': -1,
                                         'entry': -1, 'responses': 0},
                              'party': [{'action': 0, 'modal': 0,
                                         'facing': 4}],
                              'explored_count': 12, 'creatures': 3}}
        ours = {'world': {'doors': [{'name': 'DOOR01', 'open': 1,
                                     'locked': 1}],
                          'dialog': {'active': 1, 'speaker': 3,
                                     'entry': 0, 'responses': 2},
                          'party': [{'action': 23, 'modal': 1,
                                     'facing': 12}],
                          'explored_count': 13, 'creatures': 4}}
        self.assertEqual([d[0] for d in oracle.field_differences(original,
                                                                 ours)], [
            'world.creatures', 'world.dialog.active', 'world.dialog.entry',
            'world.dialog.responses', 'world.dialog.speaker',
            'world.doors[0].open', 'world.explored_count',
            'world.party[0].action', 'world.party[0].facing',
            'world.party[0].modal'])

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


class DefaultDataTest(unittest.TestCase):
    """play.py's default game data is a vanilla copy, never the install."""

    def isfile(self, present):
        return lambda path: path in present

    def test_linux_default_is_the_local_copy(self):
        env = {'HOME': '/home/u'}
        local = '/home/u/.local/share/iwd2r/game'
        self.assertEqual(play.linux_data(env), local)
        self.assertEqual(play.default_data(
            'linux', env, self.isfile({local + '/CHITIN.KEY'})), local)
        env = {'HOME': '/home/u', 'IWD2R_HOME': '/srv/iwd2r'}
        self.assertEqual(play.default_data(
            'linux', env, self.isfile({'/srv/iwd2r/game/chitin.key'})),
            '/srv/iwd2r/game')
        # An empty IWD2R_HOME counts as unset, as in gates.sh.
        self.assertEqual(play.linux_data({'HOME': '/h', 'IWD2R_HOME': ''}),
                         '/h/.local/share/iwd2r/game')

    def test_windows_default_is_the_verified_oracle_copy(self):
        game = oracle.ORACLE_DIR + '/game'
        self.assertEqual(play.WINDOWS_DATA, game)
        seen = []

        def marker_ok(path):
            seen.append(path)
            return True

        self.assertEqual(play.default_data(
            'windows', {}, self.isfile({game + '/CHITIN.KEY'}), marker_ok),
            game)
        self.assertEqual(seen, [game])

    def test_install_is_never_the_default(self):
        install = '/mnt/c/Games/Icewind Dale II'
        present = self.isfile({install + '/CHITIN.KEY'})
        for platform in ('linux', 'windows'):
            with self.assertRaises(SystemExit) as e:
                play.default_data(platform, {'HOME': '/home/u'}, present,
                                  lambda p: True)
            self.assertNotIn(install, str(e.exception))

    def test_no_vanilla_copy_message(self):
        for platform in ('linux', 'windows'):
            with self.assertRaises(SystemExit) as e:
                play.default_data(platform, {'HOME': '/home/u'},
                                  self.isfile(set()), lambda p: False)
            text = str(e.exception)
            self.assertIn('tools/oracle.py vanilla', text)
            self.assertIn('rsync -a --exclude temp', text)
            self.assertIn('IWD2_DATA', text)
        # A Windows copy without a verified marker is not used either.
        game = play.WINDOWS_DATA
        with self.assertRaises(SystemExit):
            play.default_data('windows', {},
                              self.isfile({game + '/CHITIN.KEY'}),
                              lambda p: False)

    def test_data_and_iwd2_data_override(self):
        with tempfile.TemporaryDirectory() as tmp:
            with open(os.path.join(tmp, 'CHITIN.KEY'), 'wb'):
                pass
            for platform in ('linux', 'windows'):
                self.assertEqual(play.resolve_data(tmp, platform, {}), tmp)
                self.assertEqual(play.resolve_data(
                    None, platform, {'IWD2_DATA': tmp}), tmp)


class ShotCheckTest(unittest.TestCase):
    """A --vm scenario passes only with exit 0 and every shot written."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.run_dir = self._tmp.name
        self.actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'world_entry.txt'))
        self.assertIn('world', [a['name'] for a in self.actions
                                if a['op'] == 'shot'])

    def touch(self, name):
        with open(os.path.join(self.run_dir, name), 'wb'):
            pass

    def result(self, code=0):
        return {'code': code, 'error': None, 'seconds': 1.0,
                'screen_shots': 0, 'engine_shots': 0}

    def test_exit_0_without_the_shot_fails(self):
        # The Windows world_entry run: the engine quit with 0 at the
        # chapter step and wrote no shot.
        for name in ('actions.json', 'driver.log', 'log.txt'):
            self.touch(name)
        self.touch('world-screen.png')  # the driver's capture is not enough
        r = play.check_shots(self.result(), self.run_dir, self.actions)
        self.assertEqual(r['code'], 1)
        self.assertEqual(r['error'], 'missing shot world')
        self.assertEqual(play.result_row('world_entry', r)[1], 1)

    def test_all_shots_present_passes(self):
        self.touch('world.png')
        r = play.check_shots(self.result(), self.run_dir, self.actions)
        self.assertEqual((r['code'], r['error']), (0, None))

    def test_presented_shots_are_required_with_the_flag(self):
        self.touch('world.png')
        r = play.check_shots(self.result(), self.run_dir, self.actions, True)
        self.assertEqual((r['code'], r['error']), (1, 'missing shot world'))
        self.touch('world-presented.png')
        r = play.check_shots(self.result(), self.run_dir, self.actions, True)
        self.assertEqual(r['code'], 0)

    def test_a_failing_exit_code_is_kept(self):
        r = play.check_shots(self.result(124), self.run_dir, self.actions)
        self.assertEqual((r['code'], r['error']), (124, None))

    def test_every_suite_scenario_names_its_shots(self):
        for path in scenario_files():
            actions = realinput.load_scenario(path)
            self.assertEqual(
                play.missing_shots(self.run_dir, actions),
                list(dict.fromkeys(a['name'] for a in actions
                                   if a['op'] == 'shot')), path)


class WinHomeTest(unittest.TestCase):
    """tools/winhome.py: the Windows profile the Windows-side folders hang
    off, from $IWD2R_WIN_HOME or the WSL user name."""

    def test_explicit_wins_and_drops_trailing_slash(self):
        self.assertEqual(winhome.win_home({'IWD2R_WIN_HOME': '/mnt/d/Me/',
                                           'USER': 'bob'}), '/mnt/d/Me')

    def test_empty_counts_as_unset(self):
        self.assertEqual(winhome.win_home({'IWD2R_WIN_HOME': '',
                                           'USER': 'bob'}),
                         '/mnt/c/Users/bob')

    def test_falls_back_to_the_wsl_user(self):
        self.assertEqual(winhome.win_home({'LOGNAME': 'carol'}),
                         '/mnt/c/Users/carol')

    def test_tools_hang_off_it(self):
        self.assertEqual(oracle.ORACLE_DIR, winhome.WIN_HOME + '/iwd2r-oracle')
        self.assertEqual(play.WIN_BUILD_BASE, winhome.WIN_HOME + '/iwd2r-build')
        self.assertEqual(winsandbox.VM_ROOT, winhome.WIN_HOME + '/iwd2r-vm')


class WinCheckoutTest(unittest.TestCase):
    def test_win_checkout_defaults(self):
        base = play.WIN_BUILD_BASE
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
            refresh_cache=lambda d, c, log, lock=None: events.append(
                ('refresh', list(locks.held))),
            reset_cache_inis=lambda d, c: events.append(
                ('reset', list(locks.held))),
            run_scenario=run_scenario)
        self.assertEqual(winsandbox.SANDBOX_LOCK,
                         winsandbox.VM_ROOT + '/sandbox.lock')
        scenarios = [(path, realinput.load_scenario(path))
                     for path in scenario_files()[:2]]
        base_dir = os.path.join(self.tmp, 'runs', 'stamp')
        args = types.SimpleNamespace(timeout=10, no_build=True, new_attempt=None)
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
        both = [lock, oracle.VANILLA_LOCK]
        self.assertEqual(events, [
            ('refresh', [lock]), ('reset', both), ('run', names[0], [lock]),
            ('refresh', [lock]), ('reset', both), ('run', names[1], [lock])])
        self.assertEqual(locks.paths, [(lock, 'sandbox'),
                                       (oracle.VANILLA_LOCK, 'vanilla')] * 2)


class SandboxReplayTest(unittest.TestCase):
    """replay record/compare in Windows Sandbox (winsandbox.run_replay,
    tools/sandbox/replay-driver.ps1), with the sandbox stubbed."""

    GAME = r'C:\Users\alice\iwd2r-oracle\game'

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
        from unittest import mock
        with mock.patch.dict(os.environ, {'IWD2R_RAND_TRACE': '1'}):
            self.assertEqual(oracle.recorder_env('s', 'd', 7, 0)
                             ['IWD2R_RAND_TRACE'], '1')
        with mock.patch.dict(os.environ, {'IWD2R_RAND_TRACE': '2'}):
            self.assertEqual(oracle.recorder_env('s', 'd', 7, 0)
                             ['IWD2R_RAND_TRACE'], '2')
        with mock.patch.dict(os.environ, {'IWD2R_RAND_TRACE': 'yes'}):
            self.assertEqual(oracle.recorder_env('s', 'd', 7, 0)
                             ['IWD2R_RAND_TRACE'], '1')
        with mock.patch.dict(os.environ, {'IWD2R_RAND_TRACE': '0'}):
            self.assertNotIn('IWD2R_RAND_TRACE',
                             oracle.recorder_env('s', 'd', 7, 0))
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
        os.makedirs(out, exist_ok=True)
        for name in (oracle.INJECTOR, oracle.RECORDER):
            open(os.path.join(out, name), 'w').close()
        game = os.path.join(self.tmp, 'game')
        os.makedirs(game, exist_ok=True)
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

    def test_record_takes_the_scenario_timeout(self):
        # --timeout wins; else the scenario header's "# timeout: S"
        # (realinput.header_timeout); else REPLAY_TIMEOUT.
        seen = []

        def run(build_dir, game_dir, script, env, timeout, host_run,
                out_dir, log):
            seen.append(timeout)
            os.makedirs(out_dir)
            return {'code': None, 'stdout': '', 'stderr': '',
                    'error': 'stop', 'seconds': 1.0}
        own = os.path.join(self.tmp, 'long.txt')
        with open(own, 'w') as f:
            f.write('# timeout: 420\nwait 1\ncheckpoint start\n')
        self.record(['replay', 'record', own], run)
        self.record(['replay', 'record', own, '--timeout', '60'], run)
        self.record(['replay', 'record', 'start_screen'], run)
        self.assertEqual(seen, [420, 60, oracle.REPLAY_TIMEOUT])
        self.assertEqual(oracle.replay_timeout(None, os.path.join(
            ROOT, 'scenarios', 'walk_and_talk.txt')), 420)

    def test_host_is_refused_without_the_desktop_opt_in(self):
        from unittest import mock
        env = {k: v for k, v in os.environ.items() if k != 'IWD2R_DESKTOP_OK'}
        for sub in (['record', 'start_screen'], ['compare', 'chargen_replay'],
                    ['crossload', 'x']):
            with mock.patch.dict(os.environ, env, clear=True), \
                    self.assertRaises(SystemExit) as cm:
                oracle.main(['replay', *sub, '--host'])
            self.assertEqual(cm.exception.code, 2, sub)
        with mock.patch.dict(os.environ, dict(env, IWD2R_DESKTOP_OK='0'), clear=True), \
                self.assertRaises(SystemExit):
            oracle.main(['replay', 'compare', 'chargen_replay', '--host'])
        # The error text agents read no longer points at --host.
        import inspect
        self.assertNotIn('with --host', inspect.getsource(oracle.record_in_sandbox))

    def test_host_switch(self):
        from unittest import mock
        with mock.patch.dict(os.environ, {'IWD2R_DESKTOP_OK': '1'}):
            self._host_switch()

    def _host_switch(self):
        code, events, locks, sandbox = self.record(
            ['replay', 'record', 'start_screen', '--host'])
        self.assertEqual(code, 0)
        # The sandbox lock too, while the copy's saves are aside: play.py
        # --vm windows copies the copy into its cache under it.
        self.assertEqual(events, [('host', [oracle.REPLAY_LOCK,
                                            sandbox.SANDBOX_LOCK])])
        self.assertEqual(locks.paths, [(oracle.REPLAY_LOCK, 'replay'),
                                       (sandbox.SANDBOX_LOCK, 'sandbox')])
        run = glob.glob(os.path.join(self.tmp, 'runs', '*', 'replay'))[0]
        with open(os.path.join(run, 'replay.json')) as f:
            self.assertEqual(json.load(f)['mode'], 'host')

    def test_compare_passes_the_host_switch(self):
        from unittest import mock
        seen = []
        with mock.patch.object(oracle, 'replay_compare',
                               lambda a: seen.append(a.host) or 0), \
                LockRecorder.patch(oracle, LockRecorder()), \
                mock.patch.dict(os.environ, {'IWD2R_DESKTOP_OK': '1'}):
            oracle.main(['replay', 'compare', 'chargen_replay'])
            oracle.main(['replay', 'compare', 'chargen_replay', '--host'])
        self.assertEqual(seen, [False, True])

    def test_load_gives_the_run_a_mpsave_of_its_own(self):
        # --load: the sandbox sees only the save (and default); the copy's
        # saves wait beside it and come back; what the run saved is kept.
        game = os.path.join(self.tmp, 'game')
        mpsave = os.path.join(game, 'mpsave')
        make_save(mpsave, '000000002-Quick-Save')
        make_save(mpsave, 'default')
        save = make_save(os.path.join(self.tmp, 'in'), '000000004-Probe')
        seen = []

        def run(build_dir, game_dir, script, env, timeout, host_run,
                out_dir, log):
            seen.append(sorted(os.listdir(os.path.join(game_dir, 'mpsave'))))
            seen.append(sorted(os.listdir(oracle.save_aside(game_dir))))
            make_save(os.path.join(game_dir, 'mpsave'), '000000005-New')
            os.makedirs(out_dir)
            with open(os.path.join(out_dir, 'status.txt'), 'w') as f:
                f.write('frame=10\nstate=finished\n')
            return {'code': 0, 'stdout': '', 'stderr': '', 'error': None,
                    'seconds': 1.0}
        code, _, _, _ = self.record(['replay', 'record', 'start_screen',
                                     '--load', save], run)
        self.assertEqual(seen, [['000000004-Probe', 'default'],
                                ['000000002-Quick-Save']])
        self.assertEqual(sorted(os.listdir(mpsave)),
                         ['000000002-Quick-Save', 'default'])
        self.assertFalse(os.path.exists(oracle.save_aside(game)))
        run_dir = glob.glob(os.path.join(self.tmp, 'runs', '*', 'replay'))[0]
        self.assertEqual(os.listdir(os.path.join(run_dir, 'saves')),
                         ['000000005-New'])
        with open(os.path.join(run_dir, 'replay.json')) as f:
            self.assertEqual(json.load(f)['saves'], {
                'mode': 'load', 'load': save, 'slot': '000000004-Probe',
                'made': ['000000005-New']})
        self.assertEqual(oracle.save_aside(oracle.GAME_DIR),
                         oracle.ORACLE_DIR + '/mpsave-aside')

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
        # An AI update that began before its frame's render ended (the AI
        # thread's 100 ms wait timed out during a long frame).
        overlap = dump(61, 61)
        overlap['globals'].update(ai_overlap_frames=2)
        self.assertEqual(oracle.invariant_problem('c', overlap),
                         'c: 2 frames by frame 61 had their AI update begin '
                         'before their render ended: the frame order did '
                         'not hold')
        overlap['globals'].update(ai_overlap_frames=0)
        self.assertIsNone(oracle.invariant_problem('c', overlap))
        # Our engine's replay-mode wait for the search thread gave up (a
        # lost semaphore post, no search thread); the original's dumps have
        # no such field.
        search = dump(61, 61)
        search['globals'].update(search_wait_timeouts=3)
        self.assertEqual(oracle.invariant_problem('c', search),
                         'c: 3 waits for the search thread by frame 61 gave '
                         'up at their deadline: a path search was never '
                         'taken up')
        search['globals'].update(search_wait_timeouts=0)
        self.assertIsNone(oracle.invariant_problem('c', search))
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
        # Every screen the M1 and M2 scenarios pass through (Party
        # Formation is single_player), and M4's load and save screens.
        self.assertEqual(oracle.load_screens(),
                         ['chapter', 'connection', 'create_char',
                          'single_player', 'world', 'load', 'save'])
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
                                ai_overlap_frames=2, in_async_update=1,
                                held_frames=4)
        self.assertEqual(oracle.field_differences(dump, other), [])
        # Only our engine writes search_wait_timeouts: never compared.
        other['globals'].update(search_wait_timeouts=0)
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
                            'ai_overlap_frames': 0,
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
            '10/9, ai_stalled_frames 0/0, ai_overlap_frames 0/0, '
            'in_async_update 0/0, held_frames 2/2; original/ours)',
            'checkpoint party (single_player): different, screen not '
            'enabled (frame 10/10, au_counter 10/10, ai_stalled_frames 0/0, '
            'ai_overlap_frames 0/0, in_async_update 0/0, held_frames 2/2; '
            'original/ours)',
            '  diff rng_state: original 1, ours 2',
            'checkpoint chargen (create_char): DIFFERENT (frame 10/10, '
            'au_counter 10/10, ai_stalled_frames 0/0, ai_overlap_frames 0/0, '
            'in_async_update 0/0, held_frames 2/2; original/ours)',
            '  DIFF rng_state: original 5, ours 7',
            'checkpoint known (create_char): match (frame 10/10, au_counter '
            '10/10, ai_stalled_frames 0/0, ai_overlap_frames 0/0, '
            'in_async_update 0/0, held_frames 2/2; original/ours)',
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
        # With the periodic dumps (compare_frames' counts).
        self.assertEqual(oracle.compare_verdict(
            0, 0, 0, [], {'enabled': 3, 'outside': 0, 'frames': 150,
                          'frames_outside': 1}),
            ['result: MATCH at 3 of 3 checkpoints and 150 of 150 periodic '
             'dumps',
             'note: 1 periodic dump differed outside the enabled screens'])

    def write_frames(self, folder, dumps):
        os.makedirs(folder, exist_ok=True)
        for frame, dump in dumps.items():
            with open(os.path.join(folder, 'frame-%d.json' % frame),
                      'w') as f:
                json.dump(dict(dump, frame=frame), f)

    def test_compare_frames(self):
        # M1/M2 review item 2: walk_and_talk's dialog had Reig, not Jon,
        # speak Jon's state between two checkpoints that both matched.
        screens = ['connection', 'create_char', 'world']
        with tempfile.TemporaryDirectory() as tmp:
            a = os.path.join(tmp, 'original')
            b = os.path.join(tmp, 'ours')
            self.write_frames(a, {30: self.dump('connection'),
                                  60: self.dump('create_char', rng=5),
                                  90: self.dump('single_player', rng=1),
                                  120: self.dump('world', rng=6),
                                  150: self.dump('world', rng=6),
                                  180: self.dump('world'),
                                  210: self.dump('world')})
            self.write_frames(b, {30: self.dump('connection', au=29),
                                  60: self.dump('create_char', rng=7),
                                  90: self.dump('single_player', rng=2),
                                  120: self.dump('world', rng=8),
                                  150: self.dump('world', rng=8),
                                  180: self.dump('world')})
            # Not a frame file: ignored.
            self.write_dumps(os.path.join(a, 'x'), {})
            with open(os.path.join(b, 'frame-x.json'), 'w') as f:
                f.write('{}')
            # A checkpoint's entry does not cover a frame; frame-N and "*"
            # do.
            known = [{'field': 'rng_state', 'checkpoint': 'frame-120',
                      'reason': 'r', 'evidence': '0x72DE60'},
                     {'field': 'rng_state', 'checkpoint': 'world',
                      'reason': 'r', 'evidence': '0x72DE60'}]
            counts = {}
            lines, failures, hits = oracle.compare_frames(a, b, screens,
                                                          known, counts)
            self.assertEqual(failures, 2)
            self.assertEqual(counts, {'frames': 5, 'frames_outside': 1})
            self.assertEqual([h[:4] for h in hits],
                             [('frame-120', 'rng_state', 6, 8)])
            self.assertEqual(lines, [
                'periodic dumps: 6 on both sides, 5 on an enabled screen',
                'frame 60 (create_char): DIFFERENT',
                '  DIFF rng_state: original 5, ours 7',
                'frame 90 (single_player): different, screen not enabled',
                '  diff rng_state: original 1, ours 2',
                'frame 120 (world): match',
                '  KNOWN rng_state: original 6, ours 8 (r; evidence '
                '0x72DE60)',
                'frame 150 (world): DIFFERENT',
                '  DIFF rng_state: original 6, ours 8',
                'note: 1 periodic dump only on the original (frame 210)'])
            # A "*" entry covers every frame.
            star = [dict(known[0], checkpoint='*')]
            lines, failures, hits = oracle.compare_frames(a, b, screens,
                                                          star)
            self.assertEqual(failures, 0)
            self.assertEqual([h[0] for h in hits], [
                'frame-60', 'frame-90', 'frame-120', 'frame-150'])
            # An unreadable dump fails; no dumps at all is no failure.
            with open(os.path.join(b, 'frame-180.json'), 'w') as f:
                f.write('{')
            lines, failures, hits = oracle.compare_frames(a, b, screens,
                                                          star)
            self.assertEqual(failures, 1)
            self.assertIn('frame 180: UNREADABLE on our side', lines)
            empty = os.path.join(tmp, 'empty')
            self.assertEqual(oracle.compare_frames(empty, empty, screens,
                                                   []),
                             (['periodic dumps: 0 on both sides, 0 on an '
                               'enabled screen'], 0, []))

    def test_replay_compare_fails_on_a_periodic_dump(self):
        from unittest import mock
        names = [a['name'] for a in realinput.load_scenario(
            os.path.join(ROOT, 'scenarios', 'chargen_replay.txt'))
            if a['op'] == 'checkpoint']

        def fake_run(speaker, ours=False):
            def run(*args, **kw):
                # record_original(script, actions, seed, timeout,
                # dump_every, run_dir, subdir, ...); run_ours has exe and
                # game first.
                run_dir, subdir = args[7:9] if ours else args[5:7]
                out = os.path.join(run_dir, subdir)
                self.write_dumps(out, {n: self.dump('create_char')
                                       for n in names})
                world = dict(self.dump('world'),
                             world={'dialog': {'speaker': speaker}})
                self.write_frames(out, {30: self.dump('create_char'),
                                        60: world})
                return 0
            return run

        def compare(our_speaker):
            game = tempfile.mkdtemp(dir=self.tmp)
            out = io.StringIO()
            with mock.patch.object(oracle, 'RUNS_DIR', self.tmp), \
                    mock.patch.object(oracle, 'ensure_vanilla',
                                      lambda: game), \
                    mock.patch.object(oracle, 'prepare_replay_ini',
                                      lambda g: None), \
                    mock.patch.object(oracle, 'record_original',
                                      fake_run(786444)), \
                    mock.patch.object(oracle, 'run_ours',
                                      fake_run(our_speaker, True)), \
                    contextlib.redirect_stdout(out):
                code = oracle.replay_compare(types.SimpleNamespace(
                    scenario='chargen_replay', seed=1, timeout=None,
                    dump_every=30, no_build=True, host=False))
            return code, out.getvalue()

        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        code, out = compare(786444)
        self.assertEqual(code, 0, out)
        self.assertIn('result: MATCH at %d of %d checkpoints and 2 of 2 '
                      'periodic dumps' % (len(names), len(names)), out)
        code, out = compare(983055)
        self.assertEqual(code, 1, out)
        self.assertIn('frame 60 (world): DIFFERENT\n  DIFF '
                      'world.dialog.speaker: original 786444, ours 983055',
                      out)
        self.assertIn('result: DIFFERENT: 1 difference', out)

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
        # Another scenario's checkpoint: not stale in this one.
        other = self.entry(field='z', checkpoint='reig_done')
        star = self.entry(field='z', checkpoint='*')
        self.assertEqual(oracle.stale_known([used, other, star], hits,
                                            ['cp']), [star])
        self.assertEqual(oracle.stale_known([other], [], ['reig_done']),
                         [other])

    def test_a_field_covers_the_values_inside_it(self):
        entry = self.entry(field='game.members[0].inventory[18]',
                           checkpoint='cp')
        for field in ('game.members[0].inventory[18]',
                      'game.members[0].inventory[18].resref',
                      'game.members[0].inventory[18].uses[2]'):
            self.assertIs(oracle.known_entry([entry], 'cp', field), entry)
        for field in ('game.members[0].inventory[180]',
                      'game.members[0].inventory[1]',
                      'game.members[0].inventory'):
            self.assertIsNone(oracle.known_entry([entry], 'cp', field))

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


def sav_bytes(entries):
    """A SAV V1.0 file holding entries ({name: data}), as the game packs
    ICEWIND2.SAV."""
    out = b'SAV V1.0'
    for name, data in entries.items():
        packed = zlib.compress(data)
        raw = name.encode('latin-1') + b'\0'
        out += struct.pack('<I', len(raw)) + raw
        out += struct.pack('<II', len(data), len(packed)) + packed
    return out


def make_save(folder, name, payload='x', sav=None):
    """A save folder as the game writes it: ICEWIND2.GAM with the
    GAMEV2.2 signature (CInfGame::SaveGameExists) and ICEWIND2.SAV, the
    payload itself or, with sav ({name: data}), a SAV V1.0 file of it."""
    path = os.path.join(folder, name)
    os.makedirs(path)
    with open(os.path.join(path, 'ICEWIND2.GAM'), 'wb') as f:
        f.write(b'GAMEV2.2' + payload.encode())
    with open(os.path.join(path, 'ICEWIND2.SAV'), 'wb') as f:
        f.write(sav_bytes(sav) if sav is not None else payload.encode())
    return path


class OracleSavesTest(unittest.TestCase):
    """replay --load, the "# saves:" header and the saves of a run
    (tools/oracle.py prepared_saves)."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

    def scenario(self, header, name='s.txt'):
        path = os.path.join(self.tmp, name)
        with open(path, 'w') as f:
            f.write(header + 'wait 1\ncheckpoint a\n# saves: ignored\n')
        return path

    def test_header_saves(self):
        self.assertIsNone(oracle.header_saves(self.scenario('# x\n')))
        self.assertEqual(oracle.header_saves(self.scenario(
            '# M4\n\n# saves: empty\n# timeout: 9\n')), 'empty')
        self.assertEqual(oracle.header_saves(self.scenario(
            '#saves:  LOAD \n')), 'load')
        for bad in ('# saves: some\n', '# saves: empty\n# saves: load\n'):
            with self.assertRaises(SystemExit, msg=bad):
                oracle.header_saves(self.scenario(bad))
        self.assertEqual(oracle.header_saves(
            os.path.join(ROOT, 'scenarios', 'pending', 'save_load.txt')),
            'empty')
        self.assertEqual(oracle.header_saves(
            os.path.join(ROOT, 'scenarios', 'pending', 'load_game.txt')),
            'load')
        self.assertIsNone(oracle.header_saves(
            os.path.join(ROOT, 'scenarios', 'walk_and_talk.txt')))

    def test_save_plan(self):
        plain = self.scenario('# x\n')
        empty = self.scenario('# saves: empty\n', 'e.txt')
        load = self.scenario('# saves: load\n', 'l.txt')
        save = make_save(self.tmp, '000000004-Probe Save')
        for script in (plain, empty):
            self.assertEqual(oracle.save_plan(script, None),
                             {'mode': 'empty', 'load': None})
        for script in (plain, empty, load):
            self.assertEqual(oracle.save_plan(script, save),
                             {'mode': 'load', 'load': save})
        os.makedirs(os.path.join(self.tmp, 'not-a-save'))
        make_save(self.tmp, 'default')
        for script, arg, why in (
                (load, None, 'give one with --load'),
                (plain, os.path.join(self.tmp, 'not-a-save'),
                 'not a save folder'),
                (plain, os.path.join(self.tmp, 'nowhere'),
                 'not a save folder'),
                (plain, os.path.join(self.tmp, 'default'), 'its own')):
            with self.assertRaises(SystemExit) as cm:
                oracle.save_plan(script, arg)
            self.assertIn(why, str(cm.exception))

    def test_is_save(self):
        save = make_save(self.tmp, '000000000-Auto-Save')
        self.assertTrue(oracle.is_save(save))
        os.rename(os.path.join(save, 'ICEWIND2.GAM'),
                  os.path.join(save, 'icewind2.gam'))
        self.assertTrue(oracle.is_save(save))
        with open(os.path.join(save, 'icewind2.gam'), 'wb') as f:
            f.write(b'GAMEV2.0')
        self.assertFalse(oracle.is_save(save))

    def listing(self, folder):
        return sorted(os.listdir(folder)) if os.path.isdir(folder) else None

    def test_vanilla_copy_saves_go_aside_and_come_back(self):
        mpsave = os.path.join(self.tmp, 'game', 'mpsave')
        aside = os.path.join(self.tmp, 'mpsave-aside')
        for name in ('000000000-Auto-Save', '000000004-big12345',
                     'default'):
            make_save(mpsave, name, name)
        load = make_save(os.path.join(self.tmp, 'in'), '000000004-Probe Save')
        out = os.path.join(self.tmp, 'run', 'original')
        seen = []
        with contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'load',
                                                'load': load}, out,
                                       aside) as report:
                seen.append(self.listing(mpsave))
                seen.append(self.listing(aside))
                # The run saves and autosaves.
                make_save(mpsave, '000000005-Second', 'new')
                make_save(mpsave, '000000000-Autosave - Prologue', 'auto')
        self.assertEqual(seen, [['000000004-Probe Save', 'default'],
                                ['000000000-Auto-Save',
                                 '000000004-big12345']])
        self.assertEqual(self.listing(mpsave), [
            '000000000-Auto-Save', '000000004-big12345', 'default'])
        self.assertIsNone(self.listing(aside))
        self.assertEqual(self.listing(os.path.join(out, 'saves')), [
            '000000000-Autosave - Prologue', '000000005-Second'])
        self.assertEqual(report, {
            'mode': 'load', 'load': load, 'slot': '000000004-Probe Save',
            'made': ['000000000-Autosave - Prologue', '000000005-Second']})
        with open(os.path.join(mpsave, '000000004-big12345',
                               'ICEWIND2.SAV')) as f:
            self.assertEqual(f.read(), '000000004-big12345')

    def test_saves_come_back_when_the_run_fails(self):
        mpsave = os.path.join(self.tmp, 'mpsave')
        aside = os.path.join(self.tmp, 'aside')
        make_save(mpsave, '000000002-Quick-Save')
        out = os.path.join(self.tmp, 'out')
        with self.assertRaises(KeyboardInterrupt), \
                contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'empty',
                                                'load': None}, out, aside):
                self.assertEqual(self.listing(mpsave), [])
                make_save(mpsave, '000000004-Half')
                raise KeyboardInterrupt
        self.assertEqual(self.listing(mpsave), ['000000002-Quick-Save'])
        self.assertEqual(self.listing(os.path.join(out, 'saves')),
                         ['000000004-Half'])

    def test_a_refused_move_leaves_the_copys_saves_in_place(self):
        # The second rename is refused: the first comes back from aside,
        # the second never left, and nothing of the copy's is harvested
        # or deleted.
        from unittest import mock
        mpsave = os.path.join(self.tmp, 'game', 'mpsave')
        aside = os.path.join(self.tmp, 'aside')
        for name in ('000000000-Auto-Save', '000000002-Quick-Save',
                     '000000004-big12345', 'default'):
            make_save(mpsave, name, name)
        out = os.path.join(self.tmp, 'out')
        real_rename = os.rename
        calls = []

        def rename(src, dst):
            calls.append(os.path.basename(src))
            if len(calls) == 2:
                raise PermissionError(13, 'refused', src)
            return real_rename(src, dst)
        ran = []
        with mock.patch.object(oracle.os, 'rename', rename), \
                self.assertRaises(PermissionError), \
                contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'empty',
                                                'load': None}, out, aside):
                ran.append(True)
        self.assertEqual(ran, [])
        self.assertEqual(self.listing(mpsave), [
            '000000000-Auto-Save', '000000002-Quick-Save',
            '000000004-big12345', 'default'])
        self.assertIsNone(self.listing(aside))
        self.assertIsNone(self.listing(os.path.join(out, 'saves')))
        for name in ('000000000-Auto-Save', '000000004-big12345'):
            with open(os.path.join(mpsave, name, 'ICEWIND2.SAV')) as f:
                self.assertEqual(f.read(), name)

    def test_a_load_copy_that_stops_halfway_is_deleted(self):
        from unittest import mock
        mpsave = os.path.join(self.tmp, 'game', 'mpsave')
        aside = os.path.join(self.tmp, 'aside')
        make_save(mpsave, '000000002-Quick-Save', 'own')
        load = make_save(os.path.join(self.tmp, 'in'), '000000004-Probe')
        out = os.path.join(self.tmp, 'out')

        def copytree(src, dst):
            os.makedirs(dst)
            with open(os.path.join(dst, 'ICEWIND2.GAM'), 'w') as f:
                f.write('half')
            raise OSError(28, 'no space left')
        with mock.patch.object(oracle.shutil, 'copytree', copytree), \
                self.assertRaises(OSError), \
                contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'load',
                                                'load': load}, out, aside):
                self.fail('the run must not start')
        self.assertEqual(self.listing(mpsave), ['000000002-Quick-Save'])
        self.assertIsNone(self.listing(os.path.join(out, 'saves')))
        self.assertIsNone(self.listing(aside))

    def test_a_save_over_the_loaded_slot_is_kept(self):
        mpsave = os.path.join(self.tmp, 'mpsave')
        load = make_save(os.path.join(self.tmp, 'in'), '000000004-Probe',
                         'loaded')
        for saves_over, kept in ((False, None), (True, ['000000004-Probe'])):
            out = os.path.join(self.tmp, 'out-%s' % saves_over)
            with contextlib.redirect_stdout(io.StringIO()):
                with oracle.prepared_saves(mpsave, {'mode': 'load',
                                                    'load': load}, out) as r:
                    if saves_over:
                        with open(os.path.join(mpsave, '000000004-Probe',
                                               'ICEWIND2.SAV'), 'w') as f:
                            f.write('saved over')
            self.assertEqual(self.listing(mpsave), [])
            self.assertEqual(self.listing(os.path.join(out, 'saves')), kept)
            self.assertEqual(r['made'], kept or [])
        with open(os.path.join(self.tmp, 'out-True', 'saves',
                               '000000004-Probe', 'ICEWIND2.SAV')) as f:
            self.assertEqual(f.read(), 'saved over')

    def test_a_dead_runs_saves_are_put_back_first(self):
        mpsave = os.path.join(self.tmp, 'mpsave')
        aside = os.path.join(self.tmp, 'aside')
        make_save(aside, '000000004-big12345', 'user')
        make_save(mpsave, '000000004-big12345', 'dead run')
        make_save(mpsave, 'default')
        with contextlib.redirect_stdout(io.StringIO()) as text:
            with oracle.prepared_saves(mpsave, None, self.tmp, aside) as r:
                self.assertEqual(r, {})
        self.assertIn('put back 1 entry a run that died left',
                      text.getvalue())
        self.assertEqual(self.listing(mpsave),
                         ['000000004-big12345', 'default'])
        self.assertIsNone(self.listing(aside))
        with open(os.path.join(mpsave, '000000004-big12345',
                               'ICEWIND2.SAV')) as f:
            self.assertEqual(f.read(), 'user')

    def test_a_dead_runs_own_saves_leave_the_copy(self):
        # A run died with the copy's saves aside: its record names what
        # mpsave held before; what else is there is that run's (its load
        # copy, an autosave it wrote) and moves into the next run's folder.
        mpsave = os.path.join(self.tmp, 'game', 'mpsave')
        aside = os.path.join(self.tmp, 'mpsave-aside')
        make_save(aside, '000000000-Autosave - Prologue', 'user')
        make_save(aside, '000000004-big12345', 'user')
        make_save(mpsave, '000000000-Autosave - Prologue', 'dead run')
        make_save(mpsave, '000000005-Probe', 'dead run')
        make_save(mpsave, 'default')
        with open(oracle.aside_record(aside), 'w') as f:
            json.dump(['000000000-Autosave - Prologue',
                       '000000004-big12345'], f)
        out = os.path.join(self.tmp, 'next')
        with contextlib.redirect_stdout(io.StringIO()) as text:
            with oracle.prepared_saves(mpsave, None, out, aside):
                pass
        self.assertIn('moved 000000005-Probe, which a run that died made',
                      text.getvalue())
        self.assertEqual(self.listing(mpsave), [
            '000000000-Autosave - Prologue', '000000004-big12345',
            'default'])
        with open(os.path.join(mpsave, '000000000-Autosave - Prologue',
                               'ICEWIND2.SAV')) as f:
            self.assertEqual(f.read(), 'user')
        self.assertEqual(self.listing(os.path.join(out, 'dead-run-saves')),
                         ['000000005-Probe'])
        self.assertFalse(os.path.exists(oracle.aside_record(aside)))
        # A run that ends normally leaves no record behind.
        with contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'empty',
                                                'load': None}, out, aside):
                self.assertTrue(os.path.exists(oracle.aside_record(aside)))
        self.assertFalse(os.path.exists(oracle.aside_record(aside)))

    def test_a_private_view_is_emptied_in_place(self):
        mpsave = os.path.join(self.tmp, 'view', 'mpsave')
        make_save(mpsave, '000000000-Auto-Save')
        make_save(mpsave, 'default')
        out = os.path.join(self.tmp, 'ours')
        with contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'empty',
                                                'load': None}, out):
                self.assertEqual(self.listing(mpsave), ['default'])
        self.assertEqual(self.listing(mpsave), ['default'])
        self.assertIsNone(self.listing(os.path.join(out, 'saves')))
        # A view whose install has no mpsave gets one.
        mpsave = os.path.join(self.tmp, 'bare', 'mpsave')
        with contextlib.redirect_stdout(io.StringIO()):
            with oracle.prepared_saves(mpsave, {'mode': 'empty',
                                                'load': None}, out):
                self.assertEqual(self.listing(mpsave), [])

    def test_pick_save(self):
        self.assertEqual(oracle.save_number('000000004-Probe Save'), 4)
        self.assertEqual(oracle.save_number('crash_07_23'), -1)
        made = ['000000000-Autosave - Prologue', '000000004-Probe Save']
        self.assertEqual(oracle.pick_save(made, made),
                         ('000000004-Probe Save', None))
        self.assertEqual(oracle.pick_save(made, made,
                                          '000000000-Autosave - Prologue'),
                         ('000000000-Autosave - Prologue', None))
        name, why = oracle.pick_save(made, made[:1])
        self.assertIsNone(name)
        self.assertIn('different saves', why)
        self.assertIn('neither', oracle.pick_save([], [])[1])
        self.assertIn('not both x', oracle.pick_save(made, made, 'x')[1])

    def test_record_and_compare_pass_the_plan_to_both_sides(self):
        from unittest import mock
        save = make_save(self.tmp, '000000004-Probe Save')
        seen = []

        def record(script, actions, *a, **kw):
            seen.append(('original', os.path.basename(script),
                         kw.get('saves')))
            return 0

        def ours(exe, game, script, actions, *a, **kw):
            seen.append(('ours', os.path.basename(script), kw.get('saves')))
            return 0
        oracle_play = __import__('play')
        with LockRecorder.patch(oracle, LockRecorder()), \
                mock.patch.object(oracle, 'record_original', record), \
                mock.patch.object(oracle, 'run_ours', ours), \
                mock.patch.object(oracle, 'ensure_vanilla', lambda: self.tmp), \
                mock.patch.object(oracle, 'prepare_replay_ini', lambda g: None), \
                mock.patch.object(oracle, 'RUNS_DIR',
                                  os.path.join(self.tmp, 'runs')), \
                mock.patch.object(oracle_play, 'backup_inis',
                                  lambda g, b: []), \
                mock.patch.object(oracle_play, 'restore_inis',
                                  lambda b, g, n: None), \
                contextlib.redirect_stdout(io.StringIO()):
            oracle.main(['replay', 'record', 'start_screen', '--load', save])
            oracle.main(['replay', 'compare', 'start_screen', '--no-build',
                         '--load', save])
            oracle.main(['replay', 'compare', os.path.join(
                ROOT, 'scenarios', 'pending', 'save_load.txt'),
                '--no-build'])
            oracle.main(['replay', 'record', 'start_screen'])
        plan = {'mode': 'load', 'load': save}
        empty = {'mode': 'empty', 'load': None}
        self.assertEqual(seen, [
            ('original', 'start_screen.txt', plan),
            ('original', 'start_screen.txt', plan),
            ('ours', 'start_screen.txt', plan),
            ('original', 'save_load.txt', empty),
            ('ours', 'save_load.txt', empty),
            ('original', 'start_screen.txt', empty)])

    def test_load_scenario(self):
        actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'pending', 'load_game.txt'))
        self.assertEqual([a['name'] for a in actions
                          if a['op'] == 'checkpoint'],
                         ['menu', 'load_screen', 'loaded', 'loaded_settled'])
        actions = realinput.load_scenario(os.path.join(
            ROOT, 'scenarios', 'pending', 'save_load.txt'))
        names = [a['name'] for a in actions if a['op'] == 'checkpoint']
        self.assertEqual(names[0], 'menu')
        self.assertEqual(names[-6:], ['save_screen', 'saved', 'walked_on',
                                      'load_screen', 'loaded',
                                      'loaded_settled'])
        self.assertEqual(oracle.replay_timeout(None, os.path.join(
            ROOT, 'scenarios', 'pending', 'save_load.txt')), 420)


savediff = load_tool('savediff')


def area_bytes(entrances=7, vertices=386, region_vertices=4,
               door_vertices=3, wed=b'AR1000\0P', unset=b'0 0 0 OB',
               travel=b'\xd6\x03\0\0\x16\x02\0\0'):
    """An ARE V9.1 area file at the offsets tools/savediff.py reads: its
    WED resref (wed: the original's has stack bytes after the NUL), a
    region without flag 0x200 whose +0x88-0x8F hold unset bytes (unset),
    one with it whose +0x88-0x8F are set (travel), and one door."""
    data = bytearray(0x400)
    data[0x08:0x10] = wed
    struct.pack_into('<H', data, 0x6A, 2)
    struct.pack_into('<I', data, 0x6C, 0x100)
    struct.pack_into('<II', data, 0x78, 0x380, entrances)
    struct.pack_into('<H', data, 0x84, 25)
    struct.pack_into('<H', data, 0x90, vertices)
    struct.pack_into('<II', data, 0xB4, 1, 0x300)
    data[0x100:0x100 + 9] = b'Life Raft'
    struct.pack_into('<H', data, 0x100 + 0x2A, region_vertices)
    data[0x100 + 0x88:0x100 + 0x90] = unset
    data[0x1C4:0x1C4 + 6] = b'To1001'
    struct.pack_into('<I', data, 0x1C4 + 0x60, 0x200)
    data[0x1C4 + 0x88:0x1C4 + 0x90] = travel
    data[0x300:0x300 + 12] = b'AR1000_Door1'
    struct.pack_into('<HH', data, 0x300 + 0x30, door_vertices, 4)
    struct.pack_into('<8H', data, 0x300 + 0x38, 3174, 3, 3183, 13, 958, 486,
                     1005, 540)
    return bytes(data)


def gam_bytes(names=(b'FEMALE_BARBARIAN_1\0f\0oracle\\game',),
              npcs=(b'NPC\0garbage',), before=b''):
    """A GAM V2.2 game file with party and other-creature entries (0x340
    bytes, from 0xB4) whose field_25E (+0x25E, 32 bytes) hold names, with
    whatever follows their NUL."""
    data = bytearray(b'GAMEV2.2' + bytes(0xAC) + before)
    tables = []
    for entries in (names, npcs):
        tables.append((len(data), len(entries)))
        for name in entries:
            entry = bytearray(0x340)
            entry[0x25E:0x25E + len(name[:32])] = name[:32]
            data += entry
    struct.pack_into('<II', data, 0x20, *tables[0])
    struct.pack_into('<II', data, 0x30, *tables[1])
    return bytes(data)


class SaveDiffTest(unittest.TestCase):
    """tools/savediff.py on two save folders."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

    def save(self, side, gam=None, area=None, extra=None, wmp=b'map',
             bmp=b'shot'):
        path = os.path.join(self.tmp, side)
        os.makedirs(path)
        files = {'ICEWIND2.GAM': gam or gam_bytes(),
                 'ICEWIND2.SAV': sav_bytes(dict(
                     {'AR1000.ARE': area or area_bytes()}, **(extra or {}))),
                 'WORLDMAP.WMP': wmp, 'ICEWIND2.BMP': bmp,
                 'PORTRT0.BMP': bmp}
        for name, data in files.items():
            with open(os.path.join(path, name), 'wb') as f:
                f.write(data)
        return path

    def test_the_same_data(self):
        a = self.save('a')
        b = self.save('b', bmp=b'another shot')
        lines, different = savediff.compare_saves(a, b)
        self.assertEqual(different, 0)
        self.assertEqual(lines, [
            '  ICEWIND2.GAM: same (41 unset bytes masked)',
            '  ICEWIND2.SAV:AR1000.ARE: same (9 unset bytes masked)',
            '  WORLDMAP.WMP: same'])

    def test_the_original_s_unset_bytes_are_masked(self):
        # Two of the original's saves of one game hold different stack
        # bytes after a name's NUL, in the WED resref and in a region
        # without flag 0x200 (as runs 20261016-171655 and -173235 did);
        # ours holds zeros there. All compare the same.
        a = self.save('a')
        b = self.save('b', gam=gam_bytes(
            names=(b'FEMALE_BARBARIAN_1\0' + bytes(13),),
            npcs=(b'NPC\0other bytes',)),
            area=area_bytes(wed=b'AR1000\0\0', unset=bytes(8)))
        c = self.save('c', area=area_bytes(unset=b'"" 0 0 0'))
        for other in (b, c):
            lines, different = savediff.compare_saves(a, other)
            self.assertEqual(different, 0, lines)
        # Set bytes are not masked: a name before its NUL, a travel
        # region's +0x88, the WED resref itself.
        for side, save in (
                ('d', {'gam': gam_bytes(names=(b'MALE_BARBARIAN_1\0x',))}),
                ('e', {'area': area_bytes(travel=bytes(8))}),
                ('f', {'area': area_bytes(wed=b'AR1001\0P')})):
            lines, different = savediff.compare_saves(a, self.save(side,
                                                                   **save))
            self.assertEqual(different, 1, (side, lines))

    def test_masks(self):
        gam = gam_bytes()
        self.assertEqual(savediff.gam_masks(gam), [
            (0xB4 + 0x25E + 19, 0xB4 + 0x25E + 32),
            (0xB4 + 0x340 + 0x25E + 4, 0xB4 + 0x340 + 0x25E + 32)])
        self.assertEqual(savediff.gam_masks(b'GAMEV2.2'), [])
        area = area_bytes()
        self.assertEqual(savediff.are_masks(area),
                         [(0x0F, 0x10), (0x188, 0x190)])
        self.assertEqual(savediff.are_masks(area_bytes(wed=b'AR100000')),
                         [(0x188, 0x190)])
        self.assertEqual(savediff.masked(b'abcdef', [(1, 3), (5, 9)]),
                         b'a\0\0de\0')

    def test_an_area_differs_by_its_parts(self):
        a = self.save('a', extra={'default.toh': b'toh'})
        b = self.save('b', area=area_bytes(entrances=0, vertices=225,
                                           region_vertices=2,
                                           door_vertices=2))
        lines, different = savediff.compare_saves(a, b)
        self.assertEqual(different, 2)
        self.assertIn('    entrances: 7, 0', lines)
        self.assertIn('    vertices: 386, 225', lines)
        self.assertIn('    region Life Raft: 4 vertices; 2', lines)
        self.assertIn('    door AR1000_Door1: vertices 3 open, 4 closed, '
                      'boxes (3174, 3, 3183, 13) (958, 486, 1005, 540); 2, '
                      '4, (3174, 3, 3183, 13) (958, 486, 1005, 540)', lines)
        self.assertIn('  ICEWIND2.SAV:default.toh: MISSING in %s' % b, lines)

    def test_unreadable_and_missing_files(self):
        a = self.save('a')
        b = self.save('b')
        with open(os.path.join(b, 'ICEWIND2.SAV'), 'wb') as f:
            f.write(b'SAV V1.0' + b'\x05')
        os.remove(os.path.join(b, 'WORLDMAP.WMP'))
        lines, different = savediff.compare_saves(a, b)
        self.assertEqual(different, 2)
        self.assertTrue(lines[1].startswith('  ICEWIND2.SAV: unreadable'))
        self.assertEqual(lines[2], '  WORLDMAP.WMP: MISSING in %s' % b)
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(savediff.main([a, b]), 1)
            self.assertEqual(savediff.main([a, a]), 0)
            self.assertEqual(savediff.main([a]), 2)
        self.assertIn('savediff: 2 files differ', text.getvalue())
        self.assertIn('savediff: the saves hold the same data',
                      text.getvalue())


class CrossloadTest(unittest.TestCase):
    """tools/oracle.py replay crossload, with both engines stubbed: each
    run writes its checkpoint dumps and, in the save scenario, a save."""

    CHECKPOINTS = ('menu', 'load_screen', 'loaded', 'loaded_settled')

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name

    def dump(self, name, gold):
        return {'frame': 10, 'checkpoint': name,
                'screen': 'world' if name.startswith('loaded')
                else 'connection', 'rng_state': 1,
                'globals': {'au_counter': 10, 'ai_stalled_frames': 0,
                            'ai_overlap_frames': 0, 'in_async_update': 0,
                            'reinitializing': 0, 'fullscreen': 0,
                            'loading': 0, 'loads': 0, 'held_frames': 0,
                            'ai_rng_state': 3},
                'chargen': None, 'world': None,
                'game': {'party_gold': gold, 'members': []}}

    def crossload(self, argv, gold=None, saves=None, payload=None):
        """oracle.main(['replay', 'crossload'] + argv) with the runs
        stubbed. gold: {run folder: party gold in its dumps} (36 for any
        other); saves: {'original': [...], 'ours': [...]}, the saves each
        engine's save run makes; payload: {engine: its saves' game file
        bytes} (the same for both by default). Returns (exit code, runs,
        output)."""
        from unittest import mock
        gold = gold or {}
        payload = payload or {}
        saves = saves or {'original': ['000000000-Autosave - Prologue',
                                       '000000004-Probe Save'],
                          'ours': ['000000000-Autosave - Prologue',
                                   '000000004-Probe Save']}
        runs = []

        def fake(engine, script, run_dir, subdir, plan):
            out = os.path.join(run_dir, subdir)
            os.makedirs(out)
            runs.append((subdir, engine, os.path.basename(script),
                         plan and plan['mode'],
                         plan and plan['load'] and os.path.relpath(
                             plan['load'], run_dir)))
            names = [a['name'] for a in realinput.load_scenario(script)
                     if a['op'] == 'checkpoint']
            for name in names:
                with open(os.path.join(out, 'checkpoint-%s.json' % name),
                          'w') as f:
                    json.dump(self.dump(name, gold.get(subdir, 36)), f)
            # One periodic dump, on the world screen.
            with open(os.path.join(out, 'frame-30.json'), 'w') as f:
                periodic = self.dump('loaded', gold.get(subdir, 36))
                del periodic['checkpoint']
                json.dump(periodic, f)
            if subdir.startswith('save-'):
                for name in saves[engine]:
                    make_save(os.path.join(out, 'saves'), name,
                              payload.get(engine, 'game'),
                              {'AR1000.ARE': b'area'})
            return 0

        def record(script, actions, seed, timeout, dump_every, run_dir,
                   subdir, **kw):
            return fake('original', script, run_dir, subdir, kw['saves'])

        def ours(exe, game, script, actions, seed, timeout, dump_every,
                 run_dir, subdir, **kw):
            return fake('ours', script, run_dir, subdir, kw['saves'])
        oracle_play = __import__('play')
        with LockRecorder.patch(oracle, LockRecorder()), \
                mock.patch.object(oracle, 'record_original', record), \
                mock.patch.object(oracle, 'run_ours', ours), \
                mock.patch.object(oracle, 'ensure_vanilla', lambda: self.tmp), \
                mock.patch.object(oracle, 'prepare_replay_ini', lambda g: None), \
                mock.patch.object(oracle, 'RUNS_DIR',
                                  os.path.join(self.tmp, 'runs')), \
                mock.patch.object(oracle_play, 'backup_inis',
                                  lambda g, b: []), \
                mock.patch.object(oracle_play, 'restore_inis',
                                  lambda b, g, n: None), \
                contextlib.redirect_stdout(io.StringIO()) as text:
            code = oracle.main(['replay', 'crossload', '--no-build'] + argv)
        return code, runs, text.getvalue()

    def test_each_engine_loads_the_others_save(self):
        code, runs, text = self.crossload([])
        self.assertEqual(code, 0, text)
        save = '000000004-Probe Save'
        self.assertEqual(runs, [
            ('save-original', 'original', 'save_load.txt', 'empty', None),
            ('save-ours', 'ours', 'save_load.txt', 'empty', None),
            ('original-loads-ours', 'original', 'load_game.txt', 'load',
             'save-ours/saves/' + save),
            ('ours-loads-original', 'ours', 'load_game.txt', 'load',
             'save-original/saves/' + save),
            ('ours-loads-ours', 'ours', 'load_game.txt', 'load',
             'save-ours/saves/' + save),
            # By default the original loads its own save too.
            ('original-loads-original', 'original', 'load_game.txt',
             'load', 'save-original/saves/' + save)])
        self.assertIn('== the saves, the original\'s and ours: the same '
                      'data', text)
        self.assertIn('== compare original-loads-ours with '
                      'ours-loads-original: match at 4 of 4 checkpoints '
                      'and 1 periodic dump', text)
        self.assertIn('== compare original-loads-original with '
                      'original-loads-ours: match', text)
        self.assertIn('result: MATCH: the saves (000000004-Probe Save) hold '
                      'the same data and each engine loads the other\'s to '
                      'the same state', text)
        report = glob.glob(os.path.join(self.tmp, 'runs', '*',
                                        'crossload.json'))
        with open(report[0]) as f:
            report = json.load(f)
        self.assertEqual(report['save'], save)
        self.assertEqual(report['save_files_differ'], 0)
        self.assertEqual([(p['first'], p['second']) for p in
                          report['pairs']],
                         list(oracle.CROSSLOAD_PAIRS
                              + oracle.CROSSLOAD_OWN_PAIRS))

    def test_a_difference_names_the_runs(self):
        code, _, text = self.crossload([], gold={'original-loads-ours': 40})
        self.assertEqual(code, 1)
        self.assertIn('  DIFF game.party_gold: original-loads-ours 40, '
                      'ours-loads-original 36', text)
        self.assertIn('== compare ours-loads-original with ours-loads-ours: '
                      'match', text)
        self.assertIn('frame 30 (world): DIFFERENT', text)
        # Three pairs have that run, 4 checkpoints and a periodic dump
        # each.
        self.assertIn('result: DIFFERENT: 15 differences in the loaded '
                      'states, 0 files of the saves', text)

    def test_the_saves_are_compared_as_data(self):
        # A field both our saver and our loader get wrong: the loaded
        # states match, the saves do not.
        code, runs, text = self.crossload([], payload={'ours': 'other'})
        self.assertEqual(code, 1)
        self.assertEqual(len(runs), 6)
        self.assertIn('== the saves, the original\'s and ours: 1 file '
                      'DIFFERENT', text)
        self.assertIn('  ICEWIND2.GAM: DIFFERENT', text)
        self.assertIn('  ICEWIND2.SAV:AR1000.ARE: same', text)
        self.assertIn('result: DIFFERENT: 0 differences in the loaded '
                      'states, 1 file of the saves', text)

    def test_quick_leaves_out_the_originals_own_save(self):
        code, runs, text = self.crossload(['--quick', '--seed', '7'])
        self.assertEqual(code, 0, text)
        self.assertEqual([r[0] for r in runs][2:], [
            'original-loads-ours', 'ours-loads-original', 'ours-loads-ours'])
        self.assertNotIn('original-loads-original', text)

    def test_the_sides_must_make_the_same_saves(self):
        code, runs, text = self.crossload([], saves={
            'original': ['000000004-Probe Save'],
            'ours': ['000000000-Autosave - Prologue',
                     '000000004-Probe Save']})
        self.assertEqual(code, 1)
        self.assertEqual(len(runs), 2)
        self.assertIn('crossload: the sides made different saves', text)
        code, runs, text = self.crossload(['--slot', '000000004-Probe Save'],
                                          saves={
            'original': ['000000004-Probe Save'],
            'ours': ['000000000-Autosave - Prologue',
                     '000000004-Probe Save']})
        self.assertEqual(code, 0, text)

    def test_saves_from_an_earlier_run(self):
        earlier = os.path.join(self.tmp, 'earlier')
        for side in ('original', 'ours'):
            make_save(os.path.join(earlier, side, 'saves'), '000000004-A',
                      'game', {})
        # A relative path is taken as it reads from here.
        code, runs, text = self.crossload(['--saves-from',
                                           os.path.relpath(earlier)])
        self.assertEqual(code, 0, text)
        self.assertEqual([r[0] for r in runs], [
            'original-loads-ours', 'ours-loads-original', 'ours-loads-ours',
            'original-loads-original'])
        self.assertTrue(runs[0][4].endswith('earlier/ours/saves/000000004-A'))
        report = glob.glob(os.path.join(self.tmp, 'runs', '*',
                                        'crossload.json'))
        with open(report[0]) as f:
            self.assertEqual(json.load(f)['saves_from'], earlier)
        with self.assertRaises(SystemExit), \
                contextlib.redirect_stdout(io.StringIO()):
            self.crossload(['--saves-from', self.tmp])


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


class GhidraCloneTest(unittest.TestCase):
    """make_project.worktree_clone: one read-only project copy per worktree."""

    def setUp(self):
        self.mp = load_ghidra_tool('make_project')
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        root = self._tmp.name
        self.main = os.path.join(root, 'ghidra-projects', 'iwd2')
        os.makedirs(os.path.join(self.main, 'iwd2.rep'))
        for name, text in (('iwd2.gpr', 'gpr'), ('iwd2.lock', ''),
                           ('iwd2.lock~', ''), ('headless-query.log', 'x'),
                           ('iwd2.rep/data', 'v1')):
            with open(os.path.join(self.main, name), 'w') as f:
                f.write(text)
        self.mp.PROJECT_DIR = self.main
        self.mp.LOCK_PATH = os.path.join(root, 'ghidra-projects', 'iwd2.lock')
        self.mp.CLONES_DIR = os.path.join(root, 'ghidra-projects', 'clones')
        self.mp.GENERATION_PATH = os.path.join(self.main, 'generation')

    def read(self, clone, name):
        with open(os.path.join(clone, name)) as f:
            return f.read()

    def test_copies_the_project_without_its_locks_and_logs(self):
        clone, lock = self.mp.worktree_clone('/w/p3-effects-core')
        self.assertTrue(clone.endswith(os.path.join('clones', 'p3-effects-core', 'iwd2')))
        self.assertTrue(lock.endswith(os.path.join('p3-effects-core', 'iwd2.lock')))
        self.assertEqual(self.read(clone, 'iwd2.rep/data'), 'v1')
        for name in ('iwd2.lock', 'iwd2.lock~', 'headless-query.log'):
            self.assertFalse(os.path.exists(os.path.join(clone, name)))

    def test_each_worktree_gets_its_own_copy(self):
        a, lock_a = self.mp.worktree_clone('/w/p3-a')
        b, lock_b = self.mp.worktree_clone('/w/p3-b')
        self.assertNotEqual(a, b)
        self.assertNotEqual(lock_a, lock_b)

    def test_a_copy_is_reused_until_the_main_project_changes(self):
        clone, _ = self.mp.worktree_clone('/w/p3-a')
        with open(os.path.join(self.main, 'iwd2.rep/data'), 'w') as f:
            f.write('v2')
        self.mp.worktree_clone('/w/p3-a')
        self.assertEqual(self.read(clone, 'iwd2.rep/data'), 'v1')
        self.mp.bump_generation()
        self.mp.worktree_clone('/w/p3-a')
        self.assertEqual(self.read(clone, 'iwd2.rep/data'), 'v2')

    def test_a_copy_missing_its_project_file_is_made_again(self):
        clone, _ = self.mp.worktree_clone('/w/p3-a')
        os.remove(os.path.join(clone, 'iwd2.gpr'))
        self.mp.worktree_clone('/w/p3-a')
        self.assertEqual(self.read(clone, 'iwd2.gpr'), 'gpr')


class MakeDllProjectTest(unittest.TestCase):
    """make_dll_project: one analysed Ghidra project per DLL under
    ghidra-projects/<name>, imported under make_project's lock."""

    def setUp(self):
        self.mdp = load_ghidra_tool('make_dll_project')
        # The make_project instance this module imported (the shared
        # sys.modules one), whose headless() the tool runs through.
        self.mp = self.mdp.make_project
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        root = self._tmp.name
        self.projects = os.path.join(root, 'ghidra-projects')
        self.dll = os.path.join(root, 'dll', 'IEexHelper.fork.dll')
        os.makedirs(os.path.dirname(self.dll))
        with open(self.dll, 'wb') as f:
            f.write(b'MZ fake dll')
        self.mdp.PROJECTS_DIR = self.projects
        self.mp.LOCK_PATH = os.path.join(self.projects, 'iwd2.lock')

    def fake_headless(self, runs):
        """Patch subprocess so analyzeHeadless only writes the .gpr."""

        def run(cmd, env=None, capture_output=True, text=True):
            runs.append(cmd)
            # Ghidra would create <project_location>/<project_name>.gpr.
            with open(os.path.join(cmd[1], cmd[2] + '.gpr'), 'w') as f:
                f.write('gpr')
            return subprocess.CompletedProcess(cmd, 0, 'stdout', '')

        self.addCleanup(setattr, self.mp, 'subprocess', subprocess)
        self.mp.subprocess = types.SimpleNamespace(run=run)

    def test_make_dll_project_paths_and_noop(self):
        runs = []
        self.fake_headless(runs)
        target = os.path.join(self.projects, 'ieexhelper-fork')
        self.assertEqual(self.mdp.make('ieexhelper-fork', self.dll), target)
        self.assertEqual(len(runs), 1)
        self.assertIn('-import', runs[0])
        self.assertIn(self.dll, runs[0])
        self.assertTrue(os.path.exists(
            os.path.join(target, 'ieexhelper-fork.gpr')))
        self.assertEqual(self.mdp.make('ieexhelper-fork', self.dll), target)
        self.assertEqual(len(runs), 1)  # .gpr exists: no second headless run
        with self.assertRaises(ValueError):
            self.mdp.make('../escape', self.dll)

    def test_make_dll_project_holds_lock(self):
        held = []

        @contextlib.contextmanager
        def spy(path, what, timeout=None):
            held.append(os.path.normpath(str(path)))
            yield

        self.addCleanup(setattr, self.mp.filelock, 'hold',
                        self.mp.filelock.hold)
        self.mp.filelock.hold = spy
        runs = []
        self.fake_headless(runs)
        self.mdp.make('ieexhelper-fork', self.dll)
        self.assertEqual(held, [os.path.normpath(self.mp.LOCK_PATH)])


class XrecTest(unittest.TestCase):
    """xrec: funcs reads the // 0xADDR comment with the signature, and
    classify matches the two recoveries by address before name."""

    def test_xrec_funcs_parses_signature_and_address(self):
        f = xrec.funcs(
            'int CThing::Alpha(int x)\n'
            '{\n'
            '    // TODO: real body\n'
            '    return x + 1;\n'
            '}\n'
            '\n'
            '// 0x401000\n'
            'int CThing::Beta(void)\n'
            '{\n'
            '    return 2;\n'
            '}\n')
        self.assertEqual(sorted(f), ['CThing::Alpha', 'CThing::Beta'])
        self.assertEqual(f['CThing::Alpha'], (None, '{returnx+1;}'))
        self.assertEqual(f['CThing::Beta'], ('0x401000', '{return2;}'))

    def test_xrec_matches_renamed_function_by_address(self):
        real = '{ints=m_x;s+=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        theirs = xrec.funcs('// 0x401000\nint CThing::HisName(int x)\n%s\n' % real)
        ours = xrec.funcs('// 0x401000\nint CThing::OurName(int x)\n%s\n' % real)
        c = xrec.classify(theirs, ours, {})
        self.assertEqual(c['same'], 1)
        self.assertEqual(c['only_theirs'] + c['only_ours'], 0)
        self.assertEqual(sum(c.values()), 1)

    def test_xrec_ambiguous_address_pairs_by_name(self):
        """An address two functions of one file carry is not a key: CScreenMap
        had a stub and a real body sharing 0x643CC0 in their tree only."""
        real = '{ints=m_x;s+=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        stub = '{return 0;}'
        theirs = xrec.funcs('// 0x401000\nint CThing::Real(int x)\n%s\n'
                            '\n'
                            '// 0x401000\nint CThing::Stub(void)\n%s\n' % (real, stub))
        ours = xrec.funcs('// 0x401000\nint CThing::Real(int x)\n%s\n' % real)
        c = xrec.classify(theirs, ours, {})
        self.assertEqual(c['same'], 1)
        self.assertEqual(c['only_theirs'], 1)
        self.assertEqual(c['ours_real_theirs_stub'], 0)
        self.assertEqual(sum(c.values()), 2)

    def test_xrec_name_pair_finds_renamed_base_by_address(self):
        """A pair the name loop holds still finds its base function by
        address: both trees renamed it, theirs carries the stamp twice (the
        duplicate-stamp files), and base keeps it under the old name."""
        real1 = '{ints=m_x;s+=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        real2 = '{ints=m_x;s-=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        base = xrec.funcs('// 0x401000\nint CThing::Old(int x)\n%s\n' % real1)
        theirs = xrec.funcs('// 0x401000\nint CThing::P(int x)\n%s\n'
                            '\n'
                            '// 0x401000\nint CThing::Q(int x)\n%s\n' % (real2, real1))
        ours = xrec.funcs('// 0x401000\nint CThing::P(int x)\n%s\n' % real1)
        c = xrec.classify(theirs, ours, base)
        self.assertEqual(c['theirs_changed_ours_base'], 1)
        self.assertEqual(c['both_changed_differ'], 0)
        self.assertEqual(c['only_theirs'], 1)
        self.assertEqual(sum(c.values()), 2)

    def test_xrec_classify_table(self):
        real1 = '{ints=m_x;s+=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        real2 = '{ints=m_x;s-=x;s+=m_y;s-=x;return s==0&&x!=0;}'
        real3 = '{ints=m_x;s+=x;s-=m_y;s+=x;return s==0&&x!=0;}'
        stub1 = '{return 0;}'
        stub2 = '{return m_x;}'
        theirs = {
            'same': ('0x1000', real1),
            'same_stub': ('0x1001', stub1),
            'only_theirs': ('0x1002', real1),
            'theirs_real_ours_stub': ('0x1003', real1),
            'ours_real_theirs_stub': ('0x1004', stub1),
            'theirs_changed_ours_base': ('0x1005', real2),
            'ours_changed_theirs_base': ('0x1006', real1),
            'both_changed_differ': ('0x1007', real2),
            'both_stub_diff': ('0x1008', stub1),
        }
        ours = {
            'same': ('0x1000', real1),
            'same_stub': ('0x1001', stub1),
            'only_ours': ('0x1009', real1),
            'theirs_real_ours_stub': ('0x1003', stub1),
            'ours_real_theirs_stub': ('0x1004', real1),
            'theirs_changed_ours_base': ('0x1005', real1),
            'ours_changed_theirs_base': ('0x1006', real2),
            'both_changed_differ': ('0x1007', real3),
            'both_stub_diff': ('0x1008', stub2),
        }
        base = {
            'theirs_changed_ours_base': ('0x1005', real1),
            'ours_changed_theirs_base': ('0x1006', real1),
            'both_changed_differ': ('0x1007', real1),
            'both_stub_diff': ('0x1008', stub1),
        }
        c = xrec.classify(theirs, ours, base)
        for k in ['same', 'same_stub', 'only_theirs', 'only_ours',
                  'theirs_real_ours_stub', 'ours_real_theirs_stub',
                  'theirs_changed_ours_base', 'ours_changed_theirs_base',
                  'both_changed_differ', 'both_stub_diff']:
            self.assertEqual(c[k], 1, k)
        self.assertEqual(sum(c.values()), 10)


class WsoFetchTest(unittest.TestCase):
    """wso_fetch: a dest resolves under <IWD2R_HOME>/wso and one containing
    '..' (or absolute) is rejected; --check lists files whose sha256 differs
    from the pin and exits non-zero; fetching creates missing directories and
    writes only after the downloaded bytes match the pin."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.home = tmp.name
        self.root = os.path.join(self.home, 'wso')
        self.addCleanup(os.environ.pop, 'IWD2R_HOME', None)
        os.environ['IWD2R_HOME'] = self.home

    def pins_file(self, pins):
        path = os.path.join(self.home, 'pins.json')
        with open(path, 'w') as f:
            json.dump(pins, f)
        return path

    def run_cli(self, *args):
        return subprocess.run([sys.executable, '-I',
                               os.path.join(ROOT, 'tools', 'wso_fetch.py'), *args],
                              capture_output=True, text=True)

    def test_wso_fetch_dest_under_home(self):
        root = wso_fetch.wso_root()
        self.assertEqual(str(wso_fetch.resolve_dest('lua/IEex_Core_State.lua', root)),
                         os.path.join(self.root, 'lua', 'IEex_Core_State.lua'))
        for dest in ('../escape.dll', 'lua/../../escape.dll', '/abs.dll', '', '.'):
            with self.assertRaises(ValueError):
                wso_fetch.resolve_dest(dest, root)

    def test_wso_fetch_check_reports_mismatch(self):
        os.makedirs(os.path.join(self.root, 'dll'))
        with open(os.path.join(self.root, 'dll', 'IEex.dll'), 'wb') as f:
            f.write(b'tampered bytes')
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'https://example.invalid/IEex.dll',
                           'dest': 'dll/IEex.dll',
                           'sha256': hashlib.sha256(b'pinned bytes').hexdigest()}]}
        problems = wso_fetch.check(pins, wso_fetch.wso_root())
        self.assertEqual([p for p in problems if 'dll/IEex.dll' in p and
                          'sha256' in p], problems, problems)
        run = self.run_cli('--check', '--pins', self.pins_file(pins))
        self.assertEqual(run.returncode, 1)
        self.assertIn('dll/IEex.dll', run.stderr)
        self.assertIn('sha256', run.stderr)

    def test_wso_fetch_creates_dir_when_absent(self):
        payload = b'-- pinned lua\n'
        src = os.path.join(self.home, 'src', 'IEex_X.lua')
        os.makedirs(os.path.dirname(src))
        with open(src, 'wb') as f:
            f.write(payload)
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'file://' + src,
                           'dest': 'lua/sub/IEex_X.lua',
                           'sha256': hashlib.sha256(payload).hexdigest()}]}
        self.assertFalse(os.path.exists(self.root))
        self.assertEqual(wso_fetch.fetch(pins, wso_fetch.wso_root()), [])
        with open(os.path.join(self.root, 'lua', 'sub', 'IEex_X.lua'), 'rb') as f:
            self.assertEqual(f.read(), payload)

    def test_wso_fetch_writes_only_after_hash_matches(self):
        src = os.path.join(self.home, 'src', 'IEex_Y.lua')
        os.makedirs(os.path.dirname(src))
        with open(src, 'wb') as f:
            f.write(b'attacker bytes')
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'file://' + src,
                           'dest': 'lua/IEex_Y.lua',
                           'sha256': hashlib.sha256(b'pinned bytes').hexdigest()}]}
        problems = wso_fetch.fetch(pins, wso_fetch.wso_root())
        self.assertEqual([p for p in problems if 'lua/IEex_Y.lua' in p and
                          'sha256' in p], problems, problems)
        self.assertFalse(os.path.exists(self.root))
        run = self.run_cli('--pins', self.pins_file(pins))
        self.assertEqual(run.returncode, 1)
        self.assertIn('lua/IEex_Y.lua', run.stderr)

    def test_wso_fetch_symlink_escape_is_rejected(self):
        """A symlink under the wso root pointing outside it is not a
        destination: resolve_dest follows the link before the containment
        check, so a matched hash cannot land outside the root either."""
        outside = os.path.join(self.home, 'outside')
        os.makedirs(outside)
        os.makedirs(os.path.join(self.root, 'dll'))
        os.symlink(outside, os.path.join(self.root, 'dll', 'up'))
        root = wso_fetch.wso_root()
        with self.assertRaises(ValueError):
            wso_fetch.resolve_dest('dll/up/IEex.dll', root)
        src = os.path.join(self.home, 'src', 'IEex.dll')
        os.makedirs(os.path.dirname(src))
        with open(src, 'wb') as f:
            f.write(b'pinned bytes')
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'file://' + src,
                           'dest': 'dll/up/IEex.dll',
                           'sha256': hashlib.sha256(b'pinned bytes').hexdigest()}]}
        problems = wso_fetch.fetch(pins, root)
        self.assertEqual([p for p in problems if 'escapes the wso root' in p],
                         problems, problems)
        self.assertFalse(os.path.exists(os.path.join(outside, 'IEex.dll')))

    def test_wso_fetch_bad_pins_file_is_a_clean_error(self):
        """A missing, malformed or wrongly shaped pins file exits 1 with one
        stderr line instead of a traceback."""
        malformed = os.path.join(self.home, 'malformed.json')
        with open(malformed, 'w') as f:
            f.write('{oops')
        shape = os.path.join(self.home, 'shape.json')
        with open(shape, 'w') as f:
            json.dump({'fork_commit': 'f' * 40, 'files': 'not a list'}, f)
        for pins in (os.path.join(self.home, 'absent.json'), malformed, shape):
            run = self.run_cli('--check', '--pins', pins)
            self.assertEqual(run.returncode, 1, (pins, run.stderr))
            lines = run.stderr.splitlines()
            self.assertEqual(len(lines), 1, (pins, run.stderr))
            self.assertIn('wso_fetch:', lines[0])
            self.assertIn(os.path.basename(pins), lines[0])

    @staticmethod
    def make_zip(path, members):
        with zipfile.ZipFile(path, 'w') as z:
            for name, data in members:
                z.writestr(name, data)

    def test_wso_fetch_extracts_after_hash_match(self):
        """A pin with "unzip": true is extracted beside its archive only
        after the archive's bytes match the pin; member names are
        sanitized (no '..', no absolute paths) and --check verifies the
        extracted members are still there."""
        payload = 'pinned dump\n'.encode('utf-16')
        src = os.path.join(self.home, 'src', 'dump.zip')
        os.makedirs(os.path.dirname(src))
        self.make_zip(src, [('bg2_pdb_types.txt', payload),
                            ('sub/nested.txt', b'nested\n')])
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'file://' + src,
                           'dest': 'bg2/dump.zip', 'unzip': True,
                           'sha256': hashlib.sha256(open(src, 'rb').read()).hexdigest()}]}
        self.assertEqual(wso_fetch.fetch(pins, wso_fetch.wso_root()), [])
        self.root = wso_fetch.wso_root()
        with open(os.path.join(self.root, 'bg2', 'bg2_pdb_types.txt'), 'rb') as f:
            self.assertEqual(f.read(), payload)
        self.assertTrue(os.path.isfile(os.path.join(self.root, 'bg2', 'sub',
                                                    'nested.txt')))
        # --check verifies both the archive and its extracted members.
        os.unlink(os.path.join(self.root, 'bg2', 'bg2_pdb_types.txt'))
        problems = wso_fetch.check(pins, self.root)
        self.assertEqual([p for p in problems if 'bg2_pdb_types.txt' in p],
                         problems, problems)
        # A hash mismatch leaves nothing extracted.
        tampered = os.path.join(self.home, 'src', 'tampered.zip')
        self.make_zip(tampered, [('bg2_pdb_types.txt', b'attacker\n')])
        pins['files'][0]['url'] = 'file://' + tampered
        problems = wso_fetch.fetch(pins, self.root)
        self.assertEqual(len(problems), 1, problems)
        self.assertIn('sha256', problems[0])
        self.assertEqual(sorted(os.listdir(os.path.join(self.root, 'bg2'))),
                         ['dump.zip', 'dump.zip.extracted.json', 'sub'])

    def test_wso_fetch_refuses_unsafe_archive_members(self):
        """A member named to escape the extraction directory (`..` or an
        absolute path) fails the whole extraction: the problem is
        reported, nothing is written outside the wso root, and none of
        the archive's members -- the safe ones included -- are
        extracted."""
        src = os.path.join(self.home, 'src', 'evil.zip')
        os.makedirs(os.path.dirname(src))
        self.make_zip(src, [('ok.txt', b'fine\n'), ('../evil.txt', b'escape\n')])
        pins = {'fork_commit': 'f' * 40, 'upstream_commit': 'u' * 40,
                'files': [{'url': 'file://' + src,
                           'dest': 'bg2/evil.zip', 'unzip': True,
                           'sha256': hashlib.sha256(open(src, 'rb').read()).hexdigest()}]}
        problems = wso_fetch.fetch(pins, wso_fetch.wso_root())
        self.assertEqual([p for p in problems if 'evil.txt' in p], problems,
                         problems)
        self.assertFalse(os.path.exists(os.path.join(self.home, 'evil.txt')))
        self.assertEqual(os.listdir(os.path.join(wso_fetch.wso_root(), 'bg2')),
                         ['evil.zip'])


trace_diff = load_tool('frida/trace_diff')


class TraceDiffTest(unittest.TestCase):
    """The in-memory interface the smoke gate uses: normalize drops the
    harness noise, compare turns a trace pair into an exit code."""

    @staticmethod
    def capture():
        return [
            {'tag': 'ready', 'pid': 4242},
            {'tag': 'Move', 'this': '0x1234', 'a0_s32': 7, 'ts': 11},
            {'tag': 'HITS', 'counts': {'Move': 1}},
            {'type': 'send', 'payload': {'tag': 'Move', 'this': '0x9988',
                                         'a0_s32': 9, 'tid': 3}},
            {'tag': 'Attack', 'this': '0x0', 'a0_s32': 5,
             'bt': ['0x401000', '0x401020']},
        ]

    def test_trace_diff_normalize_drops_harness_and_pointers(self):
        events = trace_diff.normalize(self.capture())
        self.assertEqual([e['tag'] for e in events],
                         ['Move', 'Move', 'Attack'])
        self.assertEqual(events[0], {'tag': 'Move', 'this': '<ptr>',
                                     'a0_s32': 7})
        self.assertEqual(events[1], {'tag': 'Move', 'this': '<ptr>',
                                     'a0_s32': 9})
        self.assertEqual(events[2], {'tag': 'Attack', 'this': '<null>',
                                     'a0_s32': 5})
        raw = trace_diff.normalize(self.capture(), raw_pointers=True)
        self.assertEqual(raw[0]['this'], '0x1234')
        self.assertEqual(raw[2]['this'], '0x0')

    def test_trace_diff_compare_match_order_args(self):
        events = [e for e in self.capture() if e.get('tag') in ('Move',
                                                                'Attack')]
        self.assertEqual(trace_diff.compare(events, events), 0)
        self.assertEqual(trace_diff.compare(events, list(reversed(events))),
                         1)
        shifted = [dict(e, a0_s32=e['a0_s32'] + 1) for e in events]
        self.assertEqual(trace_diff.compare(events, shifted), 1)

    def test_trace_diff_crash_never_passes(self):
        events = self.capture() + [{'tag': 'assert', 'where': 'Move'}]
        self.assertNotEqual(trace_diff.compare(events, events), 0)

    def test_trace_diff_empty_is_2(self):
        self.assertEqual(trace_diff.compare([], []), 2)


probe = load_tool('frida/probe')


class ProbePlanTest(unittest.TestCase):
    """probe.plan_folders' staging invariants: every mapped-folder HOST path
    must be a Windows path (the box cannot see /mnt/... and refuses the
    whole configuration over one bad folder), and both logon commands must
    be wrapped for the shell-less LogonCommand."""

    def plan(self, target):
        return probe.plan_folders(
            target,
            'C:\\Users\\thier\\iwd2r-frida\\venv',
            'C:\\Users\\thier\\iwd2r-build-p3-wso-tools\\replay',
            'C:\\Users\\thier\\iwd2r-oracle\\game',
            'C:\\Users\\thier\\iwd2r-vm\\replays\\s\\results',
            'C:\\Users\\thier\\iwd2r-vm\\replays\\s\\tools')

    def test_mapped_host_paths_are_windows_paths(self):
        for target in ('original', 'ours'):
            for host, box, _ro in self.plan(target):
                self.assertFalse(host.startswith('/'), (target, host, box))
                self.assertTrue(host[1:3] == ':\\', (target, host, box))
                self.assertTrue(box.startswith('C:\\'), (target, box))

    def test_logon_commands_survive_shell_less_logon(self):
        ns = types.SimpleNamespace(hit='MTP', hit_min=1, attach_timeout=None,
                                   loaded_addr=None, loaded_rva='0x330000',
                                   module=None, no_loaded=False)
        args = probe.sidecar_args(ns, 't.jsonl', 't.jsonl.status')
        orig = probe.original_logon(
            'C:\\Users\\thier\\iwd2r-frida\\venv\\Scripts\\python.exe',
            120, args)
        ours = probe.ours_logon(
            'C:\\Users\\thier\\iwd2r-frida\\venv\\Scripts\\python.exe',
            'C:\\game', probe.ours_env('C:\\game', 1), 120, args)
        self.assertTrue(orig.startswith('cmd.exe /c '), orig)
        self.assertIn('replay-driver.ps1 -Config', orig)
        self.assertTrue(ours.startswith('cmd.exe /c '), ours)
        self.assertIn('& echo done > ', ours)
        self.assertTrue(ours.endswith('shutdown /s /t 0'), ours)


class OrigAttachTest(unittest.TestCase):
    """attach_when_there must ride out every flavour of "process not found"
    frida raises while polling for the game -- frida 17 raises
    InvalidArgumentError there, not ProcessNotFoundError -- and only give up
    at the deadline (the replay driver boots IWD2.exe seconds after the
    sidecar starts)."""

    @classmethod
    def setUpClass(cls):
        stub = types.ModuleType('frida')
        stub.ProcessNotFoundError = type('ProcessNotFoundError', (Exception,),
                                         {})
        stub.InvalidArgumentError = type('InvalidArgumentError', (Exception,),
                                         {})
        cls._saved_frida = sys.modules.get('frida')
        sys.modules['frida'] = stub
        cls.frida_stub = stub
        cls.orig = load_tool('frida/orig')

    @classmethod
    def tearDownClass(cls):
        if cls._saved_frida is not None:
            sys.modules['frida'] = cls._saved_frida
        else:
            del sys.modules['frida']

    def setUp(self):
        now = [0.0]
        self.clock = now
        self.fake_time = types.SimpleNamespace(
            monotonic=lambda: now[0],
            sleep=lambda s: now.__setitem__(0, now[0] + s))
        saved = self.orig.time
        self.orig.time = self.fake_time
        self.addCleanup(setattr, self.orig, 'time', saved)

    def install_device(self, device):
        self.frida_stub.get_local_device = lambda: device
        self.addCleanup(delattr, self.frida_stub, 'get_local_device')

    def test_polls_through_invalid_argument_until_found(self):
        calls = []
        frida_stub = self.frida_stub

        class Device:
            def get_process_by_name(self, name):
                calls.append(name)
                if len(calls) < 3:
                    raise frida_stub.InvalidArgumentError(
                        'process not found')
                return types.SimpleNamespace(pid=7932)

            def attach(self, pid):
                return 'session-%d' % pid

        self.install_device(Device())
        session, pid = self.orig.attach_when_there('IWD2.exe',
                                                   self.clock[0] + 90.0)
        self.assertEqual((session, pid), ('session-7932', 7932))
        self.assertEqual(calls, ['IWD2.exe'] * 3)

    def test_gives_up_at_the_deadline(self):
        calls = []
        frida_stub = self.frida_stub

        class Device:
            def get_process_by_name(self, name):
                calls.append(name)
                raise frida_stub.InvalidArgumentError('process not found')

            def attach(self, pid):  # pragma: no cover - never reached
                return None

        self.install_device(Device())
        with self.assertRaises(frida_stub.InvalidArgumentError):
            self.orig.attach_when_there('IWD2.exe', self.clock[0] + 1.0)
        self.assertGreaterEqual(len(calls), 2)


def load_parity_tool(name):
    spec = importlib.util.spec_from_file_location(
        'parity_' + name, os.path.join(ROOT, 'tools', 'parity', name + '.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


THREE_FUNCTIONS = ('// test fixture\n'
                   '#include "a.h"\n'
                   '\n'
                   '// 0x401000\n'
                   'int A::f(int x)\n'
                   '{\n'
                   '    return x;\n'
                   '}\n'
                   '\n'
                   '// 0x401020\n'
                   'int A::g(int y)\n'
                   '{\n'
                   '    return y;\n'
                   '}\n'
                   '\n'
                   '// 0x401040\n'
                   'int A::h(int z)\n'
                   '{\n'
                   '    return z;\n'
                   '}\n')


class ParityTest(unittest.TestCase):
    """tools/parity: the fork's structural parity gates, on our exports."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = os.path.join(tmp.name, 'repo')
        os.makedirs(self.repo)
        home = tempfile.TemporaryDirectory()
        self.addCleanup(home.cleanup)
        self.home = home.name
        # run.py resolves the exports (functions.tsv, callgraph.tsv) under
        # IWD2R_HOME at call time, the way make_project.py does.
        self.addCleanup(os.environ.pop, 'IWD2R_HOME', None)
        os.environ['IWD2R_HOME'] = self.home
        self.git('init', '-q')
        self.git('config', 'user.name', 'Test')
        self.git('config', 'user.email', 'test@example.com')
        self.git('config', 'commit.gpgsign', 'false')

    def git(self, *args):
        return subprocess.run(['git', *args], cwd=self.repo, check=True,
                              capture_output=True, text=True).stdout

    def write(self, rel, text):
        path = os.path.join(self.repo, rel)
        os.makedirs(os.path.dirname(path) or self.repo, exist_ok=True)
        with open(path, 'w') as f:
            f.write(text)

    def commit(self, message='change'):
        self.git('add', '-A')
        self.git('commit', '-q', '--allow-empty', '-m', message)

    def exports(self, functions, callgraph):
        """functions.tsv / callgraph.tsv under the fixture IWD2R_HOME."""
        out = os.path.join(self.home, 'ghidra-exports')
        os.makedirs(out, exist_ok=True)
        for name, text in (('functions.tsv', functions),
                           ('callgraph.tsv', callgraph)):
            with open(os.path.join(out, name), 'w') as f:
                f.write(text)
        return out

    def test_parity_callee_diff(self):
        asm = load_parity_tool('asm_verify')
        missing, extra = asm.callee_diff({1, 2, 3}, {2, 3, 4})
        self.assertEqual(missing, {4})
        self.assertEqual(extra, {1})

    def test_parity_address_markers_flags_non_start(self):
        markers = load_parity_tool('address_markers')
        self.write('src/a.cpp',
                   '// 0x401002\n'
                   'int A::g(int x)\n'
                   '{\n'
                   '    return x;\n'
                   '}\n')
        exports = self.exports('401000\tA::f\t100\n', '401000\n')
        code, lines = markers.check(
            os.path.join(self.repo, 'src'),
            os.path.join(exports, 'functions.tsv'),
            os.path.join(exports, 'callgraph.tsv'), repo=self.repo)
        self.assertEqual(code, 1, lines)
        self.assertTrue(any('401002' in line for line in lines), lines)

    def test_parity_run_skips_without_exe(self):
        parity_run = load_parity_tool('run')
        old = parity_run.oracle.VANILLA_EXE
        setattr(parity_run.oracle, 'VANILLA_EXE',
                os.path.join(self.home, 'no-such.exe'))
        self.addCleanup(setattr, parity_run.oracle, 'VANILLA_EXE', old)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = parity_run.main('HEAD')
        self.assertEqual(code, 0)
        self.assertIn('skipped', out.getvalue())

    def test_parity_waiver_downgrades_asm(self):
        self.write('src/a.cpp',
                   '// 0x401000\n'
                   'int A::f(int x)\n'
                   '{\n'
                   '    return x;\n'
                   '}\n')
        self.commit('base')
        base = self.git('rev-parse', 'HEAD').strip()
        self.write('src/a.cpp',
                   '// 0x401000\n'
                   'int A::f(int x)\n'
                   '{\n'
                   '    return x * 2;\n'
                   '}\n')
        self.commit('body change')
        # The binary calls Helper::Do (0x405000); the source calls nothing.
        self.exports('401000\tA::f\t100\n405000\tHelper::Do\t50\n',
                     '401000\t405000\n')

        def evidence(waiver):
            self.write('docs/evidence/401000.md',
                       '---\n'
                       'address: 0x401000\n'
                       'name: A::f\n'
                       'exemption: "fixture"\n'
                       + (f'parity_waiver: "{waiver}"\n' if waiver else '')
                       + '---\n\n## Notes\n')

        parity_run = load_parity_tool('run')
        evidence(None)
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = parity_run.main(base, repo=self.repo)
        self.assertEqual(code, 1, out.getvalue())
        self.assertIn('405000', out.getvalue())

        evidence('known thin wrapper')
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = parity_run.main(base, repo=self.repo)
        self.assertEqual(code, 0, out.getvalue())
        self.assertIn('warning', out.getvalue())

    def test_parity_changed_only_uses_markers(self):
        self.write('src/a.cpp', THREE_FUNCTIONS)
        self.commit('base')
        base = self.git('rev-parse', 'HEAD').strip()
        changed = THREE_FUNCTIONS.replace('return x;', 'return x * 2;') \
                                 .replace('return z;', 'return z * 2;')
        self.write('src/a.cpp', changed)
        self.commit('two bodies')
        parity_run = load_parity_tool('run')
        found = parity_run.touched(self.repo, base)
        self.assertEqual({address for _, address, _ in found},
                         {0x401000, 0x401040})


class FnDigestTest(unittest.TestCase):
    """tools/fn_digest.digest -- a <=30-line brief of a decompile."""

    def tiny_case(self):
        return ('int f()\n'
                '{\n'
                '    for (;;) { }\n'
                '    while (x) { }\n'
                '    do { } while (y);\n'
                '    switch (z) { }\n'
                '}\n')

    def test_fn_digest_under_30_lines(self):
        # A 400-line decompile with strings, globals, calls, loops, a switch.
        body = []
        i = 0
        while len(body) < 396:
            body.append(f'    g_arr[{i % 8}] = DAT_009{i % 10:05d}'
                        f' + FUN_0041{i % 100:04d}();')
            if i % 40 == 0:
                body.append('    for (int j = 0; j < 3; j++) { }')
            if i % 80 == 0:
                body.append('    switch (g_mode) { case 1: break; }')
            if i % 60 == 0:
                body.append('    printf("sprite %d", i);')
            i += 1
        del body[396:]  # exactly 396 body lines
        lines = (['int CGameSprite::Render(int nX, int nY)', '{'] + body
                 + ['    return g_arr[0];', '}'])
        self.assertEqual(len(lines), 400)
        decompile = '\n'.join(lines)

        d = fn_digest.digest(decompile, {
            'name': 'CGameSprite::Render',
            'address': 0x5845D0,
            'signature': 'int CGameSprite::Render(int, int)',
            'callees': ['FUN_00410000', 'CInfGame::GetDice',
                        'CInfGame::GetDice', 'memcpy'],
            'callers': ['CGameSprite::Draw'],
        })
        out = d.splitlines()
        self.assertLessEqual(len(out), 30)
        self.assertIn('CGameSprite::Render', out[0])
        self.assertIn('0x5845d0', out[0])
        self.assertIn('400', out[0])
        self.assertTrue(any('signature: int CGameSprite::Render(int, int)' == l
                            for l in out))
        # callees are deduped, kept in order
        self.assertTrue(any(l == 'callees (3): FUN_00410000, CInfGame::GetDice, '
                              'memcpy' for l in out))
        self.assertTrue(any(l == 'callers (1): CGameSprite::Draw' for l in out))
        self.assertTrue(any(l == 'strings: "sprite %d"' for l in out))
        gline = next(l for l in out if l.startswith('globals: '))
        self.assertIn('DAT_00900000', gline)
        self.assertIn('g_mode', gline)
        self.assertIn('g_arr', gline)
        lline = next(l for l in out if l.startswith('loops: '))
        self.assertRegex(lline, r'^loops: \d+  switches: \d+$')

        # loops counts `for`/`while` keywords (a do-while's while included).
        tiny = fn_digest.digest(self.tiny_case(), None).splitlines()
        self.assertIn('loops: 3  switches: 1', tiny)


_SRC_CPP = '''// CGameSprite::Render draws one frame.
// 0x5845D0
void CGameSprite::Render(int nFrame)
{
    if (nFrame < 0)
        return;
    SetFrame(nFrame);
}

// 0x584600
int CGameSprite::GetFrame() const
{
    return m_nFrame;
}
'''

_SRC_CPP2 = '''// 0x401000
int CInfGame::GetDice(int nSides)
{
    return nSides;
}
'''

_DECOMP = '''int CGameSprite::Render(int nFrame)
{
    if (nFrame < 0)
        return 0;
    SetFrame(nFrame);
    ApplyEffect(FOO_BAR, "SW1H01");
    SetGlobal(ODD_CASE, "AR1000");
    return 0;
}
'''


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        f.write(text)


class SrcFindTest(unittest.TestCase):
    """tools/src_find.find -- markers and signatures, no grep chains."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = tmp.name
        _write(os.path.join(self.root, 'CGameSprite.cpp'), _SRC_CPP)
        _write(os.path.join(self.root, 'CInfGame.cpp'), _SRC_CPP2)

    def test_src_find_by_address_and_name(self):
        by_addr = src_find.find('0x5845D0', root=self.root)
        self.assertEqual(by_addr,
                         [('CGameSprite.cpp', 2, 'void CGameSprite::Render(int nFrame)')])
        by_name = src_find.find('CGameSprite::Render', root=self.root)
        self.assertEqual(by_name, by_addr)

        whole_class = src_find.find('CGameSprite::', root=self.root)
        self.assertEqual(whole_class,
                         [('CGameSprite.cpp', 2, 'void CGameSprite::Render(int nFrame)'),
                          ('CGameSprite.cpp', 10, 'int CGameSprite::GetFrame() const')])

        self.assertEqual(src_find.find('0x401000', root=self.root),
                         [('CInfGame.cpp', 2, 'int CInfGame::GetDice(int nSides)')])
        self.assertEqual(src_find.find('NoSuch::Thing', root=self.root), [])


class AssembleContextTest(unittest.TestCase):
    """tools/assemble_context.bundle -- one brief, in the brief's order."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = tmp.name
        self.exports = os.path.join(self.tmp, 'exports')
        self.src = os.path.join(self.tmp, 'src')
        self.ids = os.path.join(self.tmp, 'ids')
        _write(os.path.join(self.exports, 'functions.tsv'),
               '5845D0\tCGameSprite::Render\t120\n'
               '584600\tCGameSprite::GetFrame\t24\n')
        _write(os.path.join(self.exports, 'callgraph.tsv'),
               '5845D0\t584600\n')
        _write(os.path.join(self.exports, 'CGameSprite::Render.c'), _DECOMP)
        _write(os.path.join(self.src, 'CGameSprite.cpp'), _SRC_CPP)
        _write(os.path.join(self.ids, 'DEMO.IDS'), 'FOO_BAR 0\n8 ODD_CASE\n')

    def test_assemble_context_order_and_estimate(self):
        b = assemble_context.bundle(0x5845D0, exports=self.exports,
                                    src_root=self.src, ids_dir=self.ids)
        first = b.splitlines()[0]
        m = re.match(r'^context bundle: .*0x5845d0.*CGameSprite::Render'
                     r' ~(\d+) tokens$', first)
        self.assertIsNotNone(m, first)
        self.assertEqual(int(m.group(1)), len(b.split('\n', 1)[1]) // 4)

        i_digest = b.index('## digest')
        i_decomp = b.index('## decompile')
        i_callees = b.index('## callees')
        i_ids = b.index('## ids')
        self.assertLess(i_digest, i_decomp)
        self.assertLess(i_decomp, i_callees)
        self.assertLess(i_callees, i_ids)

        self.assertIn('ApplyEffect(FOO_BAR', b[i_decomp:i_callees])
        self.assertIn('CGameSprite::GetFrame', b[i_callees:i_ids])
        self.assertIn('int CGameSprite::GetFrame() const', b[i_callees:i_ids])
        self.assertIn('FOO_BAR = 0', b[i_ids:])
        self.assertIn('ODD_CASE = 8', b[i_ids:])
        resref_line = next(l for l in b[i_ids:].splitlines()
                           if l.startswith('resrefs:'))
        self.assertIn('SW1H01', resref_line)
        self.assertIn('AR1000', resref_line)
        self.assertIn('loops:', b[i_digest:i_decomp])

        # No exports, no ids dir: honest degradation, still the same order.
        bare = assemble_context.bundle(
            0x5845D0, exports=os.path.join(self.tmp, 'no-such-exports'),
            bg2_db=False)
        self.assertIn('~', bare.splitlines()[0])
        self.assertLess(bare.index('## digest'), bare.index('## decompile'))
        self.assertLess(bare.index('## decompile'), bare.index('## callees'))
        self.assertIn('no functions.tsv', bare)


_BG2_TYPES = '''\
    0x1000 | LF_FIELDLIST [size = 92]
\t- LF_MEMBER [name = `m_flags`, Type = 0x0074 (int), offset = 4, attrs = public]
\t- LF_ONEMETHOD [name = `Render`]
\t\ttype = 0x1018, vftable offset = 0, attrs = public virtual intro
\t- LF_MEMBER [name = `m_pThing`, Type = 0x1008, offset = 8, attrs = public]
    0x1008 | LF_POINTER [size = 12]
\t\treferent = 0x103C, mode = ptr
    0x100C | LF_ARGLIST [size = 8]
    0x1010 | LF_FIELDLIST [size = 72]
\t- LF_METHOD [name = `Draw`, # overloads = 2, overload list = 0x1014]
\t- LF_MEMBER [name = `m_count`, Type = 0x0074 (int), offset = 0, attrs = public]
\t- LF_MEMBER [name = `m_name`, Type = 0x0470 (char*), offset = 8, attrs = public]
\t- LF_MEMBER [name = `m_next`, Type = 0x1008, offset = 16, attrs = public]
    0x1014 | LF_METHODLIST [size = 20]
\t- Method [type = 0x1030, vftable offset = -1, attrs = private static]
\t- Method [type = 0x1034, vftable offset = 4, attrs = public virtual]
    0x1018 | LF_MFUNCTION [size = 28]
\t\treturn type = 0x0003 (void), # args = 0, param list = 0x100C
\t\tcalling conv = thiscall, options = none
    0x1030 | LF_MFUNCTION [size = 28]
\t\treturn type = 0x0003 (void), # args = 0, param list = 0x100C
\t\tcalling conv = thiscall, options = none
    0x1034 | LF_MFUNCTION [size = 28]
\t\treturn type = 0x0074 (int), # args = 0, param list = 0x100C
\t\tcalling conv = thiscall, options = none
    0x1038 | LF_FIELDLIST [size = 32]
\t- LF_ENUMERATE [THING_ON = 0]
\t- LF_ENUMERATE [THING_OFF = 1]
    0x103C | LF_CLASS [size = 48] `CThing`
\t\tfield list: 0x1000
\t\toptions: contains intro virtual, sizeof 48
    0x1040 | LF_CLASS [size = 44] `CWidget`
\t\tfield list: 0x1010
\t\toptions: contains intro virtual, sizeof 44
    0x1044 | LF_CLASS [size = 0] `CThing`
\t\tfield list: <no type>
\t\toptions: forward ref, sizeof 0
    0x1048 | LF_ENUM [size = 40] `THING_STATE`
\t\tfield list: 0x1038
\t\toptions: sizeof 4
'''


class Bg2FindTest(unittest.TestCase):
    """tools/bg2_find -- a SQLite index over the extracted PDB dumps."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dumps = os.path.join(tmp.name, 'bg2')
        os.makedirs(self.dumps)
        # The real dumps are UTF-16; exercise that path.
        with open(os.path.join(self.dumps, 'bg2_pdb_types.txt'), 'w',
                  encoding='utf-16') as f:
            f.write(_BG2_TYPES)
        self.db = os.path.join(self.dumps, 'bg2_pdb.sqlite')

    def test_bg2_find_builds_index_and_answers(self):
        self.assertEqual(bg2_find.build_db(dumps=self.dumps, db=self.db),
                         self.db)
        self.assertTrue(os.path.isfile(self.db))

        rows = bg2_find.lookup('CThing::', db=self.db)
        flags = next(r for r in rows
                     if r['kind'] == 'member' and r['name'] == 'm_flags')
        self.assertEqual(flags['class'], 'CThing')
        self.assertEqual(flags['offset'], 4)
        self.assertEqual(flags['type'], 'int')
        ptr = next(r for r in rows
                   if r['kind'] == 'member' and r['name'] == 'm_pThing')
        self.assertEqual(ptr['offset'], 8)
        self.assertEqual(ptr['type'], 'CThing*')
        render = next(r for r in rows
                      if r['kind'] == 'method' and r['name'] == 'Render')
        self.assertEqual(render['vtable'], 0)
        self.assertEqual(render['type'], '()')
        self.assertTrue(any(r['kind'] == 'class' and r['name'] == 'CThing'
                            and r['size'] == 48 for r in rows))
        self.assertFalse(any(r.get('name') == 'm_count' for r in rows))

        # Bare member name, and a scoped Class::Method query.
        bare = bg2_find.lookup('m_flags', db=self.db)
        self.assertEqual([(r['class'], r['offset'], r['type']) for r in bare
                          if r['kind'] == 'member'],
                         [('CThing', 4, 'int')])
        draw = bg2_find.lookup('CWidget::Draw', db=self.db)
        self.assertEqual(sorted((r['vtable'], r['type']) for r in draw),
                         [(-1, '()'), (4, '() -> int')])

        # The enum values are indexed; the forward ref never became a row.
        states = bg2_find.lookup('THING_STATE::', db=self.db)
        self.assertEqual([(r['name'], r['value']) for r in states],
                         [('THING_ON', 0), ('THING_OFF', 1)])
        self.assertEqual([r['name'] for r in
                          bg2_find.lookup('CThing', db=self.db)
                          if r['kind'] == 'class'], ['CThing'])


class TokenAuditTest(unittest.TestCase):
    """tools/token_audit.summarize -- where a session's tokens went."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.path = os.path.join(tmp.name, 'session.jsonl')

    def test_token_audit_totals(self):
        usage = {'input_tokens': 100, 'output_tokens': 50,
                 'cache_creation_input_tokens': 10,
                 'cache_read_input_tokens': 5}

        def assistant(tools=()):
            content = [{'type': 'text', 'text': 'thinking'}]
            content += [{'type': 'tool_use', 'id': tid, 'name': name,
                         'input': {}} for tid, name in tools]
            return json.dumps({'type': 'assistant', 'message': {
                'role': 'assistant', 'usage': dict(usage),
                'content': content}})

        def result(tid, text):
            return json.dumps({'type': 'user', 'message': {'role': 'user',
                'content': [{'type': 'tool_result', 'tool_use_id': tid,
                             'content': text}]}})

        lines = [
            assistant([('t1', 'Bash')]),
            result('t1', 'x' * 4000),
            assistant([('t2', 'Read')]),
            result('t2', 'y' * 1200),
            assistant([('t3', 'Grep')]),
            result('t3', 'z' * 800),
            assistant([('t4', 'Bash')]),
            result('t4', 'w' * 50),
            assistant(),
            json.dumps({'type': 'user', 'message': {'role': 'user',
                                                    'content': 'continue'}}),
            assistant([('t6', 'Read')]),
            json.dumps({'type': 'user', 'message': {'role': 'user',
                'content': [{'type': 'tool_result', 'tool_use_id': 't6',
                             'content': [{'type': 'text', 'text': 'q' * 999}]}]}}),
            assistant([('t7', 'Agent')]),
            result('t7', 'r' * 3000),
        ]
        lines += [assistant() for _ in range(5)]
        lines.append('{"type":"assistant","message":{"usage":{"input_tok')
        self.assertEqual(len(lines), 20)
        with open(self.path, 'w') as f:
            f.write('\n'.join(lines) + '\n')

        s = token_audit.summarize(self.path)
        self.assertEqual(s['usage'], {'input_tokens': 1200, 'output_tokens': 600,
                                      'cache_creation_input_tokens': 120,
                                      'cache_read_input_tokens': 60,
                                      'messages': 12})
        self.assertEqual(s['tools']['Bash'],
                         {'calls': 2, 'result_chars': 4050, 'result_tokens': 1012})
        self.assertEqual(s['tools']['Read'],
                         {'calls': 2, 'result_chars': 2199, 'result_tokens': 549})
        self.assertEqual(s['tools']['Grep']['result_tokens'], 200)
        self.assertEqual(s['tools']['Agent']['result_tokens'], 750)
        self.assertEqual(s['largest'][0]['tool'], 'Bash')
        self.assertEqual(s['largest'][0]['chars'], 4000)
        self.assertEqual(len(s['largest']), 6)
        self.assertEqual([r['chars'] for r in s['largest']],
                         sorted((r['chars'] for r in s['largest']),
                                reverse=True))


# tools/wave.py: the coordinator's batch loop.  The fixtures mirror
# docs/wso/disagreements.json's row shape; the close tests build a real
# temp repo (first + a batch branch + a worktree) like WorktreeScriptTest,
# with gates.sh's subprocess patched out -- the fake answers for any
# command that names gates.sh and passes everything else through.

_WAVE_ROWS = [
    dict(file='src/A.cpp', name='Render', address='0x643CC0',
         category='both_changed_differ', their_len=500, our_len=400),
    dict(file='src/A.cpp', name='Draw', address='0x643CD0',
         category='both_changed_differ', their_len=100, our_len=100),
    dict(file='src/B.cpp', name='Tick', address='0x20010',
         category='both_changed_differ', their_len=300, our_len=300),
    # Big, but the wrong category: `next` must never pick it.
    dict(file='src/B.cpp', name='TooBigButSame', address='0x20000',
         category='same', their_len=9000, our_len=999),
    dict(file='src/C.cpp', name='Init', address='0x30010',
         category='both_changed_differ', their_len=150, our_len=50),
    dict(file='src/D.cpp', name='NoAddr', category='both_changed_differ',
         their_len=8000, our_len=800),
]


class WaveTest(unittest.TestCase):
    """tools/wave.py -- next/brief/verdict/close/scorecard from state."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.wave = load_tool('wave')
        self.state = os.path.join(self.tmp, 'wave-state.json')
        self.disagreements = os.path.join(self.tmp, 'disagreements.json')
        with open(self.disagreements, 'w') as f:
            json.dump(_WAVE_ROWS, f)

    def write_disagreements(self, rows):
        with open(self.disagreements, 'w') as f:
            json.dump(rows, f)

    def next(self, size=20):
        return self.wave.next_batch(self.disagreements, self.state, size=size)

    def batches(self):
        with open(self.state) as f:
            return json.load(f)['batches']

    # -- a real repo for the close tests ---------------------------------

    def git(self, *args, cwd=None):
        env = dict(os.environ, GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                   GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t',
                   GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')
        return subprocess.run(['git', *args], cwd=cwd or self.root, env=env,
                              check=True, capture_output=True, text=True)

    def init_repo(self):
        """A repo whose `first` carries the fixture disagreements, with the
        branch and worktree `next` will name already checked out."""
        self.root = os.path.join(self.tmp, 'repo')
        os.makedirs(self.root)
        self.git('init', '-q', '-b', 'first', cwd=self.root)
        docs = os.path.join(self.root, 'docs', 'wso')
        os.makedirs(docs)
        # Draw outranks Tick here, so the batch is exactly Render + Draw
        # (both in src/A.cpp) -- the rows the evidence and verdict helpers
        # below cover.
        rows = [dict(r) for r in _WAVE_ROWS]
        rows[1]['their_len'] = 700
        with open(self.disagreements, 'w') as f:
            json.dump(rows, f)
        path = os.path.join(docs, 'disagreements.json')
        shutil.copy(self.disagreements, path)
        self.disagreements = path
        self.git('add', '-A', cwd=self.root)
        self.git('commit', '-qm', 'base', cwd=self.root)
        rc, out = self.next(size=2)
        self.assertEqual(rc, 0, out)
        self.wt = os.path.join(self.tmp, 'wt')
        self.git('worktree', 'add', '-q', '-b', 'p3/wso-b001', self.wt, 'first')

    EVIDENCE = {  # row address -> (evidence stem, kind)
        '0x643CC0': ('643CC0', 'oracle'),
        '0x643CD0': ('643CD0', 'replay'),
    }

    def write_evidence(self):
        for addr, (stem, kind) in self.EVIDENCE.items():
            path = os.path.join(self.wt, 'docs', 'evidence', stem + '.md')
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w') as f:
                f.write(f'---\naddress: 0x{stem}\nname: fn-{stem}\n'
                        f'kind: {kind}\n---\n\n'
                        f'disagreed with WSO at e8dd16a: the guard differs; '
                        f'case probe-9 decides.\n')

    def commit_worktree(self, msg='batch work'):
        self.git('add', '-A', cwd=self.wt)
        self.git('commit', '-qm', msg, cwd=self.wt)

    def verdicts(self):
        # Draw decided for the fork: ours' evidence kind is read from
        # 643CD0.md (replay).  Render decided for ours: a trace.
        self.assertEqual(self.wave.record_verdict(
            self.state, self.root, 'b001', '0x643CD0', 'fork', 'probe-9')[0], 0)
        self.assertEqual(self.wave.record_verdict(
            self.state, self.root, 'b001', '0x643CC0', 'ours', 'probe-11')[0], 0)

    def gates(self, rc):
        """Patch subprocess.run so the `bash tools/gates.sh` run exits rc;
        record its calls.  Git calls pass through -- including close's
        diff, whose pathspec names tools/gates.sh without invoking it."""
        from unittest import mock
        real = subprocess.run
        calls = []

        def run(cmd, **kw):
            if cmd[0] == 'bash':
                calls.append(cmd)
                stdout = 'gates: all passed\n' if rc == 0 else \
                    'gates: FAILED: linux-gcc build and tests\n'
                return subprocess.CompletedProcess(cmd, rc, stdout=stdout,
                                                   stderr='')
            return real(cmd, **kw)

        return mock.patch.object(self.wave.subprocess, 'run', run), calls

    def first_sha(self):
        return self.git('rev-parse', 'refs/heads/first').stdout.strip()

    # -- the eight --------------------------------------------------------

    def test_wave_next_ranks_and_keeps_files_disjoint(self):
        rc, out = self.next(size=2)
        self.assertEqual(rc, 0, out)
        self.assertIn('b001', out)
        self.assertIn('p3/wso-b001', out)
        # The category filter held: the 9,999-char "same" row is never picked.
        self.assertNotIn('TooBigButSame', out)
        # Address-less rows are counted, not silently dropped.
        self.assertIn('(skipped 1 row without an address)', out)
        b1 = self.batches()['b001']
        self.assertEqual(b1['branch'], 'p3/wso-b001')
        self.assertEqual(b1['status'], 'assigned')
        self.assertTrue(b1['assigned_at'])
        self.assertEqual(b1['verdicts'], {})
        # Ranked by their_len + our_len descending: 900 before 600.
        self.assertEqual([r['address'] for r in b1['rows']],
                         ['0x643CC0', '0x20010'])
        self.assertEqual(b1['files'], ['src/A.cpp', 'src/B.cpp'])

        rc, out = self.next(size=2)
        self.assertEqual(rc, 0, out)
        b2 = self.batches()['b002']
        # Init (C.cpp) is next; Draw waits because A.cpp is owned by b001.
        self.assertEqual([r['address'] for r in b2['rows']], ['0x30010'])
        self.assertEqual(b2['files'], ['src/C.cpp'])
        self.assertFalse(set(b1['files']) & set(b2['files']))

    def test_wave_next_exhausted_exit_2(self):
        self.write_disagreements(_WAVE_ROWS[:1])
        self.assertEqual(self.next(size=5)[0], 0)
        rc, out = self.next(size=5)
        self.assertEqual(rc, 2, out)
        self.assertEqual(list(self.batches()), ['b001'])

    def test_wave_brief_contains_rows_and_rules(self):
        self.write_disagreements(_WAVE_ROWS[:2])
        self.next(size=2)
        template = os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')
        rc, brief = self.wave.render_brief(template, self.state, 'b001')
        self.assertEqual(rc, 0, brief)
        self.assertIn('p3/wso-b001', brief)
        self.assertIn('0x643CC0', brief)
        self.assertIn('Render', brief)
        self.assertIn('src/A.cpp', brief)
        self.assertIn('theirs 500', brief)
        # The fixed rules, as the template states them:
        self.assertIn('case ', brief)
        self.assertIn('disagreed with WSO', brief)
        self.assertIn('corroborated by WSO', brief)
        self.assertIn('Imported-From', brief)
        self.assertIn('WIP', brief)
        self.assertIn('parity_waiver', brief)
        self.assertIn('git push', brief)
        # The fork is read at the pinned sha, never by branch name.
        self.assertIn(
            'git show e8dd16a750966af53f24daaf49dcb4fb13594c13:', brief)
        # A fork_base recorded in the state overrides the default pin.
        with open(self.state) as f:
            state = json.load(f)
        state['fork_base'] = 'a' * 40
        with open(self.state, 'w') as f:
            json.dump(state, f)
        rc, brief = self.wave.render_brief(template, self.state, 'b001')
        self.assertEqual(rc, 0, brief)
        self.assertIn('git show ' + 'a' * 40 + ':', brief)
        # Unknown batch id.
        self.assertEqual(
            self.wave.render_brief(template, self.state, 'b999')[0], 2)

    def test_wave_close_refuses_missing_verdict(self):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        # Render decided; only Draw's verdict is missing.
        self.assertEqual(self.wave.record_verdict(
            self.state, self.root, 'b001', '0x643CC0', 'ours', 'probe-11')[0], 0)
        first = self.first_sha()
        with self.gates(0)[0]:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 1, out)
        self.assertIn('b001 not closed', out)
        self.assertIn('no verdict for 0x643CD0', out)
        self.assertNotIn('no verdict for 0x643CC0', out)
        self.assertEqual(self.batches()['b001']['status'], 'assigned')
        self.assertEqual(self.first_sha(), first)

    def test_wave_close_refuses_uncommitted(self):
        self.init_repo()
        self.write_evidence()
        self.verdicts()
        first = self.first_sha()
        with self.gates(0)[0] as _:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 1, out)
        self.assertIn('b001 not closed', out)
        self.assertIn('uncommitted paths in', out)
        self.assertIn('docs/evidence/643CC0.md', out)
        self.assertEqual(self.first_sha(), first)

    def test_wave_close_refuses_failed_gate(self):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        self.verdicts()
        first = self.first_sha()
        with self.gates(1)[0]:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 1, out)
        self.assertIn('b001 not closed', out)
        self.assertIn('tools/gates.sh --base first failed', out)
        self.assertIn('gates: FAILED', out)
        self.assertEqual(self.batches()['b001']['status'], 'assigned')
        self.assertEqual(self.first_sha(), first)

    def test_wave_close_refuses_modified_gate(self):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        self.verdicts()
        # The branch rewrites the very gate that would certify it.
        gate = os.path.join(self.wt, 'tools', 'gates.sh')
        os.makedirs(os.path.dirname(gate), exist_ok=True)
        with open(gate, 'w') as f:
            f.write('#!/bin/sh\nexit 0\n')
        self.commit_worktree('weaken the gate')
        first = self.first_sha()
        ctx, calls = self.gates(0)
        with ctx:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 1, out)
        self.assertIn('b001 not closed', out)
        self.assertIn('tools/gates.sh', out)
        # The refusal came before the gate ran, and moved nothing.
        self.assertEqual(calls, [])
        self.assertEqual(self.batches()['b001']['status'], 'assigned')
        self.assertEqual(self.first_sha(), first)

    def test_wave_close_fast_forwards(self):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        self.verdicts()
        first_before = self.first_sha()
        branch_sha = self.git('rev-parse', 'p3/wso-b001').stdout.strip()
        self.assertNotEqual(first_before, branch_sha)
        ctx, calls = self.gates(0)
        with ctx:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 0, out)
        # The gate ran once, in the batch's worktree, against first.
        self.assertEqual(len(calls), 1)
        self.assertIn('tools/gates.sh', calls[0])
        self.assertIn('--base', calls[0])
        self.assertIn('first', calls[0])
        self.assertEqual(self.first_sha(), branch_sha)
        b = self.batches()['b001']
        self.assertEqual(b['status'], 'closed')
        self.assertTrue(b['closed_at'])
        self.assertIn('fork 1', out)
        self.assertIn('ours 1', out)
        # The fork won Draw, so ours' evidence kind is the loser (replay);
        # ours won Render, his verification is a trace.
        self.assertIn('trace 1', out)
        self.assertIn('replay 1', out)
        # A second close is refused, and moves nothing.
        rc, out = self.wave.close_batch(self.state, self.root, 'b001')
        self.assertEqual(rc, 1, out)
        self.assertIn('already closed', out)
        self.assertEqual(self.first_sha(), branch_sha)

    def test_wave_close_gate_false_skips_the_gate(self):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        self.verdicts()
        branch_sha = self.git('rev-parse', 'p3/wso-b001').stdout.strip()
        ctx, calls = self.gates(1)   # a gate that would refuse
        with ctx:
            rc, out = self.wave.close_batch(self.state, self.root, 'b001', gate=False)
        self.assertEqual(rc, 0, out)
        self.assertEqual(calls, [])
        self.assertEqual(self.first_sha(), branch_sha)
        self.assertEqual(self.batches()['b001']['status'], 'closed')

    def test_wave_scorecard_tally(self):
        state = {'batches': {
            'b001': {
                'rows': [{'address': f'0x{i}'} for i in range(4)],
                'files': ['src/A.cpp'], 'branch': 'p3/wso-b001',
                'status': 'closed', 'assigned_at': 't', 'closed_at': 't',
                'verdicts': {
                    '0x0': {'side': 'fork', 'case': 'c1',
                            'losing_kind': 'oracle'},
                    '0x1': {'side': 'fork', 'case': 'c2',
                            'losing_kind': 'replay'},
                    '0x2': {'side': 'ours', 'case': 'c3',
                            'losing_kind': 'trace'},
                    '0x3': {'side': 'neither', 'case': 'c4',
                            'losing_kind': None},
                }}}
        }
        with open(self.state, 'w') as f:
            json.dump(state, f)
        report = os.path.join(self.tmp, 'xrec-report.md')
        with open(report, 'w') as f:
            f.write('# Xrec\n\n## JSON\n\nrows.\n\n## Scorecard\n\nstale: fork 99\n')
        rc, out = self.wave.write_scorecard(self.state, report)
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            text = f.read()
        self.assertEqual(text.count('## Scorecard'), 1)
        self.assertIn('# Xrec', text)
        self.assertIn('## JSON', text)
        self.assertNotIn('stale', text)
        self.assertIn('Decided for the fork: 2', text)
        self.assertIn('oracle 1', text)
        self.assertIn('replay 1', text)
        self.assertIn('Decided for ours: 1', text)
        self.assertIn('trace 1', text)
        self.assertIn('Decided for neither: 1', text)
        self.assertIn('b001 closed (4 rows)', text)
        # Idempotent: a second run writes the same bytes.
        rc, out = self.wave.write_scorecard(self.state, report)
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            self.assertEqual(f.read(), text)

    def test_wave_deciding_kind_buckets(self):
        """A verdict's case id picks its deciding-evidence bucket: probe,
        then replay, then dump/readout, else trace."""
        k = self.wave.deciding_kind
        self.assertEqual(k('dump-421E40'), 'dump reading')
        self.assertEqual(k('readout-421E40'), 'dump reading')
        self.assertEqual(k('probe-3'), 'oracle probe')
        # probe is checked first, so a probe id carrying a / still reads
        # as an oracle probe.
        self.assertEqual(k('probe-3/recheck'), 'oracle probe')
        self.assertEqual(k('replay'), 'replay checkpoint')
        self.assertEqual(k('replay-world_entry'), 'replay checkpoint')
        self.assertEqual(k('walk_and_talk/dialog'), 'replay checkpoint')
        self.assertEqual(k('vid/cell_blt'), 'replay checkpoint')
        # b005 used the bare scenario name; the legacy set is exact, so
        # only that id reads as a replay checkpoint.
        self.assertEqual(k('world_entry'), 'replay checkpoint')
        self.assertEqual(k('world_entries'), 'trace')
        self.assertEqual(k('weird'), 'trace')
        self.assertEqual(k(''), 'trace')
        # A verdict dict without the case key is read defensively: a
        # missing case counts as a trace, not a crash.
        self.assertEqual(k(None), 'trace')

    def test_wave_scorecard_split_by_deciding_evidence(self):
        state = {'batches': {
            'b001': {
                'rows': [{'address': f'0x{i}'} for i in range(6)],
                'files': ['src/A.cpp'], 'branch': 'p3/wso-b001',
                'status': 'closed', 'assigned_at': 't', 'closed_at': 't',
                'verdicts': {
                    '0x0': {'side': 'fork', 'case': 'dump-421E40',
                            'losing_kind': 'oracle'},
                    '0x1': {'side': 'fork', 'case': 'readout-421E40',
                            'losing_kind': 'read-only'},
                    '0x2': {'side': 'ours', 'case': 'probe-3',
                            'losing_kind': 'trace'},
                    '0x3': {'side': 'ours', 'case': 'walk_and_talk/dialog',
                            'losing_kind': 'trace'},
                    '0x4': {'side': 'ours', 'case': 'replay',
                            'losing_kind': 'trace'},
                    # No case key: the deciding split reads it as a trace.
                    '0x5': {'side': 'neither', 'losing_kind': None},
                }}}}
        text = self.wave.scorecard_text(state)
        self.assertIn('split by deciding evidence kind', text)
        self.assertIn('Decided for the fork: 2 (losing kind: '
                      'oracle 1, read-only 1)'
                      ' -- deciding evidence: dump reading 2', text)
        self.assertIn('Decided for ours: 3 (losing kind: trace 3)'
                      ' -- deciding evidence: oracle probe 1, '
                      'replay checkpoint 2', text)
        self.assertIn('Decided for neither: 1 -- deciding evidence: '
                      'trace 1', text)
        self.assertIn('Deciding evidence across all 6 verdicts: '
                      'oracle probe 1, replay checkpoint 2, '
                      'dump reading 2, trace 1', text)
        # The four bucket names, verbatim.
        for bucket in ('dump reading', 'oracle probe',
                       'replay checkpoint', 'trace'):
            self.assertIn(bucket, text)
        self.assertIn('b001 closed (6 rows)', text)
        # write_scorecard replaces the section, once, byte-for-byte the
        # same on a second run.
        with open(self.state, 'w') as f:
            json.dump(state, f)
        report = os.path.join(self.tmp, 'xrec-report.md')
        with open(report, 'w') as f:
            f.write('# Xrec\n\n## JSON\n\nrows.\n\n## Scorecard\n\nstale\n')
        rc, out = self.wave.write_scorecard(self.state, report)
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            first = f.read()
        self.assertEqual(first.count('## Scorecard'), 1)
        self.assertIn('# Xrec', first)
        self.assertIn('## JSON', first)
        self.assertNotIn('stale', first)
        self.assertIn('dump reading 2', first)
        rc, out = self.wave.write_scorecard(self.state, report)
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            self.assertEqual(f.read(), first)


class GltraceTest(unittest.TestCase):
    """tools/gltrace.summarize -- apitrace dump text to per-frame counts."""

    # 12 calls: frame 1 is calls 1-8 (up to the first wglSwapBuffers),
    # frame 2 is calls 9-11; call 12 trails the last swap, so it is not
    # a frame. Frame 1: one 2048x2048 GL_RGBA upload, one FBO bind, two
    # draws. Frame 2: one draw.
    DUMP = [
        '1 glEnable(GL_TEXTURE_2D)',
        '2 glTexImage2D(target = GL_TEXTURE_2D, level = 0, '
        'internalformat = GL_RGBA, width = 2048, height = 2048, '
        'border = 0, format = GL_RGBA, type = GL_UNSIGNED_BYTE, '
        'pixels = blob(16777216))',
        '3 glBindTexture(GL_TEXTURE_2D, 1)',
        '4 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)',
        '5 glBindFramebuffer(GL_FRAMEBUFFER, 1)',
        '6 glDrawArrays(GL_TRIANGLES, 0, 3)',
        '7 glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, blob(12))',
        '8 wglSwapBuffers(hdc = 0x10010e) = TRUE',
        '9 glUseProgram(5)',
        '10 glDrawArrays(GL_TRIANGLES, 0, 3)',
        '11 wglSwapBuffers(hdc = 0x10010e) = TRUE',
        '12 glGetError() = GL_NO_ERROR',
    ]

    def test_gltrace_summarize_counts(self):
        self.assertEqual(gltrace.summarize(self.DUMP), {
            'frames': [
                {'draws': 2, 'fbo_binds': 1, 'texture_uploads': [
                    {'size': '2048x2048', 'format': 'GL_RGBA'}]},
                {'draws': 1, 'fbo_binds': 0, 'texture_uploads': []},
            ],
            'texture_dimensions': ['2048x2048'],
        })

    def test_gltrace_summarize_dump_variants(self):
        # apitrace dump without call numbers and without argument names
        # (--call-nos=no --arg-names=no), and a GLX swap, must read the
        # same. Upload format is the internal format (the 3rd argument of
        # glTexImage2D), size its width x height.
        self.assertEqual(gltrace.summarize([
            'glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 32, 0, GL_RGB, '
            'GL_UNSIGNED_BYTE, blob(6144))',
            'glBindFramebuffer(GL_FRAMEBUFFER, 0)',
            'glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, blob(12))',
            'glXSwapBuffers(dpy = 0x1, drawable = 2)',
        ]), {
            'frames': [
                {'draws': 1, 'fbo_binds': 1, 'texture_uploads': [
                    {'size': '64x32', 'format': 'GL_RGB'}]},
            ],
            'texture_dimensions': ['64x32'],
        })

    def test_gltrace_summarize_immediate_profile(self):
        # The engine's renderer is immediate-mode fixed function (the
        # 203206 trace histogram: 166204 glBegin blocks, zero
        # vertex-array draws, FBO binds as glBindFramebufferEXT, most
        # uploads as glTexSubImage2D). A glBegin/glEnd block is one
        # draw; a sub-image upload has no internal format to report; a
        # swap with nothing counted still presented a frame.
        self.assertEqual(gltrace.summarize([
            '1 glGenFramebuffersEXT(1, blob(4))',
            '2 glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, '
            'GL_RGBA, GL_UNSIGNED_BYTE, blob(1228800))',
            '3 glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 1)',
            '4 glBegin(GL_QUADS)',
            '5 glTexCoord2f(0, 0)',
            '6 glVertex2f(0, 0)',
            '7 glEnd()',
            '8 wglSwapBuffers(hdc = 0x10010e) = TRUE',
            '9 wglSwapBuffers(hdc = 0x10010e) = TRUE',
            '10 glBegin(GL_QUADS)',
            '11 glEnd()',
            '12 wglSwapBuffers(hdc = 0x10010e) = TRUE',
            '13 glClear(GL_COLOR_BUFFER_BIT)',
        ]), {
            'frames': [
                {'draws': 1, 'fbo_binds': 1, 'texture_uploads': [
                    {'size': '640x480', 'format': ''}]},
                {'draws': 0, 'fbo_binds': 0, 'texture_uploads': []},
                {'draws': 1, 'fbo_binds': 0, 'texture_uploads': []},
            ],
            'texture_dimensions': ['640x480'],
        })

    def test_gltrace_trim_after_last_checkpoint(self):
        # Everything after the last checkpoint op is dropped: the state
        # dump / shot / quit ops of the iwd2r control channel have no
        # counterpart in the original engine.
        actions = [
            {'op': 'wait', 'frames': 150, 'line': 4},
            {'op': 'checkpoint', 'name': 'menu', 'line': 5},
            {'op': 'click', 'x': 645, 'y': 262, 'line': 6},
            {'op': 'checkpoint', 'name': 'world', 'line': 7},
            {'op': 'shot', 'name': 'world', 'line': 8},
            {'op': 'quit', 'line': 9},
        ]
        trimmed = gltrace.trim_after_last_checkpoint(actions)
        self.assertEqual([a['op'] for a in trimmed],
                         ['wait', 'checkpoint', 'click', 'checkpoint'])
        self.assertEqual([a['name'] for a in trimmed
                          if a['op'] == 'checkpoint'],
                         ['menu', 'world'])

    # --- the desktop leg: the sandbox launch is replaced by a direct
    # desktop launch through cmd.exe (play.py's pattern), gated on
    # IWD2R_DESKTOP_OK=1. The copy's icewind2.ini is rewritten for the
    # run and restored after.

    INI = '\n'.join([
        '[Alias]',
        'HD0:=C:\\Games\\Icewind Dale II\\',
        'CD1:=C:\\Games\\Icewind Dale II\\Data\\',
        'CD2:=C:\\Games\\Icewind Dale II\\CD2\\',
        '',
        '[Program Options]',
        'Full Screen=1',
        'Resolution=800',
        'Volume Movie=85',
        'Volume Music=60',
        'Volume Voices=100',
        'Volume Ambients=70',
        'Volume SFX=80',
        'Maximum Frame Rate=30',
        '',
    ])

    def test_gltrace_ini_overrides(self):
        # The copy's icewind2.ini points [Alias] at the source install:
        # the run rewrites it to the game folder (the game would otherwise
        # read and write the source install) and silences the game's own
        # volumes (there is no system-wide muting on the desktop), and
        # touches nothing else.
        game = r'C:\Users\thier\iwd2r-ieex\game'
        text = gltrace.ini_overrides(self.INI, game)
        self.assertEqual(text, '\n'.join([
            '[Alias]',
            'HD0:=' + game + '\\',
            'CD1:=' + game + '\\Data\\',
            'CD2:=' + game + '\\CD2\\',
            '',
            '[Program Options]',
            'Full Screen=1',
            'Resolution=800',
            'Volume Movie=0',
            'Volume Music=0',
            'Volume Voices=0',
            'Volume Ambients=0',
            'Volume SFX=0',
            'Maximum Frame Rate=30',
            '',
        ]))
        # Idempotent: the rewritten text is already right.
        self.assertEqual(gltrace.ini_overrides(text, game), text)
        # A missing alias key must fail loudly rather than leave the game
        # pointing at the source install.
        with self.assertRaises(ValueError):
            gltrace.ini_overrides('[Alias]\nHD0:=C:\\x\\\n', game)

    def test_gltrace_desktop_config(self):
        results = r'C:\Users\thier\iwd2r-ieex\runs\stamped\world_entry'
        config = gltrace.desktop_config(
            'world_entry', r'C:\g', results, 660)
        self.assertEqual(config, {
            'scenario': 'world_entry',
            'exe': r'C:\g\IWD2EE.exe',
            'data': r'C:\g',
            'trace': results + r'\world_entry.trace',
            'results': results,
            'actions': results + r'\actions.json',
            'frame_rate': 30,
            'timeout': 660,
        })
        # The launch is IWD2EE.exe directly: apitrace only provides the
        # wrapper, nothing is traced through apitrace.exe itself.
        self.assertNotIn('apitrace', config)
        # Nothing sandbox-only: no machine shutdown, no Defender, no
        # DirectPlay surgery.
        for absent in ('shutdown', 'exclusions', 'directplay'):
            self.assertNotIn(absent, config)

    def test_gltrace_desktop_bat(self):
        bat = gltrace.desktop_bat(r'C:\Users\thier\iwd2r-ieex\runs\s\w')
        lines = bat.split('\n')
        self.assertEqual(lines[0], '@echo off')
        self.assertEqual(lines[1],
                         r'cd /d C:\Users\thier\iwd2r-ieex\runs\s\w')
        self.assertEqual(lines[2],
                         'powershell.exe -NoProfile -ExecutionPolicy '
                         'Bypass -File gltrace-desktop-driver.ps1 '
                         '-Config config.json')
        self.assertEqual(lines[-1], 'exit /b %ERRORLEVEL%')

    def test_gltrace_desktop_driver_scope(self):
        # The desktop driver drives input and the trace, and nothing else:
        # the sandbox driver's machine mutations (audio services, Defender,
        # DirectPlay files, logon tasks, the shutdown, the WDAG gate, the
        # second attempt) must not be in it.
        driver = gltrace.DESKTOP_DRIVER
        for absent in ('WDAGUtilityAccount', 'Stop-Service',
                       'Add-MpPreference', 'Get-MpComputerStatus',
                       'shutdown.exe', 'Install-DirectPlay', 'rgnupdt',
                       'IwdAudio', 'Set-Aside', 'attempt1'):
            self.assertNotIn(absent, driver)
        # The capture is the wrapper: IWD2EE.exe starts directly with
        # TRACE_FILE set (both exes are packed, so apitrace cannot hook
        # them); the resolution dialog is answered before the window is
        # awaited.
        for present in ('SendInput', 'TRACE_FILE', 'IWD2EE.exe',
                        'opengl32.dll', 'Answer-ResolutionDialog',
                        'BM_CLICK', 'Resolution', 'Find-GameWindow',
                        'SetForegroundWindow', 'WM_CLOSE', 'exitcode.txt',
                        'error.txt', 'done.txt', 'video adapter',
                        'driver.log'):
            self.assertIn(present, driver)
        # The old apitrace-hooked launch must be gone entirely.
        self.assertNotIn('trace --api gl', driver)
        # The resolution dialog is not the game window when the window is
        # picked, its confirmations are answered at close time, and the
        # dialog itself is answered at boot.
        self.assertGreaterEqual(driver.count('#32770'), 3)
        # The dialog always shows (IEex calls IEex_Helper_AskResolution
        # unconditionally) and its ListBox is owner-drawn: cross-process
        # LB_GETTEXT reads empty strings for every entry (runs 185556,
        # 200445), so no text match can pick 800x600 from it. The pick
        # goes through the Custom fields instead: WM_SETTEXT the
        # width/height edits (ids 1201/1203), BM_CLICK 'Custom:' (1200),
        # then 'SELECT' (1), answering the 2x UI minimum warning box if
        # it comes up. 800x600 because the scenario coordinates are
        # written for the copy's Resolution=800.
        for present in ('WM_SETTEXT', 'SendText', '1201', '1203',
                        '1200', 'Custom:', 'SELECT', '800x600',
                        'popup', 'CtrlId'):
            self.assertIn(present, driver)
        # The dead ListBox path must be gone, not just bypassed.
        for absent in ('LB_SETCURSEL', 'LB_GETCOUNT', 'ListText',
                       'list[', 'CB_SETCURSEL'):
            self.assertNotIn(absent, driver)
        # Every cross-process send carries a deadline: run 185556 sat 48
        # minutes inside one SendMessage into a game thread that had
        # stopped pumping, and no loop deadline can interrupt that.
        for present in ('SendMessageTimeout', 'SMTO_ABORTIFHUNG',
                        'SendDeadline'):
            self.assertIn(present, driver)
        self.assertNotIn('SendMessage(IntPtr', driver)
        # The game-window wait runs to the scenario timeout, and on expiry
        # the run ends with the game resolved to an exact pid for the
        # finally's kill (185556 left iwd2.exe alive at the dialog: the
        # kill only ever reached a window-found game).
        self.assertIn('$end = $deadline', driver)
        self.assertIn('no game window within the {0} s scenario timeout',
                      driver)
        self.assertGreaterEqual(driver.count('Find-GameProcess'), 3)
        self.assertIn("ProcessName -eq 'iwd2'", driver)
        # The game's process handle is cached where the game object is
        # adopted (the loader already does this): run 203206 finished the
        # scenario and copied a 6 GB trace home, but ExitCode threw after
        # the Stop-Process kill because the handle was never opened, and
        # the empty exitcode.txt failed the run.
        self.assertGreaterEqual(driver.count('# keeps ExitCode readable'),
                                3)
        # The kill is always by exact pid, never by image name.
        self.assertNotIn('Stop-Process -Name', driver)
        self.assertNotIn('taskkill', driver)

    def test_gltrace_pe_machine(self):
        # The wrapper and the game must match bitness: parse the machine
        # field out of synthetic PE images (DOS header, e_lfanew to the PE
        # signature, then the COFF machine).
        def pe_image(machine):
            header = bytearray(64)
            header[0:2] = b'MZ'
            header[0x3c:0x40] = (64).to_bytes(4, 'little')
            return bytes(header) + b'PE\x00\x00' + machine.to_bytes(
                2, 'little') + b'\x00' * 8

        with tempfile.TemporaryDirectory() as tmp:
            for machine, expected in ((0x014c, 0x014c), (0x8664, 0x8664)):
                path = os.path.join(tmp, 'x%d.dll' % machine)
                with open(path, 'wb') as f:
                    f.write(pe_image(machine))
                self.assertEqual(gltrace.pe_machine(path), expected)
            for name, data in (('not-pe.dll', b'MZ' + b'\x00' * 62),
                               ('truncated.dll', b'M'),
                               ('empty.dll', b'')):
                path = os.path.join(tmp, name)
                with open(path, 'wb') as f:
                    f.write(data)
                self.assertIsNone(gltrace.pe_machine(path), name)
            self.assertIsNone(
                gltrace.pe_machine(os.path.join(tmp, 'missing.dll')))

    def test_gltrace_manifest(self):
        # The post-run tree comparison: added/removed/changed relpaths,
        # '/'-normalized, with the game's scratch folders skipped.
        with tempfile.TemporaryDirectory() as tmp:
            def put(rel, data):
                path = os.path.join(tmp, rel.replace('/', os.sep))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, 'wb') as f:
                    f.write(data)

            put('data/keep.bin', b'one')
            put('logs/IEex.log', b'boot')
            put('cache/junk.bin', b'junk')
            before = gltrace.snapshot_manifest(tmp)
            # cache/ is a game scratch folder: absent from the snapshot.
            self.assertEqual(sorted(before), ['data/keep.bin',
                                              'logs/IEex.log'])
            put('data/keep.bin', b'two')
            put('logs/IEex.log', b'boot 2')
            put('cache/junk.bin', b'junk 2')
            put('save/new.sav', b'save')
            delta = gltrace.manifest_delta(before,
                                           gltrace.snapshot_manifest(tmp))
            self.assertEqual(delta, {
                'added': ['save/new.sav'],
                'removed': [],
                'changed': ['data/keep.bin', 'logs/IEex.log'],
            })
            # Byte-identical trees produce an empty delta.
            self.assertEqual(
                gltrace.manifest_delta(before, before),
                {'added': [], 'removed': [], 'changed': []})

    def test_gltrace_wrapper(self):
        # place_wrapper copies apitrace's wrapper beside iwd2.exe for the
        # run and refuses to overwrite one that is already there;
        # remove_wrapper undoes it; remove_stray_traces clears any .trace
        # that landed in the copy.
        with tempfile.TemporaryDirectory() as tmp:
            wrapper = os.path.join(tmp, 'wrapper', 'opengl32.dll')
            os.makedirs(os.path.dirname(wrapper))
            with open(wrapper, 'wb') as f:
                f.write(b'wrapper dll')
            game = os.path.join(tmp, 'game')
            os.makedirs(os.path.join(game, 'Data'))

            dest = gltrace.place_wrapper(wrapper, game)
            self.assertEqual(dest, os.path.join(game, 'opengl32.dll'))
            with open(dest, 'rb') as f:
                self.assertEqual(f.read(), b'wrapper dll')
            with self.assertRaises(SystemExit):
                gltrace.place_wrapper(wrapper, game)

            self.assertTrue(gltrace.remove_wrapper(game))
            self.assertFalse(os.path.exists(dest))
            self.assertFalse(gltrace.remove_wrapper(game))

            for rel in ('root.trace', os.path.join('Data', 'deep.trace')):
                with open(os.path.join(game, rel), 'wb') as f:
                    f.write(b'trace')
            self.assertEqual(gltrace.remove_stray_traces(game),
                             ['Data/deep.trace', 'root.trace'])
            self.assertEqual([name for _base, _dirs, files in
                              os.walk(game) for name in files], [])

    def test_gltrace_find_wrapper(self):
        with tempfile.TemporaryDirectory() as tmp:
            wrapper = os.path.join(tmp, 'apitrace-14.0-win32', 'lib',
                                   'wrappers', 'opengl32.dll')
            os.makedirs(os.path.dirname(wrapper))
            with open(wrapper, 'wb') as f:
                f.write(b'wrapper')
            self.assertEqual(gltrace.find_wrapper(tmp), wrapper)
            # Not a wrapper layout: fail loudly instead of tracing nothing.
            other = os.path.join(tmp, 'other')
            os.makedirs(other)
            with self.assertRaises(SystemExit):
                gltrace.find_wrapper(other)

    def test_gltrace_desktop_allowed(self):
        # play.py's gate: the desktop launch is the user's opt-in.
        self.assertFalse(gltrace.desktop_allowed({}))
        self.assertFalse(gltrace.desktop_allowed({'IWD2R_DESKTOP_OK': '0'}))
        self.assertTrue(gltrace.desktop_allowed({'IWD2R_DESKTOP_OK': '1'}))


def load_parity_tool(name):
    """load_tool for the tools/parity/ subdirectory."""
    spec = importlib.util.spec_from_file_location(
        f'parity_{name}', os.path.join(ROOT, 'tools', 'parity', f'{name}.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


parity_run = load_parity_tool('run')
struct_layout_audit = load_parity_tool('struct_layout_audit')


class ParityPreexistTest(unittest.TestCase):
    """tools/parity/run.py's pre-existing-red downgrade (mask + decision) and
    struct_layout_audit's phantom-member filter. Pure-function tests: the
    decision is exercised through a synthetic check function, no git."""

    HEAD_REPO = '/repo/head'
    BASE_REPO = '/repo/base'

    def synth_check(self, base_cell, calls=None):
        """A check() returning base_cell for the base tree's ctx."""
        def check(address, name, ctx):
            if calls is not None:
                calls.append(ctx['repo'])
            return base_cell
        return check

    # (a) the line-mask / equality helper
    def test_mask_line_refs_masks_line_numbers_only(self):
        self.assertEqual(
            parity_run.mask_line_refs(
                're_lint: L5874: RE001: CString::Format uses a narrow literal'),
            're_lint: L#: RE001: CString::Format uses a narrow literal')
        # hex offsets and addresses are NOT masked
        self.assertEqual(
            parity_run.mask_line_refs(
                'm_sequenceTest (0x56EC) -> m_dialog (0x56DC): '
                'offset goes backwards'),
            'm_sequenceTest (0x56EC) -> m_dialog (0x56DC): '
            'offset goes backwards')
        self.assertEqual(
            parity_run.mask_line_refs(
                '[MISSING] slot 0x002c: binary overrides -> 0x6fc260'),
            '[MISSING] slot 0x002c: binary overrides -> 0x6fc260')

    def test_same_after_mask(self):
        # equal after masking line references: downgrades
        self.assertTrue(parity_run.same_after_mask(
            ['re_lint: L5874: RE001: x', 're_lint: L5877: RE001: y'],
            ['re_lint: L5901: RE001: x', 're_lint: L5904: RE001: y']))
        # differing notes do not
        self.assertFalse(parity_run.same_after_mask(
            ['re_lint: L5874: RE001: x'],
            ['re_lint: L5874: RE002: x']))
        # order and count must match too
        self.assertFalse(parity_run.same_after_mask(
            ['re_lint: L1: x', 're_lint: L2: y'],
            ['re_lint: L9: y', 're_lint: L8: x']))
        self.assertFalse(parity_run.same_after_mask(
            ['re_lint: L1: x'], []))

    # (b) the downgrade decision, via a synthetic check
    def test_identical_red_downgrades_to_warning(self):
        calls = []
        head_cell = (parity_run.RED,
                     ['re_lint: L5874: RE001: narrow literal',
                      're_lint: L5877: RE001: narrow literal'])
        out = parity_run.downgrade_cell(
            self.synth_check(
                (parity_run.RED,
                 ['re_lint: L5901: RE001: narrow literal',
                  're_lint: L5904: RE001: narrow literal']), calls),
            0x704D40, 'CGameSprite::RenderPortrait', head_cell,
            {'repo': self.BASE_REPO}, 'first')
        self.assertEqual(calls, [self.BASE_REPO])   # the base re-run happened
        self.assertEqual(out[0], parity_run.WARNING)
        self.assertEqual(
            out[1][0], 'pre-existing at base first: red reproduces there')
        self.assertEqual(out[1][1:], head_cell[1])

    def test_differing_red_stays_red(self):
        head_cell = (parity_run.RED,
                     ['m_sequenceTest (0x56EC) -> m_dialog (0x56DC): '
                      'offset goes backwards'])
        out = parity_run.downgrade_cell(
            self.synth_check(
                (parity_run.RED,
                 ['m_other (0x56EC) -> m_dialog (0x56DC): '
                  'offset goes backwards'])),
            0x73B760, 'CGameSprite::Hit', head_cell,
            {'repo': self.BASE_REPO}, 'first')
        self.assertEqual(out, head_cell)

    def test_non_reproducing_red_stays_red(self):
        head_cell = (parity_run.RED, ['[MISSING] slot 0x002c: no source decl'])
        # base re-runs clean: the red is new with this change
        out = parity_run.downgrade_cell(
            self.synth_check((parity_run.GREEN, ['overrides match'])),
            0x1, 'C::F', head_cell, {'repo': self.BASE_REPO}, 'first')
        self.assertEqual(out, head_cell)
        # the base re-run raising is not a reproduction
        def boom(address, name, ctx):
            raise RuntimeError('no exe')
        self.assertEqual(
            parity_run.downgrade_cell(boom, 0x1, 'C::F', head_cell,
                                      {'repo': self.BASE_REPO}, 'first'),
            head_cell)
        # no base ctx (the function is not in the base tree)
        self.assertEqual(
            parity_run.downgrade_cell(self.synth_check(head_cell),
                                      0x1, 'C::F', head_cell, None, 'first'),
            head_cell)

    def test_error_and_non_red_cells_never_downgrade(self):
        # an ERROR stays ERROR even though the base re-run is red
        check = self.synth_check((parity_run.RED, ['same at base']))
        self.assertEqual(
            parity_run.downgrade_cell(check, 0x1, 'C::F',
                                      (parity_run.ERROR, ['raised']),
                                      {'repo': self.BASE_REPO}, 'first'),
            (parity_run.ERROR, ['raised']))
        # WARNING (a human-waived asm cell), GREEN, YELLOW and SKIP pass
        # through untouched
        for cell in ((parity_run.WARNING, ['waived: confirmed by hand']),
                     (parity_run.GREEN, ['clean']),
                     (parity_run.YELLOW, ['soft']),
                     (parity_run.SKIP, ['no pefile'])):
            self.assertEqual(
                parity_run.downgrade_cell(check, 0x1, 'C::F', cell,
                                          {'repo': self.BASE_REPO}, 'first'),
                cell)
        # asm is not a delegated column: its RED never reaches the downgrade
        self.assertNotIn('asm', parity_run.DELEGATED_COLUMNS)
        self.assertEqual(parity_run.DELEGATED_COLUMNS,
                         {'vtable', 'layout', 'offsets', 'markers', 'lints'})

    # (c) the phantom-member filter
    HEADER = """\
class CPhantom : public CGameObject {
public:
    /* 0x6E */ char m_tail;
    /* 0x78 */ void AddEffect(CGameEffect* pEffect, BYTE list) override;
    /* 0x70 */ void SetTargetObject(CGameObject* pTarget) override;
    /* 0x74 */ LONG m_real;
};
"""

    def test_layout_parse_rejects_override_phantom_member(self):
        from pathlib import Path
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, 'CPhantom.h')
            with open(path, 'w', encoding='utf-8') as f:
                f.write(self.HEADER)
            members = struct_layout_audit.parse_header(Path(path), 'CPhantom')
        # no phantom member named `override`; the real members survive
        self.assertEqual([(m[0], m[1]) for m in members],
                         [('m_tail', 0x6E), ('m_real', 0x74)])

    def test_layout_check_not_red_on_override_only_inversions(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, 'src')
            os.makedirs(src)
            with open(os.path.join(src, 'CPhantom.h'), 'w',
                      encoding='utf-8') as f:
                f.write(self.HEADER)
            verdict, notes = struct_layout_audit.check(
                0x401000, 'CPhantom::AddEffect', tmp, None, None)
        # used to RED on `override (0x78) -> override (0x70)`; the phantom
        # pair is gone, and the two real members are in order
        self.assertEqual(verdict, struct_layout_audit.SKIP)
        self.assertNotIn('goes backwards', ' '.join(notes))


class TriageTest(unittest.TestCase):
    TYPES = frozenset({'CGameSprite', 'int', 'void', 'BOOL'})

    def test_normalize_renames_identifiers_positionally(self):
        a = 'int f(int count){int total=count*2;return total;}'
        b = 'int f(int n){int sum=n*2;return sum;}'
        self.assertEqual(triage.normalize(a, self.TYPES), triage.normalize(b, self.TYPES))

    def test_normalize_keeps_calls_and_literals(self):
        a = 'void f(){Foo(3);}'
        b = 'void f(){Bar(3);}'
        self.assertNotEqual(triage.normalize(a, self.TYPES), triage.normalize(b, self.TYPES))
        self.assertNotEqual(triage.normalize('void f(){Foo(3);}', self.TYPES),
                            triage.normalize('void f(){Foo(4);}', self.TYPES))

    def test_classify_rename_only(self):
        cls, _ = triage.classify('int f(int a){return a+1;}', 'int f(int b){return b+1;}', self.TYPES)
        self.assertEqual(cls, 'rename-only')

    def test_classify_signature_only(self):
        cls, _ = triage.classify('BOOL f(int a){return a+1;}', 'int f(int a){return a+1;}', self.TYPES)
        self.assertEqual(cls, 'signature-only')

    def test_classify_fork_stub(self):
        ours = 'void f(){' + 'Step();' * 40 + '}'
        cls, _ = triage.classify('void f(){// TODO\n}', ours, self.TYPES)
        self.assertEqual(cls, 'fork-stub')

    def test_classify_short_fork_body_with_new_callee_is_ambiguous(self):
        ours = 'void f(){' + 'Step();' * 40 + '}'
        cls, _ = triage.classify('void f(){Other();}', ours, self.TYPES)
        self.assertEqual(cls, 'ambiguous')

    def test_classify_ours_stub(self):
        cls, _ = triage.classify('void f(){Real();Work();}', 'void f(){// TODO\n}', self.TYPES)
        self.assertEqual(cls, 'ours-stub')

    def test_classify_real_difference_is_ambiguous(self):
        cls, _ = triage.classify('int f(int a){if(a>0)return 1;return 0;}',
                                 'int f(int a){if(a>=0)return 1;return 0;}', self.TYPES)
        self.assertEqual(cls, 'ambiguous')

    def test_classify_swapped_operands_are_not_signature_only(self):
        cls, _ = triage.classify('int f(int a,int b){return a-b;}',
                                 'int f(int a,int b){return b-a;}', self.TYPES)
        self.assertEqual(cls, 'ambiguous')

    def test_classify_different_member_is_ambiguous(self):
        cls, _ = triage.classify('void f(CGameSprite* x){x->hp=1;Go();}',
                                 'void f(CGameSprite* x){x->mp=1;Go();}', self.TYPES)
        self.assertEqual(cls, 'ambiguous')

    def test_normalize_keeps_members_verbatim(self):
        self.assertNotEqual(triage.normalize('void f(){p->hp=1;}', self.TYPES),
                            triage.normalize('void f(){p->mp=1;}', self.TYPES))
        self.assertNotEqual(triage.normalize('void f(){p.hp=1;}', self.TYPES),
                            triage.normalize('void f(){p.mp=1;}', self.TYPES))

    def test_triage_rows_uses_injected_show(self):
        files = {('F', 'src/A.cpp'): '// 0x401000\nint f(int a){return a+1;}\n',
                 ('O', 'src/A.cpp'): '// 0x401000\nint f(int b){return b+1;}\n'}
        rows = [{'file': 'src/A.cpp', 'name': 'f', 'address': '0x401000',
                 'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1},
                {'file': 'src/A.cpp', 'name': 'g', 'address': '0x402000',
                 'category': 'same', 'their_len': 1, 'our_len': 1}]
        out = triage.triage_rows(rows, 'F', 'O', show=lambda ref, p: files[(ref, p)],
                                 type_names=frozenset({'int'}))
        self.assertEqual([(r['address'], r['class']) for r in out], [('0x401000', 'rename-only')])

    def test_triage_rows_missing_function_is_ambiguous(self):
        rows = [{'file': 'src/A.cpp', 'name': 'f', 'address': '0x401000',
                 'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1}]
        out = triage.triage_rows(rows, 'F', 'O', show=lambda ref, p: 'int g(){return 0;}',
                                 type_names=frozenset())
        self.assertEqual(out[0]['class'], 'ambiguous')
        self.assertIn('not found', out[0]['reason'])

    def test_load_type_names_reads_class_and_struct(self):
        src = {'src/a.h': 'class CPoint;\n// struct Hidden {}\nstruct CRect { int x; };\n',
               'src/b.h': 'typedef class CFoo CFoo;'}
        names = triage.load_type_names(lambda ref, p: src[p], 'O',
                                       list_headers=lambda ref: sorted(src))
        self.assertLessEqual(frozenset({'CPoint', 'CRect', 'CFoo'}), names)

    def test_load_type_names_reads_typedef_and_using_aliases(self):
        src = {'src/a.h': 'typedef unsigned int GameId;\nusing Handle2 = void*;\n'
                          'typedef struct { int a; } Packed;\n'}
        names = triage.load_type_names(lambda ref, p: src[p], 'O',
                                       list_headers=lambda ref: sorted(src))
        self.assertLessEqual(frozenset({'GameId', 'Handle2', 'Packed'}), names)

    def test_load_type_names_includes_builtin_win32_names(self):
        names = triage.load_type_names(lambda ref, p: '', 'O', list_headers=lambda ref: [])
        for n in ('LONG', 'INT', 'DWORD', 'BOOL', 'RESREF', 'CString', 'POSITION',
                  'unsigned', 'signed', 'int', 'void', 'bool'):
            self.assertIn(n, names)

    def test_change_of_type_alone_is_not_rename_only(self):
        names = triage.load_type_names(lambda ref, p: '', 'O', list_headers=lambda ref: [])
        cls, _ = triage.classify('void f(){LONG x=1;Go(x);}', 'void f(){INT x=1;Go(x);}', names)
        self.assertNotEqual(cls, 'rename-only')

    def test_calibrate_counts_agreement_per_class(self):
        state = {'batches': {'b001': {'status': 'closed', 'rows': [
            {'address': '0x1'}, {'address': '0x2'}],
            'verdicts': {'0x1': {'side': 'ours'}, '0x2': {'side': 'fork'}}}}}
        triaged = [{'address': '0x1', 'class': 'rename-only'},
                   {'address': '0x2', 'class': 'fork-stub'}]
        cal = triage.calibrate(state, triaged)
        self.assertEqual(cal['rename-only']['agree'], 1)
        self.assertEqual(cal['fork-stub'], {'agree': 0, 'total': 1, 'mismatches': ['0x2']})
        self.assertEqual(triage.enabled_classes(cal), frozenset({'rename-only'}))
        self.assertIn('fork-stub', triage.calibration_text(cal))

    def test_calibrate_skips_triage_verdicts(self):
        # An auto verdict is always 'ours': counting it would let a class
        # vouch for itself.
        triaged = [{'address': '0x1', 'class': 'rename-only'},
                   {'address': '0x2', 'class': 'rename-only'}]
        state = {'batches': {'b001': {'status': 'closed', 'verdicts': {
            '0x1': {'side': 'ours', 'case': 'triage-rename'}}}}}
        cal = triage.calibrate(state, triaged)
        self.assertEqual(cal['rename-only'], {'agree': 0, 'total': 0, 'mismatches': []})
        self.assertEqual(triage.enabled_classes(cal), frozenset())
        # An independent verdict still counts.
        state['batches']['b002'] = {'status': 'closed', 'verdicts': {
            '0x2': {'side': 'ours', 'case': 'dump-1'}}}
        cal = triage.calibrate(state, triaged)
        self.assertEqual(cal['rename-only'], {'agree': 1, 'total': 1, 'mismatches': []})
        self.assertEqual(triage.enabled_classes(cal), frozenset({'rename-only'}))

    def assert_auto_classes_agree(self, cal):
        # Every auto class, enabled or not: a flip of a fork-verdict row into
        # an auto class must fail even though that disables the class.
        for cls in triage.AUTO_OURS:
            self.assertEqual(cal[cls]['mismatches'], [], cls)

    def test_reclassified_closed_rows_agree_with_recorded_verdicts(self):
        state = json.load(open(os.path.join(ROOT, 'docs/wso/wave-state.json')))
        fork = state['fork_base']
        for ref in (fork, triage.CALIBRATION_OURS_REF):
            if subprocess.run(['git', '-C', ROOT, 'rev-parse', '--verify', '--quiet',
                               ref + '^{commit}'], capture_output=True).returncode:
                self.skipTest('%s not fetched' % ref)
        rows = json.load(open(os.path.join(ROOT, 'docs/wso/disagreements.json')))
        committed = json.load(open(os.path.join(ROOT, 'docs/wso/triage.json')))
        cal = triage.calibrate(state, triage.calibration_triage(state, rows, fork))
        self.assert_auto_classes_agree(cal)
        enabled = triage.enabled_classes(cal)
        self.assertEqual(enabled, frozenset(r['class'] for r in committed
                                            if r['class'] in triage.AUTO_OURS))

    def test_flipped_fork_row_reported_even_when_class_is_disabled(self):
        state = {'batches': {'b': {'status': 'closed', 'rows': [{'address': '0x401000'}],
                                   'verdicts': {'0x401000': {'side': 'fork'}}}}}
        rows = [{'file': 'src/A.cpp', 'name': 'f', 'address': '0x401000',
                 'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1}]
        files = {'F': '// 0x401000\nint f(int a){return a+1;}\n',
                 triage.CALIBRATION_OURS_REF: '// 0x401000\nint f(int b){return b+1;}\n'}
        cal = triage.calibrate(state, triage.calibration_triage(
            state, rows, 'F', show=lambda ref, p: files[ref], type_names=frozenset({'int'})))
        self.assertEqual(triage.enabled_classes(cal), frozenset())
        self.assertEqual(cal['rename-only']['mismatches'], ['0x401000'])
        with self.assertRaises(AssertionError):
            self.assert_auto_classes_agree(cal)

    def test_apply_calibration_disables_unproven_classes(self):
        cal = triage.calibrate({'batches': {'b': {'status': 'closed', 'verdicts': {
            '0x1': {'side': 'ours'}, '0x2': {'side': 'fork'}}}}},
            [{'address': '0x1', 'class': 'fork-stub'}, {'address': '0x2', 'class': 'rename-only'}])
        rows = [{'address': '0x1', 'class': 'fork-stub', 'reason': 'x'},
                {'address': '0x2', 'class': 'rename-only', 'reason': 'x'},
                {'address': '0x3', 'class': 'signature-only', 'reason': 'x'}]
        out = triage.apply_calibration(rows, cal)
        self.assertEqual([r['class'] for r in out], ['fork-stub', 'ambiguous', 'ambiguous'])
        self.assertEqual(out[1]['reason'], 'disabled by calibration')
        self.assertEqual(out[1]['auto_class'], 'rename-only')

    def test_calibration_against_committed_batches(self):
        if subprocess.run(['git', '-C', ROOT, 'rev-parse', '--verify', '--quiet',
                           'e8dd16a7^{commit}'], capture_output=True).returncode:
            self.skipTest('fork base not fetched')
        state = json.load(open(os.path.join(ROOT, 'docs/wso/wave-state.json')))
        triaged = json.load(open(os.path.join(ROOT, 'docs/wso/triage.json')))
        cal = triage.calibrate(state, triaged)
        self.assertTrue(any(cal[c]['total'] for c in triage.AUTO_OURS))
        for cls in triage.AUTO_OURS:
            self.assertEqual(cal[cls]['mismatches'], [], cls)


class WaveTriageTest(unittest.TestCase):
    """tools/wave.py next with triage: enabled auto classes get a verdict
    and the exact evidence phrase, ours-stub rows go to the import list,
    the rest stay for a worker."""

    FORK = 'e8dd16a7'
    # 0x400000 is a closed rename-only row decided for ours, so calibration
    # enables rename-only; signature-only has no closed verdict and stays off.
    TRIAGE = [
        {'address': '0x400000', 'class': 'rename-only', 'file': 'src/Z.cpp',
         'name': 'Old', 'reason': 'r'},
        {'address': '0x401000', 'class': 'rename-only', 'file': 'src/A.cpp',
         'name': 'Render', 'reason': 'r'},
        {'address': '0x402000', 'class': 'ambiguous', 'file': 'src/A.cpp',
         'name': 'Draw', 'reason': 'r'},
        {'address': '0x403000', 'class': 'ours-stub', 'file': 'src/B.cpp',
         'name': 'Tick', 'reason': 'r'},
        {'address': '0x404000', 'class': 'signature-only', 'file': 'src/C.cpp',
         'name': 'Init', 'reason': 'r'},
        # Disabled when triage.json was written; the gate has since enabled
        # rename-only, so auto_class decides.
        {'address': '0x405000', 'class': 'ambiguous', 'auto_class': 'rename-only',
         'file': 'src/D.cpp', 'name': 'Late', 'reason': 'disabled by calibration'},
    ]
    PHRASE = ('disagreed with WSO at e8dd16a7: rename-only; '
              'case triage-rename decides')

    git = WaveTest.git

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.wave = load_tool('wave')
        self.triage_path = os.path.join(self.tmp, 'triage.json')
        with open(self.triage_path, 'w') as f:
            json.dump(self.TRIAGE, f)
        self.state_path = os.path.join(self.tmp, 'wave-state.json')

    def disagreement(self, address, size=100):
        t = next(r for r in self.TRIAGE if r['address'] == address)
        return dict(file=t['file'], name=t['name'], address=address,
                    category='both_changed_differ', their_len=size,
                    our_len=size)

    def evidence_text(self, stem):
        return (f'---\naddress: 0x{stem}\nname: fn-{stem}\nkind: oracle\n---\n\n'
                f'# 0x{stem}\n\nbody.\n')

    def init_root(self, evidence=()):
        """A repo on `first` with one evidence file per stem, committed."""
        self.root = os.path.join(self.tmp, 'repo')
        os.makedirs(self.root)
        self.git('init', '-q', '-b', 'first', cwd=self.root)
        for k, v in (('user.name', 't'), ('user.email', 't@t'),
                     ('commit.gpgsign', 'false')):
            self.git('config', k, v)
        _write(os.path.join(self.root, 'src', 'A.cpp'), 'int a;\n')
        for stem in evidence:
            _write(os.path.join(self.root, 'docs', 'evidence', stem + '.md'),
                   self.evidence_text(stem))
        self.git('add', '-A')
        self.git('commit', '-qm', 'base')
        return self.root

    def write_state(self, batches=None):
        state = {'fork_base': self.FORK, 'batches': {
            'b000': {'rows': [self.disagreement('0x400000')],
                     'files': ['src/Z.cpp'], 'branch': 'p3/wso-b000',
                     'status': 'closed', 'assigned_at': 't', 'closed_at': 't',
                     'verdicts': {'0x400000': {'side': 'ours', 'case': 'probe-1',
                                               'losing_kind': 'trace'}}}}}
        state['batches'].update(batches or {})
        self.wave.save_state(self.state_path, state)

    def _repo(self, evidence=(), rows=('0x401000', '0x402000'), worktree=True):
        """The repo plus batch b001 assigned with `rows`, its branch and
        worktree already cut (apply_triage's starting point)."""
        repo = self.init_root(evidence)
        self.wt = os.path.join(self.tmp, 'wt')
        if worktree:
            self.git('worktree', 'add', '-q', '-b', 'p3/wso-b001', self.wt, 'first')
        chosen = [self.disagreement(a) for a in rows]
        files = []
        for r in chosen:
            if r['file'] not in files:
                files.append(r['file'])
        self.write_state({'b001': {
            'rows': chosen, 'files': files, 'branch': 'p3/wso-b001',
            'status': 'assigned', 'assigned_at': 't', 'closed_at': None,
            'verdicts': {}}})
        return repo, self.state_path

    def batch(self, bid='b001'):
        return self.wave.load_state(self.state_path)['batches'][bid]

    def import_list(self):
        with open(os.path.join(self.root, 'docs', 'wso', 'import-list.json')) as f:
            return json.load(f)

    # -- apply_triage ------------------------------------------------------

    def test_apply_triage_records_auto_verdict_and_phrase(self):
        repo, state_path = self._repo(evidence={'401000'})
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        state = self.wave.load_state(state_path)
        b = state['batches']['b001']
        self.assertEqual(b['verdicts']['0x401000'],
                         {'side': 'ours', 'case': 'triage-rename',
                          'losing_kind': 'trace'})
        text = self.wave._read_evidence(repo, 'p3/wso-b001', '0x401000')[0]
        self.assertIn(self.PHRASE, text)
        self.assertIn('## WSO delta\n\n' + self.PHRASE + '\n', text)
        # The front matter's kind is not changed.
        self.assertIn('kind: oracle', text)
        self.assertNotIn('0x402000', b['verdicts'])
        # Every row carries its class; the row left for a worker is ambiguous.
        self.assertEqual({r['address']: r['class'] for r in b['rows']},
                         {'0x401000': 'rename-only', '0x402000': 'ambiguous'})
        # The edit is committed on the batch branch; the worktree is clean.
        self.assertIn(self.PHRASE, self.git(
            'show', 'p3/wso-b001:docs/evidence/401000.md').stdout)
        self.assertEqual(self.git('log', '-1', '--format=%s', 'p3/wso-b001')
                         .stdout.strip(), 'triage: rename-only verdict for 0x401000')
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')
        self.assertIn('1 auto, 0 import, 1 ambiguous', msg)
        # A second run changes nothing: no second line, no second commit.
        head = self.git('rev-parse', 'p3/wso-b001').stdout
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        self.assertEqual(self.git('rev-parse', 'p3/wso-b001').stdout, head)
        text = self.wave._read_evidence(repo, 'p3/wso-b001', '0x401000')[0]
        self.assertEqual(text.count(self.PHRASE), 1)

    def test_apply_triage_skips_row_without_evidence_file(self):
        repo, state_path = self._repo(evidence=set())
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        self.assertEqual(self.wave.load_state(state_path)['batches']['b001']['verdicts'], {})
        self.assertIn('no evidence file', msg)
        self.assertIn('docs/evidence/401000.md', msg)
        self.assertEqual([r['class'] for r in self.batch()['rows']],
                         ['ambiguous', 'ambiguous'])
        self.assertIn('0 auto, 0 import, 2 ambiguous', msg)

    def test_apply_triage_moves_ours_stub_to_import_list(self):
        repo, state_path = self._repo(evidence={'401000', '403000'},
                                      rows=('0x401000', '0x402000', '0x403000'))
        # An existing import list is appended to, never replaced.
        old = {'address': '0x777000', 'file': 'src/Q.cpp', 'name': 'Q',
               'source': 'triage'}
        _write(os.path.join(repo, 'docs', 'wso', 'import-list.json'),
               json.dumps([old]))
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        b = self.batch()
        self.assertEqual([r['address'] for r in b['rows']], ['0x401000', '0x402000'])
        # src/B.cpp left with its only row, so the batch no longer owns it.
        self.assertEqual(b['files'], ['src/A.cpp'])
        self.assertNotIn('0x403000', b['verdicts'])
        self.assertEqual(self.import_list(), [
            old, {'address': '0x403000', 'file': 'src/B.cpp', 'name': 'Tick',
                  'source': 'triage'}])
        self.assertIn('1 auto, 1 import, 1 ambiguous', msg)
        # The moved row's evidence file is untouched.
        self.assertNotIn('WSO', self.git(
            'show', 'p3/wso-b001:docs/evidence/403000.md').stdout)

    def test_apply_triage_creates_import_list_when_missing(self):
        repo, state_path = self._repo(rows=('0x403000',))
        imports = os.path.join(self.tmp, 'elsewhere', 'import-list.json')
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7',
                                           import_path=imports)
        self.assertEqual(code, 0, msg)
        with open(imports) as f:
            self.assertEqual(json.load(f), [
                {'address': '0x403000', 'file': 'src/B.cpp', 'name': 'Tick',
                 'source': 'triage'}])
        self.assertEqual(self.batch()['rows'], [])
        self.assertEqual(self.batch()['files'], [])

    def test_apply_triage_gate_decides_from_current_calibration(self):
        repo, state_path = self._repo(evidence={'404000', '405000'},
                                      rows=('0x404000', '0x405000'))
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        b = self.batch()
        # signature-only has no closed verdict: it stays for a worker,
        # and the message says why.
        self.assertNotIn('0x404000', b['verdicts'])
        self.assertIn('0x404000 signature-only: class not enabled by calibration',
                      msg)
        # 0x405000 was written ambiguous with auto_class rename-only; the
        # closed b000 verdict has since enabled rename-only.
        self.assertEqual(b['verdicts']['0x405000']['case'], 'triage-rename')
        self.assertEqual({r['address']: r['class'] for r in b['rows']},
                         {'0x404000': 'ambiguous', '0x405000': 'rename-only'})
        # Without the closed batch nothing is enabled: no verdict at all.
        state2 = os.path.join(self.tmp, 'state2.json')
        state = self.wave.load_state(state_path)
        del state['batches']['b000']
        state['batches']['b001']['verdicts'] = {}
        self.wave.save_state(state2, state)
        code, msg = self.wave.apply_triage(state2, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        self.assertEqual(self.wave.load_state(state2)['batches']['b001']['verdicts'], {})
        self.assertIn('calibration enables: none', msg)

    def test_apply_triage_without_worktree_leaves_rows(self):
        repo, state_path = self._repo(evidence={'401000'}, worktree=False)
        code, msg = self.wave.apply_triage(state_path, repo, 'b001',
                                           self.triage_path, 'e8dd16a7')
        self.assertEqual(code, 0, msg)
        self.assertEqual(self.batch()['verdicts'], {})
        self.assertIn('no worktree for p3/wso-b001', msg)

    def test_apply_triage_refuses_unknown_or_closed_batch(self):
        repo, state_path = self._repo()
        self.assertEqual(self.wave.apply_triage(
            state_path, repo, 'b999', self.triage_path, 'e8dd16a7')[0], 2)
        self.assertEqual(self.wave.apply_triage(
            state_path, repo, 'b000', self.triage_path, 'e8dd16a7')[0], 1)
        self.assertEqual(self.wave.apply_triage(
            state_path, repo, 'b001', os.path.join(self.tmp, 'nope.json'),
            'e8dd16a7')[0], 2)

    def test_write_triage_evidence_appends_under_existing_heading(self):
        repo, _ = self._repo(evidence={'401000'})
        path = os.path.join(self.wt, 'docs', 'evidence', '401000.md')
        _write(path, self.evidence_text('401000')
               + '\n## WSO delta\n\nan earlier note.\n\n## Verdict\n\nours.\n')
        self.git('commit', '-qam', 'notes', cwd=self.wt)
        err = self.wave.write_triage_evidence(self.wt, '0x401000', 'rename-only',
                                              'e8dd16a7')
        self.assertIsNone(err)
        with open(path) as f:
            text = f.read()
        self.assertEqual(text.count('## WSO delta'), 1)
        self.assertIn('## WSO delta\n\nan earlier note.\n\n' + self.PHRASE
                      + '\n\n## Verdict\n\nours.\n', text)
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')

    # -- next_batch with triage -------------------------------------------

    def write_disagreements(self, addresses):
        path = os.path.join(self.tmp, 'disagreements.json')
        with open(path, 'w') as f:
            json.dump([self.disagreement(a, 1000 - i)
                       for i, a in enumerate(addresses)], f)
        return path

    def next(self, disagreements, **kw):
        kw.setdefault('worktree_root', os.path.join(self.tmp, 'wts'))
        return self.wave.next_batch(disagreements, self.state_path, size=20,
                                    triage_path=self.triage_path, repo=self.root,
                                    **kw)

    def test_next_batch_cuts_worktree_and_applies_triage(self):
        self.init_root(evidence={'401000'})
        self.write_state()
        dis = self.write_disagreements(['0x400000', '0x401000', '0x402000',
                                        '0x403000'])
        rc, out = self.next(dis)
        self.assertEqual(rc, 0, out)
        wt = os.path.join(self.tmp, 'wts', 'p3-wso-b001')
        self.assertEqual(self.wave.worktree_for(self.root, 'p3/wso-b001'), wt)
        # Cut from first: its base is first's tip.
        self.assertEqual(self.git('merge-base', 'first', 'p3/wso-b001').stdout,
                         self.git('rev-parse', 'first').stdout)
        b = self.batch()
        self.assertEqual(b['verdicts']['0x401000']['case'], 'triage-rename')
        self.assertEqual([(r['address'], r['class']) for r in b['rows']],
                         [('0x401000', 'rename-only'), ('0x402000', 'ambiguous')])
        self.assertEqual(b['files'], ['src/A.cpp'])
        # The phrase names the fork base recorded in the state.
        with open(os.path.join(wt, 'docs', 'evidence', '401000.md')) as f:
            self.assertIn(self.PHRASE, f.read())
        self.assertEqual([e['address'] for e in self.import_list()], ['0x403000'])
        self.assertIn('b001 assigned', out)
        self.assertIn(wt, out)
        self.assertIn('triage: 1 auto, 1 import, 1 ambiguous', out)
        # The imported row has left wave 1: the next `next` does not pick it.
        rc, out = self.next(dis)
        self.assertEqual(rc, 2, out)
        self.assertIn('nothing left', out)
        self.assertEqual(sorted(self.wave.load_state(self.state_path)['batches']),
                         ['b000', 'b001'])

    def test_next_batch_skips_a_probe_list_address(self):
        self.init_root()
        self.write_state()
        dis = self.write_disagreements(['0x402000'])
        _write(os.path.join(self.root, 'docs', 'wso', 'probe-list.json'),
               json.dumps([{'address': '0x402000', 'file': 'src/A.cpp',
                            'name': 'F', 'reading': 'fork', 'source': 'worker'}]))
        rc, out = self.next(dis)
        self.assertEqual(rc, 2, out)
        self.assertIn('nothing left', out)
        self.assertEqual(sorted(self.wave.load_state(self.state_path)['batches']), ['b000'])

    def test_next_batch_reuses_existing_worktree(self):
        self.init_root(evidence={'401000'})
        self.write_state()
        elsewhere = os.path.join(self.tmp, 'elsewhere')
        self.git('worktree', 'add', '-q', '-b', 'p3/wso-b001', elsewhere, 'first')
        rc, out = self.next(self.write_disagreements(['0x401000']))
        self.assertEqual(rc, 0, out)
        self.assertFalse(os.path.exists(os.path.join(self.tmp, 'wts', 'p3-wso-b001')))
        self.assertEqual(self.wave.worktree_for(self.root, 'p3/wso-b001'), elsewhere)
        with open(os.path.join(elsewhere, 'docs', 'evidence', '401000.md')) as f:
            self.assertIn(self.PHRASE, f.read())
        self.assertIn('triage: 1 auto, 0 import, 0 ambiguous', out)

    def test_next_batch_checks_out_existing_branch_without_worktree(self):
        self.init_root(evidence={'401000'})
        self.write_state()
        self.git('branch', 'p3/wso-b001', 'first')
        rc, out = self.next(self.write_disagreements(['0x401000']))
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.wave.worktree_for(self.root, 'p3/wso-b001'),
                         os.path.join(self.tmp, 'wts', 'p3-wso-b001'))
        self.assertEqual(self.batch()['verdicts']['0x401000']['case'],
                         'triage-rename')

    def test_next_batch_worktree_failure_records_nothing(self):
        self.init_root(evidence={'401000'})
        self.write_state()
        before = self.wave.load_state(self.state_path)
        # A non-empty directory where the worktree would go.
        _write(os.path.join(self.tmp, 'wts', 'p3-wso-b001', 'junk'), 'x\n')
        rc, out = self.next(self.write_disagreements(['0x401000']))
        self.assertEqual(rc, 1, out)
        self.assertIn('p3-wso-b001', out)
        self.assertEqual(self.wave.load_state(self.state_path), before)
        # git made the branch before failing on the path; it is removed.
        self.assertNotIn('p3/wso-b001', self.git('branch', '--list').stdout)

    def test_next_batch_refuses_missing_triage_file(self):
        self.init_root()
        self.write_state()
        before = self.wave.load_state(self.state_path)
        self.triage_path = os.path.join(self.tmp, 'nope.json')
        rc, out = self.next(self.write_disagreements(['0x401000']))
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.wave.load_state(self.state_path), before)
        self.assertNotIn('p3/wso-b001', self.git('branch', '--list').stdout)

    def test_default_worktree_root_is_main_worktree_plus_suffix(self):
        self.init_root()
        linked = os.path.join(self.tmp, 'linked')
        self.git('worktree', 'add', '-q', '-b', 'side', linked, 'first')
        # From the main worktree or a linked one, the root is the same.
        for repo in (self.root, linked):
            self.assertEqual(self.wave.default_worktree_root(repo),
                             self.root + '-worktrees')

    def test_cli_next_passes_triage_defaults(self):
        from unittest import mock
        calls = []

        def fake(**kw):
            calls.append(kw)
            return 0, 'ok\n'

        with mock.patch.object(self.wave, 'next_batch', fake), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.wave.main(['next', '--size', '3']), 0)
        self.assertEqual(calls, [{'size': 3, 'category': 'both_changed_differ',
                                  'triage_path': self.wave.TRIAGE_PATH,
                                  'repo': self.wave.ROOT}])
        self.assertEqual(self.wave.TRIAGE_PATH,
                         os.path.join(ROOT, 'docs', 'wso', 'triage.json'))

    # -- deciding kind and the scorecard ----------------------------------

    def test_deciding_kind_triage(self):
        k = self.wave.deciding_kind
        self.assertEqual(k('triage-stub'), 'triage')
        self.assertEqual(k('triage-rename'), 'triage')
        self.assertEqual(k('triage-sig'), 'triage')
        # Today's labels stay.
        self.assertEqual(k('probe-3'), 'oracle probe')
        self.assertEqual(k('frida-421E40'), 'trace')
        self.assertEqual(k('dump-421E40'), 'dump reading')

    def test_scorecard_counts_triage_verdicts(self):
        state = {'batches': {'b001': {
            'rows': [{'address': '0x1'}, {'address': '0x2'}],
            'files': ['src/A.cpp'], 'branch': 'p3/wso-b001',
            'status': 'closed', 'assigned_at': 't', 'closed_at': 't',
            'verdicts': {
                '0x1': {'side': 'ours', 'case': 'triage-rename',
                        'losing_kind': 'trace'},
                '0x2': {'side': 'ours', 'case': 'probe-3',
                        'losing_kind': 'trace'}}}}}
        text = self.wave.scorecard_text(state)
        self.assertIn('Decided for ours: 2 (losing kind: trace 2) -- deciding '
                      'evidence: oracle probe 1, triage 1', text)
        self.assertIn('Deciding evidence across all 2 verdicts: oracle probe 1, '
                      'replay checkpoint 0, dump reading 0, trace 0, triage 1', text)


    def _bundle_state(self):
        def row(a, **kw):
            r = self.disagreement(a)
            r.update(kw)
            return r
        rows = [row('0x402000', **{'class': 'ambiguous'}),
                row('0x401000', **{'class': 'rename-only'}),
                row('0x403000'), row('0x404000')]
        self.wave.save_state(self.state_path, {'fork_base': self.FORK, 'batches': {
            'b001': {'rows': rows, 'files': ['src/A.cpp'], 'branch': 'p3/wso-b001',
                     'status': 'assigned', 'assigned_at': 't', 'verdicts': {
                         '0x404000': {'side': 'ours', 'case': 'probe-1',
                                      'losing_kind': 'trace'}}}}})

    def test_render_brief_fills_bundle_paths(self):
        self._bundle_state()
        template = os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')
        calls = []

        def stub(addr):
            calls.append(addr)
            return 'bundle for %#x\n' % addr
        out_dir = os.path.join(self.tmp, 'bundles')
        rc, brief = self.wave.render_brief(
            template, self.state_path, 'b001', bundle_dir=out_dir, bundle_fn=stub)
        self.assertEqual(rc, 0, brief)
        # Ambiguous plus unclassed-without-verdict; not auto, not decided.
        self.assertEqual(sorted(calls), [0x402000, 0x403000])
        for a in ('0x402000', '0x403000'):
            path = os.path.join(out_dir, a + '.md')
            self.assertIn(path, brief)
            with open(path) as f:
                self.assertEqual(f.read(), 'bundle for %s\n' % a.lower())
        self.assertNotIn('0x401000.md', brief)
        self.assertNotIn('0x404000.md', brief)
        self.assertNotIn('{{', brief)
        rc, brief = self.wave.render_brief(
            template, self.state_path, 'b001', bundle_fn=stub)
        self.assertEqual(rc, 0, brief)
        self.assertIn('(no bundles)', brief)

    def test_brief_template_has_verdict_grammar(self):
        with open(os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')) as f:
            text = f.read()
        self.assertIn('{{bundle}}', text)
        self.assertIn('3. **Bundles first.** Read the bundle for a function '
                      'before its source; do not read a whole `src/*.cpp` the '
                      'bundle covers.', text)
        self.assertIn('## Verdicts', text)
        self.assertIn('verdict <0xADDR> <fork|ours|neither> case <id>', text)
        nums = re.findall(r'^(\d+)\. \*\*', text, re.M)
        self.assertEqual(nums, [str(i) for i in range(1, len(nums) + 1)])

    def test_scorecard_measured_column(self):
        def v(side, case, measured=None):
            d = {'side': side, 'case': case, 'losing_kind': None}
            if measured:
                d['measured'] = measured
            return d
        state = {'batches': {'b001': {
            'rows': [{}] * 4, 'status': 'closed', 'verdicts': {
                '0x1': v('ours', 'probe-1', 'probe'),
                '0x2': v('ours', 'probe-2', 'frida'),
                '0x3': v('ours', 'replay-1', 'replay'),
                '0x4': v('fork', 'dump-1')}}}}
        text = self.wave.scorecard_text(state)
        self.assertIn('measured: probe 1, frida 1, replay 1, none 0', text)
        self.assertIn('measured: probe 0, frida 0, replay 0, none 1', text)
        self.assertEqual(text.count('measured: '), 2)

    def test_write_scorecard_appends_calibration_once(self):
        self.write_state()
        report = os.path.join(self.tmp, 'report.md')
        with open(report, 'w') as f:
            f.write('# R\n\n## Scorecard\n\nold\n\n## Triage calibration\n\nkeep\n')
        rc, out = self.wave.write_scorecard(
            self.state_path, report, triage_path=self.triage_path)
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            text = f.read()
        self.assertEqual(text.count('## Triage calibration'), 1)
        self.assertIn('computed from triage.json classes over closed verdicts', text)
        self.assertIn('rename-only', text)
        self.assertIn('keep', text)
        self.wave.write_scorecard(self.state_path, report,
                                  triage_path=self.triage_path)
        with open(report) as f:
            self.assertEqual(f.read(), text)
        # No triage.json: no calibration block.
        rc, out = self.wave.write_scorecard(
            self.state_path, report,
            triage_path=os.path.join(self.tmp, 'absent.json'))
        with open(report) as f:
            self.assertNotIn('computed from triage.json', f.read())

class _SeqWorker:
    """A worker that returns its reports in turn (the last one repeats)
    and records every brief and cwd it was given."""

    def __init__(self, *reports, credits=None, rate_limited=False):
        self.reports = list(reports)
        self.credits = credits
        self.rate_limited = rate_limited
        self.briefs = []
        self.cwds = []

    def run(self, brief, cwd):
        self.briefs.append(brief)
        self.cwds.append(cwd)
        report = self.reports[min(len(self.briefs), len(self.reports)) - 1]
        return driver.Result(0, report, None, self.credits, self.rate_limited)


class _DirtyWorker(_SeqWorker):
    """A _SeqWorker that leaves uncommitted work in its cwd first: a
    change to a tracked file and a new untracked one."""

    def run(self, brief, cwd):
        with open(os.path.join(cwd, 'docs', 'evidence', '401000.md'), 'a') as f:
            f.write('\nhalf-written note\n')
        with open(os.path.join(cwd, 'scratch.txt'), 'w') as f:
            f.write('untracked\n')
        return super().run(brief, cwd)


class DriverTest(unittest.TestCase):
    REPORT = ('# report\n\n## Verdicts\n\nverdict 0x401000 ours case dump-401000\n'
              'verdict 0x402000 fork case probe-b011-402000\n')

    def test_parse_report(self):
        self.assertEqual(driver.parse_report(self.REPORT),
                         {'0x401000': ('ours', 'dump-401000'), '0x402000': ('fork', 'probe-b011-402000')})

    def test_parse_report_rejects_duplicate(self):
        with self.assertRaises(driver.ReportError):
            driver.parse_report(self.REPORT + 'verdict 0x401000 fork case x\n')

    def test_parse_report_rejects_malformed(self):
        with self.assertRaises(driver.ReportError):
            driver.parse_report('## Verdicts\nverdict 0x401000 maybe case x\n')

    def test_parse_undecided(self):
        text = ('## Verdicts\nverdict 0x1 ours case a\n\n## Undecided\n'
                'undecided 0x716e80 fork the exe skips the ability\n'
                '- `Undecided 0X02 NEITHER both wrong`\n3. undecided 0x0403000 neither  x y \n'
                '\n## Notes\nundecided nonsense here\n')
        self.assertEqual(driver.parse_undecided(text), {
            '0x716E80': ('fork', 'the exe skips the ability'),
            '0x2': ('neither', 'both wrong'), '0x403000': ('neither', 'x y')})
        self.assertEqual(driver.parse_undecided(self.REPORT), {})
        self.assertEqual(driver.parse_report('## Undecided\nundecided 0x1 fork r\n'), {})

    def test_parse_undecided_rejects_malformed_and_duplicates(self):
        h = '## Undecided\n'
        for bad in ('undecided 0x1 ours no\n', 'undecided 0x1 fork\n',
                    'undecided 0x1\n', 'I could not decide.\n',
                    'verdict 0x1 fork case x\n'):
            with self.assertRaises(driver.ReportError, msg=bad):
                driver.parse_undecided(h + bad)
        with self.assertRaises(driver.ReportError) as cm:
            driver.parse_undecided(h + '\nsome note\n')
        self.assertIn('line 3', str(cm.exception))
        with self.assertRaises(driver.ReportError):
            driver.parse_undecided(h + 'undecided 0x401000 fork a\nundecided 0x0401000 neither b\n')

    def test_parse_undecided_skips_code_fences(self):
        text = '## Undecided\n```text\nundecided 0x401000 fork a\n```\n```\nundecided 0x2 neither b\n```\n'
        self.assertEqual(driver.parse_undecided(text),
                         {'0x401000': ('fork', 'a'), '0x2': ('neither', 'b')})

    def test_credits_of_uses_both_formulas(self):
        lines = [json.dumps({'type': 'assistant', 'message': {'model': 'glm-5.3-flash',
                 'usage': {'input_tokens': 10000, 'cache_read_input_tokens': 100000, 'output_tokens': 1000}}})]
        with tempfile.NamedTemporaryFile('w', suffix='.jsonl', delete=False) as f:
            f.write('\n'.join(lines))
        self.addCleanup(os.unlink, f.name)
        self.assertAlmostEqual(driver.credits_of(f.name), (10000*2.3 + 100000*0.56 + 1000*8)/10000)

    def test_credits_of_full_formula_and_skips(self):
        lines = [
            json.dumps({'type': 'assistant', 'message': {'model': 'glm-5.3[1m]',
                'usage': {'input_tokens': 1000, 'cache_creation_input_tokens': 1000,
                          'cache_read_input_tokens': 5000, 'output_tokens': 100}}}),
            'not json', json.dumps({'type': 'user', 'message': {'content': 'x'}}), '[1]']
        with tempfile.NamedTemporaryFile('w', suffix='.jsonl', delete=False) as f:
            f.write('\n'.join(lines))
        self.addCleanup(os.unlink, f.name)
        self.assertAlmostEqual(driver.credits_of(f.name), (2000*6.9 + 5000*1.7 + 100*24)/10000)

    def test_headless_worker_argv(self):
        w = driver.HeadlessWorker()
        argv, env = w.command('BRIEF')
        self.assertEqual(argv[:3], ['claude-glm', '-p', 'BRIEF'])
        self.assertIn('--dangerously-skip-permissions', argv)
        self.assertEqual(env['CLAUDE_GLM_MODEL'], 'glm-5.3-flash[1m]')

    def test_slug_and_config_dir(self):
        self.assertEqual(driver.project_slug('/home/thier/iwd2-re-worktrees/p3-wso-b011'),
                         '-home-thier-iwd2-re-worktrees-p3-wso-b011')
        with mock.patch.dict(os.environ, {'CLAUDE_GLM_CONFIG_DIR': '/x/cfg'}):
            self.assertEqual(driver.HeadlessWorker().config_dir, '/x/cfg')
            self.assertEqual(driver.HeadlessWorker(config_dir='/y').config_dir, '/y')
        env = {k: v for k, v in os.environ.items() if k != 'CLAUDE_GLM_CONFIG_DIR'}
        with mock.patch.dict(os.environ, env, clear=True):
            self.assertEqual(driver.HeadlessWorker().config_dir,
                             os.path.expanduser('~/.claude-glm'))
            self.assertEqual(driver.HeadlessWorker(wrapper='claude').config_dir,
                             os.path.expanduser('~/.claude'))

    def test_run_with_stubbed_subprocess(self):
        with tempfile.TemporaryDirectory() as tmp:
            cwd = os.path.join(tmp, 'wt')
            os.mkdir(cwd)
            proj = os.path.join(tmp, 'cfg', 'projects', driver.project_slug(os.path.abspath(cwd)))
            os.makedirs(proj)
            with open(os.path.join(proj, 'sess1.jsonl'), 'w') as f:
                f.write(json.dumps({'message': {'model': 'glm-5.3-flash[1m]',
                        'usage': {'output_tokens': 10000}}}) + '\n')
            out = json.dumps({'result': 'REPORT', 'session_id': 'sess1'})
            calls = []

            def stub(argv, **kw):
                calls.append((argv, kw))
                return subprocess.CompletedProcess(argv, 0, out, '')
            w = driver.HeadlessWorker(config_dir=os.path.join(tmp, 'cfg'), run=stub)
            r = w.run('B', cwd)
            self.assertEqual((r.exit_code, r.report, r.rate_limited), (0, 'REPORT', False))
            self.assertAlmostEqual(r.credits, 8.0)
            self.assertEqual(calls[0][1]['cwd'], cwd)
            # missing transcript: no crash
            out = json.dumps({'result': 'R', 'session_id': 'nope'})
            r = w.run('B', cwd)
            self.assertIsNone(r.transcript)
            self.assertIsNone(r.credits)

    def _credits(self, lines):
        with tempfile.NamedTemporaryFile('w', suffix='.jsonl', delete=False) as f:
            f.write('\n'.join(json.dumps(x) for x in lines))
        self.addCleanup(os.unlink, f.name)
        return driver.credits_of(f.name)

    def test_credits_dedup_by_message_id(self):
        line = {'message': {'id': 'm1', 'model': 'glm-5.3-flash',
                'usage': {'output_tokens': 10000}}}
        self.assertAlmostEqual(self._credits([line, line, line]), 8.0)
        last = {'message': {'id': 'm1', 'model': 'glm-5.3-flash',
                'usage': {'output_tokens': 20000}}}
        self.assertAlmostEqual(self._credits([line, last]), 16.0)

    def test_credits_model_rules(self):
        u = {'output_tokens': 10000}
        def one(model):
            return self._credits([{'message': {'model': model, 'usage': u}}])
        self.assertAlmostEqual(one('glm-5.3-flash[1m]'), 8.0)
        self.assertAlmostEqual(one('glm-4.9'), 24.0)
        self.assertEqual(one('claude-opus-5'), 0.0)
        self.assertEqual(one('<synthetic>'), 0.0)
        self.assertEqual(self._credits([{'message': {'usage': u}}]), 0.0)

    def test_parse_report_tolerant_forms(self):
        h = '## Verdicts\n'
        self.assertEqual(driver.parse_report(h + '- verdict 0x76d370 ours case c1\n'),
                         {'0x76D370': ('ours', 'c1')})
        self.assertEqual(driver.parse_report(h + '`verdict 0x1 fork case c2`\n2. Verdict 0X02 neither case c3\n'),
                         {'0x1': ('fork', 'c2'), '0x2': ('neither', 'c3')})
        with self.assertRaises(driver.ReportError) as cm:
            driver.parse_report(h + '\nI think it is ours.\n')
        self.assertIn('line 3', str(cm.exception))
        with self.assertRaises(driver.ReportError):
            driver.parse_report(h + 'verdict 0x401000 ours case a\nverdict 0x0401000 fork case b\n')

    def test_run_non_json_output(self):
        def stub(argv, **kw):
            return subprocess.CompletedProcess(argv, 0, 'boom', '')
        r = driver.HeadlessWorker(config_dir='/nonexistent', run=stub).run('B', '/tmp')
        self.assertEqual((r.exit_code, r.report), (1, 'boom'))
        def stub2(argv, **kw):
            return subprocess.CompletedProcess(argv, 3, 'x', '')
        r = driver.HeadlessWorker(config_dir='/nonexistent', run=stub2).run('B', '/tmp')
        self.assertEqual(r.exit_code, 3)

    def test_fake_worker(self):
        r = driver.FakeWorker('R', exit_code=2, rate_limited=True).run('b', '/tmp')
        self.assertEqual((r.exit_code, r.report, r.rate_limited), (2, 'R', True))

    def test_parse_report_skips_code_fences(self):
        text = ('## Verdicts\n```text\nverdict 0x401000 ours case a\n```\n'
                '```\nverdict 0x76d370 fork case b\n```\n')
        self.assertEqual(driver.parse_report(text),
                         {'0x401000': ('ours', 'a'), '0x76D370': ('fork', 'b')})

    def _mc_repo(self, evidence):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d, True)
        g = lambda *a: subprocess.run(['git', '-C', d, *a], check=True, capture_output=True)
        g('init', '-q', '-b', 'p3/wso-b011')
        os.makedirs(os.path.join(d, 'docs', 'evidence'))
        for addr, text in evidence.items():
            with open(os.path.join(d, 'docs', 'evidence', addr + '.md'), 'w') as f:
                f.write(text)
        return d

    @staticmethod
    def _mc_batch():
        return {'id': 'b011', 'branch': 'p3/wso-b011', 'rows': [
            {'address': '0x76D370', 'class': 'ambiguous'},
            {'address': '0x401000', 'class': 'ambiguous'},
            {'address': '0x402000', 'class': 'ours-auto', 'verdict': 'ours'}]}

    def _mc_run(self, rc=0, calls=None):
        def run(argv, **kw):
            calls.append((argv, kw.get('cwd')))
            return subprocess.CompletedProcess(argv, rc, '', 'boom\n')
        return run

    def test_mechanical_checks_flags_missing_and_foreign_verdicts(self):
        d = self._mc_repo({'76D370': 'corroborated by WSO at abc'})
        calls = []
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(
                self._mc_batch(), d, {'0x76d370': ('ours', 'c'), '0x999999': ('ours', 'c')},
                run=self._mc_run(0, calls))
        self.assertEqual(len(probs), 2)
        self.assertTrue(any(p.startswith('0x401000: no verdict line') for p in probs))
        self.assertTrue(any(p.startswith('0x999999:') and 'outside' in p for p in probs))
        self.assertIn(['tools/check_evidence.py', '--base', 'first'],
                      [c[0][-3:] for c in calls])
        self.assertEqual(calls[-1][0][-2:], ['tools/gen_map.py', '--check'])
        self.assertTrue(all(c[1] == d for c in calls))

    def test_mechanical_checks_accept_undecided_in_place_of_a_verdict(self):
        d = self._mc_repo({'76D370': 'corroborated by WSO at abc'})
        batch = self._mc_batch()
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'), \
                mock.patch.object(driver._tool('check_evidence'), 'touched',
                                  return_value=[]):
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'c')}, run=self._mc_run(0, []),
                undecided=['0x401000'])
            self.assertEqual(probs, [])
            # not listed: still a missing verdict
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'c')}, run=self._mc_run(0, []))
            self.assertEqual(len(probs), 1)
            self.assertIn('## Undecided', probs[0])
            # a verdict and an undecided line together, and one outside the batch
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'c')}, run=self._mc_run(0, []),
                undecided=['0x401000', '0x76d370', '0x999999'])
        self.assertEqual(len(probs), 2, probs)
        self.assertTrue(any(p.startswith('0x76d370: both a verdict') for p in probs))
        self.assertTrue(any(p.startswith('0x999999:') and 'outside' in p for p in probs))

    def test_mechanical_checks_undecided_row_must_keep_our_body(self):
        d = self._mc_repo({'76D370': 'corroborated by WSO at abc'})
        batch = self._mc_batch()
        ce = driver._tool('check_evidence')
        args = (batch, d, {'0x76D370': ('ours', 'c')})
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            with mock.patch.object(ce, 'touched', return_value=[
                    ('src/A.cpp', 0x401000, 'F')]) as t:
                probs = driver.mechanical_checks(
                    *args, run=self._mc_run(0, []), undecided=['0x401000'])
                self.assertEqual(t.call_args[0], (d, 'first'))
            self.assertEqual(len(probs), 1, probs)
            self.assertIn('0x401000: listed under ## Undecided but its body '
                          'changed against first', probs[0])
            with mock.patch.object(ce, 'touched', return_value=[
                    ('src/A.cpp', 0x999000, 'G')]):
                self.assertEqual(driver.mechanical_checks(
                    *args, run=self._mc_run(0, []), undecided=['0x401000']), [])
            # no undecided rows: the span diff is not run
            with mock.patch.object(ce, 'touched', side_effect=AssertionError) as t:
                driver.mechanical_checks(*args, run=self._mc_run(0, []),
                                         undecided=[])
            t.assert_not_called()

    def test_mechanical_checks_import_batches_refuse_undecided(self):
        d = self._mc_repo({})
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(
                self._mc_batch(), d, {}, run=self._mc_run(0, []), kind='import',
                undecided=['0x401000'])
        self.assertEqual(probs, ['Undecided is for wave-1 batches; remove the '
                                 '## Undecided section'])

    def test_mechanical_checks_requires_phrase(self):
        d = self._mc_repo({'76D370': 'nothing here'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:1]
        calls = []
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(batch, d, {'0x76D370': ('ours', 'c')},
                                             run=self._mc_run(0, calls))
        self.assertEqual(len(probs), 1)
        self.assertIn('docs/evidence/76D370.md', probs[0])
        self.assertIn('disagreed with WSO', probs[0])
        # a missing evidence file is a problem too
        os.unlink(os.path.join(d, 'docs', 'evidence', '76D370.md'))
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(batch, d, {'0x76D370': ('ours', 'c')},
                                             run=self._mc_run(0, []))
        self.assertEqual(len(probs), 1)
        self.assertIn('76D370.md', probs[0])

    def test_mechanical_checks_subprocess_steps(self):
        d = self._mc_repo({'76D370': 'disagreed with WSO at x'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:1]
        exe = os.path.join(d, 'iwd2.exe')
        open(exe, 'w').close()
        calls = []
        with mock.patch.object(driver, '_vanilla_exe', return_value=exe), \
                mock.patch.dict(os.environ, {'IWD2R_HOME': d}):
            probs = driver.mechanical_checks(batch, d, {'0x76D370': ('ours', 'c')},
                                             run=self._mc_run(1, calls))
        # git status, git diff, tests, check_evidence, parity, gen_map
        self.assertEqual(len(calls), 6)
        self.assertEqual(calls[4][0], ['python3', '-I', 'tools/parity/run.py', '--base', 'first'])
        self.assertEqual(len(probs), 6)
        self.assertIn('failed (exit 1); last lines: boom', probs[2])
        self.assertIn('gen_map.py --check', probs[5])
        venv = os.path.join(d, 'parity-venv', 'bin')
        os.makedirs(venv)
        py = os.path.join(venv, 'python')
        open(py, 'w').close()
        os.chmod(py, 0o755)
        calls = []
        with mock.patch.object(driver, '_vanilla_exe', return_value=exe), \
                mock.patch.dict(os.environ, {'IWD2R_HOME': d}):
            driver.mechanical_checks(batch, d, {'0x76D370': ('ours', 'c')},
                                     run=self._mc_run(0, calls))
        self.assertEqual(calls[4][0][0], py)

    def test_review_needed_for_fork_rows(self):
        self.assertEqual(driver.review_needed({'id': 'b011'}, {'0x1': ('ours', 'a'), '0x2': ('fork', 'b')}, 's'),
                         (True, 'fork/neither rows'))

    def test_review_sample_is_deterministic(self):
        v = {'0x1': ('ours', 'a')}
        a = driver.review_needed({'id': 'b011'}, v, '2026-10-22')
        self.assertEqual(a, driver.review_needed({'id': 'b011'}, v, '2026-10-22'))
        self.assertEqual(driver.review_needed({'id': 'b011'}, v, 's', sample=1.0), (True, 'sample'))
        self.assertEqual(driver.review_needed({'id': 'b011'}, v, 's', sample=0.0), (False, ''))

    # -- the batch loop ---------------------------------------------------

    LOOP_ROWS = (('0x401000', 'Render', 'src/A.cpp', 'ambiguous'),
                 ('0x402000', 'Draw', 'src/A.cpp', 'ambiguous'),
                 ('0x403000', 'Tick', 'src/B.cpp', 'rename-only'))
    AUTO_LINE = ('disagreed with WSO at e8dd16a7: rename-only; '
                 'case triage-rename decides')
    CLEAN_REVIEW = '# review\n\nNothing blocks.\n'
    ALL_OURS = ('# report\n\n## Verdicts\n\nverdict 0x401000 ours case dump-401000\n'
                'verdict 0x402000 ours case dump-402000\n')
    CUT = ('b011 assigned: 3 rows, 2 files, branch p3/wso-b011\n'
           'triage: 1 auto, 2 import, 2 ambiguous\n')
    NOTHING = (2, 'nothing left: every both_changed_differ row is assigned or decided\n')

    git = WaveTest.git

    def _driver(self, worker, reviewer=None, phrases=True, **kw):
        """A Driver over a temp repo: `first`, batch b011 assigned with its
        branch and worktree, two ambiguous rows and one auto row.  The
        evidence files the worker would write are committed already when
        `phrases`.  The brief and the subprocess check steps are faked;
        record_verdict and close_batch (gate=False) are the real ones."""
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        self.tmp = tmp
        self.root = os.path.join(tmp, 'repo')
        os.makedirs(self.root)
        self.git('init', '-q', '-b', 'first')
        for addr, name, _file, _cls in self.LOOP_ROWS:
            _write(os.path.join(self.root, 'docs', 'evidence', addr[2:] + '.md'),
                   f'---\naddress: {addr}\nname: {name}\nkind: oracle\n---\n\n# {addr}\n')
        self.git('add', '-A')
        self.git('commit', '-qm', 'base')
        self.wt = os.path.join(tmp, 'wt')
        self.git('worktree', 'add', '-q', '-b', 'p3/wso-b011', self.wt, 'first')
        lines = {'403000': self.AUTO_LINE}
        if phrases:
            lines['401000'] = 'disagreed with WSO at e8dd16a7: the guard; case probe-1 decides'
            lines['402000'] = 'corroborated by WSO at e8dd16a7'
        for stem, line in lines.items():
            with open(os.path.join(self.wt, 'docs', 'evidence', stem + '.md'), 'a') as f:
                f.write('\n' + line + '\n')
        self.git('commit', '-qam', 'evidence', cwd=self.wt)
        rows = [{'address': a, 'name': n, 'file': f, 'category': 'both_changed_differ',
                 'their_len': 100, 'our_len': 100, 'class': c}
                for a, n, f, c in self.LOOP_ROWS]
        self.state_path = os.path.join(tmp, 'wave-state.json')
        with open(self.state_path, 'w') as f:
            json.dump({'fork_base': 'e8dd16a7', 'batches': {'b011': {
                'rows': rows, 'files': ['src/A.cpp', 'src/B.cpp'],
                'branch': 'p3/wso-b011', 'status': 'assigned', 'assigned_at': 't',
                'closed_at': None, 'verdicts': {'0x403000': {
                    'side': 'ours', 'case': 'triage-rename', 'losing_kind': 'trace'}}}}}, f)
        self.pushes, self.sent, self.bundle_dirs, self.logs = [], [], [], []
        d = driver.Driver(
            self.root, self.state_path, worker,
            driver.FakeWorker(self.CLEAN_REVIEW) if reviewer is None else reviewer,
            os.path.join(tmp, 'ledger.md'), os.path.join(tmp, 'driver', 'status.json'),
            push=lambda: self.pushes.append(1), send=self.sent.append, gate=False,
            log=self.logs.append, **kw)

        def brief(template_path, state_path, batch_id, bundle_dir=None, **_):
            self.bundle_dirs.append(bundle_dir)
            return 0, f'BRIEF {batch_id}'

        d.render_brief = brief
        d.mechanical_checks = self._checks
        d.report_path = os.path.join(tmp, 'xrec-report.md')
        d.triage_path = os.path.join(tmp, 'no-triage.json')
        return d

    @staticmethod
    def _checks(batch, wt, verdicts, **kw):
        ok = lambda argv, **_: subprocess.CompletedProcess(argv, 0, '', '')
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            return driver.mechanical_checks(batch, wt, verdicts, run=ok, **kw)

    def _state(self):
        with open(self.state_path) as f:
            return json.load(f)['batches']['b011']

    def _status(self, d):
        with open(d.status_path) as f:
            return json.load(f)

    def _ledger(self, d):
        with open(d.ledger_path) as f:
            return f.read()

    def test_ledger_entry_shape(self):
        rows = [{'address': '0x%X' % (0x401000 + i)} for i in range(20)]
        verdicts = {r['address']: {'side': 'ours', 'case': 'triage-rename'} for r in rows[:8]}
        verdicts.update({r['address']: ('ours', 'dump-1') for r in rows[8:19]})
        verdicts[rows[19]['address']] = ('fork', 'probe-b011-1')
        text = driver.ledger_entry(batch={'id': 'b011', 'rows': rows}, verdicts=verdicts,
                                   classes={'rename-only': 8, 'ambiguous': 12},
                                   review=(False, 'not sampled'), commits='a1..b2', credits=1180.5)
        self.assertTrue(text.startswith('## driver b011 ('))
        self.assertRegex(text.splitlines()[0], r'^## driver b011 \(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\+00:00\)$')
        self.assertIn('rename-only 8', text)
        self.assertIn('credits 1180', text)
        self.assertIn('rows by class: rename-only 8, ambiguous 12', text)
        self.assertIn('verdicts: fork 1, ours 19, neither 0', text)
        self.assertIn('oracle probe 1', text)
        self.assertIn('dump reading 11', text)
        self.assertIn('triage 8', text)
        self.assertIn('review: no, not sampled', text)
        self.assertIn('commits a1..b2', text)
        self.assertNotIn('ignored', text)
        none = driver.ledger_entry({'id': 'b011'}, {}, {}, (True, 'sample: clean'), 'x..y', None)
        self.assertIn('credits unknown', none)
        self.assertIn('review: yes, sample: clean', none)

    # -- measured verdicts (Task 12) ---------------------------------------

    PURE_EVIDENCE = ('\n## Decompiled (Ghidra)\n\n```c\nint Rules::Render(int a, int b)\n\n'
                     '{\n  return a + b;\n}\n```\n')

    def _measured_driver(self, **kw):
        """A loop driver whose 0x401000 is a pure function (a draftable
        oracle case) and whose 0x402000 is IcewindMisc::Roll, for the Frida map."""
        d = self._driver(worker=kw.pop('worker', driver.FakeWorker(self.REPORT)), **kw)
        with open(self.state_path) as f:
            state = json.load(f)
        rows = state['batches']['b011']['rows']
        rows[0]['name'] = 'Rules::Render'
        rows[1]['name'] = 'IcewindMisc::Roll'
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        with open(os.path.join(self.wt, 'docs', 'evidence', '401000.md'), 'a') as f:
            f.write('\ncorroborated by WSO at e8dd16a7\n' + self.PURE_EVIDENCE)
        with open(os.path.join(self.wt, 'docs', 'evidence', '402000.md'), 'a') as f:
            f.write('\n## Decompiled (Ghidra)\n\n```c\nint Roll(int a, int b)\n\n{\n  return g_x;\n}\n```\n')
        self.git('commit', '-qam', 'decompiled', cwd=self.wt)
        return d

    def test_driver_records_measured_from_probe_results(self):
        calls = []

        def oracle_run(batch_id, worktree, cases):
            calls.append((batch_id, worktree, [c['address'] for c in cases],
                          [c['signature'] for c in cases]))
            return {'0x401000': ('pass', '36 inputs')}
        d = self._measured_driver(oracle_run=oracle_run)
        out = d.run_batch('b011')
        self.assertEqual(out, 'closed', self._status(d)['exceptions'])
        self.assertEqual(calls, [('b011', self.wt, ['0x401000'], ['int Rules::Render(int a, int b)'])])
        b = self._state()
        self.assertEqual(b['verdicts']['0x401000'], {
            'side': 'ours', 'case': 'probe-b011-401000', 'losing_kind': 'trace', 'measured': 'probe'})
        self.assertNotIn('measured', b['verdicts']['0x402000'])   # the worker's own, unmeasured
        self.assertEqual(b['verdicts']['0x402000']['side'], 'fork')
        self.assertIn('decided by the oracle probe', '\n'.join(self.logs))
        with open(os.path.join(os.path.dirname(d.status_path), 'b011', 'worker.brief.md')) as f:
            self.assertIn('probe-b011-401000', f.read())

    def test_driver_oracle_fail_is_a_finding_and_crash_leaves_the_row(self):
        for status in ('fail', 'crash', 'pending'):
            briefs = []

            class Worker(driver.FakeWorker):
                def run(self, brief, cwd):
                    briefs.append(brief)
                    return super().run(brief, cwd)
            d = self._measured_driver(
                worker=Worker(self.REPORT),
                oracle_run=lambda b, w, c, status=status: {'0x401000': (status, 'detail-%s' % status)})
            self.assertEqual(d.run_batch('b011'), 'closed')
            v = self._state()['verdicts']['0x401000']
            self.assertEqual((v['side'], v['case']), ('ours', 'dump-401000'), status)   # the worker's
            self.assertNotIn('measured', v)
            self.assertEqual('FAILED' in briefs[0], status == 'fail', status)
            self.assertEqual('detail-fail' in briefs[0], status == 'fail')

    def test_driver_keeps_only_passing_oracle_cases_committed(self):
        report_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, report_dir, True)

        def real_files(status):
            def oracle_run(batch_id, worktree, cases):
                # the real writer and wiring, with a fake build and run
                def run(cmd, **kw):
                    path = os.path.join(report_dir, 'r.json')
                    with open(path, 'w') as f:
                        json.dump({'cases': [{'case': 'wave/b011/401000', 'iterations': 36,
                                              'failure_count': 0 if status == 'pass' else 2}]}, f)
                    return subprocess.CompletedProcess(cmd, 0, 'report: %s\n' % path, '')
                return truth.run_oracle(batch_id, worktree, cases, build=lambda w: None, run=run)
            return oracle_run
        for status in ('pass', 'fail'):
            d = self._measured_driver(oracle_run=real_files(status))
            os.makedirs(os.path.join(self.wt, 'oracle'), exist_ok=True)
            with open(os.path.join(self.wt, 'oracle', 'CMakeLists.txt'), 'w') as f:
                f.write('add_executable(r cases/store_spell.cpp)\n')
            self.git('add', 'oracle', cwd=self.wt)
            self.git('commit', '-qm', 'cmake', cwd=self.wt)
            out = d.run_batch('b011')
            self.assertEqual(out, 'closed', self._status(d)['exceptions'])
            tree = self.git('ls-tree', '-r', '--name-only', 'p3/wso-b011', cwd=self.wt).stdout
            cmake = self.git('show', 'p3/wso-b011:oracle/CMakeLists.txt', cwd=self.wt).stdout
            # a failing case never stays in the oracle suite: the file and its wiring are put back
            self.assertEqual('oracle/cases/wave-b011.cpp' in tree, status == 'pass')
            self.assertEqual('cases/wave-b011.cpp' in cmake, status == 'pass')

    def test_driver_resume_keeps_a_committed_probe_file(self):
        def real(status):
            def oracle_run(batch_id, worktree, cases):
                if status == 'error':
                    raise RuntimeError('windows is gone')
                def run(cmd, **kw):
                    path = os.path.join(rd, 'r.json')
                    with open(path, 'w') as f:
                        json.dump({'cases': [{'case': 'wave/b011/401000', 'iterations': 36,
                                              'failure_count': 0}]}, f)
                    return subprocess.CompletedProcess(cmd, 0, 'report: %s\n' % path, '')
                return truth.run_oracle(batch_id, worktree, cases, build=lambda w: None, run=run)
            return oracle_run
        rd = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, rd, True)
        d = self._measured_driver(oracle_run=real('pass'))
        os.makedirs(os.path.join(self.wt, 'oracle'), exist_ok=True)
        with open(os.path.join(self.wt, 'oracle', 'CMakeLists.txt'), 'w') as f:
            f.write('add_executable(r cases/store_spell.cpp)\n')
        self.git('add', 'oracle', cwd=self.wt)
        self.git('commit', '-qm', 'cmake', cwd=self.wt)
        batch = self._state()
        brief = d._oracle_phase('b011', batch, self.wt)           # the pass, committed, then the park
        self.assertIn('probe-b011-401000', brief)
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')
        d.oracle_run = real('error')                              # the resume: the oracle fails
        brief = d._oracle_phase('b011', batch, self.wt)
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')
        self.assertTrue(os.path.isfile(os.path.join(self.wt, 'oracle', 'cases', 'wave-b011.cpp')))
        with open(os.path.join(self.wt, 'oracle', 'CMakeLists.txt')) as f:
            self.assertIn('cases/wave-b011.cpp', f.read())
        self.assertEqual(d._probe, {'401000': ('ours', 'probe-b011-401000')})   # carried forward
        self.assertIn('probe-b011-401000', brief)
        # the settle step itself never deletes a tracked file
        d._settle_oracle_files('b011', self.wt, [{'address': '0x401000', 'name': 'Rules::Render',
                                                   'signature': 'int Rules::Render(int a, int b)'}], ())
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')
        self.assertTrue(os.path.isfile(os.path.join(self.wt, 'oracle', 'cases', 'wave-b011.cpp')))

    def test_driver_fail_finding_quotes_the_case(self):
        briefs = []

        class Worker(driver.FakeWorker):
            def run(self, brief, cwd):
                briefs.append(brief)
                return super().run(brief, cwd)
        d = self._measured_driver(worker=Worker(self.REPORT),
                                  oracle_run=lambda b, w, c: {'0x401000': ('fail', 'failure_count 2')})
        d.run_batch('b011')
        self.assertIn('wave/b011/401000', briefs[0])
        self.assertIn('Original_401000', briefs[0])               # the drafted source
        self.assertNotIn('is wave-b011.cpp', briefs[0])

    def test_mechanical_checks_want_corroborated_for_a_probe_case(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        batch = self._state()
        batch = dict(batch, branch='p3/wso-b011', id='b011')
        ok = lambda argv, **_: subprocess.CompletedProcess(argv, 0, '', '')
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            # 401000 says "disagreed", 402000 says "corroborated"
            probs = driver.mechanical_checks(batch, self.wt, {
                '0x401000': ('ours', 'probe-b011-401000'), '0x402000': ('ours', 'probe-b011-402000')}, run=ok)
            self.assertEqual([p for p in probs if 'corroborated' in p],
                             [p for p in probs if p.startswith('docs/evidence/401000.md')])
            self.assertEqual(len([p for p in probs if 'probe' in p]), 1)
            probs = driver.mechanical_checks(batch, self.wt, {
                '0x401000': ('ours', 'dump-401000'), '0x402000': ('ours', 'probe-b011-402000')}, run=ok)
            self.assertEqual(probs, [])

    def test_driver_skips_the_oracle_without_a_pure_row(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT),
                         oracle_run=lambda *a: self.fail('no pure row: nothing to build'))
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertNotIn('measured', self._state()['verdicts']['0x401000'])

    def test_driver_oracle_error_does_not_stop_the_batch(self):
        def boom(*a):
            raise RuntimeError('no windows here')
        d = self._measured_driver(oracle_run=boom)
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertIn('oracle path skipped', '\n'.join(self.logs))
        self.assertNotIn('measured', self._state()['verdicts']['0x401000'])

    def test_driver_frida_records_equal_and_flags_different(self):
        trace = TruthTest._trace
        for ours_ret, report, outcome in (
                (3, 'verdict 0x401000 ours case dump-401000\nverdict 0x402000 ours case dump-402000\n', 'equal'),
                (4, 'verdict 0x401000 ours case dump-401000\nverdict 0x402000 ours case dump-402000\n', 'different')):
            seen = []

            def frida_run(batch_id, table, scenarios, ours_ret=ours_ret):
                seen.append((batch_id, [h['name'] for h in table['original']['hooks']], list(scenarios)))
                return {'world_entry': (trace(('fn_402000', 7, 3)), trace(('fn_402000', 7, ours_ret)))}
            map_path = os.path.join(tempfile.mkdtemp(), 'iwd2r.map')
            self.addCleanup(shutil.rmtree, os.path.dirname(map_path), True)
            with open(map_path, 'w') as f:
                f.write(TruthTest.MAP)
            d = self._measured_driver(worker=_SeqWorker('# r\n\n## Verdicts\n\n' + report),
                                      frida=True, frida_map=map_path, frida_run=frida_run,
                                      oracle_run=lambda *a: {'0x401000': ('pass', 'ok')})
            d.run_batch('b011')
            self.assertEqual(seen[0][1], ['fn_402000'])
            self.assertEqual(seen[0][0], 'b011')
            b = self._state()
            if outcome == 'equal':
                self.assertEqual(b['status'], 'closed')
                self.assertEqual(b['verdicts']['0x402000']['measured'], 'frida')
                self.assertEqual(b['verdicts']['0x401000']['measured'], 'probe')
            else:   # different while the worker keeps ours: one fix round (the finding), Frida ran once,
                # the verdict stays unmeasured and the ledger says so; no exception
                self.assertEqual(b['status'], 'closed')
                self.assertEqual(len(seen), 1)
                self.assertNotIn('measured', b['verdicts']['0x402000'])
                with open(os.path.join(os.path.dirname(d.status_path), 'b011', 'worker-fix-checks.brief.md')) as f:
                    self.assertIn('Frida trace diff shows ours different from the original', f.read())
                self.assertIn('- frida different, kept ours: 0x402000 differs in world_entry', self._ledger(d))

    def test_driver_frida_runs_once_even_with_a_fix_round(self):
        calls = []
        map_path = os.path.join(tempfile.mkdtemp(), 'iwd2r.map')
        self.addCleanup(shutil.rmtree, os.path.dirname(map_path), True)
        with open(map_path, 'w') as f:
            f.write(TruthTest.MAP)

        def frida_run(batch_id, table, scenarios):
            calls.append(1)
            t = TruthTest._trace(('fn_402000', 7, 3))
            return {'world_entry': (t, t)}
        bad = '# r\n\n## Verdicts\n\nverdict 0x401000 ours case dump-401000\n'   # 0x402000 missing: a problem
        good = bad + 'verdict 0x402000 ours case dump-402000\n'
        d = self._measured_driver(worker=_SeqWorker(bad, good), frida=True, frida_map=map_path,
                                  frida_run=frida_run, oracle_run=lambda *a: {})
        self.assertEqual(d.run_batch('b011'), 'closed', self._status(d)['exceptions'])
        self.assertEqual(calls, [1])
        self.assertEqual(self._state()['verdicts']['0x402000']['measured'], 'frida')

    def test_driver_repeats_oracle_failures_on_a_resumed_brief(self):
        d = self._measured_driver(oracle_run=lambda b, w, c: {'0x401000': ('fail', 'failure_count 2')})
        brief = d._oracle_phase('b011', self._state(), self.wt)
        self.assertIn('FAILED', brief)
        saved = self._status(d)['batches']['b011']['oracle_findings']
        self.assertTrue(any('401000' in l for l in saved))
        d._finish('b011', 'parked')                                  # the park keeps them
        self.assertTrue(self._status(d)['batches']['b011']['oracle_findings'])
        d2 = self._measured_driver(oracle_run=lambda *a: self.fail('resume: no oracle run'))
        d2.status_path = d.status_path
        # the resumed driver reads the same status file; the oracle phase restates the finding
        d2.oracle_run = lambda b, w, c: {}
        resumed = d2._oracle_phase('b011', self._state(), self.wt)
        self.assertIn('Oracle findings from the earlier run', resumed)
        self.assertIn('401000', resumed)

    def test_driver_frida_off_by_default_and_read_from_the_calibration(self):
        d = self._measured_driver(frida_run=lambda *a: self.fail('--no-frida'),
                                  oracle_run=lambda *a: {})
        self.assertFalse(d.frida)
        out = d.run_batch('b011')
        self.assertEqual(out, 'closed', self._status(d)['exceptions'])
        cal = os.path.join(self.tmp, 'cal.json')
        self.assertFalse(truth.frida_default(cal))            # missing file: off
        with open(cal, 'w') as f:
            json.dump({'frida_default': True}, f)
        self.assertTrue(truth.frida_default(cal))
        with open(cal, 'w') as f:
            f.write('{broken')
        self.assertFalse(truth.frida_default(cal))
        repo = os.path.join(self.tmp, 'r2')
        os.makedirs(os.path.join(repo, 'docs', 'wso'))
        args, _ = driver._parse([])
        self.assertIsNone(args.frida)
        self.assertTrue(driver._parse(['--frida'])[0].frida)
        self.assertFalse(driver._parse(['--no-frida'])[0].frida)
        built, err = driver._build(args, repo)
        self.assertIsNone(err)
        self.assertFalse(built.frida)                         # no calibration file under repo: off
        with open(os.path.join(repo, 'docs', 'wso', 'truth-calibration.json'), 'w') as f:
            json.dump({'frida_default': True}, f)
        self.assertTrue(driver._build(args, repo)[0].frida)

    def test_run_batch_happy_path_closes(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))   # fake wave functions injected
        first = self.git('rev-parse', 'first').stdout.strip()
        self.assertEqual(d.run_batch('b011'), 'closed')
        with open(d.ledger_path) as f:
            self.assertIn('## driver b011', f.read())
        b = self._state()
        self.assertEqual(b['status'], 'closed')
        self.assertEqual(b['verdicts']['0x402000']['side'], 'fork')
        self.assertEqual(b['verdicts']['0x401000']['case'], 'dump-401000')
        self.assertEqual(self.git('rev-parse', 'first').stdout.strip(),
                         self.git('rev-parse', 'p3/wso-b011').stdout.strip())
        self.assertEqual(self.pushes, [1])
        self.assertEqual(self.sent, [])
        ledger = self._ledger(d)
        self.assertIn('commits %s..' % first[:8], ledger)
        self.assertIn('verdicts: fork 1, ours 2, neither 0', ledger)
        self.assertIn('review: yes, fork/neither rows (seed 2026-10-22): clean', ledger)
        self.assertIn('credits unknown', ledger)
        self.assertNotIn('ignored for', ledger)
        # Bundles go to <driver folder>/bundles/<id>; results to <driver folder>/<id>.
        folder = os.path.dirname(d.status_path)
        self.assertEqual(self.bundle_dirs, [os.path.join(folder, 'bundles', 'b011')])
        with open(os.path.join(folder, 'b011', 'worker.report.md')) as f:
            self.assertEqual(f.read(), self.REPORT)
        with open(os.path.join(folder, 'b011', 'worker.json')) as f:
            self.assertEqual(json.load(f)['report'], self.REPORT)
        self.assertTrue(os.path.isfile(os.path.join(folder, 'b011', 'review.diff')))
        status = self._status(d)
        self.assertEqual(status['exceptions'], {})
        self.assertEqual(status['batches']['b011']['outcome'], 'closed')
        self.assertIsNone(status['current'])
        with open(d.report_path) as f:
            self.assertIn('## Scorecard', f.read())

    def test_run_batch_second_check_failure_is_exception(self):
        d = self._driver(worker=driver.FakeWorker('# report\n## Verdicts\n'))  # no verdicts, twice
        self.assertEqual(d.run_batch('b011'), 'exception')
        with open(d.status_path) as f:
            self.assertIn('b011', json.load(f)['exceptions'])
        exc = self._status(d)['exceptions']['b011']
        self.assertEqual(exc['stage'], 'checks')
        self.assertTrue(any(p.startswith('0x401000: no verdict line') for p in exc['problems']))
        self.assertTrue(exc['at'])
        self.assertEqual(self._state()['status'], 'assigned')
        self.assertEqual(self.pushes, [])
        self.assertEqual(len(self.sent), 1)
        self.assertIn('b011', self.sent[0])
        self.assertNotIn('\n', self.sent[0])
        ledger = self._ledger(d).strip().splitlines()
        self.assertEqual(len(ledger), 1)
        self.assertIn('b011', ledger[0])
        self.assertIn('exception at checks', ledger[0])

    def test_check_fix_round_gets_problems_then_closes(self):
        w = _SeqWorker('# report\n## Verdicts\n', self.REPORT)
        d = self._driver(worker=w)
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertEqual(len(w.briefs), 2)
        self.assertEqual(w.cwds, [self.wt, self.wt])
        self.assertEqual(w.briefs[0], 'BRIEF b011')
        self.assertIn('0x401000: no verdict line', w.briefs[1])
        self.assertIn('BRIEF b011', w.briefs[1])

    def test_malformed_report_is_a_check_failure(self):
        w = _SeqWorker('## Verdicts\nI think it is ours.\n', self.REPORT)
        d = self._driver(worker=w)
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertIn('malformed verdict line', w.briefs[1])

    def test_worker_without_report_is_exception(self):
        w = _SeqWorker('')
        d = self._driver(worker=w)
        self.assertEqual(d.run_batch('b011'), 'exception')
        self.assertEqual(len(w.briefs), 1)
        exc = self._status(d)['exceptions']['b011']
        self.assertEqual(exc['stage'], 'worker')
        self.assertIn('without a report', exc['problems'][0])
        self.assertEqual(len(self.sent), 1)

    def test_second_blocking_review_is_exception(self):
        w = _SeqWorker(self.REPORT)
        r = _SeqWorker('# review\n\nBLOCKING: 0x402000 the Imported-From trailer is missing\n',
                       '- BLOCKING: 0x402000 still no trailer\n')
        d = self._driver(worker=w, reviewer=r)
        self.assertEqual(d.run_batch('b011'), 'exception')
        exc = self._status(d)['exceptions']['b011']
        self.assertEqual(exc['stage'], 'review')
        self.assertIn('BLOCKING: 0x402000 still no trailer', exc['problems'])
        # One fix round, to the batch's worker, with the findings as its prompt.
        self.assertEqual(len(w.briefs), 2)
        self.assertIn('BLOCKING: 0x402000 the Imported-From trailer is missing', w.briefs[1])
        self.assertEqual(len(r.briefs), 2)
        diff = os.path.join(os.path.dirname(d.status_path), 'b011', 'review.diff')
        self.assertIn(diff, r.briefs[0])
        with open(diff) as f:
            self.assertIn('probe-1 decides', f.read())
        self.assertEqual(self._state()['status'], 'assigned')
        self.assertEqual(self.pushes, [])
        self.assertEqual(len(self.sent), 1)

    def test_blocking_then_clean_review_closes(self):
        w = _SeqWorker(self.REPORT, credits=10.25)
        r = _SeqWorker('BLOCKING: fix the trailer\n', self.CLEAN_REVIEW, credits=1.0)
        d = self._driver(worker=w, reviewer=r)
        self.assertEqual(d.run_batch('b011'), 'closed')
        ledger = self._ledger(d)
        self.assertIn('clean after one fix round', ledger)
        self.assertIn('credits 22', ledger)   # 2 worker runs + 2 reviews

    def test_review_line_is_seeded(self):
        """The same seed and batch id draw the same review answer, so the
        ledger line is reproducible."""
        lines = []
        for _ in range(2):
            r = _SeqWorker(self.CLEAN_REVIEW)
            d = self._driver(worker=driver.FakeWorker(self.ALL_OURS), reviewer=r)
            self.assertEqual(d.run_batch('b011'), 'closed')
            line = [l for l in self._ledger(d).splitlines() if l.startswith('- review:')]
            lines.append(line)
            needed, _ = driver.review_needed('b011', {'0x401000': ('ours', 'a')}, '2026-10-22')
            self.assertEqual(len(r.briefs), 1 if needed else 0)
        self.assertEqual(len(lines[0]), 1)
        self.assertEqual(lines[0], lines[1])
        self.assertIn('seed 2026-10-22', lines[0][0])

    def test_rate_limited_worker_parks(self):
        d = self._driver(worker=_SeqWorker('', rate_limited=True))
        self.assertEqual(d.run_batch('b011'), 'parked')
        self.assertEqual(list(self._status(d)['pools']['glm']['parked']), ['b011'])
        self.assertEqual(self.sent, [])
        self.assertEqual(self._state()['status'], 'assigned')

    def test_push_failure_stops_the_run(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        d.push = lambda: 'rejected'
        d.next_batch = lambda *a, **kw: (0, self.CUT)
        self.assertEqual(d.run(), 1)
        self.assertEqual(self._state()['status'], 'closed')
        exc = self._status(d)['exceptions']['b011']
        self.assertEqual(exc['stage'], 'push')
        self.assertIn('rejected', exc['problems'][0])
        self.assertIn('## driver b011', self._ledger(d))

    def test_run_loops_until_nothing_left(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._state()['status'], 'closed')
        # ours-stub rows that triage moved out are still counted.
        self.assertIn('rows by class: rename-only 1, ours-stub 2, ambiguous 2', self._ledger(d))

    def test_run_continues_after_exception_and_exits_3_on_park(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        cuts = iter([(0, 'b011 assigned: x\n'), (0, 'b012 assigned: x\n'),
                     (0, 'b013 assigned: x\n'), (0, 'b014 assigned: x\n')])
        d.next_batch = lambda *a, **kw: next(cuts)
        outcomes = {'b011': 'exception', 'b012': 'closed', 'b013': 'parked'}
        ran = []
        d.run_batch = lambda bid: ran.append(bid) or outcomes[bid]
        self.assertEqual(d.run(), 3)
        self.assertEqual(ran, ['b011', 'b012', 'b013'])

    def test_cut_failure_is_an_exception_and_stops(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        d.next_batch = lambda *a, **kw: (1, 'b013 not assigned: git worktree add failed\n')
        d.run_batch = lambda bid: self.fail('nothing was cut')
        self.assertEqual(d.run(), 1)
        self.assertEqual(self._status(d)['exceptions']['b013']['stage'], 'cut')
        self.assertEqual(len(self.sent), 1)

    def test_run_retries_a_cleared_exception_first(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        os.makedirs(os.path.dirname(d.status_path))
        with open(d.status_path, 'w') as f:
            json.dump({'exceptions': {}, 'batches': {'b011': {'outcome': 'exception'}}}, f)
        d = self._reload(d)
        ran = []
        d.run_batch = lambda bid, resume=None: ran.append(bid) or 'closed'
        d.next_batch = lambda *a, **kw: self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(ran, ['b011'])
        # Not while the exception is still in the status file.
        with open(d.status_path, 'w') as f:
            json.dump({'exceptions': {'b011': {'stage': 'checks'}},
                       'batches': {'b011': {'outcome': 'exception'}}}, f)
        d = self._reload(d)
        ran.clear()
        d.run_batch = lambda bid, resume=None: ran.append(bid) or 'closed'
        d.next_batch = lambda *a, **kw: self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(ran, [])

    def _reload(self, d):
        """A fresh Driver over the same paths, reading the status file again."""
        n = driver.Driver(d.repo, d.state_path, d.worker, d.reviewer, d.ledger_path,
                          d.status_path, push=d.push, send=d.send, gate=False,
                          log=self.logs.append)
        n.render_brief, n.mechanical_checks = d.render_brief, d.mechanical_checks
        n.report_path, n.triage_path = d.report_path, d.triage_path
        return n

    def test_dry_report(self):
        batch = {'rows': [{'address': '0x401000', 'class': 'ambiguous'},
                          {'address': '0x402000', 'class': 'rename-only'},
                          {'address': '0x403000'}, {'address': '0x404000'}],
                 'verdicts': {'0x402000': {}, '0x404000': {}}}
        self.assertEqual(driver.parse_report(driver.dry_report(batch)),
                         {'0x401000': ('ours', 'dry-0x401000'),
                          '0x403000': ('ours', 'dry-0x403000')})

    def _dry(self, phrases):
        d = self._driver(worker=None, phrases=phrases, dry_run=True)
        cuts = iter([(0, self.CUT)])
        d.next_batch = lambda *a, **kw: next(cuts)
        d.close_batch = lambda *a, **kw: self.fail('a dry run never closes')
        d.record_verdict = lambda *a, **kw: self.fail('a dry run records nothing')
        d.push = lambda: self.fail('a dry run never pushes')
        d.send = lambda line: self.fail('a dry run never sends')
        return d

    def test_dry_run_one_batch_no_close(self):
        d = self._dry(phrases=True)
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._state()['status'], 'assigned')
        self.assertEqual(set(self._state()['verdicts']), {'0x403000'})
        folder = os.path.dirname(d.status_path)
        with open(os.path.join(folder, 'b011', 'worker.report.md')) as f:
            report = f.read()
        self.assertEqual(driver.parse_report(report),
                         {'0x401000': ('ours', 'dry-0x401000'),
                          '0x402000': ('ours', 'dry-0x402000')})
        ledger = self._ledger(d)
        self.assertIn('## driver b011', ledger)
        self.assertIn('verdicts: fork 0, ours 3, neither 0', ledger)
        self.assertIn('dry run', ledger)
        status = self._status(d)
        self.assertEqual(status['exceptions'], {})
        self.assertTrue(status['dry_run'])

    def test_dry_run_check_failure_writes_exception_but_exits_0(self):
        d = self._dry(phrases=False)
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'checks')
        ledger = self._ledger(d)
        self.assertIn('exception at checks', ledger)
        self.assertIn('## driver b011', ledger)
        self.assertEqual(self._state()['status'], 'assigned')

    def test_default_push_and_send(self):
        d = self._driver(worker=None)
        d = driver.Driver(d.repo, d.state_path, None, None, d.ledger_path, d.status_path)
        calls = []

        def stub(argv, **kw):
            calls.append(argv)
            return subprocess.CompletedProcess(argv, 0, '', '')
        with mock.patch.object(driver.subprocess, 'run', stub):
            self.assertIsNone(d.push())
            d.send('b011 exception')      # no aoe_session: nothing is sent
            d.aoe_session = 'sess-1'
            d.send('b011 exception')
        self.assertEqual(calls, [['git', '-C', d.repo, 'push', 'iwd2r', 'first:main', 'first:first'],
                                 ['aoe', 'send', 'sess-1', 'b011 exception']])

    def test_driver_from_args(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(driver.main(['--dry-run'], repo=tmp), 2)
        self.assertIn('--state', err.getvalue())
        state = os.path.join(tmp, 'state.json')
        folder = os.path.join(tmp, '.superpowers', 'sdd', '2026-10-17-wso-integration', 'driver')
        d, e = driver.driver_from_args(['--dry-run', '--state', state], repo=tmp)
        self.assertIsNone(e)
        self.assertTrue(d.dry_run)
        self.assertEqual(d.ledger_path, os.path.join(folder, 'dry-run', 'progress.md'))
        self.assertEqual(d.status_path, os.path.join(folder, 'dry-run', 'status.json'))
        self.assertEqual(d.state_path, state)
        d, e = driver.driver_from_args(['--workers', '2', '--budget', '4000', '--seed', 's',
                                        '--state', state, '--aoe-session', 'S1'], repo=tmp)
        self.assertIsNone(e)
        self.assertFalse(d.dry_run)
        self.assertIsInstance(d.worker, driver.HeadlessWorker)
        self.assertIsInstance(d.reviewer, driver.HeadlessWorker)
        self.assertEqual((d.workers, d.budget, d.seed, d.aoe_session), (2, 4000.0, 's', 'S1'))
        self.assertEqual(d.ledger_path, os.path.join(
            tmp, '.superpowers', 'sdd', '2026-10-17-wso-integration', 'progress.md'))
        self.assertEqual(d.status_path, os.path.join(folder, 'status.json'))
        self.assertTrue(d.gate)

    def test_wave_run_delegates_and_refuses_dry_run_without_state(self):
        wave = load_tool('wave')
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(wave.main(['run', '--dry-run']), 2)
        self.assertIn('--state', err.getvalue())

    def test_worker_line_never_overwrites_a_triage_verdict(self):
        """R33: a worker line for a triage-decided row is ignored: not
        recorded, no fix round, no review, and named in the ledger entry."""
        r = _SeqWorker(self.CLEAN_REVIEW)
        d = self._driver(worker=_SeqWorker(
            self.ALL_OURS + 'verdict 0x403000 fork case probe-x\n'), reviewer=r)
        with open(self.state_path) as f:
            state = json.load(f)
        b = state['batches']['b011']
        b['rows'][2]['class'] = 'fork-stub'
        auto = {'side': 'ours', 'case': 'triage-stub', 'losing_kind': 'trace'}
        b['verdicts']['0x403000'] = auto
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        self.assertEqual(d.run_batch('b011'), 'closed')
        self.assertEqual(self._state()['verdicts']['0x403000'], auto)
        self.assertEqual(len(d.worker.briefs), 1)
        # b011 is not sampled under this seed, so only the fork line could
        # have asked for a review; it did not.
        self.assertFalse(driver.review_needed('b011', {'0x1': ('ours', 'a')}, '2026-10-22')[0])
        self.assertEqual(r.briefs, [])
        ledger = self._ledger(d)
        self.assertIn('- ignored for triage-decided rows: 0x403000 fork\n', ledger)
        self.assertIn('verdicts: fork 0, ours 3, neither 0', ledger)

    def test_status_writes_keep_a_cleared_exception_cleared(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        with open(self.state_path) as f:
            state = json.load(f)
        state['batches']['b012'] = dict(state['batches']['b011'],
                                        branch='p3/wso-b012', verdicts={})
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        self.assertEqual(d.run_batch('b012'), 'exception')   # it has no worktree
        status = self._status(d)
        self.assertEqual(status['exceptions']['b012']['stage'], 'worktree')
        del status['exceptions']['b012']                     # the coordinator clears it
        with open(d.status_path, 'w') as f:
            json.dump(status, f)
        self.assertEqual(d.run_batch('b011'), 'closed')
        status = self._status(d)
        self.assertNotIn('b012', status['exceptions'])
        self.assertEqual(status['batches']['b011']['outcome'], 'closed')

    def test_run_reads_cleared_exceptions_from_disk(self):
        w = _SeqWorker('', self.REPORT)
        d = self._driver(worker=w)
        self.assertEqual(d.run_batch('b011'), 'exception')
        status = self._status(d)
        status['exceptions'] = {}
        with open(d.status_path, 'w') as f:
            json.dump(status, f)
        d.next_batch = lambda *a, **kw: self.NOTHING
        self.assertEqual(d.run(), 0)                 # the same Driver retries it
        self.assertEqual(self._state()['status'], 'closed')
        self.assertEqual(len(w.briefs), 2)

    def test_stalled_run_exits_4_and_a_cut_clears_it(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        msg = 'nothing left to assign: 3 rows wait on files owned by assigned batches or claimed by streams\n'
        d.next_batch = lambda *a, **kw: (2, msg)
        self.assertEqual(d.run(), 4)
        stalled = self._status(d)['stalled']
        self.assertEqual(stalled['message'], msg.strip())
        self.assertTrue(stalled['at'])
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        d.run_batch = lambda bid: 'closed'
        self.assertEqual(d.run(), 0)
        self.assertIsNone(self._status(d)['stalled'])

    def test_dry_run_refuses_the_live_state(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        live = driver._tool('wave').STATE_PATH
        link = os.path.join(tmp, 'link.json')
        os.symlink(live, link)   # resolves to the live file: refused too
        own = os.path.join(tmp, 'docs', 'wso', 'wave-state.json')
        for path in (live, link, own):
            d, err = driver.driver_from_args(['--dry-run', '--state', path], repo=tmp)
            self.assertIsNone(d)
            self.assertEqual(err[0], 2)
            self.assertIn('live wave-state file', err[1])

    # -- budget, 429 park, resume, --clear (Task 9) -------------------------

    # The identity the driver's WIP commit runs under in these temp repos.
    GIT_ENV = {'GIT_AUTHOR_NAME': 't', 'GIT_AUTHOR_EMAIL': 't@t',
               'GIT_COMMITTER_NAME': 't', 'GIT_COMMITTER_EMAIL': 't@t',
               'GIT_CONFIG_GLOBAL': os.devnull, 'GIT_CONFIG_NOSYSTEM': '1'}

    def _write_status(self, d, status):
        os.makedirs(os.path.dirname(d.status_path), exist_ok=True)
        with open(d.status_path, 'w') as f:
            json.dump(status, f)

    def _rev(self, ref, cwd=None):
        return self.git('rev-parse', ref, cwd=cwd).stdout.strip()

    # Transcript lines in Claude Code's shapes (read as data only).
    @staticmethod
    def _turn(text='working'):
        return {'type': 'assistant', 'message': {
            'model': 'glm-5.3-flash[1m]', 'usage': {'output_tokens': 10000},
            'content': [{'type': 'text', 'text': text}]}}

    @staticmethod
    def _api_error(text='API Error: Request rejected (429) · [1308][Usage limit '
                        'reached for 5 hour.]', error='rate_limit', status=429):
        entry = {'type': 'assistant', 'isApiErrorMessage': True,
                 'message': {'model': '<synthetic>',
                             'content': [{'type': 'text', 'text': text}]}}
        if error is not None:
            entry['error'] = error
        if status is not None:
            entry['apiErrorStatus'] = status
        return entry

    def test_headless_worker_flags_a_429(self):
        """R41, R44: rate_limited only from an error final result (is_error,
        or text starting `API Error`) with the 429 text, or from the
        transcript's last API-error entry (apiErrorStatus 429 or error
        rate_limit, its text as fallback) when no successful turn follows
        it; never from tool content, file text or prose."""
        phrase = 'Request rejected (429)'
        noise = [
            {'type': 'assistant', 'message': {'model': 'glm-5.3-flash[1m]', 'content': [
                {'type': 'tool_use', 'id': 't1', 'name': 'Bash',
                 'input': {'command': "grep -n '%s' tools/driver.py" % phrase}}]}},
            {'type': 'user', 'message': {'role': 'user', 'content': [
                {'type': 'tool_result', 'tool_use_id': 't1',
                 'content': "27:RATE_LIMIT_TEXT = '%s'" % phrase}]}},
            {'type': 'queue-operation', 'content': 'API Error: ' + phrase},
            self._turn('The driver parks on %s; nothing to do here.' % phrase),
        ]
        cases = [   # (name, transcript lines, exit, JSON result, rate limited)
            ('noise', noise, 0, {'result': '# report\n'}, False),
            ('retried', [self._turn(), self._api_error(), self._turn(), self._turn()],
             0, {'result': '# report\n'}, False),
            ('final', [self._turn(), self._api_error()], 1,
             {'is_error': True, 'result': 'API Error: %s · limit reached' % phrase}, True),
            ('result-only', None, 0, {'result': 'API Error: %s' % phrase}, True),
            ('quoted', [self._turn()], 0, {'is_error': False, 'result':
                '# report\n\nThe driver parks on `%s`; this batch did not.\n' % phrase},
             False),
            ('last-entry', [self._turn(), self._api_error()], 1,
             {'is_error': True, 'result': ''}, True),
            ('status-key', [self._turn(), self._api_error(
                'API Error: limit reached', error=None, status=429)], 1,
             {'is_error': True, 'result': ''}, True),
            ('text-fallback', [self._turn(), self._api_error(error=None, status=None)], 1,
             {'is_error': True, 'result': ''}, True),
            ('other-error-last', [self._api_error(), self._turn(), self._api_error(
                'API Error: 500 internal error', error='api_error', status=500)], 1,
             {'is_error': True, 'result': ''}, False),
            ('calm', [self._turn()], 0, {'result': '# report 429\n'}, False),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            cwd = os.path.join(tmp, 'wt')
            os.mkdir(cwd)
            proj = os.path.join(tmp, 'cfg', 'projects', driver.project_slug(cwd))
            os.makedirs(proj)
            w = driver.HeadlessWorker(config_dir=os.path.join(tmp, 'cfg'))
            for name, lines, rc, result, expected in cases:
                if lines is not None:
                    with open(os.path.join(proj, name + '.jsonl'), 'w') as f:
                        f.write(''.join(json.dumps(x) + '\n' for x in lines))
                out = json.dumps(dict(result, session_id=name))
                w._run = lambda argv, **kw: subprocess.CompletedProcess(argv, rc, out, '')
                r = w.run('B', cwd)
                self.assertEqual(r.rate_limited, expected, name)
                self.assertEqual(r.transcript is None, lines is None, name)
            self.assertAlmostEqual(driver.credits_of(os.path.join(proj, 'final.jsonl')), 8.0)

    def test_park_on_rate_limit_commits_wip_and_exit_3(self):
        w = _DirtyWorker('API Error: Request rejected (429)', rate_limited=True)
        d = self._driver(worker=w)
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
        first, tip = self._rev('first'), self._rev('p3/wso-b011')
        root_head = self._rev('HEAD')
        root_dirt = self.git('status', '--porcelain').stdout
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 3)
        self.assertEqual(len(cuts), 1)            # nothing cut after the park
        # The WIP commit is on the batch branch, made in the batch worktree.
        self.assertEqual(self._rev('p3/wso-b011^'), tip)
        self.assertEqual(self.git('log', '-1', '--format=%s', 'p3/wso-b011').stdout.strip(),
                         'WIP: parked by driver')
        files = self.git('show', '--name-only', '--format=', 'p3/wso-b011').stdout.split()
        self.assertEqual(sorted(files), ['docs/evidence/401000.md', 'scratch.txt'])
        self.assertEqual(self.git('status', '--porcelain', cwd=self.wt).stdout, '')
        # Never in the driver's own worktree.
        self.assertEqual((self._rev('first'), self._rev('HEAD')), (first, root_head))
        self.assertEqual(self.git('status', '--porcelain').stdout, root_dirt)
        status = self._status(d)
        parked = status['pools']['glm']['parked']
        self.assertEqual(list(parked), ['b011'])
        self.assertEqual(set(parked['b011']), {'at', 'reason'})
        self.assertIn('rate limited', parked['b011']['reason'])
        self.assertTrue(parked['b011']['at'])
        self.assertIsNone(status['current'])
        self.assertEqual(status['batches']['b011']['outcome'], 'parked')
        self.assertEqual(self._state()['status'], 'assigned')
        self.assertEqual(self.sent, [])
        # The resume may undo only that WIP commit (R44): its own, unpushed.
        wip = self._rev('p3/wso-b011')
        self.assertEqual(status['batches']['b011']['wip'], wip)
        preamble = d._resume_preamble('b011', self._state(), 'was parked')
        self.assertIn('The branch tip, %s, is the driver\'s own unpushed' % wip[:8], preamble)
        self.assertIn('`git reset --soft HEAD~1` once', preamble)
        # A clean worktree parks without a commit.
        d.worker = _SeqWorker('', rate_limited=True)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run_batch('b011'), 'parked')
        self.assertEqual(self._rev('p3/wso-b011'), wip)

    def test_resume_picks_parked_batch_first(self):
        order = []
        w = _SeqWorker(self.REPORT)
        run = w.run
        w.run = lambda brief, cwd: order.append('worker') or run(brief, cwd)
        d = self._driver(worker=w)
        self._write_status(d, {
            'parked': {'batch_id': 'b011', 'at': '2026-10-23T01:00:00+00:00',
                       'reason': 'worker rate limited'},
            'batches': {'b011': {'outcome': 'parked'}}})
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['worker', 'cut'])
        brief = w.briefs[0]
        self.assertTrue(brief.startswith('Resume:'))
        self.assertIn('worker rate limited', brief)
        self.assertNotIn('git reset', brief)        # the tip is no WIP commit
        self.assertIn('- verdict 0x403000 ours case triage-rename', brief)
        self.assertTrue(brief.rstrip().endswith('BRIEF b011'))   # re-rendered, after the preamble
        self.assertEqual(len(self.bundle_dirs), 1)
        status = self._status(d)
        self.assertIsNone(status['parked'])
        self.assertIsNone(status['current'])
        self.assertEqual(status['batches']['b011']['outcome'], 'closed')
        self.assertEqual(self._state()['status'], 'closed')
        # A parked batch that is no longer assigned is not run; parked clears.
        self._write_status(d, {'parked': {'batch_id': 'b011', 'at': 't', 'reason': 'r'}})
        order.clear()
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['cut'])
        self.assertIsNone(self._status(d)['parked'])

    def test_resume_interrupted_current(self):
        """A stale status['current'] (the process died mid-batch) naming an
        assigned batch takes the resume path ahead of new cuts."""
        order = []
        w = _SeqWorker(self.REPORT)
        run = w.run
        w.run = lambda brief, cwd: order.append('worker') or run(brief, cwd)
        d = self._driver(worker=w)
        self._write_status(d, {'current': {'batch': 'b011', 'stage': 'worker',
                                           'at': '2026-10-23T01:00:00+00:00'}})
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['worker', 'cut'])
        self.assertTrue(w.briefs[0].startswith('Resume:'))
        self.assertIn('interrupted', w.briefs[0])
        self.assertIn('worker stage', w.briefs[0])
        self.assertIn('- verdict 0x403000 ours case triage-rename', w.briefs[0])
        self.assertEqual(self._state()['status'], 'closed')
        self.assertIsNone(self._status(d)['current'])

    def test_resume_closed_current_writes_ledger_and_pushes_first(self):
        """current names a batch already closed with no ledger entry (a
        crash between close and the ledger): the entry, then the push,
        before anything else; never a second entry."""
        order = []
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        d.worker.run = lambda *a: self.fail('a closed batch runs no worker')
        with open(self.state_path) as f:
            state = json.load(f)
        b = state['batches']['b011']
        b.update(status='closed', closed_at='2026-10-23T01:00:00+00:00')
        b['verdicts']['0x401000'] = {'side': 'ours', 'case': 'dump-401000', 'losing_kind': 'trace'}
        b['verdicts']['0x402000'] = {'side': 'ours', 'case': 'dump-402000', 'losing_kind': 'trace'}
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        self._write_status(d, {'current': {'batch': 'b011', 'stage': 'close', 'at': 't'}})
        d.push = lambda: order.append('push')
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['push', 'cut'])
        ledger = self._ledger(d)
        self.assertEqual(ledger.count('## driver b011 ('), 1)
        self.assertIn('verdicts: fork 0, ours 3, neither 0', ledger)
        status = self._status(d)
        self.assertIsNone(status['current'])
        self.assertEqual(status['batches']['b011']['outcome'], 'closed')
        # The entry is there already: the push again, no second entry.
        self._write_status(d, dict(status, current={'batch': 'b011', 'stage': 'push', 'at': 't'}))
        order.clear()
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['push', 'cut'])
        self.assertEqual(self._ledger(d).count('## driver b011 ('), 1)
        # A failed push is the push exception: exit 1, nothing cut.
        self._write_status(d, dict(status, current={'batch': 'b011', 'stage': 'push', 'at': 't'}))
        order.clear()
        d.push = lambda: 'rejected'
        self.assertEqual(d.run(), 1)
        self.assertEqual(order, [])
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'push')

    def test_budget_stop_before_next_batch(self):
        # 700 + 100 credits for b011; 800 spent + 800 estimated > 1500.
        d = self._driver(worker=_SeqWorker(self.REPORT, credits=700.0),
                         reviewer=_SeqWorker(self.CLEAN_REVIEW, credits=100.0), budget=1500)
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
        self.assertEqual(d.run(), 0)
        self.assertEqual(len(cuts), 1)            # checked before the second cut
        self.assertEqual(self._state()['status'], 'closed')
        stopped = self._status(d)['stopped']
        self.assertEqual({k: stopped[k] for k in ('reason', 'spent', 'budget')},
                         {'reason': 'budget', 'spent': 800.0, 'budget': 1500})
        self.assertEqual(set(stopped), {'at', 'reason', 'spent', 'budget'})
        # A later run with room clears it.
        d.budget = None
        d.next_batch = lambda *a, **kw: self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertIsNone(self._status(d)['stopped'])
        # Before any batch the estimate is DEFAULT_ESTIMATE (1200).
        self.assertEqual(driver.DEFAULT_ESTIMATE, 1200)
        d = self._driver(worker=driver.FakeWorker(self.REPORT), budget=1000)
        d.next_batch = lambda *a, **kw: self.fail('over budget: nothing is cut')
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._status(d)['stopped']['spent'], 0)
        # A batch with unknown credits counts as the estimate, and a batch
        # over budget is never stopped midway.
        d = self._driver(worker=_SeqWorker(self.REPORT), budget=2000)
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
        self.assertEqual(d.run(), 0)
        self.assertEqual((len(cuts), self._state()['status']), (1, 'closed'))
        self.assertEqual(self._status(d)['stopped']['spent'], 1200)
        # The mean over this run's batches with known credits.
        d._batch_credits = [None, 600.0, 1000.0]
        self.assertEqual((d.estimate_next, d.credits_spent), (800.0, 2400.0))

    def _status_tmp(self, status):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        path = os.path.join(tmp, driver.STATUS_REL)
        os.makedirs(os.path.dirname(path))
        with open(path, 'w') as f:
            f.write(status if isinstance(status, str) else json.dumps(status))
        return tmp, path

    def test_clear_removes_an_exception(self):
        exc = {'stage': 'checks', 'problems': ['0x401000: no verdict line'], 'at': 't'}
        tmp, path = self._status_tmp({
            'exceptions': {'b011': exc, 'b012': {'stage': 'worktree', 'problems': ['x'], 'at': 't'}},
            'batches': {'b011': {'outcome': 'exception'}}, 'current': None})
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(driver.main(['--clear', 'b011'], repo=tmp), 0)
        self.assertIn('b011', out.getvalue())
        self.assertIn('0x401000: no verdict line', out.getvalue())
        with open(path) as f:
            status = json.load(f)
        self.assertEqual(set(status['exceptions']), {'b012'})
        self.assertEqual(status['batches'], {'b011': {'outcome': 'exception'}})
        # It waits for the status lock, as every driver write does.
        with filelock.hold(path + '.lock', 'test'):
            done = []
            t = threading.Thread(target=lambda: done.append(
                driver.clear_exception(path, 'b012')))
            t.start()
            t.join(0.3)
            self.assertTrue(t.is_alive())
            with open(path) as f:
                self.assertIn('b012', json.load(f)['exceptions'])
        t.join(10)
        self.assertEqual(done[0][0], 0)
        with open(path) as f:
            self.assertEqual(json.load(f)['exceptions'], {})

    def test_clear_missing_id_exits_2(self):
        tmp, path = self._status_tmp({'exceptions': {}, 'batches': {}})
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            self.assertEqual(driver.main(['--clear', 'b099'], repo=tmp), 2)
        self.assertIn('no exception for b099', err.getvalue())
        empty = tempfile.mkdtemp()                 # no status file at all
        self.addCleanup(shutil.rmtree, empty, True)
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(driver.main(['--clear', 'b099'], repo=empty), 2)
        self.assertFalse(os.path.exists(os.path.join(empty, '.superpowers')))

    def test_status_writes_take_the_lock(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        d._update_status(lambda s: None)
        with filelock.hold(d.status_path + '.lock', 'test'):
            t = threading.Thread(target=lambda: d._update_status(
                lambda s: s.__setitem__('stalled', {'at': 't', 'message': 'm'})))
            t.start()
            t.join(0.3)
            self.assertTrue(t.is_alive())
            self.assertIsNone(self._status(d)['stalled'])
        t.join(10)
        self.assertEqual(self._status(d)['stalled']['message'], 'm')

    def test_corrupt_status_file_exits_1_with_a_message(self):
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        self._write_status(d, {})
        with open(d.status_path, 'w') as f:
            f.write('{"exceptions": ')              # torn
        d.next_batch = lambda *a, **kw: self.fail('nothing is cut')
        with mock.patch.object(driver.time, 'sleep'):
            self.assertEqual(d.run(), 1)
        self.assertTrue(any(d.status_path in l for l in self.logs))
        # Mid-batch: the file goes bad while the worker runs.
        self._write_status(d, {})
        status_path = d.status_path

        class Tearing(_SeqWorker):
            def run(self, brief, cwd):
                with open(status_path, 'w') as f:
                    f.write('[1, 2')
                return super().run(brief, cwd)
        d.worker = Tearing(self.REPORT)
        d.next_batch = lambda *a, **kw: (0, self.CUT)
        self.logs.clear()
        with mock.patch.object(driver.time, 'sleep'):
            self.assertEqual(d.run(), 1)
        self.assertTrue(any(d.status_path in l for l in self.logs))
        # wave.py run, and --clear, on a corrupt file: exit 1, the file named.
        for argv in (['--state', os.path.join(self.tmp, 'state-copy.json')],
                     ['--clear', 'b011']):
            tmp, path = self._status_tmp('not json')
            err = io.StringIO()
            with contextlib.redirect_stderr(err), mock.patch.object(driver.time, 'sleep'):
                self.assertEqual(driver.main(argv, repo=tmp), 1)
            self.assertIn(path, err.getvalue())

    def test_vanished_worktree_is_a_worktree_exception(self):
        """R44: a batch worktree directory removed by hand stays listed
        (prunable).  Resuming its parked batch is that batch's `worktree`
        exception, not a crash: parked and current clear, the run goes on,
        and a second run does not crash either."""
        order = []
        d = self._driver(worker=driver.FakeWorker(self.REPORT))
        d.worker.run = lambda *a: self.fail('no worker runs without a worktree')
        self._write_status(d, {
            'parked': {'batch_id': 'b011', 'at': 't', 'reason': 'worker rate limited'},
            'current': {'batch': 'b011', 'stage': 'worker', 'at': 't'},
            'batches': {'b011': {'outcome': 'parked'}}})
        shutil.rmtree(self.wt)
        self.assertIn(self.wt, self.git('worktree', 'list', '--porcelain').stdout)
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['cut'])                # the run went on
        status = self._status(d)
        exc = status['exceptions']['b011']
        self.assertEqual(exc['stage'], 'worktree')
        self.assertIn('%s is gone' % self.wt, exc['problems'][0])
        self.assertIn('`git worktree prune`, then `git worktree add <path> p3/wso-b011`',
                      exc['problems'][0])
        self.assertIsNone(status['parked'])
        self.assertIsNone(status['current'])
        order.clear()
        self.assertEqual(d.run(), 0)                    # no crash, no retry
        self.assertEqual(order, ['cut'])

    def test_launch_failure_halts_and_parks(self):
        """R45: a worker or reviewer that cannot be launched (the wrapper
        is not on PATH) in an existing worktree halts the run after ONE
        cut: the batch is parked `launch failed`, `halted` is set, exit 1.
        The next run resumes that batch first and clears `halted`.  Only a
        missing cwd stays the batch's `worktree` exception."""
        class Missing:
            def run(self, brief, cwd):
                raise FileNotFoundError(2, 'No such file or directory', 'claude-glm')
        for worker, reviewer, label in ((Missing(), None, 'worker'),
                                        (driver.FakeWorker(self.REPORT), Missing(), 'reviewer')):
            d = self._driver(worker=worker, reviewer=reviewer)
            cuts = []
            d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
            self.assertEqual(d.run(), 1)
            self.assertEqual(len(cuts), 1)                  # no further cut
            status = self._status(d)
            reason = status['pools']['glm']['parked']['b011']['reason']
            self.assertTrue(reason.startswith('launch failed: %s: ' % label), reason)
            self.assertIn("'claude-glm'", reason)
            self.assertEqual(list(status['pools']['glm']['parked']), ['b011'])
            self.assertEqual(status['halted']['reason'], reason)
            self.assertTrue(status['halted']['at'])
            self.assertEqual(status['exceptions'], {})
            self.assertEqual(self._state()['status'], 'assigned')
            self.assertEqual(len(self.sent), 1)
            self.assertIn('halted: launch failed', self.sent[0])
        # The wrapper is back: the next run resumes the parked batch first.
        order = []
        w = _SeqWorker(self.REPORT)
        run = w.run
        w.run = lambda brief, cwd: order.append('worker') or run(brief, cwd)
        d.worker, d.reviewer = w, driver.FakeWorker(self.CLEAN_REVIEW)
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        self.assertEqual(d.run(), 0)
        self.assertEqual(order, ['worker', 'cut'])
        self.assertTrue(w.briefs[0].startswith('Resume:'))
        self.assertIn('launch failed', w.briefs[0])
        status = self._status(d)
        self.assertIsNone(status['halted'])
        self.assertIsNone(status['parked'])
        self.assertEqual(self._state()['status'], 'closed')
        # A missing cwd is the batch's worktree exception; the run goes on.
        class Vanishing:
            def run(self, brief, cwd):
                shutil.rmtree(cwd)
                raise FileNotFoundError(2, 'No such file or directory', cwd)
        d = self._driver(worker=Vanishing())
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        status = self._status(d)
        self.assertEqual(status['exceptions']['b011']['stage'], 'worktree')
        self.assertIn('git worktree prune', status['exceptions']['b011']['problems'][0])
        self.assertIsNone(status['halted'])
        self.assertIsNone(status['parked'])

    class _NoReport:
        """A worker that exits without a report; the transcript of each
        run comes from the list in turn (None: the worker never ran)."""

        def __init__(self, transcripts):
            self.transcripts = list(transcripts)

        def run(self, brief, cwd):
            return driver.Result(1, '', self.transcripts.pop(0), None, False)

    def _jsonl(self, name, lines):
        path = os.path.join(self.tmp, name)
        with open(path, 'w') as f:
            f.write(''.join(json.dumps(x) + '\n' for x in lines))
        return path

    def test_breaker_trips_after_three_batches_without_a_transcript(self):
        """R45, R46: three batch exceptions in a row whose worker never
        really ran (no transcript, or one with no successful non-synthetic
        turn) halt the run (exit 1, no fourth cut); a batch whose worker
        really ran resets the count."""
        self.assertEqual(driver.BREAKER_LIMIT, 3)
        d = self._driver(worker=self._NoReport([None] * 4))
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
        self.assertEqual(d.run(), 1)
        self.assertEqual(len(cuts), 3)                      # no fourth cut
        halted = self._status(d)['halted']
        self.assertEqual(halted['reason'], 'breaker: 3 batches without a worker transcript')
        self.assertEqual(len(self.sent), 4)                 # 3 exceptions, 1 halt
        self.assertIn('halted: breaker', self.sent[-1])
        # Transcripts holding only synthetic entries count as never ran.
        d = self._driver(worker=None)
        fake = self._jsonl('fake.jsonl', [
            {'type': 'user', 'message': {'role': 'user', 'content': 'BRIEF'}},
            {'type': 'assistant', 'message': {'model': '<synthetic>', 'content': [
                {'type': 'text', 'text': 'No response requested.'}]}},
            self._api_error('API Error: 500 internal error', error='api_error', status=500)])
        real = self._jsonl('real.jsonl', [self._turn(), self._api_error(
            'API Error: 500 internal error', error='api_error', status=500)])
        self.assertEqual(driver.scan_transcript(fake), (None, False))
        self.assertEqual(driver.scan_transcript(real), (None, True))
        d.worker = self._NoReport([fake] * 4)
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
        self.assertEqual(d.run(), 1)
        self.assertEqual(len(cuts), 3)
        self.assertIn('breaker', self._status(d)['halted']['reason'])
        # A batch whose worker really ran, in between, resets the count; a
        # later run that cuts a batch clears `halted`.
        d.worker = self._NoReport([None, fake, real, None, fake])
        cuts = iter([(0, self.CUT)] * 5 + [self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        self.assertEqual(d.worker.transcripts, [])          # all five ran
        self.assertIsNone(self._status(d)['halted'])

    def _auth_error(self):
        return self._api_error('Not logged in · Please run /login',
                               error='authentication_failed', status=None)

    def test_headless_worker_reports_auth_and_billing_errors(self):
        """R46: Result.api_error carries the kind when the final JSON result,
        or the transcript's last API-error entry with no successful turn
        after it, says authentication_failed or billing_error, or status
        401, 402 or 403."""
        billing = self._api_error('Credit balance is too low', error='billing_error', status=400)
        cases = [   # (name, transcript lines, JSON result, api_error, rate limited)
            ('billing-result', None, {'is_error': True, 'error': 'billing_error',
                                      'result': 'Credit balance is too low'}, 'billing_error', False),
            ('status-result', None, {'is_error': True, 'apiErrorStatus': 403,
                                     'result': 'Forbidden'}, 'status 403', False),
            ('auth-last', [self._turn(), self._auth_error()],
             {'is_error': True, 'result': 'Not logged in · Please run /login'},
             'authentication_failed', False),
            ('auth-retried', [self._auth_error(), self._turn(), self._turn()],
             {'result': '# report\n'}, None, False),
            ('billing-then-429', [self._turn(), billing, self._api_error()],
             {'is_error': True, 'result': ''}, None, True),
            ('normal', [self._turn()], {'result': '# report\n'}, None, False),
        ]
        with tempfile.TemporaryDirectory() as tmp:
            cwd = os.path.join(tmp, 'wt')
            os.mkdir(cwd)
            proj = os.path.join(tmp, 'cfg', 'projects', driver.project_slug(cwd))
            os.makedirs(proj)
            w = driver.HeadlessWorker(config_dir=os.path.join(tmp, 'cfg'))
            for name, lines, result, kind, limited in cases:
                if lines is not None:
                    with open(os.path.join(proj, name + '.jsonl'), 'w') as f:
                        f.write(''.join(json.dumps(x) + '\n' for x in lines))
                out = json.dumps(dict(result, session_id=name))
                w._run = lambda argv, **kw: subprocess.CompletedProcess(argv, 1, out, '')
                r = w.run('B', cwd)
                self.assertEqual((r.api_error, r.rate_limited), (kind, limited), name)
        self.assertIsNone(driver.FakeWorker('R').run('b', '/tmp').api_error)

    def test_api_error_halts_and_parks(self):
        """R46: an auth or billing error halts the run like a launch
        failure: exit 1 after one cut, the batch parked `api error:
        <kind>`, `halted` set, one AoE line.  One the worker got past does
        not halt."""
        def headless(name, result, lines=None):
            cfg = os.path.join(self.tmp, 'cfg')
            if lines is not None:
                proj = os.path.join(cfg, 'projects', driver.project_slug(self.wt))
                os.makedirs(proj, exist_ok=True)
                with open(os.path.join(proj, name + '.jsonl'), 'w') as f:
                    f.write(''.join(json.dumps(x) + '\n' for x in lines))
            out = json.dumps(dict(result, session_id=name))
            return driver.HeadlessWorker(config_dir=cfg, run=lambda argv, **kw:
                                         subprocess.CompletedProcess(argv, 1, out, ''))
        for name, result, lines, kind in (
                ('billing', {'is_error': True, 'error': 'billing_error',
                             'result': 'Credit balance is too low'}, None, 'billing_error'),
                ('auth', {'is_error': True, 'result': 'Not logged in · Please run /login'},
                 [self._turn(), self._auth_error()], 'authentication_failed')):
            d = self._driver(worker=None)
            d.worker = headless(name, result, lines)
            cuts = []
            d.next_batch = lambda *a, **kw: cuts.append(1) or (0, self.CUT)
            self.assertEqual(d.run(), 1, name)
            self.assertEqual(len(cuts), 1, name)
            status = self._status(d)
            self.assertEqual(list(status['pools']['glm']['parked']), ['b011'])
            self.assertEqual(status['pools']['glm']['parked']['b011']['reason'], 'api error: %s' % kind)
            self.assertEqual(status['halted']['reason'], 'api error: %s' % kind)
            self.assertEqual(status['exceptions'], {})
            self.assertEqual(len(self.sent), 1)
            self.assertIn('halted: api error: %s' % kind, self.sent[0])
        # An auth error followed by successful turns does not halt.
        d = self._driver(worker=None)
        d.worker = headless('auth-ok', {'result': self.REPORT},
                            [self._auth_error(), self._turn(), self._turn()])
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._state()['status'], 'closed')
        self.assertIsNone(self._status(d)['halted'])

    def test_one_driver_per_status_file(self):
        """R42: run() holds `<status>.run.lock` (with its pid in it) for its
        whole lifetime; a second run() on the same status file exits 1 and
        names that pid; --clear does not take the run lock."""
        seen = []

        class Nested(_SeqWorker):
            def run(w, brief, cwd):
                with open(d.status_path + '.run.lock') as f:
                    seen.append(('pid', f.read().strip()))
                seen.append(('second', second.run()))
                seen.append(('clear', driver.clear_exception(d.status_path, 'b099')))
                return super().run(brief, cwd)
        d = self._driver(worker=Nested(self.REPORT))
        second = self._reload(d)
        second.next_batch = lambda *a, **kw: self.fail('the second driver cuts nothing')
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        self.assertEqual(seen, [('pid', str(os.getpid())), ('second', 1), ('clear', (2, None))])
        self.assertIn('another driver is running (pid %d)' % os.getpid(), '\n'.join(self.logs))
        # The first is unaffected: b011 closed and its status is its own.
        self.assertEqual(self._state()['status'], 'closed')
        status = self._status(d)
        self.assertEqual(status['batches']['b011']['outcome'], 'closed')
        self.assertIsNone(status['current'])
        # Released when run() returns: a later driver takes it.
        second.next_batch = lambda *a, **kw: self.NOTHING
        self.assertEqual(second.run(), 0)

    # -- worker pools and the import queue (Task 15) ---------------------------

    def test_parse_pool_default_and_errors(self):
        p = driver.parse_pool(driver.DEFAULT_POOL)
        self.assertEqual(p, driver.Pool('glm', 'claude-glm', 'glm-5.3-flash[1m]', 2,
                                        'glm-5.3-flash[1m]'))
        p = driver.parse_pool('a=claude:opus')
        self.assertEqual((p.name, p.wrapper, p.model, p.workers), ('a', 'claude', 'opus', 1))
        self.assertEqual(driver.parse_pool('a=claude:opus:3').workers, 3)
        for bad in ('', '   ', 'a=claude', 'a=claude:opus:0', 'a=claude:opus:x',
                    'a=claude:opus:1:9', 'a=:opus', 'a=claude:', 'x', 'x:2',
                    'claude:two', 'claude:0', 'b a=claude:opus', '=claude:opus'):
            with self.assertRaises(ValueError, msg=bad):
                driver.parse_pool(bad)

    def test_pool_short_forms_follow_the_queue(self):
        c = driver.parse_pool('claude')
        self.assertEqual((c.wrapper, c.model, c.workers), ('claude', 'sonnet', 1))
        c = driver.parse_pool('claude:3', queue='wave1')
        self.assertEqual((c.model, c.workers), ('sonnet', 3))
        c = driver.parse_pool('claude', queue='import')
        self.assertEqual((c.wrapper, c.model), ('claude', 'haiku'))
        for queue in ('wave1', 'import'):
            g = driver.parse_pool('glm', queue=queue)
            self.assertEqual((g.wrapper, g.model, g.workers),
                             ('claude-glm', 'glm-5.3-flash[1m]', 1))

    def _tmpdir(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        return tmp

    def test_reviewer_model_defaults_and_override(self):
        self.assertEqual(driver.parse_pool('claude').reviewer_model, 'sonnet')
        self.assertEqual(driver.parse_pool('glm').reviewer_model, 'glm-5.3-flash[1m]')
        self.assertEqual(driver.parse_pool('x=claude:haiku:2').reviewer_model, 'haiku')
        pools = [driver.parse_pool('claude'), driver.parse_pool('glm')]
        out = driver.pools_with_reviewers(pools, {'claude': 'opus'})
        self.assertEqual([p.reviewer_model for p in out],
                         ['opus', 'glm-5.3-flash[1m]'])
        with self.assertRaises(ValueError):
            driver.pools_with_reviewers(pools, {'nope': 'opus'})
        tmp = self._tmpdir()
        d, err = driver.driver_from_args(
            ['--pool', 'claude', '--pool', 'x=claude:haiku', '--reviewer-model',
             'claude=opus', '--state', os.path.join(tmp, 'wave-state.json')], repo=tmp)
        self.assertIsNone(err)
        self.assertEqual([p.reviewer_model for p in d.pools], ['opus', 'haiku'])
        self.assertEqual(d.reviewers_by_pool['claude'].model, 'opus')
        self.assertEqual(d.reviewers_by_pool['claude'].wrapper, 'claude')
        self.assertEqual(d.workers_by_pool['claude'].model, 'sonnet')

    def test_headless_worker_model_flag_for_claude_wrapper(self):
        env = {k: v for k, v in os.environ.items()
               if k not in ('CLAUDE_GLM_MODEL', 'CLAUDE_GLM_CONFIG_DIR',
                            'CLAUDE_CONFIG_DIR')}
        with mock.patch.dict(os.environ, env, clear=True):
            w = driver.HeadlessWorker(model='sonnet', wrapper='claude')
            argv, e = w.command('BRIEF')
            self.assertEqual(argv[:3], ['claude', '-p', 'BRIEF'])
            self.assertEqual(argv[argv.index('--model') + 1], 'sonnet')
            self.assertNotIn('CLAUDE_GLM_MODEL', e)
            argv, e = driver.HeadlessWorker().command('BRIEF')
            self.assertNotIn('--model', argv)
            self.assertEqual(e['CLAUDE_GLM_MODEL'], 'glm-5.3-flash[1m]')
            # Its transcripts are under ~/.claude, not the GLM wrapper's.
            self.assertEqual(w.transcript_path('/x/y', 's1'), os.path.join(
                os.path.expanduser('~/.claude'), 'projects', '-x-y', 's1.jsonl'))
        with mock.patch.dict(os.environ, {'CLAUDE_GLM_CONFIG_DIR': '/glm/cfg'}):
            self.assertEqual(driver.HeadlessWorker(wrapper='claude').config_dir,
                             os.environ.get('CLAUDE_CONFIG_DIR')
                             or os.path.expanduser('~/.claude'))

    # A repo with `first` and one single-row batch per id, each with its own
    # address and a committed evidence file, so batches close in any order.
    def _pool_driver(self, pools, workers, ids=('b011', 'b012', 'b013'),
                     reviewers=None, queue='wave1', base_files=None,
                     on_branch=None, **kw):
        tmp = self._tmpdir()
        self.tmp = tmp
        self.root = os.path.join(tmp, 'repo')
        os.makedirs(self.root)
        self.git('init', '-q', '-b', 'first')
        _write(os.path.join(self.root, 'README'), 'x\n')
        for rel, text in (base_files or {}).items():
            _write(os.path.join(self.root, rel), text)
        self.git('add', '-A')
        self.git('commit', '-qm', 'base')
        batches = {}
        for bid in ids:
            n = int(bid[1:])
            addr = '0x%X' % (0x400000 + n * 0x1000)
            wt = os.path.join(tmp, 'wt-' + bid)
            self.git('worktree', 'add', '-q', '-b', 'p3/wso-' + bid, wt, 'first')
            _write(os.path.join(wt, 'docs', 'evidence', addr[2:] + '.md'),
                   f'---\naddress: {addr}\nname: F{n}\nkind: oracle\n---\n\n'
                   'corroborated by WSO at e8dd16a7\n')
            if on_branch:
                on_branch(wt, bid)
            self.git('add', '-A', cwd=wt)
            self.git('commit', '-qm', 'evidence ' + bid, cwd=wt)
            row = {'address': addr, 'name': 'F%d' % n, 'file': 'src/F%d.cpp' % n,
                   'category': 'both_changed_differ', 'their_len': 9, 'our_len': 9}
            batches[bid] = {'rows': [row], 'files': [row['file']],
                            'branch': 'p3/wso-' + bid, 'status': 'assigned',
                            'assigned_at': 't', 'closed_at': None, 'verdicts': {}}
        self.state_path = os.path.join(tmp, 'wave-state.json')
        with open(self.state_path, 'w') as f:
            json.dump({'fork_base': 'e8dd16a7', 'batches': batches}, f)
        self.pushes, self.sent, self.logs = [], [], []
        d = driver.Driver(
            self.root, self.state_path, None, None, os.path.join(tmp, 'ledger.md'),
            os.path.join(tmp, 'driver', 'status.json'), pools=pools,
            workers_by_pool=workers,
            reviewers_by_pool=reviewers or {
                p.name: _SeqWorker(self.CLEAN_REVIEW, credits=0.0) for p in pools},
            runner=lambda fn: fn(),
            push=lambda: self.pushes.append(1), send=self.sent.append, gate=False,
            log=self.logs.append, queue=queue, **kw)
        d.render_brief = lambda template_path, state_path, batch_id, bundle_dir=None, **_: (
            0, 'BRIEF ' + batch_id)
        d.mechanical_checks = self._checks
        d.report_path = os.path.join(tmp, 'xrec-report.md')
        d.triage_path = os.path.join(tmp, 'no-triage.json')
        self.pending = list(ids)

        def cut(*a, **kw):
            if self.pending:
                return 0, '%s assigned: 1 rows\n' % self.pending.pop(0)
            return self.NOTHING
        d.next_batch = cut
        # These batches are assigned already and are handed out by the cut
        # stub; adoption (R69) has its own tests.
        d._adoptable = lambda taken: []
        return d

    class _BatchWorker(_SeqWorker):
        """Answers each batch's brief with its own all-ours report, after
        running `before(cwd)` when given."""

        def __init__(self, credits=None, rate_limited=False, before=None):
            super().__init__('', credits=credits, rate_limited=rate_limited)
            self.before = before
            self.ids = []

        def run(self, brief, cwd):
            self.briefs.append(brief)
            self.cwds.append(cwd)
            if self.before:
                self.before(cwd)
            bid = re.search(r'BRIEF (b\d+)', brief).group(1)
            self.ids.append(bid)
            addr = 0x400000 + int(bid[1:]) * 0x1000
            report = '## Verdicts\n\nverdict 0x%X ours case dump-%s\n' % (addr, bid)
            return driver.Result(0, '' if self.rate_limited else report, None,
                                 self.credits, self.rate_limited)

    def _two_pools(self):
        return [driver.parse_pool('a=claude-glm:glm-5.3-flash[1m]:1'),
                driver.parse_pool('b=claude:sonnet:1')]

    def test_two_pools_share_batches(self):
        wa, wb = self._BatchWorker(credits=0.0), self._BatchWorker()
        d = self._pool_driver(self._two_pools(), {'a': wa, 'b': wb})
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual((wa.ids, wb.ids), (['b011', 'b013'], ['b012']))
        with open(self.state_path) as f:
            batches = json.load(f)['batches']
        self.assertEqual({k: v['pool'] for k, v in batches.items()},
                         {'b011': 'a', 'b012': 'b', 'b013': 'a'})
        self.assertTrue(all(v['status'] == 'closed' for v in batches.values()))
        status = self._status(d)
        self.assertEqual(set(status['pools']), {'a', 'b'})
        self.assertEqual(status['pools']['a']['parked'], None)
        # b012 was cut before b011 closed: the base was merged in first.
        self.assertTrue(self.git('rev-list', '--merges', 'first').stdout.strip())
        self.assertEqual(self.git('rev-parse', 'first').stdout,
                         self.git('rev-parse', 'p3/wso-b013').stdout)

    def test_pick_pool_rotates_and_skips(self):
        d = self._pool_driver(self._two_pools(), {'a': None, 'b': None})
        self.assertEqual([d.pick_pool() for _ in range(4)], ['a', 'b', 'a', 'b'])
        d._busy['a'] = 1       # a has no free worker
        self.assertEqual(d.pick_pool(), 'b')
        self.assertEqual(d.pick_pool(), 'b')
        d._busy['b'] = 1
        self.assertIsNone(d.pick_pool())
        d._busy.update(a=0, b=0)
        d._parked_now.add('b')
        self.assertEqual([d.pick_pool() for _ in range(2)], ['a', 'a'])

    def test_parked_pool_is_skipped(self):
        def scratch(cwd):
            _write(os.path.join(cwd, 'scratch.txt'), 'half done\n')
        wa = self._BatchWorker(rate_limited=True, before=scratch)
        wb = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': wa, 'b': wb})
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 3)
        self.assertEqual((wa.ids, wb.ids), (['b011'], ['b012', 'b013']))
        status = self._status(d)
        self.assertEqual(list(status['pools']['a']['parked']), ['b011'])
        self.assertIsNone(status['pools']['b']['parked'])
        self.assertIsNone(status['parked'])
        self.assertEqual(self.git('log', '-1', '--format=%s', 'p3/wso-b011').stdout.strip(),
                         'WIP: parked by driver')
        with open(self.state_path) as f:
            batches = json.load(f)['batches']
        self.assertEqual([batches[k]['status'] for k in ('b011', 'b012', 'b013')],
                         ['assigned', 'closed', 'closed'])
        # The next run resumes the parked batch on pool a, before any cut.
        wa.rate_limited = False
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(wa.ids, ['b011', 'b011'])
        self.assertIn('Resume: batch b011', wa.briefs[-1])
        self.assertIsNone(self._status(d)['pools']['a']['parked'])

    def test_pool_budget_stops_only_that_pool(self):
        wa, wb = self._BatchWorker(credits=1500.0), self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': wa, 'b': wb},
                              ids=('b011', 'b012', 'b013', 'b014'),
                              budgets={'a': 2000.0})
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(wa.ids, ['b011'])
        self.assertEqual(wb.ids, ['b012', 'b013', 'b014'])
        pools = self._status(d)['pools']
        self.assertEqual(pools['a']['stopped'], 'budget')
        self.assertEqual(pools['a']['budget'], 2000.0)
        self.assertEqual(pools['a']['credits'], 1500.0)
        self.assertIsNone(pools['b']['stopped'])
        self.assertIsNone(pools['b']['budget'])

    def test_budget_on_non_glm_pool_is_refused(self):
        tmp = self._tmpdir()
        state = os.path.join(tmp, 'wave-state.json')
        d, err = driver.driver_from_args(
            ['--pool', 'claude=claude:sonnet:1', '--budget-claude', '500',
             '--state', state], repo=tmp)
        self.assertIsNone(d)
        self.assertEqual(err[0], 2)
        self.assertIn('--budget-claude', err[1])
        self.assertIn('Z.ai', err[1])
        self.assertIn('sonnet', err[1])
        d, err = driver.driver_from_args(['--budget-nosuch', '5', '--state', state], repo=tmp)
        self.assertEqual((d, err[0]), (None, 2))
        self.assertIn('no pool named nosuch', err[1])
        d, err = driver.driver_from_args(['--budget-glm', 'lots', '--state', state], repo=tmp)
        self.assertEqual((d, err[0]), (None, 2))
        for argv in (['--budget-glm', '500'], ['--budget-glm=500']):
            d, err = driver.driver_from_args(argv + ['--state', state], repo=tmp)
            self.assertIsNone(err)
            self.assertEqual(d.budgets, {'glm': 500.0})
        with self.assertRaises(ValueError):
            driver.Driver(tmp, state, None, None, 'l', 's', budgets={'x': 1.0})

    def test_pool_flags(self):
        tmp = self._tmpdir()
        state = os.path.join(tmp, 'wave-state.json')
        d, err = driver.driver_from_args(['--state', state], repo=tmp)
        self.assertEqual([(p.name, p.workers) for p in d.pools], [('glm', 2)])
        d, err = driver.driver_from_args(['--workers', '3', '--state', state], repo=tmp)
        self.assertEqual(([(p.name, p.workers) for p in d.pools], d.workers),
                         ([('glm', 3)], 3))
        d, err = driver.driver_from_args(
            ['--workers', '3', '--pool', 'claude', '--state', state], repo=tmp)
        self.assertIsNone(d)
        self.assertEqual(err[0], 2)
        self.assertIn('--workers', err[1])
        self.assertIn('--pool', err[1])
        d, err = driver.driver_from_args(
            ['--pool', 'claude=claude:sonnet:1', '--pool', 'claude=claude:haiku',
             '--state', state], repo=tmp)
        self.assertEqual((d, err[0]), (None, 2))
        d, err = driver.driver_from_args(['--pool', 'bogus', '--state', state], repo=tmp)
        self.assertEqual((d, err[0]), (None, 2))
        # Tonight's command line.
        d, err = driver.driver_from_args(
            ['--queue', 'wave1', '--pool', 'claude=claude:sonnet:1', '--state', state],
            repo=tmp)
        self.assertIsNone(err)
        p, = d.pools
        self.assertEqual((p.name, p.wrapper, p.model, p.workers, p.reviewer_model),
                         ('claude', 'claude', 'sonnet', 1, 'sonnet'))
        self.assertEqual(d.status_path, os.path.join(
            tmp, '.superpowers', 'sdd', '2026-10-17-wso-integration', 'driver',
            'status.json'))

    def test_pool_name_reaches_ledger_entry(self):
        wa, wb = self._BatchWorker(credits=0.0), self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': wa, 'b': wb})
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        ledger = self._ledger(d)
        entries = ledger.split('## driver ')[1:]
        pools = {e.split(' ')[0]: re.search(r'^- pool (\S+)$', e, re.M).group(1)
                 for e in entries}
        self.assertEqual(pools, {'b011': 'a', 'b012': 'b', 'b013': 'a'})
        with open(d.report_path) as f:
            report = f.read()
        self.assertIn('b012 closed (1 rows, pool b)', report)
        self.assertIn('b011 closed (1 rows, pool a)', report)
        text = driver._tool('wave').scorecard_text({'batches': {
            'b1': {'status': 'closed', 'rows': [{}], 'verdicts': {}},
            'b2': {'status': 'closed', 'rows': [{}], 'verdicts': {}, 'pool': 'x'}}})
        self.assertIn('b1 closed (1 rows), b2 closed (1 rows, pool x)', text)
        self.assertIn('- pool a\n', driver.ledger_entry(
            {'id': 'b1'}, {}, {}, (False, 'n'), 'c', None, pool='a'))

    def test_state_mutations_hold_the_driver_lock(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        held = []

        def locked(name, fn):
            def run(*a, **kw):
                held.append((name, d._lock.locked()))
                return fn(*a, **kw)
            return run
        d.next_batch = locked('next', d.next_batch)
        d.record_verdict = locked('record', d.record_verdict)
        d.close_batch = locked('close', d.close_batch)
        d.write_scorecard = locked('scorecard', d.write_scorecard)
        d.push = locked('push', d.push)
        d.pick_pool = locked('pick', d.pick_pool)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual({n for n, _ in held},
                         {'next', 'record', 'close', 'scorecard', 'push', 'pick'})
        self.assertTrue(all(h for _, h in held), held)
        self.assertFalse(d._lock.locked())

    def test_pools_run_batches_at_the_same_time(self):
        """Real threads: two batches must meet at a barrier, which only a
        run of both at once passes."""
        d = self._pool_driver(self._two_pools(), {'a': None, 'b': None},
                              ids=('b011', 'b012'))
        d._runner = None
        barrier = threading.Barrier(2, timeout=20)
        seen = []

        def run_batch(batch_id):
            barrier.wait()
            seen.append((batch_id, d._ctx.pool, threading.current_thread().name))
            return 'closed'
        d.run_batch = run_batch
        self.assertEqual(d.run(), 0)
        self.assertEqual(sorted(s[:2] for s in seen), [('b011', 'a'), ('b012', 'b')])
        self.assertEqual({s[2] for s in seen}, {'driver-batch'})
        self.assertEqual(threading.active_count(), 1)   # every thread was joined

    def test_a_crash_in_a_batch_thread_surfaces_after_the_others_finish(self):
        d = self._pool_driver(self._two_pools(), {'a': None, 'b': None},
                              ids=('b011', 'b012'))
        d._runner = None
        ran = []

        def run_batch(batch_id):
            ran.append(batch_id)
            if batch_id == 'b011':
                raise RuntimeError('boom')
            return 'closed'
        d.run_batch = run_batch
        with self.assertRaises(RuntimeError):
            d.run()
        self.assertIn('b011', ran)

    # -- fix round 1 (R64) ---------------------------------------------------

    def test_two_429s_on_one_pool_resume_both_batches(self):
        barrier = threading.Barrier(2, timeout=20)
        w = self._BatchWorker(rate_limited=True, before=lambda cwd: barrier.wait())
        pools = [driver.parse_pool('a=claude-glm:glm-5.3-flash[1m]:2')]
        d = self._pool_driver(pools, {'a': w}, ids=('b011', 'b012'))
        d._runner = None
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 3)
        self.assertEqual(sorted(self._status(d)['pools']['a']['parked']),
                         ['b011', 'b012'])
        resume, code = d._leftovers()
        self.assertEqual(sorted(r[0] for r in resume), ['b011', 'b012'])
        w.rate_limited, w.before = False, None
        d._runner = lambda fn: fn()
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        with open(self.state_path) as f:
            self.assertEqual({v['status'] for v in json.load(f)['batches'].values()},
                             {'closed'})
        self.assertIsNone(self._status(d)['pools']['a']['parked'])

    def test_a_batch_parked_on_two_pools_resumes_once_and_clears_both(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': w, 'b': self._BatchWorker(credits=0.0)},
                              ids=('b011',))
        self._write_status(d, {'pools': {
            'a': {'parked': {'b011': {'at': 't', 'reason': 'r'}}},
            'b': {'parked': {'batch_id': 'b011', 'at': 't', 'reason': 'r'}}}})
        resume, code = d._leftovers()
        self.assertEqual([r[0] for r in resume], ['b011'])
        self.pending[:] = []
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b011'])
        pools = self._status(d)['pools']
        self.assertEqual((pools['a']['parked'], pools['b']['parked']), (None, None))

    def test_worker_env_is_scrubbed(self):
        env = {'PATH': '/bin', 'HOME': '/h', 'ANTHROPIC_API_KEY': 'k',
               'CLAUDE_CODE_CHILD_SESSION': '1', 'CLAUDE_CODE_MESSAGING_X': 'm',
               'CLAUDE_CODE_SESSION_ID': 's', 'CLAUDE_CODE_ENTRYPOINT': 'cli',
               'CLAUDE_CODE_EXECPATH': '/x', 'CLAUDE_PID': '9', 'CLAUDE_EFFORT': 'hi',
               'ANTHROPIC_BASE_URL': 'u', 'ANTHROPIC_AUTH_TOKEN': 't',
               'ANTHROPIC_DEFAULT_SONNET_MODEL': 'g', 'CLAUDE_GLM_FOO': 'x',
               'CLAUDE_CODE_SUBAGENT_MODEL': 'g', 'CLAUDE_CODE_SUBAGENT_MODEL_X': 'g',
               'CLAUDE_CONFIG_DIR': '/glm/cfg', 'CLAUDE_GLM_CONFIG_DIR': '/glm/cfg'}
        session = {'CLAUDE_CODE_CHILD_SESSION', 'CLAUDE_CODE_MESSAGING_X',
                   'CLAUDE_CODE_SESSION_ID', 'CLAUDE_CODE_ENTRYPOINT',
                   'CLAUDE_CODE_EXECPATH', 'CLAUDE_PID', 'CLAUDE_EFFORT'}
        with mock.patch.dict(os.environ, env, clear=True):
            _, e = driver.HeadlessWorker().command('B')          # claude-glm
            self.assertFalse(session & set(e))
            self.assertEqual(e['ANTHROPIC_BASE_URL'], 'u')
            self.assertEqual(e['ANTHROPIC_DEFAULT_SONNET_MODEL'], 'g')
            self.assertEqual(e['CLAUDE_GLM_MODEL'], 'glm-5.3-flash[1m]')
            self.assertEqual(e['CLAUDE_CONFIG_DIR'], '/glm/cfg')
            w = driver.HeadlessWorker(model='sonnet', wrapper='claude')
            _, e = w.command('B')
            self.assertEqual(sorted(e), ['ANTHROPIC_API_KEY',
                                         'CLAUDE_CODE_DISABLE_AUTO_MEMORY',
                                         'ENABLE_CLAUDEAI_MCP_SERVERS', 'HOME', 'PATH'])
            self.assertEqual(w.config_dir, os.path.expanduser('~/.claude'))
        with mock.patch.dict(os.environ, {'CLAUDE_CONFIG_DIR': '/own/cfg', 'X': '1'}, clear=True):
            w = driver.HeadlessWorker(model='sonnet', wrapper='claude')
            _, e = w.command('B')
            self.assertEqual(e, {'CLAUDE_CONFIG_DIR': '/own/cfg', 'X': '1',
                                 'CLAUDE_CODE_DISABLE_AUTO_MEMORY': '1',
                                 'ENABLE_CLAUDEAI_MCP_SERVERS': '0'})
            self.assertEqual(w.config_dir, '/own/cfg')

    def test_worker_env_drops_claudecode_and_glm_tuning(self):
        env = {'PATH': '/bin', 'CLAUDECODE': '1',
               'CLAUDE_CODE_AUTO_COMPACT_WINDOW': '400000', 'API_TIMEOUT_MS': '3000000',
               'ANTHROPIC_BASE_URL': 'u'}
        glm = driver.worker_env(env, True)
        self.assertNotIn('CLAUDECODE', glm)
        self.assertEqual(glm['CLAUDE_CODE_AUTO_COMPACT_WINDOW'], '400000')
        self.assertEqual(glm['API_TIMEOUT_MS'], '3000000')
        plain = driver.worker_env(env, False)
        self.assertEqual(plain, {'PATH': '/bin', 'CLAUDE_CODE_DISABLE_AUTO_MEMORY': '1'})

    # -- the worker's transcript (Task 16) --------------------------------

    def _tcalls(self, *calls):
        """A JSONL transcript with one assistant message per (name, input)."""
        tmp = self._tmpdir()
        path = os.path.join(tmp, 't.jsonl')
        with open(path, 'w') as f:
            f.write(json.dumps({'type': 'user', 'message': {'content': 'hi'}}) + '\n')
            f.write('not json\n')
            for i, (name, inp) in enumerate(calls):
                f.write(json.dumps({'type': 'assistant', 'message': {'content': [
                    {'type': 'text', 'text': 'x'},
                    {'type': 'tool_use', 'id': 't%d' % i, 'name': name, 'input': inp}]}}) + '\n')
        return path

    def _bash(self, cmd):
        return driver.transcript_checks(self._tcalls(('Bash', {'command': cmd})),
                                        ['src/A.cpp'])

    def test_transcript_rejects_wso_ref(self):
        for cmd in ('git show wso/main:src/A.cpp', 'git show wso/x', 'git diff wso/main -- src',
                    'git log wso/main --oneline', 'cd x && git diff first..wso/main',
                    'git show refs/remotes/wso/main:README'):
            probs = self._bash(cmd)
            self.assertEqual(len(probs), 1, cmd)
            self.assertIn('wso/', probs[0])
        # a path under docs/wso/ is not a ref
        self.assertEqual(self._bash('git log -- docs/wso/README.md'), [])
        self.assertEqual(self._bash('ls wso/ && git status'), [])

    def test_transcript_allows_pinned_sha(self):
        self.assertEqual(self._bash('git show e8dd16a7:src/A.cpp | head -50'), [])
        self.assertEqual(self._bash('git ls-tree e8dd16a750966af53f24daaf49dcb4fb13594c13 src'), [])

    def test_transcript_rejects_whole_read_of_covered_file(self):
        path = self._tcalls(
            ('Read', {'file_path': '/wt/src/A.cpp'}),
            ('Read', {'file_path': '/wt/src/A.cpp', 'offset': 1, 'limit': 201}),
            ('Bash', {'command': 'cat src/A.cpp'}),
            ('Bash', {'command': "sed -n '1,201p' src/A.cpp"}),
            ('Bash', {'command': 'head -n 300 src/A.cpp'}),
            ('Bash', {'command': 'tail -n 201 src/A.cpp'}))
        probs = driver.transcript_checks(path, ['src/A.cpp'])
        self.assertEqual(len(probs), 6, probs)
        self.assertIn('no limit', probs[0])
        self.assertIn('limit 201', probs[1])
        self.assertIn('cat', probs[2])
        self.assertIn('sed of', probs[3])

    def test_transcript_allows_short_read(self):
        path = self._tcalls(
            ('Read', {'file_path': '/wt/src/A.cpp', 'offset': 10, 'limit': 200}),
            ('Read', {'file_path': '/wt/src/Other.cpp'}),
            ('Read', {'file_path': '/wt/docs/evidence/X.md'}),
            ('Bash', {'command': "sed -n '1,200p' src/A.cpp"}),
            ('Bash', {'command': 'head -n 200 src/A.cpp'}),
            ('Bash', {'command': 'cat src/Other.cpp'}))
        self.assertEqual(driver.transcript_checks(path, ['src/A.cpp']), [])

    def test_transcript_rejects_wsb_launch_outside_runners(self):
        for cmd in ('cmd.exe /c start run.wsb', 'WindowsSandbox.exe run.wsb',
                    'powershell Start-Process game.exe',
                    'python3 -I tools/other.py && WindowsSandbox.exe x'):
            probs = self._bash(cmd)
            self.assertEqual(len(probs), 1, cmd)
            self.assertIn('launch outside a house runner', probs[0])

    def test_transcript_history_refuses_rewrites(self):
        forbidden = (
            'git checkout -- src/A.cpp', 'git checkout HEAD src/A.cpp',
            'git checkout abc1234 -- src/A.cpp', 'git checkout .',
            'git checkout -f main', 'git restore src/A.cpp',
            'git restore --staged src/A.cpp', 'git reset', 'git reset --hard HEAD~1',
            'git reset --soft abc1234', 'git rebase first', 'git rebase -i HEAD~3',
            'git revert abc1234', 'git clean -fd', 'git stash', 'git stash pop',
            'git commit --amend', 'git commit --amend -m x',
            'git commit -a --amend --no-edit', 'git filter-repo --path src',
            'git push', 'git push origin HEAD', 'git branch -D old',
            'git branch -d old', 'git branch --delete old', 'git update-ref -d refs/x',
            'git reflog expire --expire=now --all', 'git gc --prune=now',
            'git -C /w reset --hard', 'git -c core.x=1 revert abc1234',
        )
        for cmd in forbidden:
            for form in (cmd, 'git add -A && ' + cmd, 'git status; ' + cmd,
                         'git log | head -3 && ' + cmd):
                probs = self._bash(form)
                self.assertEqual(len(probs), 1, form)
                self.assertIn('history rewrite in a worker', probs[0])
                self.assertIn('workers only add and commit', probs[0])
        fs = driver.transcript_findings(self._tcalls(
            ('Bash', {'command': 'git reset --hard'})), ['src/A.cpp'])
        self.assertEqual([k for k, _t, _f in fs], ['history'])

    def test_transcript_history_allows_add_commit_and_branches(self):
        for cmd in ('git checkout -b feature', 'git checkout -b feature first',
                    'git checkout main', 'git checkout p3/batch-1',
                    'git switch main', 'git switch -c feature', 'git add src/A.cpp',
                    'git add -A && git commit -m "x"', 'git commit -m "reset the thing"',
                    'git status', 'git diff HEAD~1', 'git log --oneline -5',
                    'git show abc1234:src/A.cpp', 'git ls-tree -r HEAD',
                    'git merge-base first HEAD', 'git rev-parse HEAD',
                    'git blame src/A.cpp', 'git branch', 'git branch -a',
                    'git commit -m x; git status', 'git -C /w log -3',
                    'echo reset && git status', 'grep -r "git push" docs'):
            self.assertEqual(self._bash(cmd), [], cmd)

    def test_transcript_allows_house_runner(self):
        for cmd in ('python3 -I tools/play.py --vm windows --script s.txt',
                    'python3 -I tools/gltrace.py boot --dry-run',
                    'python3 -I /home/x/tools/play.py --vm windows --script a.wsb'):
            self.assertEqual(self._bash(cmd), [], cmd)
        self.assertEqual(driver.HOUSE_RUNNERS, ('tools/play.py', 'tools/gltrace.py'))

    def test_transcript_unreadable_is_a_problem(self):
        probs = driver.transcript_checks('/nonexistent/t.jsonl', [])
        self.assertEqual(len(probs), 1)
        self.assertIn('could not be read', probs[0])

    def test_mechanical_checks_include_transcript_problems(self):
        d = self._mc_repo({'76D370': 'corroborated by WSO at abc'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:1]
        path = self._tcalls(('Bash', {'command': 'git show wso/main:src/A.cpp'}))
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'c')}, transcript=path,
                covered_files=['src/A.cpp'], run=self._mc_run(0, []))
            clean = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'c')}, run=self._mc_run(0, []))
        self.assertEqual(clean, [])
        self.assertEqual(len(probs), 1)
        self.assertIn('wso/', probs[0])

    def _real_git_checks(self, d):
        """The mechanical checks with the git commands real and the rest
        faked ok."""
        def run(argv, **kw):
            if argv[0] == 'git':
                return subprocess.run(argv, **kw)
            return subprocess.CompletedProcess(argv, 0, '', '')

        def checks(batch, wt, verdicts, **kw):
            with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
                return driver.mechanical_checks(batch, wt, verdicts, run=run, **kw)
        d.mechanical_checks = checks

    def test_uncommitted_work_is_a_check_problem_not_a_wip_commit(self):
        def leave_work(cwd):
            _write(os.path.join(cwd, 'notes.txt'), 'uncommitted\n')
        w = self._BatchWorker(credits=0.0, before=leave_work)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        self._real_git_checks(d)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        exc = self._status(d)['exceptions']['b011']
        self.assertEqual(exc['stage'], 'checks')
        self.assertTrue(any('commit or delete: notes.txt' in p for p in exc['problems']), exc)
        self.assertEqual(len(w.briefs), 2)               # one fix round
        self.assertNotIn('WIP', self.git('log', '-1', '--format=%s', 'p3/wso-b011').stdout)
        self.assertEqual(self._state_of(d, 'b011')['status'], 'assigned')

    def test_paths_outside_the_allowlist_are_check_problems(self):
        def stray(wt, bid):
            _write(os.path.join(wt, 'tools', 'extra.py'), 'x\n')
            _write(os.path.join(wt, 'docs', 'wso', 'brief-template.md'), 'x\n')
            _write(os.path.join(wt, 'src', 'ok.cpp'), 'x\n')
            _write(os.path.join(wt, 'compat', 'ok.h'), 'x\n')
            _write(os.path.join(wt, 'oracle', 'cases', 'c.json'), 'x\n')
            _write(os.path.join(wt, 'oracle', 'CMakeLists.txt'), 'x\n')
            _write(os.path.join(wt, 'docs', 'MAP.md'), 'x\n')
            _write(os.path.join(wt, '.mcp.json'), '{}\n')
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',),
                              on_branch=stray)
        self._real_git_checks(d)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        exc = self._status(d)['exceptions']['b011']
        text = ' '.join(exc['problems']).split('): ', 1)[1].split(';')[0]
        for bad in ('tools/extra.py', 'docs/wso/brief-template.md', '.mcp.json'):
            self.assertIn(bad, text)
        for ok in ('src/ok.cpp', 'compat/ok.h', 'oracle/cases/c.json',
                   'oracle/CMakeLists.txt', 'docs/MAP.md'):
            self.assertNotIn(ok, text)
        self.assertFalse(driver._path_allowed('docs/evidence-other/x.md'))
        self.assertTrue(driver._path_allowed('docs/evidence/40B000.md'))

    def test_driver_syncs_the_base_before_merging_into_a_batch(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        order = []
        real = d.sync_base
        d.sync_base = lambda repo, base: (order.append('sync'), real(repo, base))[1]
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.git('commit', '-q', '--allow-empty', '-m', 'base moved')
            self.assertEqual(d.run(), 0)
        self.assertIn('sync', order)
        self.assertEqual(self.git('rev-parse', 'first').stdout,
                         self.git('rev-parse', 'p3/wso-b011').stdout)

    def test_transcript_does_not_flag_look_alikes(self):
        for cmd in ('git cat-file -p e8dd16a7:src/A.cpp', 'grep -n cat src/A.cpp',
                    'cat src/A.cpp | head -40', 'grep -rn WindowsSandbox tools',
                    'ls runs/*/run.wsb', "cat src/A.cpp | sed -n '1,50p'"):
            self.assertEqual(self._bash(cmd), [], cmd)
        self.assertEqual(len(self._bash('cat src/A.cpp | wc -l')), 1)
        self.assertEqual(len(self._bash('cat src/A.cpp | head -300')), 1)

    def test_mechanical_checks_refuse_dump_decided_fork_or_neither(self):
        d = self._mc_repo({'76D370': 'kind: oracle\ncorroborated by WSO at abc',
                           '401000': 'kind: oracle\ncorroborated by WSO at abc'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:2]
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('fork', 'dump-3'), '0x401000': ('neither', 'readout-401000')},
                run=self._mc_run(0, []))
            ok = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'dump-3'), '0x401000': ('fork', 'probe-4')},
                run=self._mc_run(0, []))
        self.assertEqual(ok, [])
        self.assertEqual(len(probs), 2, probs)
        self.assertIn('needs a probe, replay or trace case', probs[0])
        self.assertIn('(dump-3)', ' '.join(probs))

    def _twork(self, transcripts):
        """A _BatchWorker whose nth run leaves transcript transcripts[n]."""
        outer = self
        paths = list(transcripts)

        class W(self._BatchWorker):
            def run(self, brief, cwd):
                res = super().run(brief, cwd)
                n = len(self.briefs) - 1
                path = paths[min(n, len(paths) - 1)]
                return res._replace(transcript=path)
        return W(credits=0.0)

    def _ledger_text(self, d):
        with open(d.ledger_path) as f:
            return f.read()

    def test_whole_read_is_a_ledger_line_only(self):
        t = self._tcalls(('Read', {'file_path': '/wt/src/F11.cpp'}))
        w = self._twork([t])
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(self._status(d)['exceptions'], {})
        self.assertIn('transcript: whole read of src/F11.cpp', self._ledger_text(d))

    def test_missing_transcript_is_a_ledger_line(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertIn('transcript: none', self._ledger_text(d))

    def test_wso_ref_or_launch_in_a_transcript_is_an_exception_without_a_fix_round(self):
        for cmd in ('git show wso/main:src/F11.cpp', 'WindowsSandbox.exe run.wsb'):
            t = self._tcalls(('Bash', {'command': cmd}))
            w = self._twork([t])
            d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
            with mock.patch.dict(os.environ, self.GIT_ENV):
                d.run()
            exc = self._status(d)['exceptions']['b011']
            self.assertEqual(exc['stage'], 'transcript', cmd)
            self.assertEqual(len(w.briefs), 1)          # no fix round
            self.assertEqual(self._state_of(d, 'b011')['status'], 'assigned')

    def _state_of(self, d, bid):
        with open(d.state_path) as f:
            return json.load(f)['batches'][bid]

    def test_every_transcript_is_checked_not_only_the_fix_rounds(self):
        bad = self._tcalls(('Bash', {'command': 'git log wso/main'}))
        clean = self._tcalls(('Bash', {'command': 'ls'}))
        # The first run is the offender; the fix round's transcript is clean.
        w = self._twork([bad, clean])
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        calls = []
        d.mechanical_checks = lambda *a, **kw: calls.append(1) or ['fix me']
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'transcript')
        self.assertEqual(calls, [])
        # A bad transcript in the fix round is caught too.
        w = self._twork([clean, bad])
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        d.mechanical_checks = lambda *a, **kw: ['fix me']
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'transcript')
        self.assertEqual(len(w.briefs), 2)

    # -- adoption, --max-batches, the state commit, the breaker (Task 13 prep)

    def _adopt_driver(self, verdict_on=(), **kw):
        """b011 and b012 assigned in the state with no branch and no
        worktree, as the old coordinator left them: rows without a class.
        b011 has a second row triage moves to the import list."""
        evidence = {'docs/evidence/%X.md' % a: (
            '---\naddress: 0x%X\nname: F\nkind: oracle\n---\n\n'
            'corroborated by WSO at e8dd16a7\n' % a)
            for a in (0x40B000, 0x40C000)}
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w},
                              ids=('b011', 'b012'), base_files=evidence, **kw)
        del d._adoptable          # the real one
        for bid in ('b011', 'b012'):
            wt = os.path.join(self.tmp, 'wt-' + bid)
            self.git('worktree', 'remove', '--force', wt)
            self.git('branch', '-D', 'p3/wso-' + bid)
        with open(self.state_path) as f:
            state = json.load(f)
        b = state['batches']
        b['b011']['assigned_at'] = '2026-10-02T00:00:00+00:00'
        b['b012']['assigned_at'] = '2026-10-01T00:00:00+00:00'
        stub = dict(b['b011']['rows'][0], address='0x40B001', name='Stub',
                    file='src/Stub.cpp')
        b['b011']['rows'].append(stub)
        b['b011']['files'].append('src/Stub.cpp')
        for bid in verdict_on:
            addr = b[bid]['rows'][0]['address']
            b[bid]['verdicts'][addr] = {'side': 'ours', 'case': 'x',
                                        'losing_kind': 'trace'}
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        triage = [{'address': '0x40B000', 'class': 'ambiguous', 'file': 'src/F11.cpp',
                   'name': 'F11', 'reason': 'r'},
                  {'address': '0x40B001', 'class': 'ours-stub', 'file': 'src/Stub.cpp',
                   'name': 'Stub', 'reason': 'r'},
                  {'address': '0x40C000', 'class': 'ambiguous', 'file': 'src/F12.cpp',
                   'name': 'F12', 'reason': 'r'}]
        d.triage_path = os.path.join(self.tmp, 'triage.json')
        with open(d.triage_path, 'w') as f:
            json.dump(triage, f)
        return d, w

    def test_assigned_batches_without_worktrees_are_adopted_in_order_before_any_cut(self):
        d, w = self._adopt_driver()
        order = []
        d.next_batch = lambda *a, **kw: order.append('cut') or self.NOTHING
        run = w.run
        w.run = lambda brief, cwd: order.append('worker') or run(brief, cwd)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b012', 'b011'])       # oldest assigned_at first
        self.assertEqual(order, ['worker', 'worker', 'cut'])
        root = os.path.join(self.tmp, 'repo-worktrees')
        for bid in ('b011', 'b012'):
            self.assertTrue(os.path.isdir(os.path.join(root, 'p3-wso-' + bid)))
            self.assertEqual(self._state_of(d, bid)['status'], 'closed')
            self.assertEqual(self._state_of(d, bid)['pool'], 'a')
        self.assertTrue(any('b012 adopted' in l for l in self.logs))

    def test_adoption_applies_triage(self):
        d, w = self._adopt_driver()
        d.next_batch = lambda *a, **kw: self.NOTHING
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        b = self._state_of(d, 'b011')
        self.assertEqual([r['address'] for r in b['rows']], ['0x40B000'])
        self.assertTrue(all(r['class'] == 'ambiguous' for r in b['rows']))
        self.assertEqual(b['files'], ['src/F11.cpp'])
        self.assertEqual(self._state_of(d, 'b012')['rows'][0]['class'], 'ambiguous')
        with open(os.path.join(self.root, 'docs', 'wso', 'import-list.json')) as f:
            self.assertEqual([e['address'] for e in json.load(f)], ['0x40B001'])

    def test_an_assigned_batch_with_a_verdict_is_not_adopted(self):
        d, w = self._adopt_driver(verdict_on=('b012',))
        d.next_batch = lambda *a, **kw: self.NOTHING
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b011'])
        self.assertEqual(self._state_of(d, 'b012')['status'], 'assigned')
        self.assertNotIn('class', self._state_of(d, 'b012')['rows'][0])
        self.assertFalse(os.path.exists(os.path.join(
            self.tmp, 'repo-worktrees', 'p3-wso-b012')))

    def test_adoption_skips_batches_claimed_in_the_status_file(self):
        d, w = self._adopt_driver()
        os.makedirs(os.path.dirname(d.status_path), exist_ok=True)
        with open(d.status_path, 'w') as f:
            json.dump({'exceptions': {'b012': {'stage': 'checks', 'at': 't',
                                               'problems': ['x']}}}, f)
        d.next_batch = lambda *a, **kw: self.NOTHING
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b011'])

    def test_adoption_only_on_wave1(self):
        d, w = self._adopt_driver()
        d.queue = 'import'
        self.assertEqual(d._adoptable([]), [])

    def test_max_batches_starts_exactly_n_and_exits_0(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, max_batches=2)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b011', 'b012'])
        self.assertEqual(self.pending, ['b013'])
        stopped = self._status(d)['stopped']
        self.assertEqual((stopped['reason'], stopped['n']), ('max-batches', 2))
        self.assertTrue(stopped['at'])
        self.assertEqual(self._state_of(d, 'b013')['status'], 'assigned')
        # The next run, without the cap, clears it and does the third.
        d.max_batches = None
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertIsNone(self._status(d)['stopped'])
        self.assertEqual(w.ids, ['b011', 'b012', 'b013'])

    def test_max_batches_with_a_park_still_exits_3(self):
        wa = self._BatchWorker(credits=0.0, rate_limited=True)
        wb = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': wa, 'b': wb}, max_batches=2)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 3)
        self.assertEqual(wa.ids + wb.ids, ['b011', 'b012'])
        self.assertEqual(self.pending, ['b013'])
        self.assertEqual(self._status(d)['stopped']['reason'], 'max-batches')

    def test_max_batches_counts_adopted_batches_and_is_validated(self):
        d, w = self._adopt_driver(max_batches=1)
        cuts = []
        d.next_batch = lambda *a, **kw: cuts.append(1) or self.NOTHING
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b012'])
        self.assertEqual(cuts, [])
        args, err = driver._parse(['--max-batches', '3'])
        self.assertEqual(args.max_batches, 3)
        d, err = driver.driver_from_args(['--max-batches', '0'], repo=self._tmpdir())
        self.assertEqual(err[0], 2)

    def test_the_state_is_committed_after_each_close_before_the_push(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        docs = os.path.join(self.root, 'docs', 'wso')
        os.makedirs(docs)
        state = os.path.join(docs, 'wave-state.json')
        shutil.move(self.state_path, state)
        d.state_path = state
        d.report_path = os.path.join(docs, 'xrec-report.md')
        _write(d.report_path, 'old\n')
        self.git('add', 'docs/wso/wave-state.json', 'docs/wso/xrec-report.md')
        self.git('commit', '-qm', 'state')
        _write(os.path.join(self.root, 'README'), 'dirty\n')   # not ours to commit
        d.write_scorecard = lambda sp, rp, tp, calibration_path=None: (
            _write(rp, 'scorecard\n') or (0, 'scorecard written'))
        at_push = []
        d.push = lambda: at_push.append(self.git('log', '-1', '--format=%s', 'first').stdout.strip())
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(at_push, ['wave-state: close b011 (ours 1, fork 0, neither 0)'])
        head = self.git('show', '--format=%B', '--name-only', 'first').stdout
        self.assertIn('Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>', head)
        names = self.git('show', '--format=', '--name-only', 'first').stdout.split()
        self.assertEqual(sorted(names), ['docs/wso/wave-state.json', 'docs/wso/xrec-report.md'])
        self.assertEqual(self.git('status', '--porcelain').stdout.strip(), 'M README')
        # Nothing to commit: no second commit.
        before = self.git('rev-parse', 'first').stdout
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d._commit_state('b011')
        self.assertEqual(self.git('rev-parse', 'first').stdout, before)

    # -- undecided rows (R73)

    @staticmethod
    def _load(path):
        with open(path) as f:
            return json.load(f)

    def _undecided_driver(self, report):
        d = self._pool_driver(self._two_pools()[:1],
                              {'a': driver.FakeWorker(report)}, ids=('b011',))
        self._state_in_repo(d)
        d.probe_path = os.path.join(self.root, 'docs', 'wso', 'probe-list.json')
        return d

    def test_an_undecided_row_moves_to_the_probe_list(self):
        d = self._undecided_driver(
            '## Verdicts\n\n## Undecided\nundecided 0x40B000 fork exe skips it\n')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        batch = self._load(d.state_path)['batches']['b011']
        self.assertEqual((batch['status'], batch['rows'], batch['files'],
                          batch['verdicts']), ('closed', [], [], {}))
        with open(d.probe_path) as f:
            entries = json.load(f)
        self.assertEqual(len(entries), 1)
        e = entries[0]
        self.assertEqual({k: e[k] for k in ('address', 'file', 'name', 'reading',
                                            'reason', 'batch', 'source')},
                         {'address': '0x40B000', 'file': 'src/F11.cpp', 'name': 'F11',
                          'reading': 'fork', 'reason': 'exe skips it',
                          'batch': 'b011', 'source': 'worker'})
        self.assertTrue(e['at'])
        with open(os.path.join(self.tmp, 'ledger.md')) as f:
            self.assertIn('- undecided (probe needed): 0x40B000 fork\n', f.read())
        # the state commit staged the probe list with the state
        names = self.git('show', '--format=', '--name-only', 'first').stdout.split()
        self.assertEqual(sorted(names), ['docs/wso/probe-list.json',
                                         'docs/wso/wave-state.json',
                                         'docs/wso/xrec-report.md'])
        self.assertEqual(self.git('status', '--porcelain').stdout.strip(), '')

    def test_a_cleared_record_retry_completes_an_interrupted_move(self):
        d = self._undecided_driver(
            '## Verdicts\n\n## Undecided\nundecided 0x40B000 fork exe skips it\n')
        wave = driver._tool('wave')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            real = d._move_undecided

            def crash(batch_id):
                with mock.patch.object(wave, 'save_state', side_effect=OSError('disk')):
                    real(batch_id)
            d._move_undecided = crash
            self.assertEqual(d.run(), 0)
            d._move_undecided = real
            # crashed after the probe-list write, before the state write
            self.assertEqual(len(self._load(d.probe_path)), 1)
            b = self._load(d.state_path)['batches']['b011']
            self.assertEqual((b['status'], len(b['rows'])), ('assigned', 1))
            d._retry_stage['b011'] = 'record'
            self.assertEqual(d._run_batch('b011', None), 'closed')
        b = self._load(d.state_path)['batches']['b011']
        self.assertEqual((b['status'], b['rows'], b['verdicts']), ('closed', [], {}))
        self.assertEqual([e['address'] for e in self._load(d.probe_path)], ['0x40B000'])
        with open(os.path.join(self.tmp, 'ledger.md')) as f:
            self.assertIn('- undecided (probe needed): 0x40B000 fork\n', f.read())

    def test_the_resume_preamble_sends_decided_ours_rows_to_undecided(self):
        self.assertIn('A row you decided `ours` before this resume although your '
                      'reading says fork or neither goes under ## Undecided now '
                      '(rule 13): keep our body and drop its verdict line.',
                      driver.RESUME_PREAMBLE.replace('\n', ' '))

    def test_commit_state_stages_the_probe_list_by_name(self):
        d = self._undecided_driver('x')
        _write(d.probe_path, '[]\n')
        _write(os.path.join(self.root, 'README'), 'dirty\n')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d._commit_state('b011')
        names = self.git('show', '--format=', '--name-only', 'first').stdout.split()
        self.assertEqual(names, ['docs/wso/probe-list.json'])
        self.assertEqual(self.git('status', '--porcelain').stdout.strip(), 'M README')

    def test_an_undecided_row_beside_a_verdict_row_keeps_the_other(self):
        d = self._undecided_driver('')
        state = self._load(d.state_path)
        b = state['batches']['b011']
        b['rows'].append({'address': '0x40B100', 'name': 'G', 'file': 'src/G.cpp',
                          'category': 'both_changed_differ', 'their_len': 1, 'our_len': 1})
        b['files'].append('src/G.cpp')
        _write(d.state_path, json.dumps(state))
        d._undecided = {'0x40b100': ('neither', 'why')}
        with d._lock:
            d._move_undecided('b011')
        b = self._load(d.state_path)['batches']['b011']
        self.assertEqual([r['address'] for r in b['rows']], ['0x40B000'])
        self.assertEqual(b['files'], ['src/F11.cpp'])
        # moving again does not duplicate the entry
        with d._lock:
            d._move_undecided('b011')
        self.assertEqual([e['address'] for e in self._load(d.probe_path)],
                         ['0x40B100'])

    def test_a_verdict_and_an_undecided_line_for_one_row_stop_the_batch(self):
        d = self._undecided_driver(
            '## Verdicts\nverdict 0x40B000 ours case dump-x\n'
            '## Undecided\nundecided 0x40B000 fork r\n')
        verdicts, problems = d._check('b011', d.worktree_for(self.root, 'p3/wso-b011'),
                                      d.workers_by_pool['a'].report)
        self.assertTrue(any('both a verdict line and an undecided line' in p
                            for p in problems), problems)

    def test_the_fix_round_header_names_undecided_and_notes(self):
        for needle in ('## Undecided', '## Notes', 'verdict lines only', 'Never write `ours`'):
            self.assertIn(needle, driver.FIX_PROMPT)
        with open(os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')) as f:
            text = f.read()
        for needle in ('undecided <0xADDR> <fork|neither> <one-line reason>',
                       'Never\n    write `ours` for a row your reading says is fork or neither.',
                       '`## Verdicts` holds verdict lines only; put notes under `## Notes`.'):
            self.assertIn(needle, text)

    # -- the pre-launch round (R71)

    def _state_in_repo(self, d):
        docs = os.path.join(self.root, 'docs', 'wso')
        os.makedirs(docs)
        state = os.path.join(docs, 'wave-state.json')
        shutil.move(self.state_path, state)
        d.state_path = state
        d.report_path = os.path.join(docs, 'xrec-report.md')
        _write(d.report_path, 'old\n')
        self.git('add', 'docs/wso/wave-state.json', 'docs/wso/xrec-report.md')
        self.git('commit', '-qm', 'state')
        d.write_scorecard = lambda sp, rp, tp, calibration_path=None: (
            _write(rp, 'scorecard\n') or (0, 'scorecard written'))

    def test_worker_env_drops_every_aoe_variable_for_every_wrapper(self):
        env = {'PATH': '/bin', 'AOE_INSTANCE_ID': 'i', 'AOE_HOOK_BIN': '/h',
               'AOE_SERVE_PASSPHRASE': 'p', 'AOE_AGENT_BIN': 'a',
               'AOE_AGENT_PID': '1', 'AOE_PROFILE': 'x', 'AOE_SERVE_INSTANCE_ID': 's',
               'AOE_FUTURE_THING': 'f', 'NOT_AOE': 'keep'}
        for glm in (True, False):
            self.assertEqual(driver.worker_env(env, glm), {
                'PATH': '/bin', 'NOT_AOE': 'keep', 'CLAUDE_CODE_DISABLE_AUTO_MEMORY': '1'})
        with mock.patch.dict(os.environ, env, clear=True):
            for wrapper in ('claude-glm', 'claude'):
                _, e = driver.HeadlessWorker(wrapper=wrapper).command('B')
                self.assertFalse([k for k in e if k.startswith('AOE_')], wrapper)

    def test_aoe_send_drops_the_claude_code_session_but_keeps_aoe(self):
        env = {'PATH': '/bin', 'CLAUDECODE': '1', 'CLAUDE_CODE_ENTRYPOINT': 'cli',
               'CLAUDE_CODE_SESSION_ID': 's', 'CLAUDE_PID': '9', 'CLAUDE_EFFORT': 'hi',
               'AOE_INSTANCE_ID': 'i', 'AOE_PROFILE': 'x', 'CLAUDE_CONFIG_DIR': '/c'}
        self.assertEqual(driver.send_env(env), {
            'PATH': '/bin', 'AOE_INSTANCE_ID': 'i', 'AOE_PROFILE': 'x',
            'CLAUDE_CONFIG_DIR': '/c'})
        d = self._pool_driver(self._two_pools()[:1], {'a': None}, ids=('b011',))
        d.aoe_session = 'sess'
        seen = []
        with mock.patch.dict(os.environ, env, clear=True), \
                mock.patch.object(driver.subprocess, 'run', lambda argv, **kw: seen.append(
                    (argv, kw)) or subprocess.CompletedProcess(argv, 0, '', '')):
            d._aoe_send('hello')
        self.assertEqual(seen[0][0], ['aoe', 'send', 'sess', 'hello'])
        self.assertNotIn('CLAUDECODE', seen[0][1]['env'])
        self.assertEqual(seen[0][1]['env']['AOE_INSTANCE_ID'], 'i')

    def test_the_aoe_line_never_carries_problem_text(self):
        w = self._BatchWorker(credits=0.0)
        w.run = lambda brief, cwd: driver.Result(0, '', None, 0.0, False)   # no report
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self.sent, ['driver: b011 exception at worker; see %s' % d.status_path])

    def test_probe_phrase_applies_only_to_driver_recorded_probe_passes(self):
        d = self._mc_repo({'76D370': 'kind: oracle\ndisagreed with WSO at abc',
                           '401000': 'kind: oracle\ndisagreed with WSO at abc'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:2]
        ok = self._mc_run(0, [])
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            # A worker's own probe-17 fork verdict: the normal phrase rule.
            self.assertEqual(driver.mechanical_checks(
                batch, d, {'0x76D370': ('fork', 'probe-17'), '0x401000': ('ours', 'probe-17')},
                run=ok), [])
            # probe-<this batch>-... with side ours is the driver's form.
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'probe-b011-76D370'), '0x401000': ('ours', 'x')},
                run=ok)
            self.assertEqual(len(probs), 1)
            self.assertIn("'corroborated by WSO at <sha>'", probs[0])
            # The driver's probe set decides whatever the case says.
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('ours', 'x'), '0x401000': ('ours', 'x')},
                run=ok, probe=['0x401000'])
            self.assertEqual(len(probs), 1)
            self.assertIn('401000.md', probs[0])

    def test_a_fork_verdict_needs_a_kind_line_in_the_evidence(self):
        d = self._mc_repo({'76D370': 'corroborated by WSO at abc',
                           '401000': 'kind: oracle\ncorroborated by WSO at abc'})
        batch = self._mc_batch()
        batch['rows'] = batch['rows'][:2]
        with mock.patch.object(driver, '_vanilla_exe', return_value='/nonexistent'):
            probs = driver.mechanical_checks(
                batch, d, {'0x76D370': ('fork', 'probe-3'), '0x401000': ('fork', 'probe-4')},
                run=self._mc_run(0, []))
            self.assertEqual(len(probs), 1)
            self.assertIn("76D370.md: no 'kind:' line", probs[0])
            self.assertEqual(driver.mechanical_checks(      # ours needs none
                batch, d, {'0x76D370': ('ours', 'c'), '0x401000': ('ours', 'c')},
                run=self._mc_run(0, [])), [])

    def test_subagent_transcripts_are_scanned_and_counted(self):
        tmp = self._tmpdir()
        main = os.path.join(tmp, 'sess1.jsonl')
        with open(main, 'w') as f:
            f.write(json.dumps({'type': 'assistant', 'message': {
                'id': 'm1', 'usage': {'input_tokens': 10, 'output_tokens': 5},
                'content': [{'type': 'tool_use', 'id': 't1', 'name': 'Bash',
                             'input': {'command': 'ls'}}]}}) + '\n')
        self.assertEqual(driver.transcript_checks(main, []), [])
        self.assertEqual(driver.subagent_transcripts(main), [])
        sub = os.path.join(tmp, 'sess1', 'subagents')
        os.makedirs(sub)
        with open(os.path.join(sub, 'agent-1.jsonl'), 'w') as f:
            f.write(json.dumps({'type': 'assistant', 'message': {
                'id': 'm2', 'usage': {'input_tokens': 100, 'output_tokens': 50},
                'content': [{'type': 'tool_use', 'id': 't2', 'name': 'Bash',
                             'input': {'command': 'git show wso/main:x'}},
                            {'type': 'tool_use', 'id': 't3', 'name': 'Bash',
                             'input': {'command': 'ls'}}]}}) + '\n')
        probs = driver.transcript_checks(main, [])
        self.assertEqual(len(probs), 1)
        self.assertIn('wso/', probs[0])
        tokens, calls = driver.usage_all(main)
        self.assertEqual((tokens['input'], tokens['output'], calls), (110, 55, 3))
        self.assertEqual(driver.usage_of(main)[1], 1)

    def test_a_plain_claude_worker_is_isolated(self):
        with mock.patch.dict(os.environ, {'PATH': '/bin'}, clear=True):
            argv, env = driver.HeadlessWorker(model='sonnet', wrapper='claude').command('B')
            self.assertEqual(argv[argv.index('--setting-sources') + 1], 'project,local')
            self.assertIn('--strict-mcp-config', argv)
            self.assertNotIn('--mcp-config', argv)
            self.assertEqual(env['ENABLE_CLAUDEAI_MCP_SERVERS'], '0')
            argv, env = driver.HeadlessWorker().command('B')       # claude-glm
            self.assertNotIn('--setting-sources', argv)
            self.assertNotIn('--strict-mcp-config', argv)
            self.assertNotIn('ENABLE_CLAUDEAI_MCP_SERVERS', env)

    def test_the_worker_runs_without_stdin_and_with_a_timeout(self):
        seen = []

        def run(argv, **kw):
            seen.append(kw)
            raise subprocess.TimeoutExpired(argv, kw['timeout'])
        w = driver.HeadlessWorker(run=run, timeout=7)
        res = w.run('B', '/tmp')
        self.assertEqual((seen[0]['stdin'], seen[0]['timeout']), (subprocess.DEVNULL, 7))
        self.assertTrue(res.timed_out)
        self.assertEqual(driver.HeadlessWorker().timeout, 10800)
        args, _ = driver._parse(['--worker-timeout', '60'])
        self.assertEqual(args.worker_timeout, 60)

    def test_run_group_kills_the_whole_process_group_on_timeout(self):
        tmp = self._tmpdir()
        mark = os.path.join(tmp, 'child.pid')
        code = ("import subprocess,sys,time;"
                "p=subprocess.Popen(['sleep','60']);"
                "open(sys.argv[1],'w').write(str(p.pid));time.sleep(60)")
        with self.assertRaises(subprocess.TimeoutExpired):
            driver._run_group([sys.executable, '-c', code, mark], timeout=1.5,
                              stdin=subprocess.DEVNULL, text=True, capture_output=True)
        pid = int(open(mark).read())
        time.sleep(0.3)
        try:
            os.kill(pid, 0)
            alive = True
        except OSError:
            alive = False
        self.assertFalse(alive)

    def test_a_timeout_parks_the_batch_and_the_run_exits_3(self):
        def run(argv, **kw):
            raise subprocess.TimeoutExpired(argv, 1)
        w = driver.HeadlessWorker(run=run)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011', 'b012'))
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 3)
        parked = self._status(d)['pools']['a']['parked']
        self.assertEqual(parked['b011']['reason'], 'timeout')
        self.assertEqual(self._state_of(d, 'b011')['status'], 'assigned')
        self.assertEqual(self._status(d)['exceptions'], {})

    def _clear(self, d, bid):
        self.assertEqual(driver.clear_exception(d.status_path, bid)[0], 0)

    def test_a_cleared_close_exception_goes_straight_to_close(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        real = d.close_batch
        d.close_batch = lambda *a, **kw: (1, 'batch b011 not closed:\n  - gate\n')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'close')
        self.assertEqual(len(w.briefs), 1)
        self._clear(d, 'b011')
        d.close_batch = real
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(w.briefs), 1)               # the worker did not run again
        self.assertEqual(self._state_of(d, 'b011')['status'], 'closed')
        self.assertEqual(self._status(d)['batches']['b011']['outcome'], 'closed')

    def test_a_cleared_record_exception_records_the_saved_verdicts(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        real = d.record_verdict
        d.record_verdict = lambda *a, **kw: (1, 'no way\n')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'record')
        self._clear(d, 'b011')
        d.record_verdict = real
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(w.briefs), 1)
        self.assertEqual(self._state_of(d, 'b011')['status'], 'closed')
        self.assertEqual(len(self._state_of(d, 'b011')['verdicts']), 1)

    def test_a_cleared_push_exception_pushes_again_without_the_worker(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        results = iter(['rejected', None])
        d.push = lambda: self.pushes.append(1) or next(results)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'push')
        self._clear(d, 'b011')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(self.pushes), 2)
        self.assertEqual(len(w.briefs), 1)
        self.assertEqual(self._status(d)['batches']['b011']['outcome'], 'closed')

    def test_a_cleared_worker_exception_restarts_the_worker_with_a_resume_preamble(self):
        w = self._BatchWorker(credits=0.0)
        norun = w.run
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        first = []
        w.run = lambda brief, cwd: (first.append(brief) or driver.Result(0, '', None, 0.0, False)) \
            if not first else norun(brief, cwd)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            d.run()
        self.assertEqual(self._status(d)['exceptions']['b011']['stage'], 'worker')
        self._clear(d, 'b011')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertTrue(w.briefs[0].startswith('Resume:'), w.briefs[0][:80])
        self.assertIn('worker stage', w.briefs[0])

    def test_a_cleared_adoption_failure_is_adopted_again(self):
        d, w = self._adopt_driver()
        d.next_batch = lambda *a, **kw: self.NOTHING
        real = d.cut_worktree
        d.cut_worktree = lambda *a, **kw: (None, 'git worktree add failed')
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 1)
        self.assertEqual(self._status(d)['exceptions']['b012']['stage'], 'cut')
        self.assertEqual(w.ids, [])
        self._clear(d, 'b012')
        d.cut_worktree = real
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b012', 'b011'])
        self.assertEqual(self._state_of(d, 'b012')['rows'][0]['class'], 'ambiguous')

    def test_an_adopted_batch_is_cut_when_it_starts_not_at_the_scan(self):
        seen = {}
        d, w = self._adopt_driver()
        d.next_batch = lambda *a, **kw: self.NOTHING

        def before(cwd):
            bid = os.path.basename(cwd)[len('p3-wso-'):]
            other = 'b011' if bid == 'b012' else 'b012'
            seen[bid] = (
                os.path.isdir(os.path.join(self.tmp, 'repo-worktrees', 'p3-wso-' + other)),
                self.git('rev-parse', 'HEAD', cwd=cwd).stdout.strip(),
                self.git('rev-parse', 'first').stdout.strip())
            if bid == 'b012':                       # the first batch moves first
                _write(os.path.join(cwd, 'src', 'new.cpp'), 'x\n')
                self.git('add', 'src/new.cpp', cwd=cwd)
                self.git('commit', '-qm', 'work', cwd=cwd)
        w.before = before
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(w.ids, ['b012', 'b011'])
        self.assertFalse(seen['b012'][0])             # b011 not cut while b012 ran
        self.assertEqual(seen['b012'][1], seen['b012'][2])
        self.assertEqual(seen['b011'][1], seen['b011'][2])    # cut from first as moved
        self.assertNotEqual(seen['b011'][1], seen['b012'][1])
        self.assertIn('b011', d._imported)

    def test_a_failed_state_commit_unstages_sends_one_line_and_still_pushes(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        self._state_in_repo(d)
        hook = os.path.join(self.root, '.git', 'hooks', 'pre-commit')
        _write(hook, '#!/bin/sh\nexit 1\n')
        os.chmod(hook, 0o755)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(self.pushes), 1)
        self.assertEqual(self.sent, ['driver: b011 exception at state-commit; see %s' % d.status_path])
        self.assertEqual(self.git('diff', '--cached', '--name-only').stdout.strip(), '')
        self.assertEqual(self._status(d)['exceptions'], {})

    def test_an_exception_in_the_state_commit_never_skips_the_push(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        orig = d.worktree_for
        base = d.base
        d.worktree_for = lambda repo, branch: (_ for _ in ()).throw(RuntimeError('boom')) \
            if branch == base else orig(repo, branch)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(self.pushes), 1)
        self.assertTrue(any('state commit failed' in l and 'boom' in l for l in self.logs))

    def test_settle_closed_commits_the_state_before_its_push(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        order = []
        d._commit_state = lambda bid: order.append(('commit', bid))
        d.push = lambda: order.append('push')
        d._append_ledger('## driver b011 (x)\n')
        self.assertIsNone(d._settle_closed('b011'))
        self.assertEqual(order, [('commit', 'b011'), 'push'])

    def test_the_import_queue_does_not_commit_the_state(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        self._state_in_repo(d)
        d.kind = 'import'
        _write(d.state_path, '{}\n')
        before = self.git('rev-parse', 'first').stdout
        d._commit_state('b011')
        self.assertEqual(self.git('rev-parse', 'first').stdout, before)

    def test_the_brief_template_carries_the_launch_rules(self):
        with open(os.path.join(ROOT, 'docs', 'wso', 'brief-template.md')) as f:
            text = f.read()
        for needle in ('wso/main', 'systemd-run --user --scope -q -p MemoryMax=8G '
                       '-p MemorySwapMax=0 --', 'python3 -I tools/gen_map.py',
                       '--host', 'desktop'):
            self.assertIn(needle, text)

    def test_setting_the_desktop_override_is_a_launch_finding(self):
        setters = ['IWD2R_DESKTOP_OK=1 python3 -I tools/oracle.py replay record --host',
                   'export IWD2R_DESKTOP_OK=1', 'env IWD2R_DESKTOP_OK=1 python3 x.py',
                   "python3 -c \"import os; os.environ['IWD2R_DESKTOP_OK']='1'\"",
                   'cd x && export IWD2R_DESKTOP_OK', 'set IWD2R_DESKTOP_OK=1']
        for cmd in setters:
            found = driver.transcript_findings(
                self._tcalls(('Bash', {'command': cmd})), [])
            self.assertEqual([k for k, _t, _f in found].count('launch'), 1, cmd)
        for cmd in ('grep -n DESKTOP_OK tools/play.py', 'ls', 'echo $IWD2R_DESKTOP_OK'):
            self.assertEqual(driver.transcript_findings(
                self._tcalls(('Bash', {'command': cmd})), []), [], cmd)
        self.assertIn('Never set IWD2R_DESKTOP_OK', open(os.path.join(
            ROOT, 'docs', 'wso', 'brief-template.md')).read())

    def test_the_desktop_refusals_say_a_person_must_be_at_the_pc(self):
        play = load_tool('play')
        self.assertIn('need a person at the PC (IWD2R_DESKTOP_OK is set only by them)',
                      play.desktop_refusal(False, {}))
        self.assertIsNone(play.desktop_refusal(False, {'IWD2R_DESKTOP_OK': '1'}))
        oracle = load_tool('oracle')
        err = io.StringIO()
        env = {k: v for k, v in os.environ.items() if k != 'IWD2R_DESKTOP_OK'}
        with mock.patch.dict(os.environ, env, clear=True), \
                contextlib.redirect_stderr(err), self.assertRaises(SystemExit):
            oracle.main(['replay', 'compare', 'chargen_replay', '--host'])
        self.assertIn('need a person at the PC (IWD2R_DESKTOP_OK is set only by them)',
                      ' '.join(err.getvalue().split()))
        with open(os.path.join(ROOT, 'tools', 'gltrace.py')) as f:
            text = ' '.join(f.read().split())
        self.assertIn("'desktop runs need a person at the PC (%s is set ' 'only by them)'", text)

    def test_sigterm_kills_the_workers_and_leaves_the_batch_running(self):
        sleeper = subprocess.Popen(['sleep', '60'], start_new_session=True)
        self.addCleanup(lambda: sleeper.poll() is None and sleeper.kill())
        w = self._BatchWorker(credits=0.0)

        def run(brief, cwd):
            driver._LIVE.add(sleeper.pid)
            os.kill(os.getpid(), signal.SIGTERM)
            time.sleep(5)                        # the handler raises before this ends
            self.fail('not stopped')
        w.run = run
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        before = (signal.getsignal(signal.SIGTERM), signal.getsignal(signal.SIGINT))
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 1)
        driver._LIVE.discard(sleeper.pid)
        self.assertIsNotNone(sleeper.wait(timeout=5))
        self.assertEqual((signal.getsignal(signal.SIGTERM), signal.getsignal(signal.SIGINT)), before)
        status = self._status(d)
        self.assertIn('b011', status['running'])
        self.assertEqual(status['exceptions'], {})
        self.assertEqual(self._state_of(d, 'b011')['status'], 'assigned')
        # The run lock was released: another run starts (and resumes b011).
        d.worker = d.workers_by_pool['a'] = self._BatchWorker(credits=0.0)
        self.pending[:] = []
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)

    def test_a_dry_run_never_commits_the_state(self):
        d = self._pool_driver(self._two_pools()[:1], {'a': None}, ids=('b011',))
        d.dry_run = True
        docs = os.path.join(self.root, 'docs', 'wso')
        os.makedirs(docs)
        d.state_path = os.path.join(docs, 'wave-state.json')
        _write(d.state_path, '{}\n')
        before = self.git('rev-parse', 'first').stdout
        d._commit_state('b011')
        self.assertEqual(self.git('rev-parse', 'first').stdout, before)
        self.assertNotIn('wave-state.json', self.git('diff', '--cached', '--name-only').stdout)

    def test_a_transcript_violation_on_the_first_run_does_not_trip_the_breaker(self):
        """R68: the scan runs after the run's bookkeeping, so the run still
        counts as a worker run and its evidence is saved."""
        bad = self._tcalls(('Bash', {'command': 'git show wso/main:src/F11.cpp'}))
        with open(bad, 'a') as f:      # the worker really ran: a successful turn
            f.write(json.dumps(self._turn()) + '\n')
        w = self._twork([bad])
        d = self._pool_driver(self._two_pools()[:1], {'a': w})
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        status = self._status(d)
        self.assertEqual(sorted(status['exceptions']), ['b011', 'b012', 'b013'])
        self.assertTrue(all(e['stage'] == 'transcript'
                            for e in status['exceptions'].values()))
        self.assertIsNone(status['halted'])
        self.assertEqual(len(w.briefs), 3)
        for bid in ('b011', 'b012', 'b013'):
            for name in ('worker.json', 'worker.report.md', 'worker.brief.md'):
                self.assertTrue(os.path.exists(os.path.join(d.driver_dir, bid, name)))

    def test_frida_run_passes_new_attempt(self):
        truth = driver._tool('truth')
        cmds = []

        def fake(cmd, **kw):
            cmds.append(cmd)
            return subprocess.CompletedProcess(cmd, 1, '', '')
        truth.frida_run('b011', {'original': {}, 'ours': {}}, ['s1'], run=fake,
                        root=self._tmpdir(), loaded_rva='0x1')
        self.assertTrue(cmds)
        for c in cmds:
            self.assertEqual(c[c.index('--new-attempt') + 1], 'driver frida b011')

    def test_union_merge_attribute_lets_parallel_probe_lines_merge(self):
        anchor = driver._tool('truth').CMAKE_ANCHOR

        def edit(wt, bid):
            path = os.path.join(wt, 'oracle', 'CMakeLists.txt')
            with open(path) as f:
                text = f.read()
            with open(path, 'w') as f:
                f.write(text.replace(anchor, anchor + '\n        cases/wave-%s.cpp' % bid, 1))
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011', 'b012'),
                              base_files={'oracle/CMakeLists.txt': 'add(\n        %s\n        z.cpp)\n' % anchor},
                              on_branch=edit)
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        text = self.git('show', 'first:oracle/CMakeLists.txt').stdout
        self.assertIn('cases/wave-b011.cpp', text)
        self.assertIn('cases/wave-b012.cpp', text)
        attrs = os.path.join(self.root, '.git', 'info', 'attributes')
        d._union_merge_attributes()          # idempotent
        with open(attrs) as f:
            self.assertEqual(f.read().splitlines().count(d.UNION_LINE), 1)

    def test_close_retries_once_when_the_base_moved_during_the_gate(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools()[:1], {'a': w}, ids=('b011',))
        real = d.close_batch
        calls = []

        def close(*a, **kw):
            calls.append(1)
            if len(calls) == 1:   # the other queue's driver wins the race
                self.git('commit', '-q', '--allow-empty', '-m', 'other driver')
            return real(*a, **kw)
        d.close_batch = close
        with mock.patch.dict(os.environ, self.GIT_ENV):
            self.assertEqual(d.run(), 0)
        self.assertEqual(len(calls), 2)
        self.assertEqual(self.git('rev-parse', 'first').stdout,
                         self.git('rev-parse', 'p3/wso-b011').stdout)
        self.assertEqual(self._status(d)['exceptions'], {})

    def test_halted_clears_at_run_start_and_stops_launches(self):
        d = self._pool_driver(self._two_pools(), {'a': None, 'b': None}, ids=('b011', 'b012'))
        self._write_status(d, {'halted': {'at': 't', 'reason': 'old'}})
        ran = []

        def run_batch(bid):
            ran.append(bid)
            d._halt = True       # as _halt_run does
            return 'closed'
        d.run_batch = run_batch
        d.run()
        self.assertEqual(ran, ['b011'])      # nothing launched after the halt
        d2 = self._pool_driver(self._two_pools(), {'a': None, 'b': None}, ids=())
        self._write_status(d2, {'halted': {'at': 't', 'reason': 'old'}})
        self.assertEqual(d2.run(), 0)
        self.assertIsNone(self._status(d2)['halted'])

    def test_a_cut_failure_carries_no_figures_of_the_last_batch(self):
        ticks = iter([1.0, 2.0, 3.0, 4.0])
        d = self._driver(worker=driver.FakeWorker(self.REPORT), clock=lambda: next(ticks))
        cuts = iter([(0, self.CUT), (1, 'b012 not assigned: boom\n')])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 1)
        self.assertIsNone(d._ctx.t0)
        entry = self._status(d)['batches']['b012']
        self.assertEqual(entry['outcome'], 'exception')
        self.assertNotIn('wall', entry)
        self.assertNotIn('tokens', entry)

    def test_a_pending_resume_on_a_busy_pool_does_not_block_other_pools(self):
        w = self._BatchWorker(credits=0.0)
        d = self._pool_driver(self._two_pools(), {'a': None, 'b': w}, ids=('b011', 'b012'))
        self.pending[:] = ['b012']
        ran = []
        d.run_batch = lambda bid: ran.append((bid, d._ctx.pool)) or 'closed'
        d._init_pools()
        d._busy['a'] = 1            # pool a is full, so b011 must wait
        d._schedule([('b011', 'why', 'a')], [])
        self.assertEqual(ran, [('b012', 'b')])

    # -- the second queue ------------------------------------------------------

    def _import_repo(self):
        tmp = self._tmpdir()
        self.root = os.path.join(tmp, 'repo')
        os.makedirs(os.path.join(self.root, 'docs', 'wso'))
        self.git('init', '-q', '-b', 'first')
        _write(os.path.join(self.root, 'README'), 'x\n')
        self.git('add', '-A')
        self.git('commit', '-qm', 'base')
        rows = [{'file': 'src/A.cpp', 'name': 'F', 'address': '0x401000',
                 'category': 'theirs_real_ours_stub', 'their_len': 90, 'our_len': 2},
                {'file': 'src/B.cpp', 'name': 'G', 'address': '0x402000',
                 'category': 'both_changed_differ', 'their_len': 50, 'our_len': 40},
                {'file': 'src/C.cpp', 'name': 'H', 'address': '0x403000',
                 'category': 'both_changed_differ', 'their_len': 70, 'our_len': 60}]
        dp = os.path.join(tmp, 'disagreements.json')
        ip = os.path.join(tmp, 'import-list.json')
        with open(dp, 'w') as f:
            json.dump(rows, f)
        with open(ip, 'w') as f:
            json.dump([{'address': '0x403000', 'file': 'src/C.cpp', 'name': 'H',
                        'source': 'triage'},
                       {'address': '0x404000', 'file': 'src/D.cpp', 'name': 'I',
                        'source': 'triage'}], f)
        return tmp, dp, ip

    def test_queue_import_uses_its_own_state_and_ids(self):
        tmp, dp, ip = self._import_repo()
        wave = driver._tool('wave')
        imp = os.path.join(tmp, 'import-state.json')
        rc, out = wave.next_batch(dp, imp, 20, wave.IMPORT_CATEGORY, repo=self.root,
                                  import_path=ip, worktree_root=os.path.join(tmp, 'wts'),
                                  queue='import')
        self.assertEqual(rc, 0, out)
        self.assertTrue(out.startswith('i001 assigned: 3 rows'), out)
        with open(imp) as f:
            b = json.load(f)['batches']['i001']
        self.assertEqual(b['branch'], 'p3/wso-i001')
        self.assertEqual(sorted(r['address'] for r in b['rows']),
                         ['0x401000', '0x403000', '0x404000'])
        self.assertTrue(all('class' not in r for r in b['rows']))   # no triage
        self.assertEqual(b['verdicts'], {})
        # The import-list row took its lengths from disagreements.json.
        self.assertEqual({r['address']: r['their_len'] for r in b['rows']}['0x403000'], 70)
        self.assertTrue(os.path.isdir(os.path.join(tmp, 'wts', 'p3-wso-i001')))
        self.assertFalse(os.path.exists(os.path.join(tmp, 'wave-state.json')))
        rc, out = wave.next_batch(dp, imp, 20, wave.IMPORT_CATEGORY, repo=self.root,
                                  import_path=ip, worktree_root=os.path.join(tmp, 'wts'),
                                  queue='import')
        self.assertEqual(rc, 2, out)
        self.assertTrue(out.startswith('nothing left'))
        # wave1 numbering is its own: b001, whatever import cut.
        w1 = os.path.join(tmp, 'wave-state.json')
        with mock.patch.object(wave, '_cut_worktree', return_value=('/wt', 'new')):
            rc, out = wave.next_batch(dp, w1, 20, repo=self.root, import_path=ip)
        self.assertTrue(out.startswith('b001 assigned'), out)

    def test_queue_import_driver_wiring(self):
        tmp, dp, ip = self._import_repo()
        d, err = driver.driver_from_args(
            ['--queue', 'import', '--pool', 'claude', '--state',
             os.path.join(tmp, 'import-state.json')], repo=tmp)
        self.assertIsNone(err)
        self.assertEqual((d.queue, d.kind, d.category), ('import', 'import', 'theirs_real_ours_stub'))
        self.assertEqual(d.pools[0].model, 'haiku')
        self.assertEqual(os.path.basename(d.status_path), 'status-import.json')
        self.assertEqual(os.path.basename(d.template_path), 'import-brief-template.md')
        self.assertEqual(d.workers_by_pool['claude'].model, 'haiku')
        w1, err = driver.driver_from_args(
            ['--state', os.path.join(tmp, 'wave-state.json')], repo=tmp)
        self.assertEqual(os.path.basename(w1.status_path), 'status.json')
        self.assertEqual(w1.kind, 'verdicts')
        self.assertNotEqual(d.status_path + '.run.lock', w1.status_path + '.run.lock')
        # The cut passes the queue, and no triage for it.
        calls = []
        d.log = lambda m: None
        d.next_batch = lambda *a, **kw: calls.append(kw) or (2, 'nothing left: x\n')
        self.assertEqual((None, 0), d._cut())
        self.assertEqual((calls[0]['queue'], calls[0]['triage_path']), ('import', None))
        # --clear reads its queue's status file.
        status = os.path.join(tmp, driver.status_rel('import'))
        _write(status, json.dumps({'exceptions': {'i001': {'stage': 'worker'}}}))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(driver.main(['--queue', 'import', '--clear', 'i001'], repo=tmp), 0)
        with open(status) as f:
            self.assertEqual(json.load(f)['exceptions'], {})

    def test_import_brief_template_renders(self):
        tmp = self._tmpdir()
        wave = driver._tool('wave')
        state = os.path.join(tmp, 'import-state.json')
        with open(state, 'w') as f:
            json.dump({'fork_base': 'e8dd16a7', 'batches': {'i001': {
                'rows': [{'address': '0x401000', 'name': 'F', 'file': 'src/A.cpp',
                          'their_len': 90, 'our_len': 2}],
                'files': ['src/A.cpp'], 'branch': 'p3/wso-i001', 'status': 'assigned',
                'verdicts': {}}}}, f)
        rc, text = wave.render_brief(wave.IMPORT_TEMPLATE_PATH, state, 'i001',
                                     bundle_dir=os.path.join(tmp, 'b'),
                                     bundle_fn=lambda a: 'bundle %x' % a)
        self.assertEqual(rc, 0, text)
        self.assertNotIn('{{', text)
        for needle in ('i001', 'p3/wso-i001', '`src/A.cpp`', '`0x401000` **F**',
                       os.path.join(tmp, 'b', '0x401000.md'), 'Imported-From:',
                       'close --kind import', 'trace', 'e8dd16a7'):
            self.assertIn(needle, text)
        self.assertNotIn('verdict <0xADDR>', text)
        self.assertIn('No verdicts', text)

    def test_import_dry_run_refuses_the_live_import_state(self):
        tmp = self._tmpdir()
        live = driver._tool('wave').IMPORT_STATE_PATH
        d, err = driver.driver_from_args(['--queue', 'import', '--dry-run', '--state', live],
                                         repo=tmp)
        self.assertEqual((d, err[0]), (None, 2))
        self.assertIn('live import-state file', err[1])

    def test_import_batch_closes_without_verdict_lines(self):
        d = self._driver(worker=driver.FakeWorker('# report\n\nImported; no verdicts.\n'),
                         queue='import')
        with open(self.state_path) as f:
            state = json.load(f)
        b = state['batches'].pop('b011')
        b['rows'] = [dict(r) for r in b['rows'][:2]]
        for r in b['rows']:
            r.pop('class')
        b['files'], b['verdicts'] = ['src/A.cpp'], {}
        state['batches']['i001'] = b
        with open(self.state_path, 'w') as f:
            json.dump(state, f)
        self.assertEqual(d.run_batch('i001'), 'closed')
        with open(self.state_path) as f:
            closed = json.load(f)['batches']['i001']
        self.assertEqual((closed['status'], closed['verdicts']), ('closed', {}))
        self.assertEqual(self.git('rev-parse', 'first').stdout,
                         self.git('rev-parse', 'p3/wso-b011').stdout)
        self.assertIn('verdicts: fork 0, ours 0, neither 0', self._ledger(d))
        # An import state never rewrites wave 1's scorecard.
        self.assertFalse(os.path.exists(d.report_path))
        # The import checks skip the verdict lines a verdict batch needs.
        batch = {'branch': 'x', 'rows': [{'address': '0x401000'}]}
        self.assertEqual(self._checks(batch, self.wt, {}, kind='import'), [])
        self.assertTrue(self._checks(batch, self.wt, {})[0].startswith('0x401000: no verdict line'))

    # -- per-batch figures (R61) -------------------------------------------------

    @staticmethod
    def _transcript(tmp, lines):
        path = os.path.join(tmp, 'sess.jsonl')
        with open(path, 'w') as f:
            f.write('\n'.join(json.dumps(x) if not isinstance(x, str) else x
                              for x in lines) + '\n')
        return path

    def test_usage_of_dedups_messages_and_counts_tool_calls(self):
        tmp = self._tmpdir()
        use = lambda i, c, cr, o: {'input_tokens': i, 'cache_creation_input_tokens': c,
                                   'cache_read_input_tokens': cr, 'output_tokens': o}
        tool = lambda i: {'type': 'tool_use', 'id': i, 'name': 'Bash', 'input': {}}
        path = self._transcript(tmp, [
            {'type': 'assistant', 'message': {'id': 'm1', 'model': 'claude-sonnet',
             'usage': use(10, 5, 100, 1), 'content': [tool('t1')]}},
            {'type': 'assistant', 'message': {'id': 'm1', 'model': 'claude-sonnet',
             'usage': use(10, 5, 100, 20), 'content': [tool('t2')]}},
            {'type': 'assistant', 'message': {'id': 'm1', 'model': 'claude-sonnet',
             'usage': use(10, 5, 100, 20), 'content': [tool('t2')]}},
            {'type': 'user', 'message': {'content': [{'type': 'tool_result'}]}},
            'not json',
            {'type': 'assistant', 'message': {'id': 'm2', 'model': 'glm-5.3-flash',
             'usage': use(1, 0, 2, 3), 'content': [tool('t3'), {'type': 'text', 'text': 'x'}]}},
            {'type': 'assistant', 'message': {'model': 'glm-5.3-flash',
             'usage': use(1, 1, 1, 1), 'content': [dict(tool('t4'), id=None)]}},
        ])
        tokens, calls = driver.usage_of(path)
        self.assertEqual(tokens, {'input': 15 + 1 + 2, 'cached': 100 + 2 + 1,
                                  'output': 20 + 3 + 1})
        self.assertEqual(calls, 4)
        # Credits stay GLM-only, from the same file.
        self.assertGreater(driver.credits_of(path), 0)
        self.assertEqual(driver.credits_of(self._transcript(tmp, [
            {'message': {'id': 'm', 'model': 'claude-sonnet', 'usage': use(9, 9, 9, 9)}}])), 0.0)
        self.assertEqual(driver.usage_of(self._transcript(tmp, ['{}'])),
                         ({'input': 0, 'cached': 0, 'output': 0}, 0))

    def test_batch_figures_reach_status_and_ledger(self):
        tmp = self._tmpdir()
        path = self._transcript(tmp, [
            {'type': 'assistant', 'message': {'id': 'm1', 'model': 'claude-sonnet',
             'usage': {'input_tokens': 7, 'cache_read_input_tokens': 50, 'output_tokens': 9},
             'content': [{'type': 'tool_use', 'id': 't1'}, {'type': 'tool_use', 'id': 't2'}]}}])

        class W:
            def run(self_, brief, cwd):
                return driver.Result(0, DriverTest.REPORT, path, 0.0, False)

        ticks = iter([100.0, 193.4])
        d = self._driver(worker=W(), clock=lambda: next(ticks))
        self.assertEqual(d.run_batch('b011'), 'closed')
        entry = self._status(d)['batches']['b011']
        self.assertEqual(entry['tokens'], {'input': 7, 'cached': 50, 'output': 9})
        self.assertEqual((entry['tool_calls'], entry['wall']), (2, 93.4))
        ledger = self._ledger(d)
        self.assertIn('- wall 93s\n', ledger)
        self.assertIn('- tokens input 7, cached 50, output 9\n', ledger)
        self.assertIn('- tool calls 2\n', ledger)

    def test_wall_runs_from_the_cut(self):
        ticks = iter([50.0, 200.0])
        d = self._driver(worker=driver.FakeWorker(self.REPORT), clock=lambda: next(ticks))
        cuts = iter([(0, self.CUT), self.NOTHING])
        d.next_batch = lambda *a, **kw: next(cuts)
        self.assertEqual(d.run(), 0)
        self.assertEqual(self._status(d)['batches']['b011']['wall'], 150.0)


class TruthTest(unittest.TestCase):
    def test_is_pure_accepts_arithmetic_on_args(self):
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a,int b){return (a*3+b)%7;}\n')
        self.assertTrue(ok)

    def test_is_pure_rejects_global_through_pointer_chain(self):
        ok, why = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return *(int*)(DAT_008cf6d8+0x1c54)+a;}\n')
        self.assertFalse(ok); self.assertIn('DAT_008cf6d8', why)

    def test_is_pure_rejects_unknown_callee(self):
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return Helper(a);}\n')
        self.assertFalse(ok)
        ok, _ = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){return Helper(a);}\n', frozenset({'Helper'}))
        self.assertTrue(ok)

    def test_is_pure_ignores_keywords_casts_and_the_signature(self):
        text = ('## Decompiled (Ghidra)\n\n```c\nuint Foo::Bar(int a, uint b)\n\n{\n'
                '  if (a < 0) {\n    return (uint)b;\n  }\n'
                '  while (b != 0) {\n    b = b - sizeof(int);\n  }\n'
                '  return (int)(char *)a * (b + 1);\n}\n```\n\n## Callers\n\n- x\n')
        ok, why = truth.is_pure(text)
        self.assertTrue(ok, why)

    def test_is_pure_names_the_first_offending_line(self):
        text = '## Decompiled (Ghidra)\nint f(int a)\n{\n  int b;\n  b = a + 1;\n  *(int *)(a + 4) = b;\n  return Other(b);\n}\n'
        ok, why = truth.is_pure(text)
        self.assertFalse(ok)
        self.assertIn('*(int *)(a + 4) = b;', why)
        self.assertNotIn('Other', why)
        ok, why = truth.is_pure('## Decompiled (Ghidra)\nint f(int a){\n return this->x + a;\n}\n')
        self.assertFalse(ok); self.assertIn('this', why)

    def test_is_pure_reads_only_the_decompiled_section(self):
        text = '## Decompiled (Ghidra)\nint f(int a){return a;}\n## Callees\n\n- 0x401000 Helper\n DAT_00400000\n'
        self.assertTrue(truth.is_pure(text)[0])
        self.assertFalse(truth.is_pure('no section here')[0])

    def test_draft_case_matches_rand_case_shape(self):
        src = truth.draft_case('0x5845D0', 'IcewindMisc::Roll', 'int Roll(int dice, int sides)', 'b011')
        self.assertIn('ORACLE_CASE("wave/b011/5845D0"', src)
        self.assertIn('CallOriginal', src)
        self.assertNotIn('#include <windows.h>', src)
        self.assertIn('IcewindMisc::Roll(dice, sides)', src)
        self.assertIn('using Fn_5845D0 = int(__cdecl*)(int, int);', src)

    def test_draft_case_skips_what_it_cannot_call_honestly(self):
        for sig, kwargs in (('uint GetAbilityNo(void)', {'thiscall': True}),
                            ('int F(char *p)', {}), ('float F(int a)', {}),
                            ('int F(float a)', {}), ('void F(int a)', {}),
                            ('int F(int a, int b, int c, int d, int e)', {}),
                            ('int __thiscall F(int a)', {})):
            src = truth.draft_case('0x401000', 'X::F', sig, 'b011', **kwargs)
            self.assertTrue(src.startswith('// skipped 0x401000'), sig)
            self.assertNotIn('ORACLE_CASE', src)

    def test_write_cases_writes_only_into_cases_dir(self):
        with tempfile.TemporaryDirectory() as d:
            path = truth.write_cases('b011', [
                {'address': '0x5845D0', 'name': 'IcewindMisc::Roll', 'signature': 'int Roll(int a, int b)'},
                {'address': '0x4016D0', 'name': 'CResItem::GetAbilityNo', 'signature': 'uint GetAbilityNo(void)',
                 'thiscall': True}], cases_dir=d)
            self.assertEqual(path, os.path.join(d, 'wave-b011.cpp'))
            self.assertEqual(os.listdir(d), ['wave-b011.cpp'])
            with open(path) as f:
                text = f.read()
        self.assertIn('ORACLE_CASE("wave/b011/5845D0"', text)
        self.assertIn('// skipped 0x4016D0', text)
        self.assertNotIn('<windows.h>', text.replace('must not include <windows.h>', ''))
        self.assertLess(text.index('} // namespace'), text.index('ORACLE_CASE('))

    def test_is_pure_allowlist_rejects_globals_statics_and_deref_after_cast(self):
        for body in ('return _DAT_008cf6d8 + param_1;', '_DAT_008cf6d8 = param_1;',
                     'return (int)PTR_DAT_00812345 + param_1;', 'return (int)s_hello_00812345;',
                     'return (int)*(char *)(param_1 + 0x10);', 'return (uint)*(byte *)param_1;',
                     'return g_nTicks + param_1;', 'CChitin::m_nTicks = param_1;'):
            ok, why = truth.is_pure('## Decompiled (Ghidra)\nint f(int param_1)\n{\n  %s\n}\n' % body)
            self.assertFalse(ok, body)
            self.assertIn(body, why)

    def test_is_pure_allowlist_accepts_locals_types_and_pseudo_ops(self):
        text = ('## Decompiled (Ghidra)\nulonglong f(uint param_1, ushort param_2)\n{\n'
                '  undefined4 uVar1;\n  int iVar2, iVar3;\n  uVar1 = (undefined4)(param_1 * 3);\n'
                '  iVar2 = (int)param_2 * (iVar3 + 0x10) + abs(iVar3);\n'
                '  return CONCAT44(uVar1,ZEXT24(param_2)) + Helper(iVar2);\n}\n')
        self.assertFalse(truth.is_pure(text)[0])
        ok, why = truth.is_pure(text, frozenset({'Helper'}))
        self.assertTrue(ok, why)
        ok, why = truth.is_pure('## Decompiled (Ghidra)\nint f(int a)\n{\n  return f(a - 1);\n}\n')
        self.assertFalse(ok); self.assertIn('call to f', why)

    def test_draft_case_skips_unknowable_ours_and_one_bad_row_is_harmless(self):
        for name in ('Roll', 'FUN_005845d0', 'IcewindMisc::FUN_005845d0', 'thunk_Roll'):
            src = truth.draft_case('0x5845D0', name, 'int Roll(int a)', 'b011')
            self.assertTrue(src.startswith('// skipped 0x5845D0'), name)
        src = truth.draft_case('0x5845D0', 'Roll', 'int Roll(int a)', 'b011', header='Roll.h')
        self.assertIn('ORACLE_CASE', src)
        with tempfile.TemporaryDirectory() as d:
            path = truth.write_cases('b011', [
                {'address': '0x1'},
                {'address': '0x401000', 'name': 'FUN_00401000', 'signature': 'int F(int a)'},
                {'address': '0x5845D0', 'name': 'IcewindMisc::Roll', 'signature': 'int Roll(int a, int b)'}],
                cases_dir=d)
            with open(path) as f:
                text = f.read()
        self.assertIn('ORACLE_CASE("wave/b011/5845D0"', text)
        self.assertEqual(text.count('// skipped'), 2)

    def _report(self, *cases):
        base = {'expect': 'pass', 'pending': None, 'has_body': True, 'iterations': 36,
                'passed': True, 'failure_count': 0, 'failures': [], 'crashed': False, 'crash': None}
        out = []
        for name, kw in cases:
            c = dict(base, case=name)
            c.update(kw)
            out.append(c)
        return json.dumps({'file': 'wave-b011', 'complete': True, 'running': None,
                           'seed': 1, 'iterations': 10, 'cases': out})

    def test_probe_verdicts_reports_each_status(self):
        crash = {'inputs': 'F(0)', 'exception': '0xC0000094', 'address': '0x1', 'kind': 'divide by zero', 'type': 'x'}
        report = self._report(
            ('wave/b011/5845D0', {}),
            ('wave/b011/401000', {'failure_count': 2, 'passed': False}),
            ('wave/b012/402000', {'crashed': True, 'crash': crash}),
            ('wave/b011/403000', {'pending': 'not yet', 'iterations': 0}),
            ('rand/roll', {}), ('wave/b011/not-hex', {}))
        got = truth.probe_verdicts(report)
        self.assertEqual({k: v[0] for k, v in got.items()}, {
            '0x5845D0': 'pass', '0x401000': 'fail', '0x402000': 'crash', '0x403000': 'pending'})
        self.assertIn('divide by zero', got['0x402000'][1])
        self.assertIn('2', got['0x401000'][1])

    def test_probe_verdicts_batch_filter_and_last_wins(self):
        report = self._report(('wave/b011/5845D0', {}),
                              ('wave/b012/5845D0', {'failure_count': 1, 'passed': False}))
        self.assertEqual(truth.probe_verdicts(report)['0x5845D0'][0], 'fail')
        self.assertEqual(truth.probe_verdicts(report, 'b011')['0x5845D0'][0], 'pass')
        self.assertEqual(truth.probe_verdicts(report, 'b013'), {})


    # ---- R49: is_pure fixes ----
    def _pure(self, body, names=frozenset(), sig='int f(int param_1)'):
        return truth.is_pure('## Decompiled (Ghidra)\n%s\n{\n%s\n}\n' % (sig, body), names)

    def test_is_pure_initializer_names_do_not_become_locals(self):
        ok, why = self._pure('  int iVar1 = Helper(param_1, g_nTicks);\n  return iVar1;', frozenset({'Helper'}))
        self.assertFalse(ok)
        ok, why = self._pure('  int iVar1 = Helper(param_1);\n  return iVar1;', frozenset({'Helper'}))
        self.assertFalse(ok)  # a declaration with a call is skipped: iVar1 is not a known local
        ok, why = self._pure('  int a = 1, b = 2;\n  return a + b + param_1;')
        self.assertTrue(ok, why)
        ok, why = self._pure('  int a = g_x, b;\n  return a + b;')
        self.assertFalse(ok)

    def test_is_pure_win32_types_are_types_not_pointers(self):
        for t in ('DWORD', 'BOOL', 'UINT', 'WORD', 'BYTE', 'LONG', 'ULONG', 'SHORT',
                  'USHORT', 'CHAR', 'UCHAR', 'INT'):
            ok, why = self._pure('  %s v;\n  v = (%s)param_1;\n  return v;' % (t, t))
            self.assertTrue(ok, '%s: %s' % (t, why))
        ok, _ = self._pure('  HANDLE h;\n  h = param_1;\n  return h;')
        self.assertFalse(ok)

    def test_is_pure_allows_goto_and_labels(self):
        ok, why = self._pure('  if (param_1 == 0) goto LAB_00401010;\n  param_1 = param_1 + 1;\n'
                             'LAB_00401010:\n  return param_1;')
        self.assertTrue(ok, why)
        ok, _ = self._pure('  goto elsewhere;\n  return 1;')
        self.assertFalse(ok)

    def test_is_pure_sizeof_is_not_a_cast_then_dereference(self):
        ok, why = self._pure('  return sizeof(int) * param_1;')
        self.assertTrue(ok, why)
        ok, _ = self._pure('  return (int)*(char *)param_1;')
        self.assertFalse(ok)

    # ---- Task 11: hook tables and the batched Frida run ----
    MAP = (' Preferred load address is 00400000\n\n'
           '  Address         Publics by Value              Rva+Base       Lib:Object\n\n'
           ' 0001:00001230       ?Roll@IcewindMisc@@YAHHH@Z 00402230 f   rand.obj\n'
           ' 0001:00004560       ?GetAbilityNo@CGameSprite@@QAEIXZ 00405560 f   spr.obj\n'
           ' 0001:00005560       ?Missing@Gone@@YAXXZ 00406560 f   gone.obj\n'
           ' 0001:00006000       ?Twice@Gone@@QAEHH@Z 00407000 f   gone.obj\n'
           ' 0001:00006100       ?Twice@Gone@@QAEHHH@Z 00407100 f   gone.obj\n'
           ' 0001:00006300       ?Std@Gone@@YGHH@Z 00407300 f   gone.obj\n'
           ' 0001:00006400       ?Fast@Gone@@YIHH@Z 00407400 f   gone.obj\n'
           ' 0001:00006500       ?Stat@Gone@@SAHH@Z 00407500 f   gone.obj\n'
           ' 0001:00006200       ?EngineActivated@CScreenWorld@@QAEXXZ 00407200 f   w.obj\n')
    ROWS = [{'address': '0x5845D0', 'name': 'IcewindMisc::Roll'},
            {'address': '0x73F560', 'name': 'CGameSprite::GetAbilityNo'},
            {'address': '0x777777', 'name': 'Gone::Missing'},
            {'address': '0x888888', 'name': 'Gone::Unmapped'},
            {'address': '0x999999', 'name': 'Gone::Twice'}]
    # Like the real evidence: no calling convention in the signature.
    SIGS = {'0x5845D0': 'int Roll(int dice, short sides)',
            '0x73F560': 'uint GetAbilityNo(byte param_1)',
            '0x777777': 'void Missing(void)',
            '0x888888': 'int Unmapped(int a)',
            '0x999999': 'int Twice(int a)'}

    def test_hook_table_from_signatures(self):
        table = truth.hook_table(self.ROWS, self.SIGS, self.MAP, ['world_entry'])
        orig = {h['name']: h for h in table['original']['hooks']}
        ours = {h['name']: h for h in table['ours']['hooks']}
        self.assertEqual(table['scenarios'], ['world_entry'])
        self.assertEqual(sorted(orig), ['fn_5845D0', 'fn_73F560', 'fn_777777'])
        self.assertEqual(orig['fn_5845D0'], {'name': 'fn_5845D0', 'addr': '0x5845D0',
                                             'conv': 'cdecl', 'args': ['s32', 's16'],
                                             'ret': 's32', 'max': 200})
        self.assertEqual(orig['fn_73F560']['conv'], 'thiscall')
        self.assertEqual(orig['fn_73F560']['args'], ['u8'])
        self.assertEqual(orig['fn_73F560']['ret'], 'u32')
        self.assertNotIn('ret', orig['fn_777777'])
        # ours: rva from the map (Rva+Base minus the preferred load address)
        self.assertEqual(ours['fn_5845D0']['rva'], '0x2230')
        self.assertNotIn('addr', ours['fn_5845D0'])
        self.assertEqual(ours['fn_73F560']['rva'], '0x5560')
        self.assertEqual(ours['fn_5845D0']['args'], orig['fn_5845D0']['args'])
        # no map entry: no ours hook, and the row is listed as skipped
        self.assertNotIn('fn_888888', ours)
        self.assertNotIn('fn_888888', orig)
        self.assertIn('0x888888', table['skipped'])
        for h in list(orig.values()) + list(ours.values()):
            self.assertEqual(h['max'], 200)
        # overloads at different rvas are never guessed between
        self.assertEqual(table['skipped']['0x999999'], 'ambiguous: 2 overloads of Gone::Twice in our map')
        self.assertNotIn('fn_999999', orig)
        self.assertEqual(table['ours_loaded_rva'], '0x7200')

    def test_hook_table_takes_the_convention_from_the_decorated_name(self):
        rows = [{'address': '0x600000', 'name': 'Gone::Std'}, {'address': '0x600001', 'name': 'Gone::Fast'},
                {'address': '0x600002', 'name': 'Gone::Stat'}, {'address': '0x600003', 'name': 'CGameSprite::GetAbilityNo'},
                {'address': '0x600004', 'name': 'IcewindMisc::Roll'}]
        sigs = {'0x600000': 'int Std(int a)', '0x600001': 'int Fast(int a)', '0x600002': 'int Stat(int a)',
                '0x600003': 'uint GetAbilityNo(CGameSprite *this, byte x)',
                '0x600004': 'int __stdcall Roll(int a)'}  # the signature's own convention wins
        table = truth.hook_table(rows, sigs, self.MAP, ['s'])
        convs = {h['name']: h['conv'] for h in table['ours']['hooks']}
        self.assertEqual(convs, {'fn_600000': 'stdcall', 'fn_600002': 'cdecl',
                                 'fn_600003': 'thiscall', 'fn_600004': 'stdcall'})
        self.assertIn('__fastcall', table['skipped']['0x600001'])
        hooks = {h['name']: h for h in table['original']['hooks']}
        self.assertEqual(hooks['fn_600003']['args'], ['u8'])  # the explicit this is dropped
        for code, conv in (('?F@C@@QAEXXZ', '__thiscall'), ('?F@C@@UAEXXZ', '__thiscall'),
                           ('?F@C@@AAEXXZ', '__thiscall'), ('?F@C@@IAEXXZ', '__thiscall'),
                           ('?F@C@@EAEXXZ', '__thiscall'), ('?F@C@@MAEXXZ', '__thiscall'),
                           ('?F@@YAXXZ', '__cdecl'), ('?F@@YGXXZ', '__stdcall'),
                           ('?F@@YIXXZ', '__fastcall'), ('?F@@3HA', None),
                           # const / volatile this, and the private and protected statics (R53)
                           ('?F@C@@QBEXXZ', '__thiscall'), ('?F@C@@UCEXXZ', '__thiscall'),
                           ('?F@C@@AADEXXZ', None), ('?F@C@@IDEXXZ', '__thiscall'),
                           ('?F@C@@CAXXZ', '__cdecl'), ('?F@C@@KAXXZ', '__cdecl'),
                           ('?F@C@@CGXXZ', '__stdcall'), ('?F@C@@KGXXZ', '__stdcall'),
                           ('?F@C@@SGXXZ', '__stdcall'), ('?F@C@@CIXXZ', '__fastcall'),
                           ('?F@C@@QFEXXZ', None)):
            self.assertEqual(truth.decorated_convention(code), conv, code)

    def test_hook_table_skips_what_the_hooks_cannot_log(self):
        rows = [{'address': '0x%X' % (0x5845D0 + i), 'name': 'IcewindMisc::Roll'} for i in range(6)]
        sigs = {'0x5845D0': 'int __fastcall Roll(int a)', '0x5845D1': 'int Roll(int a)',
                '0x5845D2': 'double Roll(int a)', '0x5845D3': 'int Roll(float a)',
                '0x5845D4': 'int Roll(longlong a)', '0x5845D5': 'int Roll(BOOL a, bool b)'}
        table = truth.hook_table(rows, sigs, self.MAP, ['s'])
        self.assertEqual(sorted(table['skipped']), ['0x5845D%d' % i for i in range(5) if i != 1])
        self.assertIn('__fastcall', table['skipped']['0x5845D0'])
        for i in (2, 3, 4):
            self.assertIn('8 bytes or floating point', table['skipped']['0x5845D%d' % i])
        self.assertEqual([h['name'] for h in table['ours']['hooks']], ['fn_5845D1', 'fn_5845D5'])
        self.assertEqual(table['ours']['hooks'][1]['args'], ['s32', 'u8'])  # BOOL is 4 bytes, bool one
        # neither the signature nor a decorated name gives a convention
        table = truth.hook_table([{'address': '0x1', 'name': 'Gone::Odd'}], {'0x1': 'int Odd(int a)'},
                                 ' 0001:00000000 ?Odd@Gone@@3HA 00400010 f x.obj\n', ['s'])
        self.assertIn('no calling convention', table['skipped']['0x1'])

    @staticmethod
    def _trace(*calls):
        events = [{'tag': 'ready', 'pid': 1}]
        for tag, a0, ret in calls:
            events.append({'tag': tag, 'this': '0x1234', 'a0_s32': a0, 'ts': 5})
            events.append({'tag': tag + '_ret', 'ret': ret})
        return events

    def test_frida_verdicts_equal_different_not_exercised(self):
        rows = self.ROWS
        orig = self._trace(('fn_5845D0', 7, 3), ('fn_73F560', 1, 9))
        ours = self._trace(('fn_5845D0', 7, 3), ('fn_73F560', 1, 8))
        v = truth.frida_verdicts({'world_entry': (orig, ours)}, rows)
        self.assertEqual(v['0x5845D0'][0], 'equal')
        self.assertEqual(v['0x73F560'][0], 'different')
        self.assertIn('world_entry', v['0x73F560'][1])
        self.assertEqual(v['0x777777'][0], 'not-exercised')
        # fired on one side only is a difference
        v = truth.frida_verdicts({'s': (self._trace(('fn_5845D0', 7, 3)), self._trace())}, rows)
        self.assertEqual(v['0x5845D0'][0], 'different')
        # a crash event in a trace that exercised the row never passes
        crashed = self._trace(('fn_5845D0', 7, 3)) + [{'tag': 'crash'}]
        v = truth.frida_verdicts({'s': (self._trace(('fn_5845D0', 7, 3)), crashed)}, rows)
        self.assertEqual(v['0x5845D0'][0], 'different')
        # two scenarios: one exercises, one does not -> equal; any difference wins
        t = self._trace(('fn_5845D0', 7, 3))
        v = truth.frida_verdicts({'a': (t, t), 'b': (self._trace(), self._trace())}, rows)
        self.assertEqual(v['0x5845D0'][0], 'equal')
        bad = self._trace(('fn_5845D0', 8, 3))
        v = truth.frida_verdicts({'a': (t, t), 'b': (t, bad)}, rows)
        self.assertEqual(v['0x5845D0'][0], 'different')
        self.assertIn('b: event 1', v['0x5845D0'][1])
        self.assertIn('"a0_s32": 7', v['0x5845D0'][1])
        self.assertIn('"a0_s32": 8', v['0x5845D0'][1])
        v = truth.frida_verdicts({'s': (t, crashed)}, rows)
        self.assertIn('ours logged {"tag": "crash"}', v['0x5845D0'][1])
        # skipped rows say so, with the reason
        v = truth.frida_verdicts({'s': (t, t)}, rows, {'0x73F560': 'ambiguous: 2 overloads of X in our map'})
        self.assertEqual(v['0x73F560'], ('skipped', 'ambiguous: 2 overloads of X in our map'))
        # a failed scenario is left out and reported
        failed = {'orig': [], 'ours': [], 'failed': 'ours side exited 2'}
        good = {'orig': t, 'ours': t, 'failed': None}
        v = truth.frida_verdicts({'a': failed, 'b': good}, rows)
        self.assertEqual(v['0x5845D0'][0], 'equal')
        self.assertEqual(v['0x73F560'][0], 'scenario failed')
        v = truth.frida_verdicts({'a': failed}, rows)
        self.assertEqual(v['0x5845D0'][0], 'scenario failed')
        self.assertIn('a: ours side exited 2', v['0x5845D0'][1])
        # a row that appears in a failed scenario is never equal: ours crashed in a
        crash = {'orig': t, 'ours': t[:2] + [{'tag': 'crash', 'where': 'x'}], 'failed': 'ours side exited 1'}
        v = truth.frida_verdicts({'a': crash, 'b': good}, rows)
        self.assertEqual(v['0x5845D0'][0], 'different')
        self.assertIn('ours logged {"tag": "crash", "where": "x"}', v['0x5845D0'][1])
        partial = {'orig': t, 'ours': t[:2], 'failed': 'ours side exited 2'}
        v = truth.frida_verdicts({'a': partial, 'b': good}, rows)
        self.assertEqual(v['0x5845D0'][0], 'scenario failed')
        # pointer returns are compared by presence, not by heap address
        p1 = [{'tag': 'fn_5845D0', 'a0_s32': 1}, {'tag': 'fn_5845D0_ret', 'ret': '0x1a2b3c'}]
        p2 = [{'tag': 'fn_5845D0', 'a0_s32': 1}, {'tag': 'fn_5845D0_ret', 'ret': '0x99ff00'}]
        self.assertEqual(truth.frida_verdicts({'s': (p1, p2)}, rows)['0x5845D0'][0], 'equal')
        p3 = [p2[0], {'tag': 'fn_5845D0_ret', 'ret': '0x0'}]
        self.assertEqual(truth.frida_verdicts({'s': (p1, p3)}, rows)['0x5845D0'][0], 'different')

    def test_frida_run_is_injectable_and_never_launches(self):
        import subprocess as sp
        import tempfile
        calls = []
        with tempfile.TemporaryDirectory() as root:
            def fake(cmd, **kw):
                calls.append(cmd)
                target = cmd[cmd.index('--target') + 1]
                out = cmd[cmd.index('--out') + 1]
                d = os.path.join(root, 'runs', 'x-' + target, 'replay')
                os.makedirs(d, exist_ok=True)
                with open(os.path.join(d, out), 'w') as f:
                    f.write(json.dumps({'tag': 'fn_5845D0', 'a0_s32': 1}) + '\n')
                return sp.CompletedProcess(cmd, 0, stdout='probe: runs/x-%s\n' % target, stderr='')
            table = truth.hook_table(self.ROWS, self.SIGS, self.MAP, ['s1'])
            traces = truth.frida_run('b011', table, ['s1'], run=fake, root=root)
            self.assertEqual(sorted(traces), ['s1'])
            self.assertEqual(len(calls), 2)
            self.assertEqual([c[c.index('--target') + 1] for c in calls], ['original', 'ours'])
            for c in calls:
                self.assertIn('--probe', c)
                self.assertEqual(c[c.index('--scenario') + 1], 's1')
            # ours loads off the map's EngineActivated rva; the original keeps its default
            self.assertNotIn('--loaded-rva', calls[0])
            self.assertEqual(calls[1][calls[1].index('--loaded-rva') + 1], '0x7200')
            r = traces['s1']
            self.assertIsNone(r['failed'])
            self.assertEqual(r['orig'][0]['tag'], 'fn_5845D0')
            self.assertEqual(r['ours'][0]['a0_s32'], 1)
            self.assertTrue(os.path.isfile(r['orig_path']) and os.path.isfile(r['ours_path']))
            # an explicit loaded_rva wins; with none anywhere ours runs --no-loaded
            calls.clear()
            truth.frida_run('b011', table, ['s1'], run=fake, root=root, loaded_rva='0x1234')
            self.assertEqual(calls[1][calls[1].index('--loaded-rva') + 1], '0x1234')
            calls.clear()
            traces = truth.frida_run('b011', dict(table, ours_loaded_rva=None), ['s1'], run=fake, root=root)
            self.assertEqual(calls, [])
            self.assertIn('EngineActivated', traces['s1']['failed'])
            # no trace file: a failed scenario, not an empty trace
            traces = truth.frida_run('b011', table, ['s1'],
                                     run=lambda cmd, **kw: sp.CompletedProcess(cmd, 0, stdout='', stderr=''),
                                     root=root)
            self.assertIn('left no trace file', traces['s1']['failed'])
            self.assertEqual((traces['s1']['orig'], traces['s1']['ours']), ([], []))
            # a non-zero exit is a failed scenario even with a trace
            def rc2(cmd, **kw):
                done = fake(cmd, **kw)
                done.returncode = 2
                return done
            traces = truth.frida_run('b011', table, ['s1'], run=rc2, root=root)
            self.assertIn('exited 2', traces['s1']['failed'])

    def test_trace_source_hashes_the_files_and_check_evidence_accepts_it(self):
        import tempfile
        ce = load_tool('check_evidence')
        with tempfile.TemporaryDirectory() as d:
            o, r = os.path.join(d, 'o.jsonl'), os.path.join(d, 'r.jsonl')
            for path, data in ((o, b'{"tag":"a"}\n'), (r, b'{"tag":"a"}\r\n')):
                with open(path, 'wb') as f:
                    f.write(data)
            src = truth.trace_source('hooks-b011', o, r)
            self.assertTrue(any(p.match(src) for p in ce.TRACE_SOURCES), src)
            self.assertIn('orig:' + hashlib.sha256(b'{"tag":"a"}\n').hexdigest(), src)
            self.assertIn('ours:' + hashlib.sha256(b'{"tag":"a"}\r\n').hexdigest(), src)

    # ---- Task 12: calibration and the oracle path ----

    CAL_STATE = None

    @staticmethod
    def _cal_state():
        def verdicts(batch, spec):
            return {'0x%X' % (0x410000 + i * 16 + batch): {'side': side, 'case': case, 'losing_kind': 'trace'}
                    for i, (side, case) in enumerate(spec)}
        batches = {}
        for n, bid in enumerate(('b001', 'b002')):
            spec = [('fork', 'probe-1'), ('neither', 'x')] + [('ours', 'dump-%d' % i) for i in range(8)] \
                + [('ours', 'replay-w'), ('ours', 'triage-rename')]
            v = verdicts(n, spec)
            batches[bid] = {'status': 'closed', 'verdicts': v,
                            'rows': [{'address': a, 'name': 'C::f%s' % a[2:]} for a in v]}
        batches['b003'] = {'status': 'assigned', 'rows': [{'address': '0x999999', 'name': 'C::g'}],
                           'verdicts': {'0x999999': {'side': 'fork', 'case': 'p', 'losing_kind': 'trace'}}}
        return {'fork_base': 'x', 'batches': batches}

    def test_calibration_rows_are_seeded_and_closed_only(self):
        state = self._cal_state()
        rows, sample = truth.calibration_rows(state, '2026-10-22')
        self.assertEqual((rows, sample), truth.calibration_rows(state, '2026-10-22'))
        self.assertEqual(len(sample), 10)
        self.assertEqual(sample, sorted(sample, key=lambda a: [r for r in rows if r['address'] == a][0]['batch'] + a))
        ruling = [r for r in rows if not r['sample']]
        self.assertEqual(len(ruling), 4)                       # 2 fork + 2 neither; b003 is not closed
        self.assertEqual({r['side'] for r in ruling}, {'fork', 'neither'})
        picked = [r for r in rows if r['sample']]
        self.assertTrue(all(r['side'] == 'ours' and r['case'].startswith('dump-') for r in picked))
        self.assertNotEqual(sample, truth.calibration_rows(state, 'another seed')[1])

    def test_truth_calibrate_counts(self):
        state = self._cal_state()
        rows, sample = truth.calibration_rows(state, 's')
        addrs = [r['address'] for r in rows]
        oracle = {addrs[0]: ('pass', '6'), addrs[1]: ('fail', 'x'), addrs[2]: ('crash', 'c'),
                  addrs[3]: ('pending', 'p'), addrs[4].lower().replace('0x', '0x'): ('pass', '6')}
        cal = truth.calibrate(state, rows, {'oracle': oracle, 'frida': None}, seed='s', sample=sample)
        self.assertEqual(cal['seed'], 's')
        self.assertEqual(cal['sample'], sample)
        self.assertEqual(cal['paths']['oracle'], {'measured': 3, 'agree': 2, 'disagree': 1, 'of': 14})
        self.assertEqual(cal['paths']['frida'], {'measured': 0, 'agree': 0, 'disagree': 0, 'of': 14,
                                                 'deferred': True})
        self.assertFalse(cal['frida_default'])
        # Frida measuring a fifth of the sample (2 of 10) switches it on by default; 1 does not.
        two = {sample[0]: ('equal', ''), sample[1]: ('different', 'd'), addrs[0]: ('equal', '')}
        cal = truth.calibrate(state, rows, {'oracle': {}, 'frida': two}, seed='s', sample=sample)
        self.assertEqual(cal['paths']['frida'], {'measured': 3, 'agree': 2, 'disagree': 1, 'of': 14})
        self.assertTrue(cal['frida_default'])
        one = {sample[0]: ('equal', ''), addrs[0]: ('equal', ''), sample[1]: ('not-exercised', '')}
        cal = truth.calibrate(state, rows, {'oracle': {}, 'frida': one}, seed='s', sample=sample)
        self.assertFalse(cal['frida_default'])
        text = truth.calibration_text(cal)
        self.assertIn('Truth calibration (seed s, sample ' + ', '.join(sample), text)

    def test_evidence_signature_and_oracle_candidates(self):
        pure = ('---\naddress: 0x1\n---\n## Decompiled (Ghidra)\n\n```c\n'
                'int Rules::Mix(int a,\n    int b)\n\n{\n  return a * 3 + b;\n}\n```\n')
        impure = pure.replace('a * 3 + b', 'g_total')
        self.assertEqual(truth.evidence_signature(pure), 'int Rules::Mix(int a, int b)')
        self.assertIsNone(truth.evidence_signature('no section'))
        texts = {'0x1': pure, '0x2': impure, '0x3': pure.replace('Rules::Mix', 'Mix'),
                 '0x4': pure.replace('int a,', 'int *a,')}
        rows = [{'address': a, 'name': 'Rules::Mix'} for a in ('0x1', '0x2', '0x3', '0x4', '0x5')]
        rows[2]['name'] = 'Mix'
        keep, why = truth.oracle_candidates(rows, texts.get)
        self.assertEqual([c['address'] for c in keep], ['0x1'])
        self.assertEqual(keep[0]['signature'], 'int Rules::Mix(int a, int b)')
        self.assertIn('not pure', why['0x2'])
        self.assertIn('no Class::', why['0x3'])
        self.assertIn('not integer-like', why['0x4'])
        self.assertEqual(why['0x5'], 'no evidence file')

    def test_wire_cases_and_run_oracle_with_fakes(self):
        with tempfile.TemporaryDirectory() as wt:
            os.makedirs(os.path.join(wt, 'oracle'))
            cmake = os.path.join(wt, 'oracle', 'CMakeLists.txt')
            with open(cmake, 'w') as f:
                f.write('add_executable(iwd2r_oracle a.cpp\n        cases/store_spell.cpp\n        $<X>)\n')
            self.assertTrue(truth.wire_cases(cmake, 'b011'))
            self.assertFalse(truth.wire_cases(cmake, 'b011'))
            with open(cmake) as f:
                text = f.read()
            self.assertEqual(text.count('cases/wave-b011.cpp'), 1)
            self.assertIn('cases/store_spell.cpp\n        cases/wave-b011.cpp\n', text)
            with open(cmake, 'w') as f:
                f.write('nothing\n')
            with self.assertRaises(truth.OracleError):
                truth.wire_cases(cmake, 'b012')
            with open(cmake, 'w') as f:
                f.write('x cases/store_spell.cpp\n')
            cases = [{'address': '0x5845D0', 'name': 'IcewindMisc::Roll', 'signature': 'int Roll(int a, int b)'}]
            built, ran = [], []

            def run(cmd, **kw):
                ran.append((cmd[2:], kw['cwd']))
                report = {'cases': [{'case': 'wave/b011/5845D0', 'iterations': 36, 'failure_count': 0}]}
                os.makedirs(os.path.join(wt, 'runs', 'x'), exist_ok=True)
                with open(os.path.join(wt, 'runs', 'x', 'oracle.json'), 'w') as f:
                    json.dump(report, f)
                return subprocess.CompletedProcess(cmd, 0, '36 inputs\nreport: runs/x/oracle.json\n', '')
            got = truth.run_oracle('b011', wt, cases, build=built.append, run=run)
            self.assertEqual(got, {'0x5845D0': ('pass', '36 inputs')})
            self.assertEqual(built, [wt])
            self.assertEqual(ran, [(['tools/oracle.py'.replace('/', os.sep), 'run', '--file', 'wave-b011'] if False
                                    else [os.path.join('tools', 'oracle.py'), 'run', '--file', 'wave-b011'], wt)])
            self.assertTrue(os.path.isfile(os.path.join(wt, 'oracle', 'cases', 'wave-b011.cpp')))
            self.assertEqual(truth.run_oracle('b011', wt, [], build=built.append, run=run), {})
            self.assertEqual(len(built), 1)                  # no case: no build
            with self.assertRaises(truth.OracleError):       # no report line
                truth.run_oracle('b011', wt, cases, build=built.append,
                                 run=lambda cmd, **kw: subprocess.CompletedProcess(cmd, 1, 'nothing\n', 'boom'))

    def test_calibrate_cli_writes_the_record_and_the_scorecard_paragraph(self):
        with tempfile.TemporaryDirectory() as d:
            state = self._cal_state()
            state_path = os.path.join(d, 'state.json')
            with open(state_path, 'w') as f:
                json.dump(state, f)
            os.makedirs(os.path.join(d, 'docs', 'evidence'))
            out = os.path.join(d, 'cal.json')
            seen = []
            with mock.patch.object(truth, 'run_oracle', lambda b, w, c: seen.append((b, w, c)) or {}):
                with contextlib.redirect_stdout(io.StringIO()) as buf:
                    rc = truth.main(['calibrate', '--seed', 's', '--state', state_path, '--out', out,
                                     '--evidence-root', d, '--worktree', d])
            self.assertEqual(rc, 0)
            self.assertEqual(seen, [])                          # no evidence, no pure row, no build
            with open(out) as f:
                cal = json.load(f)
            self.assertEqual(cal['seed'], 's')
            self.assertEqual(len(cal['sample']), 10)
            self.assertEqual(cal['paths']['oracle']['of'], 14)
            self.assertTrue(cal['paths']['frida']['deferred'])
            self.assertFalse(cal['frida_default'])
            self.assertIn('Frida trace diff: measured 0 of 14 rows', buf.getvalue())
            wave = load_tool('wave')
            report = os.path.join(d, 'report.md')
            with open(report, 'w') as f:
                f.write('# R\n\n## Scorecard\n\nold\n\n## Other\n\nkeep\n')
            rc, msg = wave.write_scorecard(state_path, report, os.path.join(d, 'no-triage.json'), out)
            self.assertEqual(rc, 0, msg)
            with open(report) as f:
                text = f.read()
            self.assertEqual(text.count('Truth calibration (seed s'), 1)
            self.assertLess(text.index('Truth calibration'), text.index('## Other'))
            wave.write_scorecard(state_path, report, os.path.join(d, 'no-triage.json'), out)
            with open(report) as f:
                self.assertEqual(f.read(), text)               # idempotent
            wave.write_scorecard(state_path, report, os.path.join(d, 'no-triage.json'),
                                 os.path.join(d, 'absent.json'))
            with open(report) as f:
                self.assertNotIn('Truth calibration', f.read())

    def test_rules_math_names_are_the_pure_fixpoint(self):
        def ev(name, body):
            return ('---\nname: %s\n---\n## Decompiled (Ghidra)\n\n```c\nint %s(int a)\n\n{\n%s\n}\n```\n'
                    % (name, name, body))
        texts = {'0x1': ev('R::A', '  return a * 2;'),
                 '0x2': ev('R::B', '  int x;\n  x = R::A(a);\n  return x + 1;'),
                 '0x3': ev('R::C', '  int in_ECX;\n  return *(int *)(in_ECX + 0x92a) + a;'),
                 '0x4': ev('R::D', '  return R::C(a);')}
        names = truth.rules_math_names(texts.get, addresses=[1, 2, 3, 4])
        self.assertEqual(names, frozenset({'R::A', 'R::B'}))      # C reads in_ECX; D calls C
        caller = '---\naddress: 0x9\n---\n' + ev('Rules::Mix', '  return R::C(a);').split('---\n', 2)[2]
        impure = '---\naddress: 0x9\n---\n' + ev('Rules::Mix', '  return R::C(a);').split('---\n', 2)[2]
        good = '---\naddress: 0x8\n---\n' + ev('Rules::Ok', '  return R::B(a);').split('---\n', 2)[2]
        self.assertFalse(truth.is_pure(impure, names)[0])
        self.assertTrue(truth.is_pure(good, names)[0])
        texts['0x8'], texts['0x9'] = good, caller
        keep, why = truth.oracle_candidates(
            [{'address': '0x8', 'name': 'Rules::Ok'}, {'address': '0x9', 'name': 'Rules::Mix'}], texts.get,
            pure_names=names)
        self.assertEqual([c['address'] for c in keep], ['0x8'])
        self.assertIn('not pure', why['0x9'])
        # the real set: nothing outside the fixpoint is admitted, and a repeat call is the cached object
        ce = load_tool('check_evidence')
        real = truth.rules_math_names(lambda a: None)
        self.assertEqual(real, frozenset())
        self.assertIs(truth.rules_math_names(texts.get, addresses=[1, 2, 3, 4]), names)

    def test_calibrate_requires_a_worktree_to_probe(self):
        with tempfile.TemporaryDirectory() as d:
            state_path = os.path.join(d, 'state.json')
            with open(state_path, 'w') as f:
                json.dump(self._cal_state(), f)
            os.makedirs(os.path.join(d, 'docs', 'evidence'))
            name = self._cal_state()['batches']['b001']['rows'][0]['address'][2:]
            with open(os.path.join(d, 'docs', 'evidence', name + '.md'), 'w') as f:
                f.write('## Decompiled (Ghidra)\n\n```c\nint C::f(int a)\n\n{\n  return a;\n}\n```\n')
            args = ['calibrate', '--seed', 's', '--state', state_path, '--out', os.path.join(d, 'c.json'),
                    '--evidence-root', d]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
                self.assertEqual(truth.main(args), 2)
            self.assertIn('--worktree', err.getvalue())
            self.assertFalse(os.path.exists(os.path.join(d, 'c.json')))

    def test_record_verdict_stores_measured(self):
        wave = load_tool('wave')
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 's.json')
            with open(path, 'w') as f:
                json.dump({'fork_base': 'x', 'batches': {'b1': {
                    'rows': [{'address': '0x1'}], 'branch': 'nope', 'status': 'assigned', 'verdicts': {}}}}, f)
            rc, _ = wave.record_verdict(path, d, 'b1', '0x1', 'ours', 'probe-b1-1', measured='probe')
            self.assertEqual(rc, 0)
            with open(path) as f:
                v = json.load(f)['batches']['b1']['verdicts']['0x1']
            self.assertEqual((v['measured'], v['case']), ('probe', 'probe-b1-1'))
            self.assertEqual(wave.record_verdict(path, d, 'b1', '0x1', 'ours', 'c', measured='guess')[0], 2)
            wave.record_verdict(path, d, 'b1', '0x1', 'ours', 'c')
            with open(path) as f:
                self.assertNotIn('measured', json.load(f)['batches']['b1']['verdicts']['0x1'])


class WaveResolveTest(unittest.TestCase):
    """wave.py resolve (a probe-list row into its batch) and the rule-3
    audit paragraph of the scorecard."""

    setUp = WaveTest.setUp
    PROBED = '0x716E80'
    ROW = {'address': '0x716E80', 'category': 'both_changed_differ',
           'class': 'ambiguous', 'file': 'src/CGameSprite.cpp',
           'name': 'CGameSprite::sub_716E80', 'our_len': 300, 'their_len': 310}

    def repo(self, evidence='disagreed with WSO at e8dd16a: x; case dump-716E80 decides\n',
             probes=True):
        self.root = os.path.join(self.tmp, 'repo')
        shutil.rmtree(self.root, ignore_errors=True)
        wso = os.path.join(self.root, 'docs', 'wso')
        first = dict(self.ROW, address='0x643CC0', name='Render')
        _write(os.path.join(wso, 'disagreements.json'),
               json.dumps([first, self.ROW]))
        self.state = os.path.join(wso, 'wave-state.json')
        self.wave.save_state(self.state, {'batches': {'b011': {
            'rows': [first], 'files': ['src/CGameSprite.cpp'],
            'branch': 'p3/wso-b011', 'status': 'closed', 'assigned_at': 't',
            'closed_at': 't', 'verdicts': {
                '0x643CC0': {'side': 'ours', 'case': 'probe-1',
                             'losing_kind': 'trace'}}}}})
        if probes:
            _write(os.path.join(wso, 'probe-list.json'), json.dumps([
                {'address': '0x716E80', 'at': 't', 'batch': 'b011',
                 'file': 'src/CGameSprite.cpp', 'name': 'CGameSprite::sub_716E80',
                 'reading': 'fork', 'reason': 'r', 'source': 'worker'}]))
        if evidence is not None:
            _write(os.path.join(self.root, 'docs', 'evidence', '716E80.md'),
                   '---\nkind: oracle\n---\n\n' + evidence)

    def resolve(self, side='ours', case='dump-716E80', address='0x716E80'):
        return self.wave.resolve_probe(address, side, case, self.state, self.root)

    def probe_list(self):
        with open(os.path.join(self.root, 'docs', 'wso', 'probe-list.json')) as f:
            return json.load(f)

    def test_resolve_probe_records_ours_verdict(self):
        self.repo()
        rc, out = self.resolve()
        self.assertEqual(rc, 0, out)
        self.assertEqual(out, 'resolved 0x716E80 in b011: ours (case dump-716E80); '
                              'probe list now 0 entries\n')
        b = self.wave.load_state(self.state)['batches']['b011']
        self.assertEqual([r['address'] for r in b['rows']], ['0x643CC0', '0x716E80'])
        self.assertEqual(sorted(b['rows'][1]), sorted(b['rows'][0]))
        self.assertEqual(b['rows'][1], self.ROW)
        self.assertEqual(b['rows'][1]['class'], 'ambiguous')
        self.assertIn('src/CGameSprite.cpp', b['files'])
        self.assertEqual(b['files'], self.wave._files_of(b['rows']))
        self.assertEqual(b['verdicts']['0x716E80'],
                         {'side': 'ours', 'case': 'dump-716E80',
                          'losing_kind': 'trace', 'measured': 'none'})
        self.assertEqual(b['status'], 'closed')
        self.assertEqual(self.probe_list(), [])

    def test_resolve_probe_is_idempotent_on_rows(self):
        self.repo()
        st = self.wave.load_state(self.state)
        st['batches']['b011']['rows'].append(dict(self.ROW))
        self.wave.save_state(self.state, st)
        rc, out = self.resolve()
        self.assertEqual(rc, 0, out)
        b = self.wave.load_state(self.state)['batches']['b011']
        self.assertEqual([r['address'] for r in b['rows']], ['0x643CC0', '0x716E80'])
        self.assertEqual(b['verdicts']['0x716E80']['side'], 'ours')

    def test_resolve_probe_refuses_dump_decided_fork(self):
        self.repo()
        for side in ('fork', 'neither'):
            rc, out = self.resolve(side=side)
            self.assertEqual(rc, 1, out)
            self.assertIn('a fork/neither verdict decided by a dump reading', out)
            self.assertIn('only way to record it', out)
            self.assertNotIn('--allow-dump', out)
        self.assertEqual(len(self.probe_list()), 1)
        self.assertNotIn('0x716E80',
                         self.wave.load_state(self.state)['batches']['b011']['verdicts'])

    def test_resolve_probe_unknown_address(self):
        self.repo()
        rc, out = self.resolve(address='0x999999')
        self.assertEqual(rc, 2, out)
        self.assertEqual(out, '0x999999 is not on the probe list\n')
        self.repo(probes=False)
        self.assertEqual(self.resolve()[0], 2)
        self.repo()
        _write(os.path.join(self.root, 'docs', 'wso', 'probe-list.json'), json.dumps([
            {'address': '0x716E80', 'batch': 'b099', 'file': 'f', 'name': 'n'}]))
        rc, out = self.resolve()
        self.assertEqual(rc, 2, out)
        self.assertIn('b099', out)

    def test_resolve_probe_requires_evidence_phrase(self):
        for ev in (None, 'no phrase here; case dump-716E80 decides\n',
                   'disagreed with WSO at e8dd16a: x; case probe-9 decides\n'):
            self.repo(evidence=ev)
            rc, out = self.resolve()
            self.assertEqual(rc, 1, out)
            self.assertIn('docs/evidence/716E80.md', out)
            self.assertEqual(len(self.probe_list()), 1)
            self.assertEqual(self.wave.load_state(self.state)['batches']['b011']['rows'][0]
                             ['address'], '0x643CC0')

    # -- the audit paragraph ------------------------------------------------

    AUDIT_PARA = ('Audit sample (rule 3): seed rule3-2026-10-23 over 199 '
                  'dump-decided ours rows at 9fad8484; 10 drawn: 7 probed '
                  '(replay 6, oracle 1), 3 parked, 0 flips.\n')

    def audit(self, rows=None, **kw):
        if rows is None:
            rows = ([{'address': f'0x{i}', 'probe': 'replay', 'result': 'match'}
                     for i in range(6)]
                    + [{'address': '0x7', 'probe': 'oracle', 'result': 'match'}]
                    + [{'address': f'0x{i}', 'probe': 'park', 'result': 'parked'}
                       for i in (8, 9, 10)])
        data = {'rule': 3, 'seed': 'rule3-2026-10-23', 'state': '9fad8484',
                'pool': 199, 'drawn': 10, 'rows': rows}
        data.update(kw)
        _write(os.path.join(os.path.dirname(self.state), 'audit.json'),
               json.dumps(data))

    def scorecard(self):
        if not hasattr(self, 'root'):
            self.repo()
        report = os.path.join(self.tmp, 'xrec-report.md')
        rc, out = self.wave.write_scorecard(
            self.state, report, os.path.join(self.tmp, 'no-triage.json'), None)
        return rc, out, report

    def test_scorecard_renders_audit_sample(self):
        self.repo()
        self.audit()
        rc, out, report = self.scorecard()
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            text = f.read()
        self.assertTrue(text.endswith('\n' + self.AUDIT_PARA), text)

    def test_scorecard_without_audit_file_unchanged(self):
        rc, out, report = self.scorecard()
        self.assertEqual(rc, 0, out)
        with open(report) as f:
            text = f.read()
        with open(self.state) as f:
            self.assertEqual(text, self.wave.scorecard_text(json.load(f)))
        self.assertNotIn('Audit sample', text)

    def test_scorecard_rejects_malformed_audit(self):
        self.repo()
        for bad in ({'seed': 's', 'pool': 1, 'state': 'b', 'drawn': 1},
                    {'seed': 's', 'pool': 1, 'state': 'b', 'drawn': 1, 'rows': 'x'},
                    {'seed': 's', 'pool': 1, 'state': 'b', 'drawn': 1,
                     'rows': [{'probe': 'replay', 'result': 'maybe'}]},
                    {'seed': 's', 'pool': 1, 'state': 'b', 'drawn': 1,
                     'rows': [{'probe': 'guess', 'result': 'match'}]},
                    [], 'just a string', '"a json string"', '{not json'):
            _write(os.path.join(os.path.dirname(self.state), 'audit.json'),
                   bad if isinstance(bad, str) else json.dumps(bad))
            report = os.path.join(self.tmp, 'bad.md')
            rc, out = self.wave.write_scorecard(
                self.state, report, os.path.join(self.tmp, 'no-triage.json'), None)
            self.assertEqual(rc, 2, out)
            self.assertEqual(len(out.splitlines()), 1, out)
            self.assertFalse(os.path.exists(report))


class WaveCloseRulesTest(unittest.TestCase):
    """Task 16: close refuses dump-decided fork/neither verdicts, syncs the
    base before it merges, and the checked-out base follows the ref."""

    setUp = WaveTest.setUp
    write_disagreements = WaveTest.write_disagreements
    next = WaveTest.next
    batches = WaveTest.batches
    git = WaveTest.git
    init_repo = WaveTest.init_repo
    EVIDENCE = WaveTest.EVIDENCE
    write_evidence = WaveTest.write_evidence
    commit_worktree = WaveTest.commit_worktree
    gates = WaveTest.gates
    first_sha = WaveTest.first_sha

    def ready(self, render=('ours', 'probe-11'), draw=('fork', 'probe-9')):
        self.init_repo()
        self.write_evidence()
        self.commit_worktree()
        for addr, (side, case) in (('0x643CC0', render), ('0x643CD0', draw)):
            self.assertEqual(self.wave.record_verdict(
                self.state, self.root, 'b001', addr, side, case)[0], 0)

    def close(self, **kw):
        return self.wave.close_batch(self.state, self.root, 'b001', gate=False, **kw)

    def ledger(self, text):
        path = os.path.join(self.tmp, 'ledger.md')
        with open(path, 'w') as f:
            f.write(text)
        return path

    def test_close_refuses_dump_decided_fork_verdict(self):
        self.ready(draw=('fork', 'dump-3'))
        first = self.first_sha()
        rc, out = self.close()
        self.assertEqual(rc, 1, out)
        self.assertIn('0x643CD0: a fork/neither verdict decided by a dump reading '
                      '(dump-3); probe before import, or --allow-dump <ruling id>', out)
        self.assertNotIn('0x643CC0', out)
        self.assertEqual(self.first_sha(), first)
        self.assertEqual(self.batches()['b001']['status'], 'assigned')

    def test_close_refuses_dump_decided_neither_verdict(self):
        self.ready(draw=('neither', 'readout-643CD0'))
        rc, out = self.close()
        self.assertEqual(rc, 1, out)
        self.assertIn('a fork/neither verdict decided by a dump reading (readout-643CD0)', out)

    def test_close_dump_decided_ours_verdict_passes(self):
        self.ready(render=('ours', 'dump-1'))
        rc, out = self.close()
        self.assertEqual(rc, 0, out)

    def test_close_allow_dump_needs_a_ledger_ruling(self):
        self.ready(draw=('fork', 'dump-3'))
        ledger = self.ledger('# rulings\nRuling R70: dump readings may decide b001.\n'
                             'not a Ruling R71 line\n')
        for kw in ({'allow_dump': 'R71', 'ledger_path': ledger},
                   {'allow_dump': 'R70', 'ledger_path': os.path.join(self.tmp, 'none.md')},
                   {'allow_dump': 'R70'}):
            rc, out = self.close(**kw)
            self.assertEqual(rc, 1, out)
            self.assertIn('--allow-dump', out)
        self.assertEqual(self.batches()['b001']['status'], 'assigned')
        rc, out = self.close(allow_dump='R70', ledger_path=ledger)
        self.assertEqual(rc, 0, out)
        self.assertIn('Ruling R70: dump readings may decide b001.', out)
        self.assertEqual(self.batches()['b001']['status'], 'closed')

    def test_close_cli_takes_allow_dump_and_ledger(self):
        from unittest import mock
        seen = {}

        def fake(**kw):
            seen.update(kw)
            return 0, 'ok\n'
        with mock.patch.object(self.wave, 'close_batch', fake), \
                contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.wave.main(
                ['close', 'b001', '--allow-dump', 'R70', '--ledger', '/l.md']), 0)
        self.assertEqual((seen['allow_dump'], seen['ledger_path']), ('R70', '/l.md'))
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(self.wave.main(['close', 'b001', '--allow-dump', 'R70']), 2)

    # -- sync before merge --------------------------------------------------

    def upstream(self):
        """A bare upstream with `first`, tracked by the repo's first; returns
        a second clone's path for pushing from."""
        self.bare = os.path.join(self.tmp, 'up.git')
        self.git('clone', '-q', '--bare', self.root, self.bare, cwd=self.tmp)
        self.git('remote', 'add', 'origin', self.bare)
        self.git('fetch', '-q', 'origin')
        self.git('branch', '-q', '--set-upstream-to=origin/first', 'first')
        other = os.path.join(self.tmp, 'other')
        self.git('clone', '-q', self.bare, other, cwd=self.tmp)
        return other

    def push_upstream(self, other, name='upstream.txt'):
        with open(os.path.join(other, name), 'w') as f:
            f.write('from upstream\n')
        self.git('add', name, cwd=other)
        self.git('commit', '-qm', 'upstream ' + name, cwd=other)
        self.git('push', '-q', 'origin', 'HEAD:first', cwd=other)
        return self.git('rev-parse', 'HEAD', cwd=other).stdout.strip()

    def test_close_pulls_before_fast_forward(self):
        from unittest import mock
        self.ready()
        other = self.upstream()
        x = self.push_upstream(other)
        # The batch branch was cut after the fetch: it already holds x.
        self.git('fetch', '-q', 'origin')
        self.git('merge', '-q', '--no-edit', x, cwd=self.wt)
        order = []
        real_sync, real_ff = self.wave._sync_base, self.wave._fast_forward
        with mock.patch.object(self.wave, '_sync_base',
                               lambda *a: (order.append('sync'), real_sync(*a))[1]), \
                mock.patch.object(self.wave, '_fast_forward',
                                  lambda *a: (order.append('ff'), real_ff(*a))[1]):
            rc, out = self.close()
        self.assertEqual(rc, 0, out)
        self.assertEqual(order, ['sync', 'ff'])
        self.assertEqual(self.first_sha(),
                         self.git('rev-parse', 'p3/wso-b001').stdout.strip())
        self.git('merge-base', '--is-ancestor', x, 'first')
        self.assertTrue(os.path.isfile(os.path.join(self.root, 'upstream.txt')))

    def test_close_refuses_when_base_diverged(self):
        self.ready()
        other = self.upstream()
        x = self.push_upstream(other)
        branch = self.git('rev-parse', 'p3/wso-b001').stdout.strip()
        rc, out = self.close()
        self.assertEqual(rc, 1, out)
        self.assertIn('not a fast-forward', out)
        self.assertEqual(self.batches()['b001']['status'], 'assigned')
        # The pull happened; the branch did not move first.
        self.assertEqual(self.first_sha(), x)
        self.assertEqual(self.git('rev-parse', 'p3/wso-b001').stdout.strip(), branch)

    def test_sync_base_refuses_when_the_pull_cannot_fast_forward(self):
        self.ready()
        other = self.upstream()
        self.push_upstream(other)
        with open(os.path.join(self.root, 'local.txt'), 'w') as f:
            f.write('local\n')
        self.git('add', 'local.txt')
        self.git('commit', '-qm', 'local only')
        err = self.wave._sync_base(self.root, 'first')
        self.assertIn('git pull --ff-only failed', err)
        rc, out = self.close()
        self.assertEqual(rc, 1, out)

    def test_sync_base_survives_pull_rebase_config_and_a_dirty_tree(self):
        self.ready()
        other = self.upstream()
        self.git('config', 'pull.rebase', 'true')
        path = os.path.join(self.root, 'docs', 'wso', 'disagreements.json')
        with open(path, 'a') as f:
            f.write('\n')                       # a dirty, unrelated tracked file
        self.assertIsNone(self.wave._sync_base(self.root, 'first'))   # nothing incoming
        x = self.push_upstream(other)
        self.assertIsNone(self.wave._sync_base(self.root, 'first'))   # incoming
        self.assertEqual(self.first_sha(), x)
        self.assertTrue(os.path.isfile(os.path.join(self.root, 'upstream.txt')))

    def test_sync_base_without_upstream_is_a_no_op(self):
        self.ready()
        self.assertIsNone(self.wave._sync_base(self.root, 'first'))

    def test_fast_forward_moves_checked_out_base(self):
        self.ready()
        branch = self.git('rev-parse', 'p3/wso-b001').stdout.strip()
        rc, out = self.close()
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.first_sha(), branch)
        # The checkout followed the ref: files present, nothing staged or dirty.
        self.assertTrue(os.path.isfile(
            os.path.join(self.root, 'docs', 'evidence', '643CC0.md')))
        self.assertEqual(self.git('status', '--porcelain').stdout.strip(), '')

    def test_fast_forward_without_a_checkout_uses_update_ref(self):
        self.ready()
        self.git('checkout', '-q', '-b', 'elsewhere')
        branch = self.git('rev-parse', 'p3/wso-b001').stdout.strip()
        self.assertIsNone(self.wave._fast_forward(self.root, 'first', 'p3/wso-b001'))
        self.assertEqual(self.first_sha(), branch)


class AttemptTest(unittest.TestCase):
    """tools/attempt.py: the per-attempt launch limit (ruling R16).  Nothing
    here launches anything."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.attempt = load_tool('attempt')

    def data(self):
        with open(os.path.join(self.tmp, 'attempt.json')) as f:
            return json.load(f)

    def test_attempt_refuses_third_launch(self):
        self.assertIsNone(self.attempt.launch(self.tmp, 'gltrace'))
        self.assertIsNone(self.attempt.launch(self.tmp, 'gltrace'))
        self.assertEqual(self.data()['launches'], 2)
        self.assertEqual(self.data()['limit'], 2)
        msg = self.attempt.launch(self.tmp, 'gltrace')
        self.assertEqual(msg, 'launch limit reached (2 of 2) for this attempt; '
                              'rerun with --new-attempt "<why>"')
        self.assertEqual(self.data()['launches'], 2)    # a refusal counts nothing
        self.assertEqual(os.listdir(self.tmp), ['attempt.json'])   # no temp file left

    def test_new_attempt_resets_and_records_reason(self):
        for _ in range(2):
            self.attempt.launch(self.tmp, 'play')
        self.assertIsNotNone(self.attempt.launch(self.tmp, 'play'))
        self.attempt.reset(self.tmp, 'fixed the scenario')
        d = self.data()
        self.assertEqual(d['launches'], 0)
        self.assertEqual(len(d['history']), 1)
        self.assertEqual(d['history'][0]['reason'], 'fixed the scenario')
        self.assertTrue(d['history'][0]['at'])
        self.assertIsNone(self.attempt.launch(self.tmp, 'play'))
        with self.assertRaises(ValueError):
            self.attempt.reset(self.tmp, '  ')

    def test_gate_resets_first_when_given_a_reason(self):
        for _ in range(2):
            self.attempt.gate(self.tmp, 'play')
        self.assertIn('launch limit reached', self.attempt.gate(self.tmp, 'play'))
        self.assertIsNone(self.attempt.gate(self.tmp, 'play', 'new idea'))
        self.assertEqual(self.data()['launches'], 1)

    def test_corrupt_counter_starts_over_and_folder_is_created(self):
        folder = os.path.join(self.tmp, 'runs')
        os.makedirs(folder)
        with open(os.path.join(folder, 'attempt.json'), 'w') as f:
            f.write('{broken')
        self.assertIsNone(self.attempt.launch(folder, 'x'))
        self.assertIsNone(self.attempt.launch(os.path.join(self.tmp, 'new', 'runs'), 'x'))

    def test_counters_are_separate_files(self):
        self.attempt.launch(self.tmp, 'play', name='attempt-play.json')
        self.attempt.launch(self.tmp, 'gltrace', name='attempt-gltrace.json')
        self.assertEqual(sorted(os.listdir(self.tmp)),
                         ['attempt-gltrace.json', 'attempt-play.json'])

    def test_empty_new_attempt_reason_exits_2(self):
        gltrace = load_tool('gltrace')
        play = load_tool('play')
        oracle = load_tool('oracle')
        for fn, argv in ((gltrace.parse_args, ['s', '--new-attempt', '']),
                         (play.parse_args, ['--linux', '--new-attempt', ' ']),
                         (oracle.main, ['replay', 'record', 'x', '--new-attempt', ''])):
            err = io.StringIO()
            with contextlib.redirect_stderr(err), self.assertRaises(SystemExit) as cm:
                fn(argv)
            self.assertEqual(cm.exception.code, 2)
            self.assertIn('needs a reason', err.getvalue())

    def test_winsandbox_start_counts_once_per_process_and_refuses_at_the_limit(self):
        from unittest import mock
        ws = load_tool('winsandbox')
        launches = []
        host = os.path.join(self.tmp, 'host')
        os.makedirs(host)
        wsb = os.path.join(host, 'run.wsb')
        patches = [mock.patch.object(ws, 'sandbox_running', lambda: False),
                   mock.patch.object(ws, 'wslpath_w', lambda p: p),
                   mock.patch.object(ws, 'RUNS_ROOT', self.tmp),
                   mock.patch.object(ws, '_attempt', {'counted': False, 'reason': None}),
                   mock.patch.object(ws.subprocess, 'run',
                                     lambda *a, **k: launches.append(a))]
        for pt in patches:
            pt.start()
            self.addCleanup(pt.stop)
        ws.start_sandbox(host, wsb, lambda m: None)
        ws.start_sandbox(host, wsb, lambda m: None)    # the same process: one launch
        self.assertEqual(len(launches), 2)
        with open(os.path.join(self.tmp, 'attempt-winsandbox.json')) as f:
            self.assertEqual(json.load(f)['launches'], 1)
        # A new process at the limit is refused before anything launches.
        with open(os.path.join(self.tmp, 'attempt-winsandbox.json'), 'w') as f:
            json.dump({'launches': 2, 'limit': 2, 'history': []}, f)
        ws._attempt.update(counted=False, reason=None)
        with self.assertRaises(RuntimeError) as cm:
            ws.start_sandbox(host, wsb, lambda m: None)
        self.assertIn('launch limit reached (2 of 2)', str(cm.exception))
        self.assertEqual(len(launches), 2)
        ws.set_new_attempt('driver frida b011')
        ws.start_sandbox(host, wsb, lambda m: None)
        self.assertEqual(len(launches), 3)
        with open(os.path.join(self.tmp, 'attempt-winsandbox.json')) as f:
            d = json.load(f)
        self.assertEqual((d['launches'], d['history'][-1]['reason']), (1, 'driver frida b011'))

    def test_play_counts_once_per_process(self):
        from unittest import mock
        play = load_tool('play')
        args = play.parse_args(['--linux', '--headless'])
        with mock.patch.object(play, '_LAUNCH_COUNTED', False), \
                mock.patch.object(play, 'launch_refusal',
                                  lambda a: calls.append(1)) as _:
            calls = []
            play.count_launch(args)
            play.count_launch(args)
        self.assertEqual(calls, [1])

    def test_launchers_take_new_attempt_and_check_the_counter(self):
        gltrace = load_tool('gltrace')
        play = load_tool('play')
        self.assertEqual(gltrace.parse_args(['s', '--new-attempt', 'why']).new_attempt, 'why')
        self.assertIsNone(gltrace.parse_args(['s']).new_attempt)
        args = play.parse_args(['--linux', '--headless', '--new-attempt', 'why'])
        self.assertEqual(args.new_attempt, 'why')
        # play.launch_refusal counts in the runs folder it is given.
        args = play.parse_args(['--linux', '--headless'])
        self.assertIsNone(play.launch_refusal(args, self.tmp))
        self.assertIsNone(play.launch_refusal(args, self.tmp))
        self.assertIn('launch limit reached', play.launch_refusal(args, self.tmp))
        args = play.parse_args(['--linux', '--headless', '--new-attempt', 'why'])
        self.assertIsNone(play.launch_refusal(args, self.tmp))


class ClaimsTest(unittest.TestCase):
    """tools/claims.py and the gate lock in tools/gates.sh."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.tmp = self._tmp.name
        self.claims = load_tool('claims')
        self.wave = load_tool('wave')
        self.cp = os.path.join(self.tmp, 'claims.json')
        self.sp = os.path.join(self.tmp, 'wave-state.json')
        self.kw = dict(claims_path=self.cp, state_path=self.sp)

    def write_state(self, files, status='assigned'):
        with open(self.sp, 'w') as f:
            json.dump({'batches': {'b7': {
                'files': files, 'rows': [], 'branch': 'x',
                'status': status, 'verdicts': {}}}}, f)

    def test_claim_and_release(self):
        self.claims.claim('s1', 'claude', ['src/A.cpp', 'src/B.cpp'], **self.kw)
        with open(self.cp) as f:
            entry = json.load(f)['s1']
        self.assertEqual(entry['holder'], 'claude')
        self.assertEqual(entry['files'], ['src/A.cpp', 'src/B.cpp'])
        self.assertTrue(entry['since'])
        self.assertEqual(self.claims.owned('src/A.cpp', **self.kw), 's1')
        # The same stream may claim more; it does not conflict with itself.
        self.claims.claim('s1', 'claude', ['src/C.cpp'], **self.kw)
        self.assertEqual(self.claims.owned('src/C.cpp', **self.kw), 's1')
        self.claims.release('s1', **self.kw)
        self.assertIsNone(self.claims.owned('src/A.cpp', **self.kw))
        self.claims.release('s1', **self.kw)  # releasing twice is fine

    def test_claim_conflicts_with_other_stream(self):
        self.claims.claim('s1', 'claude', ['src/A.cpp'], **self.kw)
        with self.assertRaises(self.claims.ClaimError) as cm:
            self.claims.claim('s2', 'other', ['src/B.cpp', 'src/A.cpp'], **self.kw)
        self.assertIn('s1', str(cm.exception))
        self.assertIsNone(self.claims.owned('src/B.cpp', **self.kw))

    def test_claim_conflicts_with_assigned_batch(self):
        self.write_state(['src/A.cpp'])
        with self.assertRaises(self.claims.ClaimError) as cm:
            self.claims.claim('s1', 'claude', ['src/A.cpp'], **self.kw)
        self.assertIn('b7', str(cm.exception))
        self.write_state(['src/A.cpp'], status='closed')
        self.claims.claim('s1', 'claude', ['src/A.cpp'], **self.kw)

    def test_owned_reports_batches_and_streams(self):
        self.write_state(['src/A.cpp'])
        self.claims.claim('s1', 'claude', ['src/B.cpp'], **self.kw)
        self.assertEqual(self.claims.owned('src/A.cpp', **self.kw), 'b7')
        self.assertEqual(self.claims.owned('src/B.cpp', **self.kw), 's1')
        self.assertIsNone(self.claims.owned('src/Z.cpp', **self.kw))

    def test_cli(self):
        tool = os.path.join(ROOT, 'tools', 'claims.py')

        def run(*a):
            return subprocess.run(
                ['python3', '-I', tool, '--claims', self.cp, '--state', self.sp,
                 *a], capture_output=True, text=True)
        self.assertEqual(run('claim', 's1', 'me', 'src/A.cpp').returncode, 0)
        r = run('claim', 's2', 'you', 'src/A.cpp')
        self.assertEqual(r.returncode, 1)
        self.assertIn('s1', r.stderr)
        self.assertIn('s1', run('list').stdout)
        self.assertEqual(run('release', 's1').returncode, 0)
        self.assertNotIn('s1', run('list').stdout)

    def test_next_batch_skips_claimed_file(self):
        disagreements = os.path.join(self.tmp, 'disagreements.json')
        with open(disagreements, 'w') as f:
            json.dump(_WAVE_ROWS, f)
        self.claims.claim('s1', 'claude', ['src/A.cpp'], **self.kw)
        rc, _ = self.wave.next_batch(disagreements, self.sp, size=20,
                                     claims_path=self.cp)
        self.assertEqual(rc, 0)
        with open(self.sp) as f:
            b = json.load(f)['batches']['b001']
        self.assertNotIn('src/A.cpp', b['files'])
        self.assertIn('src/B.cpp', b['files'])

    # -- the gate lock -----------------------------------------------------

    def lock_repo(self):
        root = os.path.join(self.tmp, 'repo')
        os.makedirs(os.path.join(root, 'tools'))
        subprocess.run(['git', 'init', '-q', root], check=True)
        shutil.copy(os.path.join(ROOT, 'tools', 'gates.sh'),
                    os.path.join(root, 'tools', 'gates.sh'))
        return root

    def hold_lock(self, root):
        """The test itself holds the temp repo's gate lock, as a gate would."""
        fd = os.open(os.path.join(root, '.gates.lock'),
                     os.O_RDWR | os.O_CREAT, 0o666)
        fcntl.flock(fd, fcntl.LOCK_EX)
        os.write(fd, ('held by pid %d\n' % os.getpid()).encode())
        return fd

    def test_gate_lock_serializes(self):
        root = self.lock_repo()
        gates = os.path.join(root, 'tools', 'gates.sh')
        fd = self.hold_lock(root)
        try:
            # --no-wait fails fast, exit 3, naming the holder.
            a = subprocess.run(['bash', gates, '--no-wait', '--dry-lock', '0'],
                               capture_output=True, text=True, timeout=30)
            self.assertEqual(a.returncode, 3, a.stderr)
            self.assertIn('pid %d' % os.getpid(), a.stderr)
            # Without it the gate blocks: it says so, and has not exited.
            b = subprocess.Popen(['bash', gates, '--dry-lock', '0'],
                                 stderr=subprocess.PIPE, text=True)
            try:
                self.assertIn('waiting for the gate lock', b.stderr.readline())
                self.assertIsNone(b.poll())
            except BaseException:
                b.kill()
                b.communicate()
                raise
        finally:
            os.close(fd)  # releases the flock
        _, err = b.communicate(timeout=30)
        self.assertEqual(b.returncode, 0, err)

    def test_gate_lock_not_inherited_by_children(self):
        root = self.lock_repo()
        gates = os.path.join(root, 'tools', 'gates.sh')
        holder = subprocess.Popen(['bash', gates, '--dry-lock', '30'],
                                  start_new_session=True)
        self.addCleanup(holder.wait)

        def kill_group():
            try:
                os.killpg(holder.pid, signal.SIGKILL)
            except ProcessLookupError:   # nothing left in the group
                pass
        self.addCleanup(kill_group)
        lock = os.path.join(root, '.gates.lock')
        deadline = time.time() + 30
        while time.time() < deadline and not (
                os.path.exists(lock) and os.path.getsize(lock) > 0):
            time.sleep(0.02)
        os.kill(holder.pid, signal.SIGKILL)
        holder.wait(timeout=30)
        # The orphaned sleep is still running, and must not hold the lock.
        # The kill can land between bash's fork and the child closing fd 9,
        # so the lock may take a moment to free (ruling R60).
        fd = os.open(lock, os.O_RDWR)
        self.addCleanup(os.close, fd)
        end = time.time() + 5
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.time() >= end:
                    raise
                time.sleep(0.05)

    def test_queues_keep_files_disjoint(self):
        # A file in an assigned wave1 batch is skipped by the import
        # queue's next_batch, and claims.owned names that batch.
        self.write_state(['src/A.cpp'])
        rows = [
            {'file': 'src/A.cpp', 'name': 'F', 'address': '0x401000',
             'category': 'theirs_real_ours_stub', 'their_len': 90, 'our_len': 2},
            {'file': 'src/B.cpp', 'name': 'G', 'address': '0x402000',
             'category': 'theirs_real_ours_stub', 'their_len': 50, 'our_len': 2}]
        dp = os.path.join(self.tmp, 'disagreements.json')
        with open(dp, 'w') as f:
            json.dump(rows, f)
        self.assertEqual(self.claims.owned('src/A.cpp', **self.kw), 'b7')
        self.assertIsNone(self.claims.owned('src/B.cpp', **self.kw))
        imp = os.path.join(self.tmp, 'import-state.json')
        rc, out = self.wave.next_batch(
            dp, imp, 20, queue='import', repo=self.tmp,
            import_path=os.path.join(self.tmp, 'import-list.json'),
            worktree_root=os.path.join(self.tmp, 'wts'),
            triage_path=None, base='first')
        # No repository here: the worktree cut fails, and nothing is
        # recorded; so ask without cutting through the plain state path.
        self.assertFalse(os.path.exists(imp), out)
        with mock.patch.object(self.wave, '_cut_worktree',
                               return_value=('/wt', 'new')):
            rc, out = self.wave.next_batch(
                dp, imp, 20, queue='import', repo=self.tmp,
                import_path=os.path.join(self.tmp, 'import-list.json'),
                base='first')
        self.assertEqual(rc, 0, out)
        self.assertIn('i001 assigned: 1 rows', out)
        with open(imp) as f:
            self.assertEqual(json.load(f)['batches']['i001']['files'],
                             ['src/B.cpp'])
        # The import batch's file is now held from the claims side too.
        self.assertEqual(self.claims.owned('src/B.cpp', **self.kw), 'i001')
        with self.assertRaises(self.claims.ClaimError) as cm:
            self.claims.claim('s9', 'me', ['src/B.cpp'], **self.kw)
        self.assertIn('i001', str(cm.exception))

    def test_a_missing_second_state_file_holds_nothing(self):
        self.write_state(['src/A.cpp'])
        self.assertFalse(os.path.exists(
            os.path.join(self.tmp, 'import-state.json')))
        self.claims.claim('s1', 'me', ['src/Z.cpp'], **self.kw)
        self.assertEqual(self.claims.owned('src/Z.cpp', **self.kw), 's1')

    # -- claims go to the checkout that has `first` ------------------------

    def two_worktrees(self, first_checked_out=True):
        env = dict(os.environ, GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t',
                   GIT_COMMITTER_NAME='t', GIT_COMMITTER_EMAIL='t@t',
                   GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM='1')

        def git(*a, cwd):
            subprocess.run(['git', *a], cwd=cwd, env=env, check=True,
                           capture_output=True)
        main = os.path.join(self.tmp, 'main')
        other = os.path.join(self.tmp, 'other')
        os.makedirs(os.path.join(main, 'tools'))
        os.makedirs(os.path.join(main, 'docs', 'wso'))
        for n in ('claims.py', 'filelock.py'):
            shutil.copy(os.path.join(ROOT, 'tools', n),
                        os.path.join(main, 'tools', n))
        git('init', '-q', '-b', 'first' if first_checked_out else 'main',
            cwd=main)
        git('add', '-A', cwd=main)
        git('commit', '-q', '-m', 'x', cwd=main)
        git('worktree', 'add', '-q', '-b', 'stream', other, cwd=main)
        return main, other

    def test_claims_go_to_the_first_checkout(self):
        main, other = self.two_worktrees()
        tool = os.path.join(other, 'tools', 'claims.py')
        r = subprocess.run(['python3', '-I', tool, 'claim', 's1', 'me',
                            './src/A.cpp', os.path.join(other, 'src', 'B.cpp')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(os.path.join(main, 'docs', 'wso', 'claims.json')) as f:
            self.assertEqual(json.load(f)['s1']['files'],
                             ['src/A.cpp', 'src/B.cpp'])
        self.assertFalse(os.path.exists(
            os.path.join(other, 'docs', 'wso', 'claims.json')))

    def test_claims_refuse_without_a_first_checkout(self):
        main, other = self.two_worktrees(first_checked_out=False)
        tool = os.path.join(other, 'tools', 'claims.py')
        r = subprocess.run(['python3', '-I', tool, 'claim', 's1', 'me', 'a.c'],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2)
        self.assertIn('first', r.stderr)

    def test_claim_paths_are_normalised(self):
        self.claims.claim('s1', 'me', ['./src/A.cpp',
                                       os.path.join(ROOT, 'src', 'B.cpp')],
                          **self.kw)
        with open(self.cp) as f:
            self.assertEqual(json.load(f)['s1']['files'],
                             ['src/A.cpp', 'src/B.cpp'])
        self.assertEqual(self.claims.owned('src/../src/A.cpp', **self.kw), 's1')
        for bad in ('../x.cpp', '/etc/passwd', '.'):
            with self.assertRaises(self.claims.ClaimError):
                self.claims.claim('s2', 'me', [bad], **self.kw)


if __name__ == '__main__':
    unittest.main()
