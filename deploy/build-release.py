"""Build/package entirely on the workstation. Never installs/builds remotely.

Windows example:
 python deploy/build-release.py --zig .tools/zig-x86_64-windows-0.15.2/zig.exe \
   --npm-cli .tools/npm/package/bin/npm-cli.js
Native Linux/macOS can use an installed Zig/npm with the same target.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
from datetime import datetime, timezone

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser()
parser.add_argument('--zig', default='zig')
parser.add_argument('--npm-cli')
parser.add_argument('--engine', help='Already cross-built Linux x86_64 static ELF')
args = parser.parse_args()
os.chdir(root)
node = shutil.which('node')
npm = [node, str(Path(args.npm_cli).resolve())] if args.npm_cli else [shutil.which('npm') or 'npm']
subprocess.run(npm + ['ci', '--no-audit', '--no-fund'], cwd=root/'web', check=True)
subprocess.run([node, 'scripts/build.mjs'], cwd=root/'web', check=True)
release = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
out = root/'deploy'/'artifacts'/release
server = out/'server'
(server/'bin').mkdir(parents=True)
if args.engine:
    shutil.copy2(args.engine, server/'bin'/'cardgame_server')
else:
    sources = sorted(str(p.relative_to(root)) for p in (root/'src').rglob('*.cpp'))
    env = {**os.environ, 'ZIG_GLOBAL_CACHE_DIR': str(root/'.tools'/'zig-global-cache')}
    zig = str(Path(args.zig).resolve()) if Path(args.zig).exists() else args.zig
    subprocess.run([zig,'c++','-target','x86_64-linux-musl','-std=c++17','-O2','-pthread','-Iinclude',
                    *sources,'-o',str(server/'bin'/'cardgame_server')],env=env,check=True)
if (server/'bin'/'cardgame_server').read_bytes()[:4] != b'\x7fELF':
    raise SystemExit('Expected Linux ELF, not Windows executable')
shutil.copytree(root/'web'/'dist',out/'client')
for f in ['main.mjs','server.mjs','commands.mjs']: shutil.copy2(root/'web'/'server'/f,server/f)
for f in ['package.json','package-lock.json']: shutil.copy2(root/'web'/f,server/f)
# Pure JS production tree, materialized here on the workstation. No optional
# native accelerators, development packages, compiler or npm cache are shipped.
subprocess.run(npm+['ci','--omit=dev','--omit=optional','--ignore-scripts','--no-audit','--no-fund'],cwd=server,check=True)
shutil.copy2(root/'deploy'/'ecosystem.config.cjs',out/'ecosystem.config.cjs')
shutil.copy2(root/'deploy'/'nginx.conf',out/'nginx.conf')
(out/'release.json').write_text(json.dumps({'release':release,'target':'x86_64-linux-musl','node':'>=20'},indent=2))
archive=out.parent/(release+'.tar.gz')
with tarfile.open(archive,'w:gz') as tar:
    for path in out.iterdir(): tar.add(path,arcname=path.name)
digest=hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.sha256').write_text(digest+'  '+archive.name+'\n')
print(json.dumps({'archive':str(archive),'sha256':digest,'release':release}))
