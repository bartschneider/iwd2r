# iwd2r. Distributed under LICENSE.md.
"""Tests for tools/check_notices.py, tools/gen_map.py and the pure parts of
tools/play.py.

Run from the repository root: python3 -I tests/test_tools.py

Each test builds a throwaway git repository in a temporary directory, runs the
checker as a subprocess with cwd set to that repository, and removes the
directory afterwards.
"""

import contextlib
import datetime
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest

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


def load_play():
    spec = importlib.util.spec_from_file_location(
        'play', os.path.join(ROOT, 'tools', 'play.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


play = load_play()


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


if __name__ == '__main__':
    unittest.main()
