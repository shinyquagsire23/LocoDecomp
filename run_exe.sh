#!/bin/zsh
cp $(pwd)/build/Loco-port.exe $(pwd)/loco/Loco-port.exe
cp $(pwd)/build/Loco-port.ilk $(pwd)/loco/Loco-port.ilk
pushd loco
# NOTE: do NOT pre-warm a persistent wineserver here — a `wineserver -p` started in a headless
# context leaves the bottle with a windowless server that later GUI runs inherit (no window).
# wineserver persistence lives only in the build wrappers (toolchain/bin/{cl,link}, ~/.wine).
#PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" /Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle "General" $(pwd)/loco.exe ;
PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin:/usr/bin:/bin:/usr/sbin:/sbin" DYLD_LIBRARY_PATH="/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64:/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib32on64" /Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine --bottle "General" $(pwd)/Loco-port.exe ;
popd