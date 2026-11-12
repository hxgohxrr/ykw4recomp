import argparse
import json
import math
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import font as tkfont
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText
import webbrowser

from launcher_core import LauncherEvents, LinuxGamepad, Preparation, gamebanana_json, graphics_info, import_mod, install_gamebanana, installation_directory, remember_installation, read_ini, save_mods, scan_mods, update_ini, validate_settings
from discord_linux import DiscordPresence


COLORS = {
    'canvas': '#f2f5fb', 'surface': '#ffffff', 'ink': '#172541',
    'muted': '#52627c', 'border': '#c7d2e3', 'blue': '#2454cd',
    'blue_hover': '#1943ae', 'navy': '#142442', 'nav_hover': '#233a60',
    'yellow': '#ffda45', 'yellow_hover': '#f0c72c', 'disabled': '#e2e8f2',
}


def configure_theme(root):
    palette = COLORS
    families = tkfont.families(root)
    family = next((name for name in ('Noto Sans', 'DejaVu Sans') if name in families), tkfont.nametofont('TkDefaultFont').actual('family'))
    for name in ('TkDefaultFont', 'TkTextFont', 'TkMenuFont', 'TkHeadingFont'):
        tkfont.nametofont(name).configure(family=family, size=10)
    root.configure(background=palette['canvas'])
    root.option_add('*Listbox.background', palette['surface'])
    root.option_add('*Listbox.foreground', palette['ink'])
    root.option_add('*Listbox.selectBackground', palette['blue'])
    root.option_add('*Listbox.selectForeground', palette['surface'])
    root.option_add('*TCombobox*Listbox.font', (family, 10))
    style = ttk.Style(root)
    style.theme_use('clam')
    style.configure('.', font=(family, 10), background=palette['surface'], foreground=palette['ink'])
    style.configure('TFrame', background=palette['surface'])
    style.configure('Shell.TFrame', background=palette['canvas'])
    style.configure('Sidebar.TFrame', background=palette['navy'])
    style.configure('TLabel', background=palette['surface'], foreground=palette['ink'])
    style.configure('Sidebar.TLabel', background=palette['navy'], foreground='#cbd8ef')
    style.configure('Header.TLabel', background=palette['canvas'], foreground=palette['ink'], font=(family, 23, 'bold'))
    style.configure('Hint.TLabel', background=palette['canvas'], foreground=palette['muted'])
    style.configure('Status.TLabel', background=palette['canvas'], foreground=palette['muted'])
    style.configure('Badge.TLabel', background=palette['yellow'], foreground=palette['ink'], padding=(12, 7), font=(family, 9, 'bold'))
    style.configure('TButton', background='#eaf0fc', foreground=palette['blue'], borderwidth=0, padding=(12, 9), focusthickness=2, focuscolor=palette['blue'], anchor='center')
    style.map('TButton', background=[('disabled', palette['disabled']), ('pressed', '#cedbf6'), ('active', '#dce6fb')], foreground=[('disabled', '#65738b')])
    style.configure('Play.TButton', background=palette['blue'], foreground=palette['surface'], font=(family, 10, 'bold'), padding=(24, 10))
    style.map('Play.TButton', background=[('disabled', palette['disabled']), ('pressed', palette['blue_hover']), ('active', palette['blue_hover'])], foreground=[('disabled', '#65738b'), ('!disabled', palette['surface'])])
    style.configure('Nav.TButton', background=palette['navy'], foreground='#dce6f8', anchor='w', padding=(16, 13), focuscolor=palette['yellow'])
    style.map('Nav.TButton', background=[('active', palette['nav_hover'])], foreground=[('active', palette['surface'])])
    style.configure('Selected.Nav.TButton', background=palette['yellow'], foreground=palette['ink'], font=(family, 10, 'bold'))
    style.map('Selected.Nav.TButton', background=[('pressed', palette['yellow_hover']), ('active', palette['yellow_hover'])], foreground=[('active', palette['ink'])])
    style.configure('TEntry', fieldbackground=palette['surface'], foreground=palette['ink'], bordercolor=palette['border'], lightcolor=palette['border'], darkcolor=palette['border'], padding=8, insertcolor=palette['ink'])
    style.map('TEntry', bordercolor=[('focus', palette['blue'])])
    style.configure('TCombobox', fieldbackground=palette['surface'], background=palette['surface'], foreground=palette['ink'], arrowcolor=palette['blue'], bordercolor=palette['border'], lightcolor=palette['border'], darkcolor=palette['border'], padding=7, arrowsize=14)
    style.map('TCombobox', fieldbackground=[('readonly', palette['surface'])], foreground=[('readonly', palette['ink'])], bordercolor=[('focus', palette['blue'])])
    style.configure('TCheckbutton', background=palette['surface'], foreground=palette['ink'], padding=(0, 5), indicatorbackground=palette['surface'], indicatorforeground=palette['blue'], indicatorsize=16)
    style.map('TCheckbutton', background=[('active', palette['surface'])], indicatorbackground=[('selected', palette['blue']), ('active', '#eaf0fc')], indicatorforeground=[('selected', palette['surface'])])
    style.configure('Launcher.TNotebook', background=palette['surface'], bordercolor=palette['surface'], lightcolor=palette['surface'], darkcolor=palette['surface'], borderwidth=0, tabmargins=0)
    style.layout('Launcher.TNotebook.Tab', [])
    style.configure('Treeview', background=palette['surface'], fieldbackground=palette['surface'], foreground=palette['ink'], rowheight=36, borderwidth=0)
    style.map('Treeview', background=[('selected', '#dce6fb')], foreground=[('selected', palette['ink'])])
    style.configure('Treeview.Heading', background='#edf2fb', foreground=palette['ink'], borderwidth=0, padding=(10, 10), font=(family, 10, 'bold'))
    style.map('Treeview.Heading', background=[('active', '#e1e9f8')])
    style.configure('Vertical.TScrollbar', background='#c7d2e3', troughcolor=palette['canvas'], borderwidth=0, arrowsize=12)
    return style


class Launcher:
    def __init__(self, root, runtime, base=None):
        self.root, self.runtime = root, Path(runtime).resolve()
        self.style = configure_theme(root)
        self.base = installation_directory(self.runtime, base)
        self.explicit_base = base is not None
        self.tools = Path(__file__).resolve().parent
        self.events = LauncherEvents()
        self.busy = False
        self.preparation = None
        self.gamepad = LinuxGamepad()
        self.closing = False
        self.destroyed = False
        self.translatable = []
        self.settings = read_ini(self.base / 'YKW4.ini')
        self.mods = scan_mods(self.base)
        self.lang = tk.StringVar(value=self.get('Launcher', 'ui_language', 'es'))
        self.dictionary = {}
        self.load_dictionary()
        root.title('YKW4Recomp')
        root.geometry('1060x820')
        root.minsize(980, 780)
        sidebar = ttk.Frame(root, style='Sidebar.TFrame', padding=(18, 24))
        sidebar.pack(side='left', fill='y')
        sidebar.configure(width=214)
        sidebar.pack_propagate(False)
        outer = ttk.Frame(root, style='Shell.TFrame', padding=(28, 26, 28, 20))
        outer.pack(side='left', fill='both', expand=True)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)
        logo = self.tools / 'launcher-logo.png'
        if not logo.is_file():
            logo = self.tools.parent / 'dist/icons/launcher-logo.png'
        if logo.is_file():
            self.logo = tk.PhotoImage(file=str(logo))
            self.logo = self.logo.subsample(max(1, math.ceil(self.logo.width() / 178)))
            ttk.Label(sidebar, image=self.logo, style='Sidebar.TLabel').pack(fill='x', pady=(0, 12))
            root.iconphoto(True, self.logo)
        ttk.Label(sidebar, text=self.tr('YKW4Recomp'), style='Sidebar.TLabel', font=('TkDefaultFont', 15, 'bold')).pack(anchor='w')
        ttk.Label(sidebar, text=self.tr('Linux · Desktop'), style='Sidebar.TLabel').pack(anchor='w', pady=(5, 28))
        header = ttk.Frame(outer, style='Shell.TFrame')
        header.grid(row=0, column=0, sticky='ew', pady=(0, 22))
        self.page_title = tk.StringVar()
        ttk.Label(header, textvariable=self.page_title, style='Header.TLabel').pack(side='left')
        self.state_badge = ttk.Label(header, style='Badge.TLabel')
        self.state_badge.pack(side='right', padx=(12, 0))
        self.label(outer, 'Launcher para Linux · instalación local', style='Hint.TLabel').grid(row=1, column=0, sticky='w', pady=(0, 16))
        self.notebook = ttk.Notebook(outer, style='Launcher.TNotebook')
        self.notebook.grid(row=2, column=0, sticky='nsew')
        self.tabs = []
        self.nav_buttons = []
        for title in ('Juego y ajustes', 'Mods', 'Discord', 'Compilar moviles', 'Equipo'):
            tab = ttk.Frame(self.notebook, padding=24)
            self.notebook.add(tab, text=self.tr(title))
            self.tabs.append((tab, title))
            nav = self.button(sidebar, title, lambda tab=tab: self.notebook.select(tab))
            nav.configure(style='Nav.TButton')
            nav.pack(fill='x', pady=4)
            self.nav_buttons.append(nav)
        self.notebook.bind('<<NotebookTabChanged>>', self.select_page)
        self.select_page()
        self.button(sidebar, 'Salir', self.close).pack(side='bottom', fill='x')
        self.values = {}
        self.game_tab(self.tabs[0][0])
        self.mods_tab(self.tabs[1][0])
        self.discord_tab(self.tabs[2][0])
        self.mobile_tab(self.tabs[3][0])
        self.binding_tab(self.tabs[4][0])
        self.status = tk.StringVar()
        self.status_label = ttk.Label(outer, textvariable=self.status, style='Status.TLabel', wraplength=700)
        self.status_label.grid(row=3, column=0, sticky='ew', pady=(16, 12))
        self.status_label.bind('<Configure>', lambda event: self.status_label.configure(wraplength=max(240, event.width)))
        self.activity_window = tk.Toplevel(root)
        self.activity_window.title(self.tr('YKW4Recomp · Actividad'))
        self.activity_window.geometry('760x320')
        self.activity_window.withdraw()
        self.activity_window.protocol('WM_DELETE_WINDOW', self.activity_window.withdraw)
        self.log_panel = ttk.Frame(self.activity_window, padding=16, style='Shell.TFrame')
        self.log_panel.pack(fill='both', expand=True)
        self.log = ScrolledText(self.log_panel, height=5, state='disabled', font=('TkFixedFont', 9), background=COLORS['surface'], foreground=COLORS['ink'], selectbackground=COLORS['blue'], selectforeground=COLORS['surface'], relief='flat', borderwidth=0, highlightthickness=1, highlightbackground=COLORS['border'], padx=12, pady=10)
        self.log.pack(fill='both', expand=True)
        footer = ttk.Frame(outer, style='Shell.TFrame')
        footer.grid(row=4, column=0, sticky='ew')
        self.label(footer, 'Idioma:', style='Hint.TLabel').pack(side='left')
        language = ttk.Combobox(footer, values=('es', 'en', 'ja', 'de'), textvariable=self.lang, state='readonly', width=5)
        language.pack(side='left', padx=8)
        language.bind('<<ComboboxSelected>>', self.change_language)
        self.button(footer, 'Actividad', self.toggle_activity).pack(side='left', padx=(8, 0))
        self.apply_button = self.button(footer, 'Aplicar', self.save)
        self.play_button = self.button(footer, 'Jugar', self.play)
        self.play_button.configure(style='Play.TButton')
        self.play_button.pack(side='right')
        self.apply_button.pack(side='right', padx=8)
        root.bind('<Escape>', lambda event: self.close())
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.refresh()
        root.after(80, self.pump)
        self.saved_settings = self.settings_snapshot()

    def select_page(self, event=None):
        selected = self.notebook.select()
        for index, (tab, title) in enumerate(self.tabs):
            active = str(tab) == selected
            self.nav_buttons[index].configure(style='Selected.Nav.TButton' if active else 'Nav.TButton')
            if active:
                self.page_title.set(self.tr(title))

    def toggle_activity(self):
        if self.activity_window.state() == 'normal':
            self.activity_window.withdraw()
        else:
            self.activity_window.deiconify()
            self.activity_window.lift()

    def get(self, section, key, fallback):
        return self.settings.get(section, key, fallback=fallback)

    def tr(self, text):
        return self.dictionary.get(text, text)

    def load_dictionary(self):
        path = self.tools / 'langs' / (self.lang.get() + '.json')
        if not path.is_file():
            path = self.tools.parent / 'src/nx/runtime/langs' / (self.lang.get() + '.json')
        self.dictionary = json.loads(path.read_text(encoding='utf-8')).get('strings', {}) if path.is_file() else {}

    def label(self, parent, text, **options):
        widget = ttk.Label(parent, text=self.tr(text), **options)
        self.translatable.append((widget, text))
        return widget

    def button(self, parent, text, command):
        widget = ttk.Button(parent, text=self.tr(text), command=command)
        self.translatable.append((widget, text))
        return widget

    def change_language(self, event=None):
        self.load_dictionary()
        self.activity_window.title(self.tr('YKW4Recomp · Actividad'))
        for column, text in (('#0', 'Nombre'), ('enabled', 'Activo'), ('kind', 'Tipo')):
            self.modlist.heading(column, text=self.tr(text))
        for widget, text in self.translatable:
            if widget.winfo_exists():
                widget.configure(text=self.tr(text))
        for tab, title in self.tabs:
            self.notebook.tab(tab, text=self.tr(title))
        self.select_page()
        self.refresh()

    def field(self, parent, label, name, default, row, choices=None, boolean=False, target=None):
        target = self.values if target is None else target
        variable = tk.BooleanVar(value=bool(int(default))) if boolean else tk.StringVar(value=default)
        target[name] = variable
        if boolean:
            control = ttk.Checkbutton(parent, text=self.tr(label), variable=variable)
            self.translatable.append((control, label))
            control.grid(row=row, column=0, columnspan=2, sticky='w', pady=5)
        else:
            self.label(parent, label).grid(row=row, column=0, sticky='w', padx=(0, 16), pady=5)
            control = ttk.Combobox(parent, values=choices, textvariable=variable, state='readonly' if name != 'fps' else 'normal') if choices else ttk.Entry(parent, textvariable=variable)
            control.grid(row=row, column=1, sticky='ew', pady=5)
        parent.columnconfigure(1, weight=1)
        return variable

    def game_tab(self, tab):
        self.field(tab, 'Traducción:', 'translation', self.get('Launcher', 'translation', 'original'), 0, ('original', 'es'))
        self.field(tab, 'Límite de FPS:', 'fps', self.get('Performance', 'fps_limit', 'game'), 1, ('game', '0', '30', '60', '120', '144'))
        self.field(tab, 'Resolución:', 'resolution', self.get('Graphics', 'resolution', '1'), 2, ('0.5', '0.75', '1', '1.5', '2'))
        for row, (name, text, section, key, default) in enumerate((('fullscreen', 'Pantalla completa', 'Display', 'fullscreen', '0'), ('vsync', 'Sincronización vertical', 'Graphics', 'vsync', '1'), ('shadows', 'Sombras', 'Graphics', 'shadows', '1'), ('swap', 'Intercambiar botones A y B', 'Controllers', 'gamepad_swap_ab', '0')), 3):
            self.field(tab, text, name, self.get(section, key, default), row, boolean=True)
        self.field(tab, 'Zona muerta (%):', 'deadzone', self.get('Controllers', 'gamepad_deadzone', '15'), 7)
        self.field(tab, 'Vibración (%):', 'rumble', self.get('Controllers', 'gamepad_rumble', '100'), 8)
        self.button(tab, 'Preparar / actualizar juego…', self.setup_dialog).grid(row=9, column=0, columnspan=2, sticky='ew', pady=(20, 8))
        self.label(tab, 'Se usan tus archivos locales. No se descarga ni se incluye el juego.', wraplength=650).grid(row=10, column=0, columnspan=2, sticky='w')

    def mods_tab(self, tab):
        self.modlist = ttk.Treeview(tab, columns=('enabled', 'kind'), show='tree headings', height=9, selectmode='browse')
        self.modlist.heading('#0', text=self.tr('Nombre'))
        self.modlist.heading('enabled', text=self.tr('Activo'))
        self.modlist.heading('kind', text=self.tr('Tipo'))
        self.modlist.column('#0', width=330)
        self.modlist.column('enabled', width=70, stretch=False)
        self.modlist.column('kind', width=100, stretch=False)
        self.modlist.pack(fill='both', expand=True)
        self.modlist.bind('<space>', lambda event: self.toggle_mod())
        self.modlist.bind('<Double-1>', lambda event: self.toggle_mod())
        buttons = ttk.Frame(tab)
        buttons.pack(fill='x', pady=10)
        for text, action in (('Añadir mod…', lambda: self.add_mod(False)), ('Añadir traducción…', lambda: self.add_mod(True)), ('Activar / desactivar', self.toggle_mod), ('Subir', lambda: self.move_mod(-1)), ('Bajar', lambda: self.move_mod(1))):
            self.button(buttons, text, action).pack(side='left', padx=(0, 5))
        self.button(tab, 'GameBanana…', self.catalog_dialog).pack(anchor='w')
        self.label(tab, 'Arriba tiene prioridad. Admite carpetas con romfs y archivos ZIP; extrae los RAR antes de importarlos.', wraplength=650).pack(anchor='w', pady=12)

    def redraw_mods(self, selected=None):
        self.modlist.delete(*self.modlist.get_children())
        for index, mod in enumerate(self.mods):
            self.modlist.insert('', 'end', iid=str(index), text=mod.name, values=('✓' if mod.enabled else '—', self.tr('Traducción' if mod.translation else 'Mod')))
        if selected is not None and 0 <= selected < len(self.mods):
            self.modlist.selection_set(str(selected))

    def catalog_dialog(self):
        if self.busy:
            return
        dialog = tk.Toplevel(self.root)
        dialog.title(self.tr('GameBanana · YKW4++'))
        dialog.geometry('700x540')
        dialog.transient(self.root)
        frame = ttk.Frame(dialog, padding=16)
        frame.pack(fill='both', expand=True)
        ttk.Label(frame, text=self.tr('Mods de YKW4++ · consulta las instrucciones y los derechos del autor.')).pack(anchor='w', pady=(0, 10))
        records, files, files_owner = [], [], [None]
        listing = ttk.Treeview(frame, columns=('author',), show='tree headings', selectmode='browse', height=8)
        listing.heading('#0', text=self.tr('Mod'))
        listing.heading('author', text=self.tr('Autor'))
        listing.pack(fill='both', expand=True)
        filelist = tk.Listbox(frame, height=5, exportselection=False)
        filelist.pack(fill='x', pady=10)
        def show_records(result):
            if not dialog.winfo_exists():
                return
            records[:] = result
            listing.delete(*listing.get_children())
            for index, entry in enumerate(records):
                listing.insert('', 'end', iid=str(index), text=entry.get('_sName', 'Mod'), values=(entry.get('_aSubmitter', {}).get('_sName', ''),))
        def refresh():
            def fetch():
                result = gamebanana_json()
                self.events.put(('callback', lambda: show_records(result)))
            self.start(fetch)
        def selected():
            return records[int(listing.selection()[0])] if listing.selection() else None
        def show_files(result, owner):
            if not dialog.winfo_exists():
                return
            files[:] = result
            files_owner[0] = owner
            filelist.delete(0, 'end')
            for entry in files:
                filelist.insert('end', entry.get('_sFile', 'ZIP'))
        def fetch_files():
            entry = selected()
            if entry:
                def fetch():
                    result = gamebanana_json(entry['_idRow'])
                    self.events.put(('callback', lambda: show_files(result, entry['_idRow'])))
                self.start(fetch)
        def install():
            selected_file, mod = filelist.curselection(), selected()
            if not selected_file or not mod or self.busy or files_owner[0] != mod['_idRow']:
                return
            if mod.get('_aRootCategory', {}).get('_sName') == 'Code Patches':
                messagebox.showinfo(self.tr('Parche de código'), self.tr('Solo se importan cambios de RomFS. Consulta al autor sobre parches de código.'), parent=dialog)
                return
            if not messagebox.askyesno(self.tr('Instalar mod'), self.tr('¿Descargar e importar el ZIP seleccionado? Se comprobarán rutas y enlaces. El mod se añadirá desactivado.'), parent=dialog, default='no'):
                return
            entry = files[selected_file[0]]
            translation = mod.get('_aRootCategory', {}).get('_sName') == 'Translation'
            def download():
                install_gamebanana(self.base, entry, translation)
                self.events.put(('rescan', None))
            self.start(download)
        buttons = ttk.Frame(frame)
        buttons.pack(fill='x')
        ttk.Button(buttons, text=self.tr('Actualizar'), command=refresh).pack(side='left')
        ttk.Button(buttons, text=self.tr('Archivos ZIP'), command=fetch_files).pack(side='left', padx=8)
        ttk.Button(buttons, text=self.tr('Ver página'), command=lambda: webbrowser.open('https://gamebanana.com/mods/' + str(selected()['_idRow'])) if selected() else None).pack(side='left')
        ttk.Button(buttons, text=self.tr('Instalar ZIP'), command=install).pack(side='right')
        refresh()

    def toggle_mod(self):
        selected = self.modlist.selection()
        if selected and not self.busy:
            index = int(selected[0])
            self.mods[index].enabled = not self.mods[index].enabled
            self.redraw_mods(index)

    def move_mod(self, direction):
        selected = self.modlist.selection()
        if selected and not self.busy:
            index = int(selected[0])
            other = index + direction
            if 0 <= other < len(self.mods):
                self.mods[index], self.mods[other] = self.mods[other], self.mods[index]
                self.redraw_mods(other)

    def add_mod(self, translation):
        if self.busy:
            return
        archive = messagebox.askyesnocancel(self.tr('Importar'), self.tr('¿Importar desde ZIP?\nSí: ZIP. No: carpeta con romfs.'), parent=self.root)
        if archive is None:
            return
        source = filedialog.askopenfilename(parent=self.root, filetypes=[('ZIP', '*.zip')]) if archive else filedialog.askdirectory(parent=self.root)
        if source:
            def import_selected():
                import_mod(self.base, source, translation)
                self.events.put(('rescan', None))
            self.start(import_selected)

    def discord_tab(self, tab):
        parser = read_ini(self.base / 'YKW4.discord.ini')
        settings = dict(parser['Discord']) if parser.has_section('Discord') else {}
        self.rpc = {}
        for row, (name, text, default) in enumerate((('enabled', 'Mostrar actividad en Discord', '1'), ('show_time', 'Mostrar tiempo de esta sesión', '1'), ('application_id', 'ID de aplicación:', '1561797893252317345'), ('details', 'Texto principal:', 'YKW4Recomp'), ('context', 'Contexto o zona:', ''), ('normal_image', 'Imagen sin DLC:', ''), ('dlc_image', 'Imagen con DLC:', ''))):
            self.field(tab, text, name, settings.get(name, default), row, boolean=row < 2, target=self.rpc)
        self.label(tab, 'Discord debe estar abierto en la misma sesión Linux. El contexto es editable; no se detecta la zona automáticamente.', wraplength=650).grid(row=8, column=0, columnspan=2, sticky='w', pady=18)

    def mobile_tab(self, tab):
        self.label(tab, 'Compila APK experimentales desde Linux. Los IPA requieren macOS y Xcode. Se comprueban las herramientas y se exige aceptar las condiciones por compilación. No incluye archivos del juego.', wraplength=650).pack(anchor='w', pady=(0, 20))
        self.button(tab, 'Abrir compilador móvil…', lambda: subprocess.Popen([sys.executable, str(self.tools / 'mobile_builder.py'), '--gui'])).pack(anchor='w')

    def ready(self):
        return (self.base / 'YKW4_code.so').is_file() and (self.base / 'YKW4_data/exefs/main').is_file() and ((self.base / 'YKW4_data/romfs.bin').is_file() or (self.base / 'YKW4_data/translations/es/romfs.bin').is_file())

    def binding_tab(self, tab):
        self.label(tab, 'Vinculación local a este usuario e instalación Linux. No se envían identificadores. Cambiar de instalación, usuario o perder la clave privada puede exigir preparar de nuevo desde tus propios archivos. No modifica partidas ni convierte módulos de Windows.', wraplength=650).pack(anchor='w', pady=(0, 20))
        self.button(tab, 'Comprobar vinculación', lambda: self.binding_action(False)).pack(anchor='w', pady=6)
        self.button(tab, 'Vincular instalación actual...', lambda: self.binding_action(True)).pack(anchor='w', pady=6)

    def binding_action(self, seal):
        if self.busy or not self.ready():
            return
        if seal and not messagebox.askyesno(self.tr('Vinculación local'), self.tr('El módulo dejará de ser portable. Necesitarás esta instalación Linux, este usuario y su clave privada. No es una prueba de propiedad ni una protección invulnerable. ¿Vincular ahora?'), parent=self.root, default='no'):
            return
        def action():
            command = [str(self.runtime), '--binding-seal' if seal else '--binding-check', str(self.base / 'YKW4_code.so')]
            if seal:
                command.append('--accept-local-binding')
            result = subprocess.run(command, capture_output=True, text=True)
            if result.returncode:
                raise RuntimeError(result.stderr.strip() or 'No se pudo comprobar la vinculación.')
            self.events.put(('log', 'Vinculación comprobada.' if seal else 'Módulo compatible con esta instalación; los módulos antiguos pueden no estar vinculados.'))
        self.start(action)

    def refresh(self):
        self.redraw_mods()
        self.play_button.configure(state='normal' if self.ready() and not self.busy else 'disabled')
        self.apply_button.configure(state='disabled' if self.busy else 'normal')
        self.state_badge.configure(text=self.tr('En curso' if self.busy else 'Listo para jugar' if self.ready() else 'Sin preparar'))
        self.status.set(self.tr('Instalación: {path} · {state}').format(path=self.base, state=self.tr('Lista para iniciar.' if self.ready() else 'Prepara el juego para empezar.')))

    def settings_snapshot(self):
        return ({name: variable.get() for name, variable in self.values.items()},
                {name: variable.get() for name, variable in self.rpc.items()},
                self.lang.get(), tuple((mod.identifier, mod.enabled) for mod in self.mods))

    def save(self):
        if self.busy:
            return False
        try:
            values = {name: variable.get() for name, variable in self.values.items()}
            sections = validate_settings(values)
            sections['Launcher']['ui_language'] = self.lang.get()
            rpc = {name: int(variable.get()) if isinstance(variable, tk.BooleanVar) else variable.get() for name, variable in self.rpc.items()}
            app_id = str(rpc['application_id'])
            if not app_id.isdecimal() or not 0 < int(app_id) < 2**64 or len(app_id) > 20 or any(len(str(value)) > 128 for value in rpc.values()):
                raise ValueError('Indica un ID de Discord válido y textos de hasta 128 caracteres.')
            if values['translation'] == 'es' and not (self.base / 'YKW4_data/translations/es/romfs.bin').is_file() and not any(mod.translation and mod.enabled for mod in self.mods):
                raise ValueError('Importa y activa una traducción antes de seleccionar español.')
            save_mods(self.base, self.mods, sections=sections)
            update_ini(self.base / 'YKW4.discord.ini', {'Discord': rpc})
            self.saved_settings = self.settings_snapshot()
            self.status.set(self.tr('Ajustes guardados. Se aplicarán al iniciar el juego.'))
            return True
        except (OSError, ValueError) as error:
            messagebox.showerror(self.tr('No se pudo guardar'), self.tr(str(error)), parent=self.root)
            return False

    def play(self):
        if self.busy or not self.ready():
            return
        if self.settings_snapshot() != self.saved_settings:
            choice = messagebox.askyesnocancel(self.tr('Cambios sin aplicar'), self.tr('Hay cambios sin aplicar. ¿Quieres aplicarlos antes de jugar?\n\nSí: aplicar los cambios y jugar.\nNo: jugar con los ajustes guardados.\nCancelar: volver al launcher.'), parent=self.root, default='cancel')
            if choice is None or (choice and not self.save()):
                return
        env = dict(os.environ)
        saved = read_ini(self.base / 'YKW4.ini')
        translation = saved.get('Launcher', 'translation', fallback='original')
        mods = scan_mods(self.base)
        env['YKW4_LANGUAGE'] = 'es' if translation == 'es' and not any(mod.translation and mod.enabled for mod in mods) else 'original'
        settings = read_ini(self.base / 'YKW4.discord.ini')
        rpc = dict(settings['Discord']) if settings.has_section('Discord') else {}
        def run_game():
            information = graphics_info()
            if information['status'] == 'software':
                allowed, answered = threading.Event(), threading.Event()
                def confirm():
                    try:
                        if messagebox.askyesno(self.tr('Renderizado por CPU'), self.tr('Vulkan solo detecta renderizado por CPU en este entorno. El juego puede ir muy lento, aunque bajes la resolucion. Necesitas una GPU accesible mediante Vulkan para evaluar el rendimiento real de Linux.\n\nContinuar de todos modos?'), parent=self.root, default='no'):
                            allowed.set()
                    finally:
                        answered.set()
                self.events.put(('callback', confirm))
                while not answered.wait(0.1):
                    if self.destroyed:
                        return
                if not allowed.is_set():
                    return
            elif information['status'] == 'unavailable':
                self.events.put(('log', 'No se pudo comprobar la GPU Vulkan antes del arranque. Consulta el adaptador real en el registro del runtime.'))
            with subprocess.Popen([str(self.runtime), '--game=' + str(self.base / 'YKW4')], cwd=self.base, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace') as process:
                presence = DiscordPresence(rpc, process.pid, any((self.base / 'YKW4_data/system/archives').glob('*.romfs')))
                presence.start()
                try:
                    for line in process.stdout:
                        self.events.put(('log', line.rstrip()))
                    if process.wait():
                        raise RuntimeError('El juego terminó con un error. Revisa el registro; si la vinculación no coincide, prepara en otra carpeta.')
                finally:
                    presence.stop()
        self.start(run_game)

    def start(self, action):
        if self.busy:
            return
        self.busy = True
        self.refresh()
        def worker():
            try:
                action()
            except Exception as error:
                self.events.put(('error', str(error)))
            finally:
                self.events.put(('done', None))
        threading.Thread(target=worker, daemon=True).start()

    def pump(self):
        focused = self.root.focus_get()
        for action in self.gamepad.poll():
            if focused is None or focused.winfo_toplevel() != self.root:
                continue
            if action == 'cancel':
                self.close()
                if self.destroyed:
                    return
            elif action in ('next', 'previous'):
                target = focused.tk_focusNext() if action == 'next' else focused.tk_focusPrev()
                if target:
                    target.focus_set()
                    focused = target
            elif action == 'accept':
                if isinstance(focused, (ttk.Button, ttk.Checkbutton)):
                    focused.invoke()
                elif isinstance(focused, ttk.Combobox):
                    count = len(focused.cget('values'))
                    if count:
                        focused.current((focused.current() + 1) % count)
                        focused.event_generate('<<ComboboxSelected>>')
            if self.destroyed:
                return
        log_lines = []
        for unused in range(100):
            try:
                kind, value = self.events.get_nowait()
            except queue.Empty:
                break
            if kind == 'rescan':
                enabled = {mod.identifier: mod.enabled for mod in self.mods}
                self.mods = scan_mods(self.base)
                for mod in self.mods:
                    mod.enabled = enabled.get(mod.identifier, mod.enabled)
            elif kind == 'installed':
                self.base = Path(value)
                self.settings = read_ini(self.base / 'YKW4.ini')
                for tab in (self.tabs[0][0], self.tabs[2][0]):
                    for child in tab.winfo_children():
                        self.translatable = [(widget, text) for widget, text in self.translatable if widget != child]
                        child.destroy()
                self.values.clear()
                self.game_tab(self.tabs[0][0])
                self.discord_tab(self.tabs[2][0])
                self.mods = scan_mods(self.base)
                if not self.explicit_base:
                    try:
                        remember_installation(self.base)
                    except OSError as error:
                        messagebox.showwarning(self.tr('YKW4Recomp'), self.tr('Instalación preparada, pero no se pudo recordar su carpeta: {error}').format(error=error), parent=self.root)
                self.saved_settings = self.settings_snapshot()
            elif kind == 'callback':
                value()
            elif kind == 'done':
                self.busy = False
                self.preparation = None
                self.refresh()
                if self.closing:
                    self.destroy()
                    return
            elif kind == 'error':
                messagebox.showerror(self.tr('YKW4Recomp'), self.tr(value), parent=self.root)
            else:
                log_lines.append(self.tr(value))
        if log_lines:
            self.log.configure(state='normal')
            self.log.insert('end', '\n'.join(log_lines) + '\n')
            excess = int(self.log.index('end-1c').split('.')[0]) - 2000
            if excess > 0:
                self.log.delete('1.0', f'{excess + 1}.0')
            if self.activity_window.state() == 'normal':
                self.log.see('end')
            self.log.configure(state='disabled')
        self.root.after(80, self.pump)

    def destroy(self):
        self.destroyed = True
        self.gamepad.close()
        self.root.destroy()

    def close(self):
        if self.busy:
            if self.preparation and messagebox.askyesno(self.tr('Preparación en curso'), self.tr('¿Cancelar y cerrar cuando termine la herramienta?'), parent=self.root):
                self.closing = True
                self.preparation.cancel()
            elif not self.preparation:
                messagebox.showinfo(self.tr('Tarea en curso'), self.tr('Espera a que termine la tarea o cierra el juego antes de salir del launcher.'), parent=self.root)
        else:
            self.destroy()

    def setup_dialog(self):
        if self.busy:
            return
        dialog = tk.Toplevel(self.root)
        dialog.title(self.tr('Preparar / actualizar YKW4Recomp'))
        dialog.transient(self.root)
        frame = ttk.Frame(dialog, padding=20)
        frame.pack(fill='both', expand=True)
        fields = {}
        mode = tk.StringVar(value=self.tr('Carpetas extraídas'))
        ttk.Combobox(frame, textvariable=mode, values=(self.tr('Carpetas extraídas'), 'NSP / XCI'), state='readonly').grid(row=0, column=0, columnspan=3, sticky='ew', pady=8)
        defaults = {'backend': str(self.base / 'backend/hactool') if (self.base / 'backend/hactool').is_file() else shutil.which('hactool') or '', 'keys': str(Path.home() / '.switch/prod.keys'), 'install': str(self.base)}
        for row, (name, title, folder) in enumerate((('install', 'Carpeta de instalación', True), ('exefs', 'ExeFS (modo carpetas)', True), ('romfs', 'RomFS (modo carpetas)', True), ('dlc', 'DLC extraídos (opcional)', True), ('package', 'NSP / XCI base', False), ('update', 'Actualización NSP/XCI (opcional)', False), ('keys', 'prod.keys', False), ('title_keys', 'title.keys (opcional)', False), ('backend', 'hactool para Linux', False)), 1):
            fields[name] = tk.StringVar(value=defaults.get(name, ''))
            ttk.Label(frame, text=self.tr(title)).grid(row=row, column=0, sticky='w', padx=(0, 12), pady=5)
            ttk.Entry(frame, textvariable=fields[name], width=45).grid(row=row, column=1, sticky='ew', pady=5)
            def choose(name=name, folder=folder):
                selected = filedialog.askdirectory(parent=dialog) if folder else filedialog.askopenfilename(parent=dialog)
                if selected:
                    fields[name].set(selected)
            ttk.Button(frame, text=self.tr('Elegir…'), command=choose).grid(row=row, column=2, padx=(8, 0))
        dlcs = []
        def select_dlcs():
            dlcs[:] = filedialog.askopenfilenames(parent=dialog, filetypes=[('DLC NSP/XCI', '*.nsp *.xci')])
            dlc_text.set(self.tr('{count} paquetes DLC seleccionados').format(count=len(dlcs)))
        dlc_text = tk.StringVar(value=self.tr('{count} paquetes DLC seleccionados').format(count=0))
        ttk.Button(frame, text=self.tr('Añadir DLC NSP/XCI…'), command=select_dlcs).grid(row=10, column=0, pady=10)
        ttk.Label(frame, textvariable=dlc_text).grid(row=10, column=1, columnspan=2, sticky='w')
        ttk.Label(frame, text=self.tr('La nueva instalación se vincula a este usuario e instalación Linux mediante una clave privada y un identificador local. No se envía el HWID. No convierte DLL de Windows en módulos Linux. Las instalaciones antiguas sin vincular se conservan.'), wraplength=650).grid(row=11, column=0, columnspan=3, sticky='w', pady=12)
        def prepare():
            selection = {name: value.get().strip() for name, value in fields.items()}
            selection['dlcs'] = list(dlcs)
            if mode.get() == self.tr('Carpetas extraídas'):
                selection['package'] = ''
                if not selection['exefs'] or not selection['romfs']:
                    messagebox.showerror(self.tr('Faltan carpetas'), self.tr('Selecciona ExeFS y RomFS.'), parent=dialog)
                    return
            elif not all(selection.get(name) for name in ('package', 'keys', 'backend')):
                messagebox.showerror(self.tr('Faltan archivos'), self.tr('Selecciona el NSP/XCI base, prod.keys y hactool para Linux.'), parent=dialog)
                return
            if not selection['install']:
                return
            if not messagebox.askyesno(self.tr('Confirmar preparación local'), self.tr('Se compilará desde tus archivos autorizados. Una nueva instalación quedará vinculada a este usuario y Linux; no será portable. Conserva tus archivos y guardados. ¿Continuar?'), parent=dialog, default='no'):
                return
            target = Path(selection.pop('install')).resolve()
            self.preparation = Preparation(self.runtime, target, lambda line: self.events.put(('log', line)))
            preparation = self.preparation
            dialog.destroy()
            def run_preparation():
                preparation.prepare(selection, accepted=True)
                self.events.put(('installed', str(target)))
            self.start(run_preparation)
        ttk.Button(frame, text=self.tr('Preparar'), command=prepare).grid(row=12, column=1, sticky='e', pady=8)
        ttk.Button(frame, text=self.tr('Cancelar'), command=dialog.destroy).grid(row=12, column=2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--base', type=Path)
    parser.add_argument('--smoke-test', action='store_true')
    args = parser.parse_args()
    root = tk.Tk()
    launcher = Launcher(root, args.runtime, args.base)
    if args.smoke_test:
        def verify():
            assert len(launcher.notebook.tabs()) == 5
            assert hasattr(launcher, 'logo')
            assert launcher.play_button.instate(['disabled']) == (not launcher.ready())
            assert launcher.values['resolution'].get() in ('0.5', '0.75', '1', '1.5', '2')
            launcher.notebook.select(launcher.tabs[1][0])
            launcher.notebook.select(launcher.tabs[3][0])
            print('Desktop launcher: logo, tabs, settings and missing-game gating OK', flush=True)
            root.destroy()
        root.after(300, verify)
    root.mainloop()
    launcher.gamepad.close()


if __name__ == '__main__':
    main()
