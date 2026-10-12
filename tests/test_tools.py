# iwd2r. Distributed under LICENSE.md.
"""Tests for tools/check_notices.py.

Run from the repository root: python3 -I tests/test_tools.py

Each test builds a throwaway git repository in a temporary directory, runs the
checker as a subprocess with cwd set to that repository, and removes the
directory afterwards.
"""

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


if __name__ == '__main__':
    unittest.main()
