# iwd2r. Distributed under LICENSE.md.
"""Tests for tools/check_notices.py, tools/gen_map.py and the pure parts of
tools/play.py, tools/realinput.py and tools/winsandbox.py.

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
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECKER = os.path.join(ROOT, 'tools', 'check_notices.py')
MODIFIED = '// Modified for iwd2r; see NOTICE.md'
NEW_CPP = '// iwd2r. Distributed under LICENSE.md.'
NEW_HASH = '# iwd2r. Distributed under LICENSE.md.'


class CheckNoticesTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.repo = self._tmp.name
        self.addCleanup(self._tmp.cleanup)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Test')
        self.git('config', 'user.email', 'test@example.com')
        self.git('config', 'commit.gpgsign', 'false')
        self.write('src/a.cpp', 'int a;\n')
        self.git('add', '-A')
        self.git('commit', '-q', '-m', 'base')
        self.base = self.git('rev-parse', 'HEAD').strip()

    def git(self, *args):
        return subprocess.run(['git', *args], cwd=self.repo, check=True,
                              capture_output=True, text=True).stdout

    def write(self, rel, text):
        path = os.path.join(self.repo, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            f.write(text)

    def check(self):
        return subprocess.run([sys.executable, '-I', CHECKER, '--base', self.base],
                              cwd=self.repo, capture_output=True, text=True)

    def test_clean_repo_passes(self):
        self.assertEqual(self.check().returncode, 0)

    def test_modified_file_without_notice_fails(self):
        self.write('src/a.cpp', 'int a;\nint b;\n')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('src/a.cpp', r.stdout)

    def test_modified_file_with_notice_passes(self):
        self.write('src/a.cpp', MODIFIED + '\nint a;\nint b;\n')
        self.assertEqual(self.check().returncode, 0)

    def test_new_cpp_without_header_fails(self):
        self.write('src/b.cpp', 'int b;\n')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('src/b.cpp', r.stdout)

    def test_new_cpp_with_header_passes(self):
        self.write('src/b.cpp', NEW_CPP + '\nint b;\n')
        self.assertEqual(self.check().returncode, 0)

    def test_new_cmake_and_python_use_hash_form(self):
        self.write('CMakeLists.txt', 'project(x)\n')
        self.write('tools/x.py', NEW_CPP + '\n')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('CMakeLists.txt', r.stdout)
        self.assertIn('tools/x.py', r.stdout)
        self.write('CMakeLists.txt', NEW_HASH + '\nproject(x)\n')
        self.write('tools/x.py', NEW_HASH + '\n')
        self.assertEqual(self.check().returncode, 0)

    def test_exempt_paths_and_non_source_files(self):
        self.write('third_party/z/z.c', 'int z;\n')
        self.write('docs/x.cpp', 'int x;\n')
        self.write('.github/workflows/ci.yml', 'a: b\n')
        self.write('README.md', 'hi\n')
        self.write('data.json', '{}\n')
        self.assertEqual(self.check().returncode, 0)

    def test_modified_file_with_new_header_fails(self):
        self.write('src/a.cpp', NEW_CPP + '\nint a;\n')
        r = self.check()
        self.assertEqual(r.returncode, 1)
        self.assertIn('src/a.cpp', r.stdout)

    def test_deleted_file_ignored(self):
        os.remove(os.path.join(self.repo, 'src', 'a.cpp'))
        self.assertEqual(self.check().returncode, 0)


GEN_MAP = os.path.join(ROOT, 'tools', 'gen_map.py')


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
winsandbox = load_tool('winsandbox')


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
        for argv in (['--resolution', '1366'], ['--set', 'NoSlash=1'],
                     ['--set', 'A/B'], ['--set', 'A/B=1;C/D=2'],
                     ['--frames', '0'], ['--fullscreen', '--windowed']):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit, msg=argv):
                    play.parse_args(argv)

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

    def test_batch_set_escapes_percent(self):
        self.assertEqual(play.batch_set('X', 'a & b 100%'), 'set "X=a & b 100%%"')

    def test_sync_list_skips_build_and_runs(self):
        self.assertEqual(play.sync_list(['a.cpp', 'build/x', 'runs/1/log.txt',
                                         'tools/build/y', '']),
                         ['a.cpp', 'tools/build/y'])


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
            'quit\n')
        self.assertEqual(actions, [
            {'op': 'wait', 'frames': 60, 'line': 3},
            {'op': 'click', 'x': 10, 'y': 20, 'line': 4},
            {'op': 'rclick', 'x': 3, 'y': 4, 'line': 5},
            {'op': 'move', 'x': 0, 'y': 0, 'line': 6},
            {'op': 'key', 'key': 'ESCAPE', 'line': 7},
            {'op': 'key', 'key': 'A', 'line': 8},
            {'op': 'type', 'text': 'Hello  World 2', 'line': 9},
            {'op': 'shot', 'name': '01_start', 'line': 10},
            {'op': 'quit', 'line': 11},
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
            ('wait 1\n\n# x\njump 1\n', "line 4: unknown command 'jump'"),
        ]
        for text, message in cases:
            with self.assertRaises(realinput.ScenarioError, msg=text) as cm:
                realinput.parse_scenario(text)
            self.assertEqual(str(cm.exception), message)

    def test_shipped_scenarios_parse(self):
        for path in scenario_files():
            self.assertTrue(realinput.load_scenario(path), path)

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

    def test_vk_names_match_script_cpp(self):
        # Every name in platform/script.cpp's kNamedKeys, and no other.
        with open(os.path.join(ROOT, 'platform', 'script.cpp')) as f:
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

    def test_describe_round_trips(self):
        text = 'wait 5\nclick 1 2\nkey TAB\ntype ab\nshot x\nquit\n'
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
        self.assertEqual(realinput.screen_size(800), (800, 600))
        self.assertEqual(realinput.screen_size(1024), (1024, 768))
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
            with open(os.path.join(tmp, 'driver.ps1')) as f:
                self.assertEqual(f.readline().strip(), NEW_HASH)

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


if __name__ == '__main__':
    unittest.main()
