import argparse
from dataclasses import replace
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import mobile_builder as builder


class MobileBuilderTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source space;not-a-shell"
        (self.source / "scripts").mkdir(parents=True)
        for relative in ["CMakeLists.txt", "scripts/android-apk.sh", "scripts/android-llvm.sh", "scripts/apple-app.sh", "scripts/apple-llvm.sh"]:
            (self.source / relative).touch()
        self.sdk = self.root / "sdk"
        (self.sdk / "platforms/android-36").mkdir(parents=True)
        (self.sdk / "platforms/android-36/android.jar").touch()
        self.ndk = self.sdk / "ndk/29.0.1"
        (self.ndk / "build/cmake").mkdir(parents=True)
        (self.ndk / "build/cmake/android.toolchain.cmake").touch()
        self.request = builder.BuildRequest("android", self.source, self.root / "output", self.root / "llvm", android_sdk=self.sdk, accepted_version=builder.TERMS_VERSION)

    def plan(self, request=None):
        return builder.build_plan(request or self.request, host="Linux", environ={}, which=lambda tool: "/bin/" + tool)

    def fake_plan(self, program):
        return patch.object(builder, "build_plan", return_value=([[sys.executable, "-c", program]], dict(os.environ)))

    def test_android_plan_uses_argument_vectors(self):
        commands, environment = self.plan()
        self.assertEqual(len(commands), 2)
        self.assertEqual(commands[0][1], str(self.source / "scripts/android-llvm.sh"))
        self.assertEqual(commands[1][1], str(self.source / "scripts/android-apk.sh"))
        self.assertEqual(environment["ANDROID_NDK_ROOT"], str(self.ndk.resolve()))

    def test_existing_target_llvm_is_reused(self):
        config = self.request.llvm / "arm64-v8a/lib/cmake/llvm/LLVMConfig.cmake"
        config.parent.mkdir(parents=True)
        config.touch()
        commands, unused = self.plan()
        self.assertEqual(len(commands), 1)

    def test_missing_llvm_is_reported(self):
        with self.assertRaisesRegex(ValueError, "LLVM"):
            self.plan(replace(self.request, build_llvm=False))

    def test_ios_rejects_linux_and_windows(self):
        for host in ["Linux", "Windows"]:
            with self.subTest(host=host), self.assertRaisesRegex(ValueError, "macOS"):
                builder.build_plan(replace(self.request, target="ios"), host=host)

    def test_ios_plan_uses_apple_recipes(self):
        molten = self.root / "MoltenVK"
        molten.mkdir()
        request = replace(self.request, target="ios", moltenvk=molten)
        commands, unused = builder.build_plan(request, host="Darwin", environ={}, which=lambda tool: "/bin/" + tool)
        self.assertEqual(commands[0][1], str(self.source / "scripts/apple-llvm.sh"))
        self.assertEqual(commands[-1][1], str(self.source / "scripts/apple-app.sh"))
        self.assertIn("--preset", commands[1])
        self.assertNotIn("codesign", str(commands))

    def test_missing_sdk_and_platform_are_reported(self):
        with self.assertRaisesRegex(ValueError, "SDK"):
            self.plan(replace(self.request, android_sdk=None))
        (self.sdk / "platforms/android-36/android.jar").unlink()
        with self.assertRaisesRegex(ValueError, "36"):
            self.plan()

    def test_missing_ndk_is_reported(self):
        (self.ndk / "build/cmake/android.toolchain.cmake").unlink()
        with self.assertRaisesRegex(ValueError, "NDK"):
            self.plan()

    def test_missing_tool_is_reported(self):
        with self.assertRaisesRegex(ValueError, "gradle"):
            builder.build_plan(self.request, host="Linux", environ={}, which=lambda tool: None if tool == "gradle" else "/bin/" + tool)

    def test_source_and_ancestor_cannot_be_output(self):
        for output in [self.source, self.root]:
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.plan(replace(self.request, output=output))

    def test_nonempty_unrelated_output_is_rejected(self):
        self.request.output.mkdir()
        (self.request.output / "keep.txt").write_text("untouched")
        with self.assertRaises(ValueError):
            self.plan()
        self.assertEqual((self.request.output / "keep.txt").read_text(), "untouched")

    def test_overlapping_llvm_is_rejected(self):
        with self.assertRaises(ValueError):
            self.plan(replace(self.request, llvm=self.request.output / "llvm"))

    def test_acceptance_is_required_before_writing_or_running(self):
        with patch.object(builder.subprocess, "Popen") as process:
            for version in ["", "old"]:
                with self.assertRaisesRegex(ValueError, "aceptar"):
                    builder.BuildRunner().run(replace(self.request, accepted_version=version))
            process.assert_not_called()
        self.assertFalse(self.request.output.exists())

    def test_success_requires_artifact_and_records_acceptance(self):
        artifact = self.request.output / "stage/gradle/outputs/apk/debug/test.apk"
        program = f"from pathlib import Path; artifact=Path({str(artifact)!r}); artifact.parent.mkdir(parents=True); artifact.write_bytes(b'test-only')"
        with self.fake_plan(program):
            artifacts = builder.BuildRunner().run(self.request, lambda line: None)
        self.assertEqual(artifacts, [str(artifact)])
        record = json.loads((self.request.output / "mobile-build-result.json").read_text())
        self.assertEqual(record["status"], "success")
        self.assertEqual(record["terms_version"], builder.TERMS_VERSION)
        self.assertIn("accepted_at", record)

    def test_zero_exit_without_artifact_is_failure(self):
        with self.fake_plan("pass"), self.assertRaisesRegex(RuntimeError, "paquete nuevo"):
            builder.BuildRunner().run(self.request, lambda line: None)
        record = json.loads((self.request.output / "mobile-build-result.json").read_text())
        self.assertEqual(record["status"], "failed")

    def test_failed_tool_is_not_reported_as_success(self):
        with self.fake_plan("raise SystemExit(9)"), self.assertRaisesRegex(RuntimeError, "9"):
            builder.BuildRunner().run(self.request, lambda line: None)

    def test_stale_artifact_is_not_a_fresh_build(self):
        artifact = self.request.output / "stage/gradle/outputs/apk/debug/stale.apk"
        artifact.parent.mkdir(parents=True)
        artifact.write_bytes(b"old-test-only")
        os.utime(artifact, (1, 1))
        with self.fake_plan("pass"), self.assertRaisesRegex(RuntimeError, "paquete nuevo"):
            builder.BuildRunner().run(self.request, lambda line: None)

    def test_running_build_can_be_cancelled(self):
        ready = threading.Event()
        errors = []
        runner = builder.BuildRunner()
        def run():
            try:
                runner.run(self.request, lambda line: ready.set())
            except RuntimeError as error:
                errors.append(str(error))
        with self.fake_plan("import time; print('ready', flush=True); time.sleep(60)"):
            thread = threading.Thread(target=run)
            thread.start()
            self.assertTrue(ready.wait(10))
            runner.cancel()
            thread.join(10)
            self.assertFalse(thread.is_alive())
        self.assertTrue(errors)
        record = json.loads((self.request.output / "mobile-build-result.json").read_text())
        self.assertEqual(record["status"], "cancelled")

    def test_cancellation_before_start_creates_nothing(self):
        runner = builder.BuildRunner()
        runner.cancel()
        with self.fake_plan("pass"), self.assertRaises(ValueError):
            runner.run(self.request)
        self.assertFalse(self.request.output.exists())

    def test_wsl_forwards_paths_and_acceptance_without_shell(self):
        arguments = argparse.Namespace(target="android", source="C:/source space", output="C:/output", llvm="C:/llvm", moltenvk=None, android_sdk=None, android_ndk=None, build_llvm=False, accept_terms=builder.TERMS_VERSION, check=False)
        command = builder.wsl_command(arguments, "Ubuntu-24.04", lambda value: "/converted/" + value)
        self.assertEqual(command[:5], ["wsl.exe", "--distribution", "Ubuntu-24.04", "--exec", "python3"])
        self.assertIn("/converted/C:/source space", command)
        self.assertIn(builder.TERMS_VERSION, command)
        self.assertIn("--no-build-llvm", command)

    def test_terms_do_not_claim_total_transfer_of_liability(self):
        self.assertIn("no transfiere toda responsabilidad", builder.TERMS)
        self.assertIn("no puedan excluirse legalmente", builder.TERMS)
        self.assertNotIn("Compilaci?n", builder.TERMS)


if __name__ == "__main__":
    unittest.main()
