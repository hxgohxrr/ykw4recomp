import ast
import json
from pathlib import Path
import re
import string
import sys
import unittest
from unittest.mock import Mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from desktop_launcher import Launcher


def read_pack(language):
    def unique_object(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('Duplicate translation key: ' + key)
            result[key] = value
        return result
    return json.loads((ROOT / 'src/nx/runtime/langs' / (language + '.json')).read_text(encoding='utf-8'), object_pairs_hook=unique_object)['strings']


def spanish(text):
    return re.search(r'[áéíóúñ¿¡]|\b(?:juego|Juego|No se|Preparar|Selecciona|Idioma|Cerrar|Salir|Instalar|Carpeta|Archivos)\b', text)


class LocalizationTests(unittest.TestCase):
    def setUp(self):
        self.packs = {language: read_pack(language) for language in ('en', 'de')}

    def test_german_covers_every_english_key(self):
        self.assertFalse(self.packs['en'].keys() - self.packs['de'].keys())

    def test_translations_preserve_format_arguments(self):
        for language, pack in self.packs.items():
            for source, target in pack.items():
                with self.subTest(language=language, source=source):
                    self.assertTrue(isinstance(target, str) and target.strip())
                    fields = lambda text: sorted(field for unused, field, unused_spec, unused_conversion in string.Formatter().parse(text) if field is not None)
                    self.assertEqual(fields(source), fields(target))
                    formats = lambda text: re.findall(r'%(?:\d+\$)?[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:ll|l|h|z)?[sdiuoxXfgec]', text)
                    self.assertEqual(formats(source), formats(target))

    def test_windows_visible_spanish_literals_have_mappings(self):
        runtime = ROOT / 'src/nx/runtime'
        for name in ('launcher_win32.rc', 'translation_win32.c', 'launcher_pending.c', 'launcher_setup.c', 'launcher_mods.c', 'launcher_catalog.c', 'launcher_security.c', 'launcher_update.c'):
            source = (runtime / name).read_text(encoding='utf-8')
            for match in re.finditer(r'(?<!\w)(?:L)?"((?:[^"\\]|\\.)*)"', source):
                text = match[1].replace(r'\r', '\r').replace(r'\n', '\n').replace(r'\"', '"').replace('\\\\', '\\')
                if spanish(text) and not text.startswith('%ls'):
                    for language, pack in self.packs.items():
                        with self.subTest(file=name, text=text, language=language):
                            self.assertTrue(text in pack, 'Missing mapping: ' + text)

    def test_linux_visible_spanish_literals_have_mappings(self):
        source = (ROOT / 'scripts/desktop_launcher.py').read_text(encoding='utf-8')
        for node in ast.walk(ast.parse(source)):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and spanish(node.value):
                for language, pack in self.packs.items():
                    with self.subTest(line=node.lineno, language=language, text=node.value):
                        self.assertTrue(node.value in pack, 'Missing mapping: ' + node.value)

    def test_linux_direct_translation_calls_have_mappings(self):
        source = (ROOT / 'scripts/desktop_launcher.py').read_text(encoding='utf-8')
        for node in ast.walk(ast.parse(source)):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute) or node.func.attr not in ('tr', 'label', 'button', 'field'):
                continue
            index = 0 if node.func.attr == 'tr' else 1
            if len(node.args) <= index or not isinstance(node.args[index], ast.Constant) or not isinstance(node.args[index].value, str):
                continue
            text = node.args[index].value
            if text in ('YKW4Recomp', 'GameBanana · YKW4++', 'GameBanana…'):
                continue
            for language, pack in self.packs.items():
                with self.subTest(language=language, line=node.lineno):
                    self.assertTrue(text in pack, 'Missing mapping: ' + text)

    def test_linux_dialogs_use_translation_lookup(self):
        source = (ROOT / 'scripts/desktop_launcher.py').read_text(encoding='utf-8')
        for node in ast.walk(ast.parse(source)):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Attribute):
                continue
            if isinstance(node.func.value, ast.Name) and node.func.value.id == 'messagebox':
                for argument in node.args[:2]:
                    self.assertTrue(isinstance(argument, ast.Call) and isinstance(argument.func, ast.Attribute))
                    self.assertIn(argument.func.attr, ('tr', 'format'))

    def test_linux_loads_german_without_a_window(self):
        launcher = Launcher.__new__(Launcher)
        launcher.tools = ROOT / 'scripts'
        launcher.lang = Mock(get=Mock(return_value='de'))
        launcher.load_dictionary()
        self.assertEqual(launcher.tr('Vinculación local'), 'Lokale Bindung')
        self.assertEqual(launcher.tr('Unknown diagnostic'), 'Unknown diagnostic')
        self.assertEqual(launcher.tr('{count} paquetes DLC seleccionados').format(count=3), 'Ausgewählte DLC-Pakete: 3')

    def test_linux_language_change_updates_dynamic_headings(self):
        launcher = Launcher.__new__(Launcher)
        launcher.tools = ROOT / 'scripts'
        launcher.lang = Mock(get=Mock(return_value='en'))
        launcher.activity_window = Mock()
        launcher.modlist = Mock()
        launcher.translatable = []
        launcher.tabs = []
        launcher.select_page = Mock()
        launcher.refresh = Mock()
        launcher.change_language()
        launcher.modlist.heading.assert_any_call('enabled', text='Enabled')
        launcher.activity_window.title.assert_called_once_with('YKW4Recomp · Activity')


if __name__ == '__main__':
    unittest.main()
