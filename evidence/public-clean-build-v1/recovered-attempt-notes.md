# Harness repair notes

The first disposable attempts stopped before build for harness-only reasons: the receipt path was relative to the wrong directory, the outer path with a space was quoted incorrectly, and the retry initially encountered its own already-extracted source directory. Each was corrected within this new run's owned subtree; the final `run.log` is the successful full build/install. No project source file was changed.
