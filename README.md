# MacOS 11 Haskell Work-around

A work around for GHC bug #18446 in macOS 11 ("Big Sur").

Details at: https://gitlab.haskell.org/ghc/ghc/-/issues/18446
The bug will be fixed in GHC 9.0.1, 8.10.3, and 8.8.5 which are not currently released.

This work-around should allow using GHC in macOS 11 until the bug is fixed,
and also with older GHC versions after it is fixed (in case one needs to use an older version).

Build:

    clang -target x86_64-darwin -dynamiclib macos11ghcwa.c -o macos11ghcwa.dylib

Use:

    DYLD_INSERT_LIBRARIES=<PATH>/macos11ghcwa.dylib stack args
