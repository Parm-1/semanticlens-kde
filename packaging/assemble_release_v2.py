#!/usr/bin/env python3
"""Assemble deterministic source and staged-native development archives."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
GIT = "/usr/bin/git"
RUNTIME_PATHS = {
    "bin/embedding-probe",
    "bin/kde-index",
    "bin/kde-index-cli",
    "bin/kde-index-service",
    "lib/qt6/plugins/kf6/krunner/kde-index-runner.so",
    "share/kde-index/model_download.py",
    "share/applications/org.kdeindex.Search.desktop",
    "share/metainfo/org.kdeindex.Search.metainfo.xml",
}
SECRET_PATTERNS = (
    re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(rb"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(rb"\bgh[pousr]_[A-Za-z0-9]{30,}\b"),
    re.compile(rb"\bsk-[A-Za-z0-9]{24,}\b"),
)
FORBIDDEN_NAMES = (".env", ".env.", ".sqlite", ".gguf", ".gguf.part")


def fail(message: str) -> None:
    raise ValueError(message)


def run_git(*args: str, cwd: Path = ROOT) -> bytes:
    return subprocess.run([GIT, *args], cwd=cwd, check=True, stdout=subprocess.PIPE).stdout


def git_commit(value: str) -> str:
    resolved = run_git("rev-parse", "--verify", f"{value}^{{commit}}").decode().strip()
    if resolved != value:
        fail(f"commit must be a full immutable object ID: {value}")
    return resolved


def git_tree(commit: str) -> dict[str, tuple[str, str]]:
    raw = run_git("ls-tree", "-rz", "--full-tree", "--long", commit)
    result: dict[str, tuple[str, str]] = {}
    for row in raw.split(b"\0"):
        if not row:
            continue
        header, path = row.split(b"\t", 1)
        mode, kind, oid, _size = header.split(b" ", 3)
        result[path.decode("utf-8", "surrogateescape")] = (
            mode.decode(),
            oid.decode(),
        )
    return result


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def inspect_source(commit: str, build_commit: str) -> dict:
    tree = git_tree(commit)
    build_tree = git_tree(build_commit)
    if run_git("merge-base", build_commit, commit).decode().strip() != build_commit:
        fail("the supplied native build commit must be an ancestor of the source archive commit")
    diff = run_git("diff", "--name-only", "-z", build_commit, commit).decode(
        "utf-8", "surrogateescape"
    ).split("\0")
    disallowed = [
        p for p in diff if p and not (
            p == "packaging/assemble_release_v2.py"
            or p.startswith("docs/")
            or p.startswith("evidence/")
        )
    ]
    if disallowed:
        fail("native build commit changes files outside packaging/docs/evidence: " + ", ".join(disallowed[:8]))
    if "LICENSE" not in tree or "THIRD_PARTY_NOTICES.md" not in tree:
        fail("source commit is missing required license/notice files")
    forbidden = [p for p in tree if any(token in p.lower() for token in FORBIDDEN_NAMES)]
    forbidden += [p for p in tree if "/models/" in f"/{p.lower()}/" or "/vendor/" in f"/{p.lower()}/"]
    if forbidden:
        fail("source tree contains ignored runtime/model/vendor data: " + ", ".join(forbidden[:8]))
    return {"source_tree": tree, "build_tree": build_tree, "changed_paths": [p for p in diff if p]}


def normalize_member(member: tarfile.TarInfo, epoch: int) -> tarfile.TarInfo:
    member.uid = 0
    member.gid = 0
    member.uname = ""
    member.gname = ""
    member.mtime = epoch
    member.pax_headers = {}
    return member


def git_archive(repo: Path, commit: str, prefix: str, destination: Path, epoch: int, tree: dict[str, tuple[str, str]]) -> dict:
    proc = subprocess.Popen(
        [GIT, "archive", "--format=tar", f"--prefix={prefix}/", commit],
        cwd=repo,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    assert proc.stdout is not None
    seen: dict[str, str] = {}
    workspace_path_files: set[str] = set()
    home_path_files: set[str] = set()
    secret_checks = 0
    workspace_root = str(ROOT).encode("utf-8")
    try:
        with destination.open("wb") as raw_out:
            with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw_out, mtime=epoch) as gz:
                with tarfile.open(fileobj=gz, mode="w|", format=tarfile.GNU_FORMAT) as out_tar:
                    with tarfile.open(fileobj=proc.stdout, mode="r|") as in_tar:
                        for member in in_tar:
                            if member.name.rstrip("/") == prefix:
                                continue
                            if not member.name.startswith(prefix + "/"):
                                fail("source archive member escaped its declared top directory")
                            relative = member.name[len(prefix) + 1 :]
                            if not relative:
                                continue
                            if member.isdir():
                                continue
                            if relative not in tree:
                                fail("source archive contains a path absent from the pinned Git tree")
                            mode, oid = tree[relative]
                            if member.isfile():
                                stream = in_tar.extractfile(member)
                                if stream is None:
                                    fail("source archive has an unreadable regular file")
                                content = stream.read()
                                git_oid = hashlib.sha1(b"blob " + str(len(content)).encode() + b"\0" + content).hexdigest()
                                if git_oid != oid:
                                    fail(f"source archive content mismatch at {relative}")
                                check_payload(relative, content)
                                if workspace_root in content:
                                    workspace_path_files.add(relative)
                                if b"/home/" in content:
                                    home_path_files.add(relative)
                                secret_checks += len(SECRET_PATTERNS)
                                seen[relative] = git_oid
                                member = normalize_member(member, epoch)
                                out_tar.addfile(member, __import__("io").BytesIO(content))
                            elif member.issym():
                                link = member.linkname.encode()
                                git_oid = hashlib.sha1(b"blob " + str(len(link)).encode() + b"\0" + link).hexdigest()
                                if git_oid != oid:
                                    fail(f"source archive symlink mismatch at {relative}")
                                seen[relative] = git_oid
                                out_tar.addfile(normalize_member(member, epoch))
                            else:
                                fail(f"unsupported source archive object type at {relative}")
        _stdout, stderr = proc.communicate()
        if proc.returncode:
            fail("git archive failed: " + stderr.decode("utf-8", "replace")[:500])
    except BaseException:
        proc.kill()
        proc.communicate()
        raise
    expected_blobs = {p for p, (mode, _oid) in tree.items() if mode != "160000"}
    if set(seen) != expected_blobs:
        fail(f"source archive tree readback differs (missing={len(expected_blobs-set(seen))}, extra={len(set(seen)-expected_blobs)})")
    return {
        "tracked_paths": len(seen),
        "secret_pattern_file_checks": secret_checks,
        "workspace_path_reference_files": len(workspace_path_files),
        "home_path_reference_files": len(home_path_files),
    }


def check_payload(path: str, content: bytes) -> None:
    low = path.lower()
    if any(token in low for token in FORBIDDEN_NAMES) or "/models/" in f"/{low}/" or "/vendor/" in f"/{low}/":
        fail(f"forbidden private/runtime payload path: {path}")
    for pattern in SECRET_PATTERNS:
        if pattern.search(content):
            fail(f"possible credential/private key pattern in package path: {path}")


def native_inventory(stage_usr: Path, manifest_path: Path, source_commit: str, source_tree: dict[str, tuple[str, str]]) -> dict[str, str]:
    manifest = json.loads(manifest_path.read_text())
    expected = manifest.get("actual_new")
    if not isinstance(expected, dict) or not expected:
        fail("stage manifest must provide actual_new file hashes")
    actual: dict[str, str] = {}
    for path in sorted(stage_usr.rglob("*")):
        if path.is_symlink():
            fail("native stage must not contain symlinks: " + str(path.relative_to(stage_usr)))
        if path.is_file():
            rel = path.relative_to(stage_usr).as_posix()
            actual[rel] = sha256_file(path)
    if actual != expected:
        missing = sorted(set(expected) - set(actual))
        extra = sorted(set(actual) - set(expected))
        changed = sorted(p for p in set(actual) & set(expected) if actual[p] != expected[p])
        fail(f"native stage pin mismatch: missing={missing[:4]} extra={extra[:4]} changed={changed[:4]}")
    if "LICENSE" not in source_tree or "THIRD_PARTY_NOTICES.md" not in source_tree:
        fail("source commit is missing the required license/notice files")
    license_sources = [p for p in source_tree if p.startswith("LICENSES/")]
    if any("/" in p[len("LICENSES/"):] for p in license_sources):
        fail("nested LICENSES paths need an explicit reviewed native path mapping")
    expected_paths = set(RUNTIME_PATHS)
    expected_paths.update({"share/licenses/kde-index/LICENSE", "share/licenses/kde-index/THIRD_PARTY_NOTICES.md"})
    expected_paths.update(f"share/licenses/kde-index/{PurePosixPath(p).name}" for p in license_sources)
    if len(expected_paths) != len(RUNTIME_PATHS) + 2 + len(license_sources):
        fail("source license filenames collide in the native license directory")
    missing = sorted(expected_paths - set(actual))
    unexpected = sorted(set(actual) - expected_paths)
    if missing or unexpected:
        fail(f"native payload is not the runtime-only allowlist: missing={missing[:5]} unexpected={unexpected[:8]}")
    stage_secret_checks = 0
    for rel, digest in actual.items():
        content = (stage_usr / rel).read_bytes()
        check_payload(rel, content)
        stage_secret_checks += len(SECRET_PATTERNS)
        if sha256_file(stage_usr / rel) != digest:
            fail(f"native stage changed while being inspected: {rel}")
    for source_path, stage_name in [("LICENSE", "LICENSE"), ("THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.md")]:
        content = run_git("show", f"{source_commit}:{source_path}")
        if (stage_usr / "share/licenses/kde-index" / stage_name).read_bytes() != content:
            fail(f"staged notice differs from source commit: {source_path}")
    for source_path in license_sources:
        content = run_git("show", f"{source_commit}:{source_path}")
        stage_path = stage_usr / "share/licenses/kde-index" / PurePosixPath(source_path).name
        if stage_path.read_bytes() != content:
            fail(f"staged license differs from source commit: {source_path}")
    return {"files": actual, "license_file_count": 2 + len(license_sources), "secret_pattern_file_checks": stage_secret_checks}


def add_native(stage_usr: Path, prefix: str, destination: Path, epoch: int, inventory: dict[str, str]) -> None:
    with destination.open("wb") as raw_out:
        with gzip.GzipFile(filename="", mode="wb", compresslevel=9, fileobj=raw_out, mtime=epoch) as gz:
            with tarfile.open(fileobj=gz, mode="w|", format=tarfile.GNU_FORMAT) as tar:
                root = tarfile.TarInfo(prefix)
                root.type = tarfile.DIRTYPE
                root.mode = 0o755
                tar.addfile(normalize_member(root, epoch))
                usr = tarfile.TarInfo(prefix + "/usr")
                usr.type = tarfile.DIRTYPE
                usr.mode = 0o755
                tar.addfile(normalize_member(usr, epoch))
                directories = {str(PurePosixPath(p).parent) for p in inventory if str(PurePosixPath(p).parent) != "."}
                for rel_dir in sorted(directories, key=lambda x: (x.count("/"), x)):
                    info = tarfile.TarInfo(prefix + "/usr/" + rel_dir)
                    info.type = tarfile.DIRTYPE
                    info.mode = 0o755
                    tar.addfile(normalize_member(info, epoch))
                for rel in sorted(inventory):
                    path = stage_usr / rel
                    info = tar.gettarinfo(str(path), arcname=prefix + "/usr/" + rel)
                    info = normalize_member(info, epoch)
                    with path.open("rb") as src:
                        tar.addfile(info, src)


def verify_native_archive(path: Path, prefix: str, inventory: dict[str, str]) -> None:
    got: dict[str, str] = {}
    with tarfile.open(path, "r:gz") as tar:
        for member in tar:
            if member.name.rstrip("/") == prefix:
                continue
            if not member.name.startswith(prefix + "/"):
                fail("native archive member escaped its declared top directory")
            if member.isfile() and "/usr/" in member.name:
                rel = member.name.split("/usr/", 1)[1]
                stream = tar.extractfile(member)
                if stream is None:
                    fail("native archive contains unreadable file")
                got[rel] = hashlib.sha256(stream.read()).hexdigest()
    if got != inventory:
        fail(f"native archive readback does not match all {len(inventory)} staged file hashes")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--commit", required=True, help="full Git commit to archive as source")
    p.add_argument("--build-commit", required=True, help="full commit used for native stage provenance")
    p.add_argument("--stage", required=True, type=Path, help="staged usr directory")
    p.add_argument("--stage-manifest", required=True, type=Path, help="JSON with the pinned actual_new stage inventory")
    p.add_argument("--output", required=True, type=Path, help="new, absent output directory")
    p.add_argument("--version", required=True, help="candidate archive label, not a public release identity")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    repo = ROOT
    source = git_commit(args.commit)
    build = git_commit(args.build_commit)
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.+_-]{0,63}", args.version):
        fail("version must be a short filename-safe candidate label")
    try:
        epoch = int(os.environ["SOURCE_DATE_EPOCH"])
        if epoch < 0:
            fail("SOURCE_DATE_EPOCH must be a nonnegative integer")
    except (KeyError, ValueError):
        fail("set SOURCE_DATE_EPOCH to a declared nonnegative integer")
    output = args.output.resolve()
    if output.exists():
        fail("output already exists; choose a fresh path")
    stage = args.stage.resolve()
    if not stage.is_dir():
        fail("stage must point to the installed usr directory")
    manifest_path = args.stage_manifest.resolve()
    source_info = inspect_source(source, build)
    native_result = native_inventory(stage, manifest_path, source, source_info["source_tree"])
    inventory = native_result["files"]
    temp_parent = output.parent
    temp_parent.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=".release-assembly-", dir=temp_parent))
    prefix = f"kde-index-{args.version}"
    try:
        source_name = f"{prefix}-source.tar.gz"
        native_name = f"{prefix}-native.tar.gz"
        source_readback = git_archive(repo, source, prefix + "-source", work / source_name, epoch, source_info["source_tree"])
        add_native(stage, prefix + "-native", work / native_name, epoch, inventory)
        verify_native_archive(work / native_name, prefix + "-native", inventory)
        files = {source_name: sha256_file(work / source_name), native_name: sha256_file(work / native_name)}
        receipt = {
            "format": 1,
            "candidate_version": args.version,
            "source_commit": source,
            "native_build_commit": build,
            "source_epoch": epoch,
            "source_tree_tracked_paths": source_readback["tracked_paths"],
            "source_secret_pattern_file_checks": source_readback["secret_pattern_file_checks"],
            "source_workspace_path_reference_files": source_readback["workspace_path_reference_files"],
            "source_home_path_reference_files": source_readback["home_path_reference_files"],
            "native_secret_pattern_file_checks": native_result["secret_pattern_file_checks"],
            "native_license_file_count": native_result["license_file_count"],
            "source_build_commit_changed_paths": source_info["changed_paths"],
            "native_stage_file_count": len(inventory),
            "native_stage_files_sha256": inventory,
            "archives_sha256": files,
            "limits": [
                "Development candidate package; no publication or RC acceptance.",
                "The source package is the original tracked Git archive and retains frozen synthetic validation evidence.",
                "Native archive is the pinned Arch CPU stage; it is not portable to unvalidated distributions.",
                "Archive assembly/readback does not qualify installation, KRunner layout/open, simultaneous clients, or physical desktop presentation.",
                "The native archive contains only the runtime path allowlist and notices present in the selected source commit.",
                "System shared libraries are supplied by the operating system and are not bundled.",
            ],
        }
        (work / "package-receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
        (work / "SHA256SUMS").write_text("".join(f"{files[name]}  {name}\n" for name in sorted(files)))
        work.rename(output)
    except BaseException:
        shutil.rmtree(work, ignore_errors=True)
        raise
    print(json.dumps({"output": str(output), "archives": files, "source_files": source_readback["tracked_paths"], "native_files": len(inventory)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError, OSError, subprocess.CalledProcessError, tarfile.TarError) as exc:
        print(f"assembly failed: {exc}", file=sys.stderr)
        raise SystemExit(2)
