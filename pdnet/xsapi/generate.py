"""Generates pdnet's stand-in for the Xbox Live library (XSAPI) from Microsoft.Xbox.Services.winmd.

  py -I generate.py <path to Microsoft.Xbox.Services.winmd>

- gen/      C++/WinRT projection headers + module.g.cpp (regenerated every run; not in git)
- stubs/    one .h/.cpp per XSAPI runtime class. Created only when MISSING, so hand-written implementations
            are never overwritten. Unimplemented methods log "xsapi-stub: Class::Method" to pdnet.log and
            report E_NOTIMPL to the game.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CPPWINRT = r'C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\x64\cppwinrt.exe'
SDK = '10.0.22000.0'


def transform(path, text):
    """Make a generated stub compilable and self-reporting."""
    text = re.sub(r'\n// Note: Remove this static_assert.*?\nstatic_assert\(false, "[^"]*"\);\n', '\n', text, flags=re.S)
    if path.endswith('.cpp'):
        text = text.replace('#include "pch.h"', '#include "xsapi_pch.h"', 1)
        out, signature = [], ''
        for line in text.split('\n'):
            stripped = line.strip()
            if stripped == 'throw hresult_not_implemented();':
                name = re.search(r'([\w:~]+)\s*\(', signature)
                fn = name.group(1) if name else '?'
                macro = 'PDNET_XSAPI_STUB_NOEXCEPT' if 'noexcept' in signature else 'PDNET_XSAPI_STUB'
                line = line.replace('throw hresult_not_implemented();', f'{macro}("{fn}");')
            elif stripped and stripped not in ('{', '}'):
                signature = stripped
            out.append(line)
        text = '\n'.join(out)
    return text


def main():
    winmd = sys.argv[1]
    gen = os.path.join(HERE, 'gen')
    stubs = os.path.join(HERE, 'stubs')
    tmp = os.path.join(HERE, 'gen-component')
    shutil.rmtree(gen, ignore_errors=True)
    shutil.rmtree(tmp, ignore_errors=True)
    subprocess.check_call([CPPWINRT, '-in', winmd, '-ref', SDK, '-component', tmp, '-out', gen])

    os.makedirs(stubs, exist_ok=True)
    created = kept = 0
    for name in sorted(os.listdir(tmp)):
        src = os.path.join(tmp, name)
        dst = os.path.join(stubs, name)
        if os.path.exists(dst):
            kept += 1
            continue
        with open(src, encoding='utf-8') as f:
            text = transform(name, f.read())
        with open(dst, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        created += 1
    shutil.rmtree(tmp, ignore_errors=True)

    # module.g.cpp dispatches activation by class name; pdnet calls it in xbox=off mode.
    module = os.path.join(gen, 'module.g.cpp')
    with open(module, encoding='utf-8') as f:
        text = f.read().replace('#include "pch.h"', '#include "xsapi_pch.h"', 1)
    with open(module, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print(f'projection + module.g.cpp regenerated in {gen}')
    print(f'stubs: {created} created, {kept} already present (kept as-is)')


main()
