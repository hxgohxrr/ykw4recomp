import os
from pathlib import Path
import sys
import tempfile
import tkinter as tk
from tkinter import ttk
import unittest

tools = Path(os.environ.get('YKW4_DESKTOP_TOOLS', str(Path(__file__).resolve().parents[1] / 'scripts')))
sys.path.insert(0, str(tools))
from desktop_launcher import COLORS, Launcher
from launcher_core import update_ini


@unittest.skipUnless(sys.platform.startswith('linux') and os.environ.get('DISPLAY'), 'Requires Linux with an X display (xvfb-run is supported)')
class DesktopInterfaceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.root = tk.Tk()
        self.launcher = Launcher(self.root, os.environ.get('YKW4_RUNTIME', str(self.base / 'YKW4Recomp')), self.base)
        self.root.update()

    def tearDown(self):
        self.launcher.destroy()
        self.temporary.cleanup()

    def test_sidebar_selects_every_page(self):
        self.assertEqual(len(self.launcher.nav_buttons), 5)
        for index, button in enumerate(self.launcher.nav_buttons):
            button.invoke()
            self.root.update()
            self.assertEqual(self.launcher.notebook.select(), str(self.launcher.tabs[index][0]))
            self.assertEqual(self.launcher.page_title.get(), self.launcher.tr(self.launcher.tabs[index][1]))
            self.assertEqual(button.cget('style'), 'Selected.Nav.TButton')
            self.assertEqual(sum(nav.cget('style') == 'Selected.Nav.TButton' for nav in self.launcher.nav_buttons), 1)

    def test_flat_theme_and_missing_game_state(self):
        self.assertEqual(self.launcher.style.lookup('Shell.TFrame', 'background'), COLORS['canvas'])
        self.assertEqual(self.launcher.style.layout('Launcher.TNotebook.Tab'), [('null', {'sticky': 'nswe'})])
        self.assertTrue(self.launcher.play_button.instate(['disabled']))
        self.assertEqual(self.launcher.state_badge.cget('text'), 'Sin preparar')
        self.assertEqual(self.launcher.style.lookup('Play.TButton', 'background', ['active']), COLORS['blue_hover'])

    def test_activity_does_not_displace_primary_actions(self):
        self.root.geometry('980x780')
        self.root.update()
        before = self.launcher.play_button.winfo_rooty()
        self.launcher.toggle_activity()
        self.root.update()
        self.assertEqual(self.launcher.activity_window.state(), 'normal')
        self.assertEqual(self.launcher.play_button.winfo_rooty(), before)
        self.assertTrue(self.launcher.play_button.winfo_ismapped())
        self.launcher.toggle_activity()
        self.assertEqual(self.launcher.activity_window.state(), 'withdrawn')

    def test_every_page_fits_at_minimum_and_default_sizes(self):
        def children(widget):
            for child in widget.winfo_children():
                if child.winfo_ismapped():
                    yield child
                    yield from children(child)
        for size in ('980x780', '1060x820'):
            self.root.geometry(size)
            for button in self.launcher.nav_buttons:
                button.invoke()
                self.root.update()
                self.assertTrue(self.launcher.play_button.winfo_ismapped(), size)
                self.assertTrue(self.launcher.apply_button.winfo_ismapped(), size)
                self.assertGreaterEqual(self.launcher.play_button.winfo_height(), self.launcher.play_button.winfo_reqheight(), size)
                self.assertGreaterEqual(self.launcher.apply_button.winfo_height(), self.launcher.apply_button.winfo_reqheight(), size)
                selected = self.root.nametowidget(self.launcher.notebook.select())
                for child in selected.winfo_children():
                    self.assertTrue(child.winfo_ismapped(), f'{size}: hidden content {child}')
                for widget in children(self.root):
                    parent = widget.nametowidget(widget.winfo_parent())
                    self.assertLessEqual(widget.winfo_rooty() + widget.winfo_height(), parent.winfo_rooty() + parent.winfo_height() + 1, f'{size}: clipped by parent {widget}')
                    self.assertGreaterEqual(widget.winfo_rootx(), self.root.winfo_rootx() - 1, str(widget))
                    self.assertGreaterEqual(widget.winfo_rooty(), self.root.winfo_rooty() - 1, str(widget))
                    self.assertLessEqual(widget.winfo_rootx() + widget.winfo_width(), self.root.winfo_rootx() + self.root.winfo_width() + 1, f'{size}: {widget}')
                    self.assertLessEqual(widget.winfo_rooty() + widget.winfo_height(), self.root.winfo_rooty() + self.root.winfo_height() + 1, f'{size}: {widget}')

    def test_language_change_keeps_title_and_navigation_in_sync(self):
        self.launcher.nav_buttons[4].invoke()
        self.launcher.lang.set('en')
        self.launcher.change_language()
        self.root.update()
        self.assertEqual(self.launcher.page_title.get(), self.launcher.tr('Equipo'))
        self.assertEqual(self.launcher.nav_buttons[4].cget('text'), self.launcher.tr('Equipo'))

    def test_installation_switch_reloads_settings_without_stale_widgets(self):
        target = self.base / 'second'
        update_ini(target / 'YKW4.ini', {'Performance': {'fps_limit': '30'}})
        self.launcher.events.put(('installed', str(target)))
        self.launcher.pump()
        self.root.update()
        self.assertEqual(self.launcher.base, target)
        self.assertEqual(self.launcher.values['fps'].get(), '30')
        self.assertTrue(all(widget.winfo_exists() for widget, text in self.launcher.translatable))

    def test_preparation_dialog_inherits_the_theme(self):
        self.launcher.setup_dialog()
        self.root.update()
        dialogs = [widget for widget in self.root.winfo_children() if isinstance(widget, tk.Toplevel) and widget is not self.launcher.activity_window]
        self.assertEqual(len(dialogs), 1)
        self.assertEqual(ttk.Style(dialogs[0]).lookup('TEntry', 'fieldbackground'), COLORS['surface'])
        dialogs[0].destroy()


if __name__ == '__main__':
    unittest.main()
