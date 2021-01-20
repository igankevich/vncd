(packages->manifest
  (append
    (list
      (@ (gnu packages build-tools) meson)
      (@ (stables packages ninja) ninja/lfs)
      (@ (gnu packages check) googletest)
      (@ (gnu packages pkg-config) pkg-config)
      (@ (gnu packages pre-commit) python-pre-commit)
      (@ (gnu packages python-xyz) python-chardet)
      (@ (gnu packages elf) elfutils)
      (@ (gnu packages compression) xz)
      (@ (gnu packages unistdx) unistdx)
      (list (@ (gnu packages llvm) clang-10) "extra") ;; clang-tidy
      (@ (gnu packages valgrind) valgrind)
      (list (@ (gnu packages gcc) gcc) "lib")
      (@ (gnu packages commencement) gcc-toolchain))))
