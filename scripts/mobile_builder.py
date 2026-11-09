import argparse
import json
import os
from pathlib import Path
import platform
import queue
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from datetime import datetime, timezone


TERMS_VERSION = "2026-11-02.1"
TERMS = """Compilación móvil experimental: condiciones de uso

Generar un APK o IPA no concede derechos sobre el juego, las claves, las marcas,
los mods ni las traducciones. Utiliza únicamente material que estás autorizado a
usar. Este asistente no incorpora ni solicita ROM, NSP/XCI, claves o guardados.

Eres responsable de tus actos de uso, modificación y distribución conforme a la
ley aplicable. No distribuyas contenido del juego, claves ni módulos derivados
sin autorización. Si compartes componentes de código abierto, debes cumplir sus
licencias, incluyendo atribuciones y el código fuente correspondiente cuando
proceda. La firma de una aplicación tampoco concede derechos de distribución.

Esta aceptación no transfiere toda responsabilidad legal al usuario ni exonera
a los desarrolladores de sus propias obligaciones o de responsabilidades que
no puedan excluirse legalmente. No es asesoramiento jurídico ni una garantía
de legalidad; consulta a un profesional antes de distribuir un paquete.

Android: se genera un APK de depuración, no una publicación de tienda.
iOS: requiere macOS, Xcode y MoltenVK; el IPA generado no incluye una identidad
de firma de distribución. Su instalación depende de firma, permisos y del
dispositivo. Compilar no demuestra que el juego funcione en un móvil.

La aceptación se solicita en cada compilación. Se registra localmente su versión
y fecha junto al resultado; no se suben esos registros ni tus archivos.
"""


@dataclass(frozen=True)
class BuildRequest:
    target: str
    source: Path
    output: Path
    llvm: Path
    moltenvk: Path | None = None
    android_sdk: Path | None = None
    android_ndk: Path | None = None
    build_llvm: bool = True
    accepted_version: str = ""


def build_plan(request, host=None, environ=None, which=shutil.which):
    host = host or platform.system()
    environ = dict(os.environ if environ is None else environ)
    if request.target not in ("android", "ios"):
        raise ValueError("Destino no soportado.")
    if request.target == "ios" and host != "Darwin":
        raise ValueError("Un IPA requiere macOS y Xcode. No se puede compilar en Windows o WSL.")
    if host == "Windows":
        raise ValueError("Para Android en Windows usa WSL y selecciona una distribución instalada.")
    source = request.source.resolve()
    output = request.output.resolve()
    llvm = request.llvm.resolve()
    if llvm == source or llvm in source.parents or llvm == output or llvm in output.parents or output in llvm.parents:
        raise ValueError("Usa una carpeta LLVM separada del código y de la salida.")
    if not (source / "CMakeLists.txt").is_file() or not (source / "scripts/android-apk.sh").is_file():
        raise ValueError("Selecciona el árbol de código fuente completo del launcher.")
    if output == source or output in source.parents or output == Path.home().resolve():
        raise ValueError("La salida debe ser una carpeta de compilación dedicada, no el código ni tu carpeta personal.")
    if output.exists() and any(output.iterdir()) and not (output / ".ykw4-mobile-build").is_file():
        raise ValueError("La carpeta de salida no está vacía. Selecciona una carpeta nueva para no sobrescribir archivos.")
    required = ["sh", "cmake", "ninja", "clang", "clang++", "make", "perl", "nasm"]
    required += ["java", environ.get("GRADLE", "gradle")] if request.target == "android" else ["xcrun", "xcodebuild", "zip", "plutil"]
    missing = [tool for tool in required if not which(tool)]
    if missing:
        raise ValueError("Faltan herramientas en PATH: " + ", ".join(missing))
    commands = []
    if request.target == "android":
        sdk_value = request.android_sdk or environ.get("ANDROID_HOME") or environ.get("ANDROID_SDK_ROOT")
        if not sdk_value or not Path(sdk_value).is_dir():
            raise ValueError("Indica el Android SDK mediante el campo SDK o ANDROID_HOME.")
        sdk = Path(sdk_value).resolve()
        ndk_value = request.android_ndk or environ.get("ANDROID_NDK_ROOT")
        if not ndk_value:
            candidates = sorted((sdk / "ndk").glob("*"), key=lambda path: tuple(int(part) if part.isdigit() else 0 for part in path.name.split(".")))
            ndk_value = candidates[-1] if candidates else None
        if not ndk_value or not (Path(ndk_value) / "build/cmake/android.toolchain.cmake").is_file():
            raise ValueError("Falta el NDK de Android: indica ANDROID_NDK_ROOT o instálalo dentro del SDK.")
        if not (sdk / "platforms/android-36/android.jar").is_file():
            raise ValueError("Instala la plataforma Android SDK 36 antes de compilar.")
        environ.update(ANDROID_HOME=str(sdk), ANDROID_NDK_ROOT=str(Path(ndk_value).resolve()))
        config = llvm / "arm64-v8a/lib/cmake/llvm/LLVMConfig.cmake"
        if not config.is_file():
            if not request.build_llvm:
                raise ValueError("Falta LLVM para arm64-v8a. Activa su compilación o indica un prefijo válido.")
            commands.append(["sh", str(source / "scripts/android-llvm.sh"), "arm64-v8a", str(llvm / "arm64-v8a")])
        commands.append(["sh", str(source / "scripts/android-apk.sh"), str(output), str(llvm), "arm64-v8a", "--", "-DSNX_BUILD_3DS=OFF", "-DCMAKE_BUILD_TYPE=Release"])
    else:
        if not request.moltenvk or not request.moltenvk.is_dir():
            raise ValueError("Selecciona la distribución de MoltenVK para iOS.")
        if not (llvm / "lib/cmake/llvm/LLVMConfig.cmake").is_file():
            if not request.build_llvm:
                raise ValueError("Falta LLVM para iOS. Activa su compilación o indica un prefijo válido.")
            commands.append(["sh", str(source / "scripts/apple-llvm.sh"), "ios", str(llvm)])
        native = output / "native"
        commands.extend([
            ["cmake", "--preset", "ios", "-B", str(native), "-DLLVM_DIR=" + str(llvm / "lib/cmake/llvm")],
            ["cmake", "--build", str(native), "--parallel", str(min(os.cpu_count() or 2, 8))],
            ["cmake", "--install", str(native), "--prefix", str(output / "stage")],
            ["sh", str(source / "scripts/apple-app.sh"), "ios", str(output / "stage"), str(output / "packages"), str(request.moltenvk.resolve())],
        ])
    return commands, environ


class BuildRunner:
    def __init__(self):
        self.cancelled = threading.Event()
        self.process = None
        self.lock = threading.Lock()
        self.wsl_cancel_file = None

    def cancel(self):
        self.cancelled.set()
        with self.lock:
            if self.wsl_cancel_file is not None:
                self.wsl_cancel_file.touch()
                return
            process = self.process
            if process is not None and process.poll() is None:
                if os.name == "nt":
                    subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=subprocess.CREATE_NO_WINDOW)
                else:
                    try:
                        os.killpg(process.pid, signal.SIGTERM)
                    except ProcessLookupError:
                        pass
                    def force_stop():
                        try:
                            os.killpg(process.pid, signal.SIGKILL)
                        except ProcessLookupError:
                            pass
                    timer = threading.Timer(3, force_stop)
                    timer.daemon = True
                    timer.start()

    def run(self, request, emit=print):
        if request.accepted_version != TERMS_VERSION:
            raise ValueError("Debes leer y aceptar las condiciones actuales antes de compilar.")
        commands, environ = build_plan(request)
        if self.cancelled.is_set():
            raise ValueError("Compilación cancelada.")
        output = request.output.resolve()
        output.mkdir(parents=True, exist_ok=True)
        (output / ".ykw4-mobile-build").write_text(TERMS_VERSION, encoding="utf-8")
        record = {"terms_version": TERMS_VERSION, "accepted_at": datetime.now(timezone.utc).isoformat(), "target": request.target, "status": "running", "artifacts": []}
        result_file = output / "mobile-build-result.json"
        result_file.write_text(json.dumps(record, indent=2), encoding="utf-8")
        started = time.time_ns()
        try:
            with (output / "mobile-build.log").open("w", encoding="utf-8") as log:
                for command in commands:
                    with self.lock:
                        if self.cancelled.is_set():
                            raise RuntimeError("Compilación cancelada.")
                        flags = {"creationflags": subprocess.CREATE_NO_WINDOW | subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
                        self.process = subprocess.Popen(command, cwd=request.source, env=environ, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", **flags)
                    with self.process.stdout:
                        for line in self.process.stdout:
                            log.write(line)
                            log.flush()
                            emit(line.rstrip())
                    status = self.process.wait()
                    if self.cancelled.is_set():
                        raise RuntimeError("Compilación cancelada.")
                    if status:
                        raise RuntimeError(f"La herramienta terminó con código {status}. Consulta mobile-build.log.")
            pattern = "stage/gradle/outputs/apk/debug/*.apk" if request.target == "android" else "packages/*.ipa"
            artifacts = [str(path) for path in output.glob(pattern) if path.is_file() and path.stat().st_mtime_ns >= started]
            if not artifacts:
                raise RuntimeError("No se ha generado ningún paquete nuevo. No se considera una compilación correcta.")
            record.update(status="success", artifacts=artifacts)
            return artifacts
        except Exception as error:
            record.update(status="cancelled" if self.cancelled.is_set() else "failed", error=str(error))
            raise
        finally:
            record["finished_at"] = datetime.now(timezone.utc).isoformat()
            result_file.write_text(json.dumps(record, indent=2), encoding="utf-8")


def wsl_command(args, distro, convert):
    command = ["wsl.exe", "--distribution", distro, "--exec", "python3", convert(str(Path(__file__).resolve()))]
    command.extend(["--target", args.target, "--source", convert(args.source), "--output", convert(args.output), "--llvm", convert(args.llvm)])
    for name in ("android_sdk", "android_ndk", "moltenvk"):
        value = getattr(args, name, None)
        if value:
            command.extend(["--" + name.replace("_", "-"), convert(value)])
    if not args.build_llvm:
        command.append("--no-build-llvm")
    if args.accept_terms:
        command.extend(["--accept-terms", args.accept_terms])
    if args.check:
        command.append("--check")
    if getattr(args, "cancel_file", None):
        command.extend(["--cancel-file", convert(args.cancel_file)])
    return command


def launch_gui(smoke_test=False):
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
    from tkinter.scrolledtext import ScrolledText

    root = tk.Tk()
    root.title("YKW4Recomp · Compilar para móviles")
    root.geometry("860x820")
    root.minsize(680, 620)
    frame = ttk.Frame(root, padding=18)
    frame.pack(fill="both", expand=True)
    frame.columnconfigure(1, weight=1)
    ttk.Label(frame, text="Compilar para móviles", font=("TkDefaultFont", 17, "bold")).grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 6))
    ttk.Label(frame, text="Android APK experimental · iOS IPA solo en macOS con Xcode. No incluye el juego.", wraplength=760).grid(row=1, column=0, columnspan=3, sticky="w", pady=(0, 12))
    target = tk.StringVar(value="android")
    ttk.Label(frame, text="Destino").grid(row=2, column=0, sticky="w")
    ttk.Combobox(frame, textvariable=target, values=("android", "ios"), state="readonly").grid(row=2, column=1, sticky="ew")
    source_default = Path(__file__).resolve().parents[1]
    fields = {}
    defaults = {"source": str(source_default) if (source_default / "CMakeLists.txt").is_file() else "", "output": str(Path.home() / "YKW4-mobile/android"), "llvm": str(Path.home() / "YKW4-mobile/llvm"), "android_sdk": os.environ.get("ANDROID_HOME", ""), "android_ndk": os.environ.get("ANDROID_NDK_ROOT", ""), "moltenvk": "", "wsl": "Ubuntu-24.04" if os.name == "nt" else ""}
    for row, (name, label) in enumerate((("source", "Código fuente"), ("output", "Carpeta de salida"), ("llvm", "LLVM del destino"), ("android_sdk", "Android SDK"), ("android_ndk", "Android NDK"), ("moltenvk", "MoltenVK (iOS)"), ("wsl", "Distribución WSL (Windows)")), start=3):
        variable = tk.StringVar(value=defaults[name])
        fields[name] = variable
        ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", padx=(0, 12), pady=3)
        ttk.Entry(frame, textvariable=variable).grid(row=row, column=1, sticky="ew", pady=3)
        if name != "wsl":
            def choose(value=variable):
                selected = filedialog.askdirectory(parent=root)
                if selected:
                    value.set(selected)
            ttk.Button(frame, text="Elegir…", command=choose).grid(row=row, column=2, padx=(8, 0))
    build_llvm = tk.BooleanVar(value=True)
    ttk.Checkbutton(frame, text="Compilar LLVM del destino si falta (descarga y compilación largas)", variable=build_llvm).grid(row=10, column=0, columnspan=3, sticky="w", pady=6)
    terms = ScrolledText(frame, height=12, wrap="word")
    terms.insert("1.0", TERMS)
    terms.configure(state="disabled")
    terms.grid(row=11, column=0, columnspan=3, sticky="nsew", pady=8)
    frame.rowconfigure(11, weight=1)
    accepted = tk.BooleanVar(value=False)
    ttk.Checkbutton(frame, text="He leído y acepto estas condiciones y la responsabilidad por mis propios actos.", variable=accepted).grid(row=12, column=0, columnspan=3, sticky="w")
    output_text = ScrolledText(frame, height=8, wrap="word", state="disabled")
    output_text.grid(row=14, column=0, columnspan=3, sticky="nsew", pady=8)
    frame.rowconfigure(14, weight=1)
    events = queue.Queue()
    runner = None
    busy = False
    closing = False

    def eligibility(*unused):
        compile_button.configure(state="normal" if accepted.get() and not busy else "disabled")

    def changed_target(*unused):
        accepted.set(False)

    def worker(arguments, active_runner):
        cancel_path = None
        try:
            if os.name == "nt" and arguments.target == "android":
                if not fields_distro:
                    raise ValueError("Indica una distribución WSL instalada.")
                def convert(value):
                    if value.startswith("/"):
                        return value
                    return subprocess.check_output(["wsl.exe", "-d", fields_distro, "--exec", "wslpath", "-a", "-u", value], text=True, encoding="utf-8", timeout=20, creationflags=subprocess.CREATE_NO_WINDOW).strip()
                descriptor, name = tempfile.mkstemp(prefix="ykw4-mobile-cancel-")
                os.close(descriptor)
                cancel_path = Path(name)
                cancel_path.unlink()
                arguments.cancel_file = str(cancel_path)
                command = wsl_command(arguments, fields_distro, convert)
                with active_runner.lock:
                    if active_runner.cancelled.is_set():
                        raise RuntimeError("Compilación cancelada.")
                    active_runner.wsl_cancel_file = cancel_path
                    active_runner.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace", creationflags=subprocess.CREATE_NO_WINDOW)
                with active_runner.process.stdout:
                    for line in active_runner.process.stdout:
                        events.put(line.rstrip())
                if active_runner.process.wait():
                    raise RuntimeError("La compilación en WSL ha fallado o se ha cancelado. Consulta el registro.")
                events.put("Paquete generado en la carpeta de salida indicada.")
            else:
                request = BuildRequest(arguments.target, Path(arguments.source), Path(arguments.output), Path(arguments.llvm), Path(arguments.moltenvk) if arguments.moltenvk else None, Path(arguments.android_sdk) if arguments.android_sdk else None, Path(arguments.android_ndk) if arguments.android_ndk else None, arguments.build_llvm, arguments.accept_terms)
                artifacts = active_runner.run(request, events.put)
                events.put("Paquetes generados:\n" + "\n".join(artifacts))
        except Exception as error:
            events.put("ERROR: " + str(error))
        finally:
            if cancel_path:
                cancel_path.unlink(missing_ok=True)
            events.put(None)

    def compile_now():
        nonlocal busy, runner, fields_distro
        if not accepted.get() or busy:
            return
        arguments = argparse.Namespace(target=target.get(), source=fields["source"].get(), output=fields["output"].get(), llvm=fields["llvm"].get(), moltenvk=fields["moltenvk"].get(), android_sdk=fields["android_sdk"].get(), android_ndk=fields["android_ndk"].get(), build_llvm=build_llvm.get(), accept_terms=TERMS_VERSION, check=False)
        if not all((arguments.source, arguments.output, arguments.llvm)):
            messagebox.showerror("Faltan rutas", "Indica código fuente, salida y LLVM.", parent=root)
            return
        fields_distro = fields["wsl"].get().strip()
        busy = True
        runner = BuildRunner()
        eligibility()
        cancel_button.configure(state="normal")
        threading.Thread(target=worker, args=(arguments, runner), daemon=True).start()

    fields_distro = ""
    compile_button = ttk.Button(frame, text="Compilar paquete", command=compile_now, state="disabled")
    compile_button.grid(row=13, column=1, sticky="e", pady=(8, 0))
    cancel_button = ttk.Button(frame, text="Cancelar compilación", command=lambda: runner.cancel() if runner else None, state="disabled")
    cancel_button.grid(row=13, column=2, padx=(8, 0), pady=(8, 0))
    accepted.trace_add("write", eligibility)
    target.trace_add("write", changed_target)

    def pump():
        nonlocal busy
        while not events.empty():
            entry = events.get_nowait()
            if entry is None:
                busy = False
                accepted.set(False)
                cancel_button.configure(state="disabled")
                if closing:
                    root.destroy()
                    return
            else:
                output_text.configure(state="normal")
                output_text.insert("end", entry + "\n")
                output_text.see("end")
                output_text.configure(state="disabled")
        root.after(100, pump)

    def close():
        nonlocal closing
        if busy:
            if messagebox.askyesno("Compilación en curso", "¿Cancelar la compilación y cerrar?", parent=root):
                closing = True
                runner.cancel()
        else:
            root.destroy()

    root.protocol("WM_DELETE_WINDOW", close)
    if smoke_test:
        def check_widgets():
            assert compile_button.instate(["disabled"])
            accepted.set(True)
            assert not compile_button.instate(["disabled"])
            target.set("ios")
            assert not accepted.get() and compile_button.instate(["disabled"])
            assert terms.cget("state") == "disabled"
            print("GUI consent gate, target reset and read-only terms: OK", flush=True)
            root.destroy()
        root.after(100, check_widgets)
    pump()
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description="Compilación móvil experimental; no incluye archivos del juego.")
    parser.add_argument("--gui", action="store_true")
    parser.add_argument("--target", choices=("android", "ios"))
    parser.add_argument("--source", default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument("--output")
    parser.add_argument("--llvm")
    parser.add_argument("--moltenvk")
    parser.add_argument("--android-sdk")
    parser.add_argument("--android-ndk")
    parser.add_argument("--no-build-llvm", dest="build_llvm", action="store_false")
    parser.add_argument("--accept-terms", default="")
    parser.add_argument("--show-terms", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--cancel-file", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.show_terms:
        print(TERMS_VERSION + "\n" + TERMS)
        return 0
    if args.gui:
        launch_gui()
        return 0
    if not args.target or not args.output or not args.llvm:
        parser.error("Se requieren --target, --output y --llvm, o bien --gui.")
    request = BuildRequest(args.target, Path(args.source), Path(args.output), Path(args.llvm), Path(args.moltenvk) if args.moltenvk else None, Path(args.android_sdk) if args.android_sdk else None, Path(args.android_ndk) if args.android_ndk else None, args.build_llvm, args.accept_terms)
    try:
        if args.check:
            commands, unused = build_plan(request)
            print(json.dumps(commands, indent=2))
        else:
            runner = BuildRunner()
            def interrupted(signum, frame):
                runner.cancel()
            signal.signal(signal.SIGTERM, interrupted)
            signal.signal(signal.SIGINT, interrupted)
            done = threading.Event()
            if args.cancel_file:
                def watch_cancel():
                    while not done.wait(0.2):
                        if Path(args.cancel_file).exists():
                            runner.cancel()
                            return
                threading.Thread(target=watch_cancel, daemon=True).start()
            try:
                print("\n".join(runner.run(request)))
            finally:
                done.set()
        return 0
    except (ValueError, RuntimeError, OSError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
