# Baloo baseline controls v1

Observed 2026-10-11 on Baloo 6.30.0. This is a control of the comparison method,
not a KDE Index retrieval benchmark or a general claim of semantic improvement.

Run from the repository root:

```sh
python tools/baloo_baseline.py --output evidence/baloo-baseline-v2
```

Use a fresh output directory. The helper creates two synthetic text files under
ignored `local/`, writes a separate `baloofilerc`, and explicitly sets its own
`BALOO_DB_PATH`. Bubblewrap makes the host filesystem read-only, allows writes
only in that sandbox, isolates the PID/network namespaces, and starts a separate
D-Bus session. The folder configuration selects only those synthetic files.

Both files reached `status=done, indexing=content` through the JSON status API.
`balooshow6 -x` independently showed their content tokens. Seven controls passed:
filename lookup, text lookup, implicit AND, explicit OR, ordered quoted phrase,
`content:` field lookup and a paraphrase with no matching indexed terms.
All eleven raw queries and result identities are in `results.json`.

The whole sandbox run took 6.301 seconds, including startup and completion waits;
that is not an indexing throughput or query latency benchmark. Each query result
records both subprocess elapsed time and Baloo's own reported search time.
The owned daemon was terminated and waited for (exit -15); its enclosing PID
namespace and session command exited with code 0. No control processes remained
when the parent checked the process list. Empty logs are retained as observed.

The installed commands were checked against KDE/baloo tag `v6.30.0`, commit
`a5b829873849ce72eb35d2c1b48337b2d81a0593`, particularly:

- [database path selection](https://github.com/KDE/baloo/blob/a5b829873849ce72eb35d2c1b48337b2d81a0593/src/engine/global.cpp)
- [configuration keys](https://github.com/KDE/baloo/blob/a5b829873849ce72eb35d2c1b48337b2d81a0593/src/lib/baloosettings.kcfg)
- [query parsing](https://github.com/KDE/baloo/blob/a5b829873849ce72eb35d2c1b48337b2d81a0593/src/lib/advancedqueryparser.cpp)
- [query/result CLI](https://github.com/KDE/baloo/blob/a5b829873849ce72eb35d2c1b48337b2d81a0593/src/tools/baloosearch/main.cpp)
- [per-file content state](https://github.com/KDE/baloo/blob/a5b829873849ce72eb35d2c1b48337b2d81a0593/src/tools/balooctl/statuscommand.cpp)

Frozen SHA-256 pins:

```text
614df457a20666b57e88f7e2abe28f2d86b925ba19c0ed9eb75138a47b391681 tools/baloo_baseline.py
67875b0fa7653628de87993d20e18ebabc1ca97f5f784debcc5b7bbb87b6616a results.json
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 daemon.log
e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 session.log
```

Limitations: only two text files, no PDF/image extraction coverage, no production
desktop integration, no comparison with our model, and no evidence that future
versions preserve these behaviors. Document IDs are run-local filesystem
identities; corpus-relative paths are the useful comparison keys. The final
benchmark must freeze more representative data and relevance labels before tuning.
