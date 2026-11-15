"""Reject potential automatic effect cycles in the combined card-design graph.

This is a conservative contract check, not a proof about arbitrary C++ programs.
JSON effects are derived from atomic operations. Handwritten effects have reviewed,
source-bound summaries in cards/native_effects.json. No game is started here.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('effect_card_importer', ROOT/'tools/import_function_card.py')
importer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(importer)

# Empty outputs really mean no immediate event is emitted by this primitive.
# Adding a primitive to the importer without its event semantics must fail closed.
ACTION_OUTPUTS = {
    'add_score': ('ScoreChanged',),
    'gain_coins': ('CoinsChanged',),
    'add_pending_score': (),
    'add_pending_coins': (),
    'reduce_shop_cost': (),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    require(path.stat().st_size <= 1024*1024, f'{path}: definition exceeds 1 MiB')
    return json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=importer.unique_object)


def event_names(root):
    text = (root/'include/Common/enums.hpp').read_text(encoding='utf-8-sig')
    match = re.search(r'enum class EventType\s*:[^{]+\{(.*?)\};', text, re.S)
    require(match is not None, 'Cannot read EventType declarations')
    return set(re.findall(r'^\s*(\w+)\s*=\s*\d+', match[1], re.M)) - {'Count'}


def native_sources(root):
    suffixes = {'.cpp', '.hpp', '.cc', '.cxx', '.h', '.hh', '.hxx'}
    result = {'tools/import_function_card.py'}
    for directory in ('include', 'src'):
        for path in (root/directory).rglob('*'):
            relative = path.relative_to(root).as_posix()
            if path.is_file() and path.suffix in suffixes and '/cards/generated/' not in relative:
                result.add(relative)
    return result


def source_hash(path):
    # Git uses LF, while a workstation may have CRLF. Compare logical UTF-8 text.
    return hashlib.sha256(path.read_text(encoding='utf-8-sig').encode('utf-8')).hexdigest()


def check_native_sources(root, declared):
    require(type(declared) is dict, 'native effects: source_sha256 must be an object')
    expected = native_sources(root)
    missing, stale = sorted(expected-declared.keys()), sorted(declared.keys()-expected)
    require(not missing and not stale,
            f'Native effect source coverage changed; review cards/native_effects.json. Missing: {missing}; obsolete: {stale}')
    changed = [name for name in sorted(expected) if declared[name] != source_hash(root/name)]
    require(not changed, 'Native effect summaries need review after source changes:\n  '+
            '\n  '.join(changed)+'\nReview event outputs, update the summaries, then update their source_sha256 entries.')


def cycle_path(edges):
    """Return one complete cycle, including edge provenance, or an empty list."""
    graph = {}
    for edge in edges:
        graph.setdefault(edge['event'], []).append(edge)
    colors, trail, positions = {}, [], {}

    def visit(event):
        colors[event] = 1
        positions[event] = len(trail)
        for edge in graph.get(event, []):
            target = edge['emits']
            if colors.get(target) == 1:
                return trail[positions[target]:] + [edge]
            if colors.get(target, 0) == 0:
                trail.append(edge)
                found = visit(target)
                if found:
                    return found
                trail.pop()
        colors[event] = 2
        del positions[event]
        return []

    for event in sorted(graph):
        if colors.get(event, 0) == 0:
            found = visit(event)
            if found:
                return found
    return []


def reject_cycles(edges):
    cycle = cycle_path(edges)
    if cycle:
        lines = [f'  {e["event"]} -- {e["card"]} / ability {e["ability"]} ({e["source"]}) --> {e["emits"]}' for e in cycle]
        raise ValueError('Potential automatic effect cycle (card design rejected):\n'+'\n'.join(lines)+
                         '\nRemove a feedback edge. Player actions and numeric overflow are not termination guarantees.')


def design_edges(card, source, known, native=False):
    require(type(card) is dict and type(card.get('name')) is str and card['name'].strip(), f'{source}: missing card name')
    abilities = card.get('abilities')
    require(type(abilities) is list and bool(abilities), f'{source}: missing abilities')
    edges = []
    for index, ability in enumerate(abilities, 1):
        at = f'{source}: {card["name"]} ability {index}'
        require(type(ability) is dict, f'{at}: expected an object')
        event = ability.get('event')
        require(type(event) is str and event in known, f'{at}: unknown trigger event {event!r}')
        if native:
            require(set(ability) == {'event', 'emits'}, f'{at}: native abilities require event and emits only')
            outputs = ability['emits']
            require(type(outputs) is list, f'{at}: emits must be an array')
        else:
            op = ability.get('op')
            require(type(op) is str and op in ACTION_OUTPUTS, f'{at}: atomic action needs event semantics: {op!r}')
            outputs = ACTION_OUTPUTS[op]
        for output in outputs:
            require(type(output) is str and output in known, f'{at}: unknown output event {output!r}')
            edges.append(dict(event=event, emits=output, card=card['name'], source=source, ability=index))
    return edges


def check_project(root=ROOT):
    require(set(ACTION_OUTPUTS) == set(importer.ACTIONS), 'Atomic action event semantics are out of sync with the importer')
    known = event_names(root)
    manifest = read_json(root/'cards/native_effects.json')
    require(type(manifest) is dict and set(manifest) == {'version', 'source_sha256', 'cards'}, 'Invalid native effect manifest fields')
    require(type(manifest['version']) is int and manifest['version'] == 1, 'Unsupported native effect manifest version')
    check_native_sources(root, manifest['source_sha256'])
    require(type(manifest['cards']) is list and bool(manifest['cards']), 'Native effect summaries cannot be empty')
    edges, cards = [], []
    for card in manifest['cards']:
        require(type(card) is dict and set(card) == {'name', 'abilities'}, 'Invalid native card summary')
        edges.extend(design_edges(card, 'cards/native_effects.json', known, native=True))
    for path in sorted((root/'cards/function').glob('*.json')):
        card = read_json(path)
        edges.extend(design_edges(card, path.relative_to(root).as_posix(), known))
        cards.append(card)
    # Analyze the whole catalog before per-card allowlists so a feedback design
    # gets a useful cycle path even when its event is also outside the JSON API.
    reject_cycles(edges)
    for card in cards:
        importer.validate(card)
    print(f'Effect cycle check: passed ({len(cards)} JSON definitions, {len(manifest["cards"])} native summaries, {len(edges)} edges)')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT, help='Project checkout to inspect; never modified')
    args = parser.parse_args()
    try:
        check_project(args.root.resolve())
    except (ValueError, OSError, UnicodeError, RecursionError) as error:
        print(f'Effect cycle check FAILED: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
