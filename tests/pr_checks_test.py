"""Verify checks reject stale generated files and missing local links without repairing them."""
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pr_checks',ROOT/'tools/check_pr.py')
checks=importlib.util.module_from_spec(spec);spec.loader.exec_module(checks)


class ChecksTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        for name in ['tools/import_function_card.py','cards/function/steady_income.json',
                     'include/cards/generated/steady_income.hpp','src/cards/generated/steady_income.cpp','src/CardCatalog.cpp']:
            dest=self.root/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/name,dest)
        (self.root/'docs').mkdir()
        (self.root/'README.md').write_text('[design](design.md)\n',encoding='utf-8')
        (self.root/'design.md').write_text('# Design\n',encoding='utf-8')
        self.scope=patch.object(checks,'ROOT',self.root);self.scope.start();self.addCleanup(self.scope.stop)

    def test_clean_then_stale_json_fails_without_repair(self):
        checks.check_generated_cards()
        source=self.root/'cards/function/steady_income.json'
        card=json.loads(source.read_text(encoding='utf-8'));card['abilities'][0]['amount']+=1
        source.write_text(json.dumps(card,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        cpp=self.root/'src/cards/generated/steady_income.cpp';before=cpp.read_bytes()
        with self.assertRaisesRegex(ValueError,'out of sync'):checks.check_generated_cards()
        self.assertEqual(cpp.read_bytes(),before)

    def test_deleted_json_is_not_silently_registered(self):
        (self.root/'cards/function/steady_income.json').unlink()
        with self.assertRaisesRegex(ValueError,'out of sync'):checks.check_generated_cards()

    def test_missing_file_link_fails(self):
        checks.check_documents()
        (self.root/'docs/broken.md').write_text('[missing](missing.md#section)',encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'Broken local documentation links'):checks.check_documents()


if __name__=='__main__':unittest.main()
