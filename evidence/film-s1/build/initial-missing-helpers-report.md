# Combined source build v1

Fresh source/runtime/build tree at public commit e2bdfa4316af30f1f28162efd05372ec88f76071 configured and built successfully on the target host. Runtime came from a separate local pinned-source clone and passed the original setup script/patch check. The staged installation was incomplete: 22 files, without film tokenizer/worker, because install rules inside the excluded probe subdirectory did not enter the top-level install. Preserve this as a failed packaging attempt. No live installation, UI or model test ran.
