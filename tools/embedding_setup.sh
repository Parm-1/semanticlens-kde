#!/usr/bin/env bash
# Public source/model downloads only; no package installation. Run at repository root.
set -euo pipefail
mkdir -p vendor models build-probe
GIT=/usr/bin/git
if [[ ! -d vendor/llama.cpp/.git ]]; then
  "$GIT" -c advice.detachedHead=false clone --depth 1 --branch b11461 https://github.com/ggml-org/llama.cpp vendor/llama.cpp
fi
[[ $("$GIT" -C vendor/llama.cpp rev-parse HEAD) == 4d756bc72bf00a4aacf410ae15a2d315f3db400d ]]
patch="$PWD/evidence/embedding-v1/source/llama.cpp-no-logits.patch"
if "$GIT" -C vendor/llama.cpp apply --check "$patch" 2>/dev/null; then "$GIT" -C vendor/llama.cpp apply "$patch"; fi
"$GIT" -C vendor/llama.cpp apply --reverse --check "$patch"
[[ $("$GIT" -C vendor/llama.cpp diff | sha256sum | cut -d' ' -f1) == $(sha256sum "$patch" | cut -d' ' -f1) ]]
[[ $("$GIT" -C vendor/llama.cpp status --porcelain --untracked-files=all) == " M src/llama-context.cpp" ]]
python3 - <<'PY'
import hashlib,urllib.request
from pathlib import Path
revision='bfcd298762cc34d0357ece5ebdd31791a3a374d8'
files=[('embeddinggemma-2-Q8_0.gguf',309855456,'2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135'),('mmproj-embeddinggemma-2-Q8_0.gguf',554821024,'c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f')]
def verify(path,size,digest):
 if not path.exists() or path.stat().st_size!=size:return False
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(8<<20),b''):h.update(chunk)
 return h.hexdigest()==digest
for name,size,digest in files:
 path=Path('models')/name
 if not verify(path,size,digest):
  part=path.with_suffix(path.suffix+'.part')
  urllib.request.urlretrieve(f'https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/{revision}/{name}',part)
  if not verify(part,size,digest):raise RuntimeError(f'{name}: download integrity failed')
  part.replace(path)
 print(name,digest)
PY
cmake -S probe -B build-probe -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-probe -j 4 --target embedding-probe sizing-check
