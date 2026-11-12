"""Importer contracts: real filesystem writes, validation, rollback and C++ syntax."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('card_importer',ROOT/'tools/import_function_card.py')
importer = importlib.util.module_from_spec(spec);spec.loader.exec_module(importer)
COMPILER = None


class ImporterTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name);self.root = self.base/'project';self.root.mkdir()
        self.card = json.loads((ROOT/'cards/function/steady_income.json').read_text(encoding='utf-8'))
        self.card.update(key='first_card',id=7001,name='测试 "引号" 与 \\ 路径')
        self.source = self.base/'source.json'

    def write(self, card=None):
        self.source.write_text(json.dumps(self.card if card is None else card,ensure_ascii=False),encoding='utf-8')
        return self.source

    def files(self):
        return {str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file()}

    def test_real_import_registration_and_idempotence(self):
        self.assertEqual(len(importer.import_card(self.write(),self.root)),4)
        self.assertTrue((self.root/'include/cards/generated/first_card.hpp').exists())
        self.assertIn('make_first_card',(self.root/'src/CardCatalog.cpp').read_text(encoding='utf-8'))
        self.assertEqual(importer.import_card(self.source,self.root),[])
        self.card.update(key='second_card',id=7002)
        importer.import_card(self.write(),self.root)
        catalog=(self.root/'src/CardCatalog.cpp').read_text(encoding='utf-8')
        self.assertIn('make_first_card',catalog);self.assertIn('make_second_card',catalog)

    def test_check_does_not_write(self):
        self.assertEqual(len(importer.import_card(self.write(),self.root,check=True)),4)
        self.assertEqual(self.files(),{})

    def test_update_requires_force_and_preserves_identity(self):
        importer.import_card(self.write(),self.root);before=self.files()
        self.card['abilities'][0]['amount']=7
        with self.assertRaises(importer.CardError):importer.import_card(self.write(),self.root)
        self.assertEqual(before,self.files())
        importer.import_card(self.source,self.root,force=True)
        self.assertIn('7LL+scoreBonus_',(self.root/'src/cards/generated/first_card.cpp').read_text())
        canonical=self.root/'cards/function/first_card.json'
        changed=json.loads(canonical.read_text(encoding='utf-8'));changed['id']=7999
        canonical.write_text(json.dumps(changed),encoding='utf-8');before=self.files()
        with self.assertRaises(importer.CardError):importer.import_card(canonical,self.root,force=True)
        self.assertEqual(before,self.files())

    def test_invalid_data_is_rejected_before_any_write(self):
        cases=[]
        for field,value in [('id',True),('id',1),('id',1<<60),('price',-1),('key','../escape'),
                            ('key','con'),('version',2),('type','consumable'),('abilities',[])]:
            card=copy.deepcopy(self.card);card[field]=value;cases.append(card)
        for field,value in [('event','ScoreChanged'),('op','eval'),('amount',1.2),('amount',True),
                            ('amount',1<<63),('scope','enemy')]:
            card=copy.deepcopy(self.card);card['abilities'][0][field]=value;cases.append(card)
        card=copy.deepcopy(self.card);card['abilities'][0]['event']='ShopBuyQuote';cases.append(card)
        card=copy.deepcopy(self.card);card['unexpected_cpp']='system("bad")';cases.append(card)
        for card in cases:
            with self.subTest(card=card):
                with self.assertRaises(importer.CardError):importer.import_card(self.write(card),self.root)
                self.assertEqual(self.files(),{})
        self.source.write_text('{"id":7001,"id":7002}',encoding='utf-8')
        with self.assertRaises(importer.CardError):importer.import_card(self.source,self.root)

    def test_duplicate_ids_and_hand_written_files_are_protected(self):
        importer.import_card(self.write(),self.root);before=self.files()
        self.card['key']='second_card'
        with self.assertRaises(importer.CardError):importer.import_card(self.write(),self.root)
        self.assertEqual(before,self.files())
        self.card['id']=7002
        target=self.root/'src/cards/generated/second_card.cpp';target.write_text('// hand written\n')
        before=self.files()
        with self.assertRaises(importer.CardError):importer.import_card(self.write(),self.root,force=True)
        self.assertEqual(before,self.files())

    def test_partial_write_failure_rolls_back_existing_files(self):
        importer.import_card(self.write(),self.root);before=self.files()
        self.card['abilities'][0]['amount']=9;self.write()
        replace=importer.os.replace;calls=0
        def fail_second(source,dest):
            nonlocal calls
            calls+=1
            if calls==2:raise OSError('simulated filesystem failure')
            replace(source,dest)
        with patch.object(importer.os,'replace',side_effect=fail_second):
            with self.assertRaises(OSError):importer.import_card(self.source,self.root,force=True)
        self.assertEqual(before,self.files())

    def test_all_supported_event_action_pairs_compile(self):
        if not COMPILER:self.skipTest('pass --compiler to check generated C++')
        self.card['abilities']=[]
        for event,(_,subject,kind) in importer.EVENTS.items():
            for op,action_kind in importer.ACTIONS.items():
                if action_kind!=kind:continue
                for scope in (['self','any_player'] if subject else ['self']):
                    self.card['abilities'].append({'event':event,'scope':scope,'op':op,'amount':1})
        importer.import_card(self.write(),self.root)
        sources=[str(self.root/'src/cards/generated/first_card.cpp'),str(self.root/'src/CardCatalog.cpp')]
        if Path(COMPILER).stem.lower()=='cl':
            args=[COMPILER,'/nologo','/std:c++17','/EHsc','/utf-8','/Zs',f'/I{ROOT/"include"}',f'/I{self.root/"include"}',*sources]
        else:
            args=[COMPILER,'-std=c++17','-fsyntax-only',f'-I{ROOT/"include"}',f'-I{self.root/"include"}',*sources]
        result=subprocess.run(args,capture_output=True,timeout=45)
        self.assertEqual(result.returncode,0,result.stderr.decode(errors='replace'))


if __name__=='__main__':
    parser=argparse.ArgumentParser(add_help=False);parser.add_argument('--compiler')
    options,remaining=parser.parse_known_args();COMPILER=options.compiler
    unittest.main(argv=[sys.argv[0],*remaining])
