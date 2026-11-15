"""Exercise design-time rejection with real temporary card definitions, never a live room."""
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('effect_cycles', ROOT/'tools/check_effect_cycles.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class EffectCycleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ('src', 'include', 'cards'):
            shutil.copytree(ROOT/directory, self.root/directory)
        (self.root/'tools').mkdir()
        shutil.copyfile(ROOT/'tools/import_function_card.py', self.root/'tools/import_function_card.py')
        self.prototype = json.loads((self.root/'cards/function/steady_income.json').read_text(encoding='utf-8'))

    def add_card(self, key, event, op, identity=7001):
        card = copy.deepcopy(self.prototype)
        card.update(id=identity, key=key, name=key,
                    abilities=[dict(event=event, op=op, amount=1, scope='self')])
        path = self.root/'cards/function'/f'{key}.json'
        path.write_text(json.dumps(card)+'\n', encoding='utf-8')
        return card

    def add_native(self, name, event, *outputs):
        path = self.root/'cards/native_effects.json'
        manifest = json.loads(path.read_text(encoding='utf-8'))
        manifest['cards'].append(dict(name=name, abilities=[dict(event=event, emits=list(outputs))]))
        path.write_text(json.dumps(manifest)+'\n', encoding='utf-8')

    def test_current_catalog_passes(self):
        checker.check_project(self.root)

    def test_single_card_feedback_is_rejected_with_filename(self):
        self.add_card('self_loop', 'CoinsChanged', 'gain_coins')
        with self.assertRaisesRegex(ValueError, r'(?s)cycle.*CoinsChanged.*self_loop.*self_loop.json.*CoinsChanged'):
            checker.check_project(self.root)

    def test_pair_only_becomes_cyclic_when_combined(self):
        first = self.add_card('coins_to_score', 'CoinsChanged', 'add_score')
        second = self.add_card('score_to_coins', 'ScoreChanged', 'gain_coins', 7002)
        known = checker.event_names(self.root)
        for card in (first, second):
            self.assertEqual(checker.cycle_path(checker.design_edges(card, 'test.json', known)), [])
        with self.assertRaises(ValueError) as caught:
            checker.check_project(self.root)
        self.assertIn('coins_to_score.json', str(caught.exception))
        self.assertIn('score_to_coins.json', str(caught.exception))

    def test_json_and_native_effects_are_analyzed_together(self):
        self.add_card('coins_to_score', 'CoinsChanged', 'add_score')
        self.add_native('native_partner', 'ScoreChanged', 'CoinsChanged')
        with self.assertRaisesRegex(ValueError, r'(?s)cycle.*native_partner'):
            checker.check_project(self.root)

    def test_system_resource_request_cannot_hide_a_cycle(self):
        self.add_native('request_coin_bonus', 'CoinsChanged', 'AddCoins')
        with self.assertRaisesRegex(ValueError, r'(?s)cycle.*System resource card'):
            checker.check_project(self.root)

    def test_three_card_cycle(self):
        self.add_native('a', 'ScoreChanged', 'CoinsChanged')
        self.add_native('b', 'CoinsChanged', 'AbilityResolved')
        self.add_native('c', 'AbilityResolved', 'ScoreChanged')
        with self.assertRaisesRegex(ValueError, 'cycle'):
            checker.check_project(self.root)

    def test_repeated_triggers_and_converging_paths_are_not_cycles(self):
        self.add_native('diamond-a', 'ShopEnter', 'CardChanged', 'AbilityResolved')
        self.add_native('diamond-b', 'CardChanged', 'ScoreChanged')
        self.add_native('diamond-c', 'AbilityResolved', 'ScoreChanged')
        self.add_card('another_play_bonus', 'PlayCommitted', 'gain_coins')
        checker.check_project(self.root)

    def test_snapshot_modification_does_not_emit_a_resource_event(self):
        card = dict(name='pending', abilities=[dict(event='ScoreChanged', op='add_pending_score')])
        edges = checker.design_edges(card, 'test', checker.event_names(self.root))
        self.assertEqual(edges, [])
        self.assertEqual(checker.ACTION_OUTPUTS['reduce_shop_cost'], ())

    def test_new_atomic_action_needs_output_semantics(self):
        with patch.dict(checker.importer.ACTIONS, {'draw_cards': 'fact'}):
            with self.assertRaisesRegex(ValueError, 'semantics are out of sync'):
                checker.check_project(self.root)

    def test_changed_cpp_requires_contract_review(self):
        path = self.root/'src/SystemCards.cpp'
        path.write_text(path.read_text(encoding='utf-8')+'\n// changed\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, r'(?s)need review.*src/SystemCards.cpp'):
            checker.check_project(self.root)

    def test_new_native_file_cannot_be_silently_skipped(self):
        (self.root/'src/UnreviewedCard.cpp').write_text('// new card\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'coverage changed'):
            checker.check_project(self.root)

    def test_source_hash_ignores_checkout_line_endings(self):
        path = self.root/'src/SystemCards.cpp'
        text = path.read_text(encoding='utf-8')
        path.write_bytes(text.replace('\n', '\r\n').encode('utf-8'))
        checker.check_project(self.root)

    def test_unknown_emissions_fail_instead_of_being_dropped(self):
        self.add_native('unsupported', 'ScoreChanged', 'MisspelledEvent')
        with self.assertRaisesRegex(ValueError, 'unknown output event'):
            checker.check_project(self.root)

    def test_cli_returns_error_and_leaves_bad_design_unchanged(self):
        self.add_card('bad_cli_card', 'ScoreChanged', 'add_score')
        before = {p.relative_to(self.root): p.read_bytes() for p in (self.root/'cards').rglob('*.json')}
        result = subprocess.run([sys.executable, ROOT/'tools/check_effect_cycles.py', '--root', self.root],
                                capture_output=True, text=True, encoding='utf-8', timeout=5,
                                env={**os.environ, 'PYTHONIOENCODING': 'utf-8'})
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('bad_cli_card.json', result.stderr)
        self.assertIn('ability 1', result.stderr)
        self.assertEqual(before, {p.relative_to(self.root): p.read_bytes() for p in (self.root/'cards').rglob('*.json')})


if __name__ == '__main__':
    unittest.main()
