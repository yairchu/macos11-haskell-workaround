# MacOS 11 Haskell Work-around

A work around for a bug in older GHCs (<= 8.10.3) on macOS 11 "Big Sur" (bug #18446).

Details at: https://gitlab.haskell.org/ghc/ghc/-/issues/18446

This work-around should allow using affected GHC versions in macOS 11.

Build:

    clang -target x86_64-darwin -dynamiclib macos11ghcwa.c -o macos11ghcwa.dylib

Use:

    DYLD_INSERT_LIBRARIES=<PATH>/macos11ghcwa.dylib stack args

To test one can use (with and without the fix):

    stack exec -- runghc --ghc-arg="-framework OpenGL" Test.hs

    DYLD_INSERT_LIBRARIES=`pwd`/macos11ghcwa.dylib stack exec -- runghc --ghc-arg="-framework OpenGL" Test.hs
