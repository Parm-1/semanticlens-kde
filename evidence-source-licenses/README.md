# Licenses for retained reference-source evidence

The evidence tree contains upstream Qt, KDE, portal and Linux source snapshots
used to inspect actual producer/consumer contracts. They retain original headers,
copyright statements and SPDX identifiers. They are reference material, not
linked into KDE Index by its build. `source-identifiers.json` lists SPDX headers
found in the first forty lines of tracked textual evidence files at collection;
it is a bounded header inventory rather than a complete license audit.

The GNU GPL 2 and LGPL 2.0, 2.1 and 3 texts here were retrieved byte-for-byte from
the official GNU URLs recorded with hashes in `sources.json`. The project root
`LICENSE` contains GPL 3. Source files with alternative SPDX expressions retain
those choices; these copies do not change their original licenses. A commercial
or KDE alternative is not selected merely by recording its SPDX expression.

The four copied system binaries are excluded from the public export by its exact
profile; their internal proof remains unchanged. This source-reference directory
is included in the public source snapshot but not installed as application
runtime material. Application and CPU-worker notices remain under `LICENSES/`
and `THIRD_PARTY_NOTICES.md`. Preserve the reference headers, these texts and the
source identity records if redistributing the evidence subset.
