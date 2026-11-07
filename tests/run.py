# SPDX-License-Identifier: MIT
# Copyright (c) 2026 ALOLAN
"""Windows LLVM 无 CRT 测试：python tests/run.py --clang PATH/clang.exe。"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--clang', required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
clang = Path(args.clang).resolve()
vendor = root / 'User/ThirdParty/MultiButton'
manifest = json.loads((vendor / 'provenance.json').read_text(encoding='utf-8'))
for name, digest in manifest['sha256'].items():
    assert hashlib.sha256((vendor / name).read_bytes()).hexdigest() == digest, name
print('PASS upstream SHA-256', flush=True)
sources = ['tests/test_button.c', 'tests/runtime.c', 'tests/start_fault.c', 'User/Module/mod_button.c',
           'User/ThirdParty/MultiButton/multi_button.c', 'examples/button_example.c']
common = ['-std=c99', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
          '-Itests/stubs', '-IUser/Module', '-IUser/ThirdParty/MultiButton', '-Iexamples',
          '-Wall', '-Wextra']
tests = ['test_not_initialized', 'test_debounce', 'test_single', 'test_double_repeat',
         'test_long', 'test_lifecycle', 'test_parameters', 'test_reentry_and_isolation',
         'test_start_rollback', 'test_example_schedule', 'test_example_wrap']
failures = []
for opt in ['0', '2']:
    out = root / 'build' / ('host-O' + opt)
    out.mkdir(parents=True, exist_ok=True)
    objects = []
    for index, source in enumerate(sources):
        obj = out / f'{index}.obj'
        strict = [] if 'ThirdParty' in source else ['-Werror', '-Wconversion', '-Wsign-conversion']
        seam = ['-Dbutton_start=button_start_real'] if 'ThirdParty' in source else []
        subprocess.run([str(clang), *common, *strict, *seam, '-O' + opt, '-c', source, '-o', str(obj)],
                       cwd=root, check=True)
        objects.append(str(obj))
    dll = out / 'tests.dll'
    subprocess.run([str(clang.with_name('lld.exe')), '-flavor', 'link', '/dll', '/noentry',
                    '/nodefaultlib', f'/out:{dll}', *objects], check=True)
    library = ctypes.CDLL(str(dll))
    for name in tests:
        function = getattr(library, name)
        function.restype = ctypes.c_int
        result = function()
        print(f'{"FAIL" if result else "PASS"} O{opt} {name}'
              + (f': test source line {result}' if result else ''), flush=True)
        if result:
            failures.append((opt, name, result))
out = root / 'build' / 'compile32'
out.mkdir(exist_ok=True)
for index, source in enumerate(sources):
    subprocess.run([str(clang), *common, '-m32', '-c', source, '-o', str(out / f'{index}.obj')],
                   cwd=root, check=True)
print('PASS 32-bit compile (not executed)', flush=True)
if failures:
    raise SystemExit(f'{len(failures)} failed behavior tests: {failures}')
