"""Run the same checks locally and in GitHub pull requests (Python 3.10+)."""
import argparse
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]


def run(args, cwd=ROOT, env=None):
    print('\n> '+subprocess.list2cmdline([str(x) for x in args]), flush=True)
    subprocess.run([str(x) for x in args], cwd=cwd, env=env, check=True)


def check_documents():
    missing = []
    paths = [ROOT/'README.md', ROOT/'design.md', *(ROOT/'docs').rglob('*.md')]
    for path in paths:
        for link in re.findall(r'\]\(([^\s)]+)\)', path.read_text(encoding='utf-8')):
            parts = urlsplit(link.strip('<>'))
            if parts.scheme or not parts.path: continue
            target = (path.parent/unquote(parts.path)).resolve()
            if not target.is_relative_to(ROOT) or not target.exists():
                missing.append(f'{path.relative_to(ROOT)} -> {link}')
    if missing: raise ValueError('Broken local documentation links:\n'+'\n'.join(missing))
    print('Documentation file links: passed', flush=True)


def check_generated_cards():
    spec = importlib.util.spec_from_file_location('card_importer', ROOT/'tools/import_function_card.py')
    importer = importlib.util.module_from_spec(spec); spec.loader.exec_module(importer)
    definitions = sorted((ROOT/'cards/function').glob('*.json'))
    changed = set();keys = set()
    for source in definitions:
        card = importer.load_card(source);keys.add(card['key'])
        changed.update(importer.import_card(source,ROOT,force=True,check=True))
    if not definitions and (ROOT/'src/CardCatalog.cpp').read_text(encoding='utf-8') != importer.render_catalog([]):
        changed.add('src/CardCatalog.cpp')
    for directory,suffix in [('include/cards/generated','.hpp'),('src/cards/generated','.cpp')]:
        for path in (ROOT/directory).glob('*'+suffix):
            if path.stem not in keys: changed.add(str(path.relative_to(ROOT)))
    if changed:
        raise ValueError('JSON cards and generated files are out of sync:\n'+'\n'.join(sorted(changed))+
                         '\nRegenerate changed cards with tools/import_function_card.py --force and commit the outputs.')
    print(f'JSON/generated C++ consistency: passed ({len(definitions)} card definitions)', flush=True)


def native(args, build):
    configure = ['cmake','-S',ROOT,'-B',build,'-DCMAKE_BUILD_TYPE=Debug',
                 '-DBUILD_TESTING=ON',f'-DPython3_EXECUTABLE={sys.executable}']
    if args.generator: configure.extend(['-G',args.generator])
    run(configure)
    run(['cmake','--build',build,'--config','Debug','--parallel',args.jobs])
    run(['ctest','--test-dir',build,'--build-config','Debug','--output-on-failure'])


def npm_command(cli):
    node = shutil.which('node')
    if not node: raise ValueError('Node.js is required for web checks')
    if cli:
        script = Path(cli).resolve()
        if not script.is_file(): raise ValueError(f'npm CLI does not exist: {script}')
        return [node,str(script)]
    npm = shutil.which('npm')
    if not npm: raise ValueError('npm is not on PATH; install Node.js/npm or pass --npm-cli path/to/npm-cli.js')
    if Path(npm).suffix.lower() in ('.cmd','.bat'):
        script = Path(npm).parent/'node_modules/npm/bin/npm-cli.js'
        if not script.is_file(): raise ValueError('Pass --npm-cli to the npm-cli.js installed with Node.js')
        return [node,str(script)]
    return [npm]


def find_engine(build):
    for path in [build/'cardgame_server',build/'cardgame_server.exe',build/'Debug/cardgame_server.exe']:
        if path.is_file():return path.resolve()
    raise ValueError(f'No server binary in {build}; run --suite native first or provide --engine')


def web(args, build):
    npm = npm_command(args.npm_cli);node = shutil.which('node')
    engine = Path(args.engine).resolve() if args.engine else find_engine(build)
    if not engine.is_file(): raise ValueError(f'Server binary does not exist: {engine}')
    env = dict(os.environ, CARDGAME_ENGINE=str(engine))
    env.pop('TARGET_URL',None)  # PR checks always use disposable local rooms.
    directory = ROOT/'web'
    run(npm+['ci','--no-audit','--no-fund'],directory,env)
    run(npm+['run','build'],directory,env)
    run(npm+['test'],directory,env)
    if args.install_browser:
        run([node,'node_modules/playwright-core/cli.js','install','--with-deps','chromium'],directory,env)
    run(npm+['run','test:browser'],directory,env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite',choices=['native','web','all'],default='all')
    parser.add_argument('--build-dir',default='build/pr-checks')
    parser.add_argument('--generator',help='For example: MinGW Makefiles on a MinGW workstation')
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--npm-cli',help='Optional local npm-cli.js path when npm is not on PATH')
    parser.add_argument('--engine',help='Existing native server binary for --suite web')
    parser.add_argument('--install-browser',action='store_true',help='Install Chromium and Linux dependencies on a CI runner')
    args = parser.parse_args()
    try:
        if args.jobs<1:raise ValueError('--jobs must be positive')
        build = (ROOT/args.build_dir).resolve()
        if not build.is_relative_to(ROOT):raise ValueError('--build-dir must stay inside the project')
        check_documents();check_generated_cards()
        if args.suite in ('native','all'):native(args,build)
        if args.suite in ('web','all'):web(args,build)
    except (ValueError,OSError,subprocess.CalledProcessError) as e:
        print(f'\nPR checks FAILED: {e}',file=sys.stderr);return 1
    print(f'\nPR checks PASSED: {args.suite}',flush=True);return 0


if __name__=='__main__':
    raise SystemExit(main())
