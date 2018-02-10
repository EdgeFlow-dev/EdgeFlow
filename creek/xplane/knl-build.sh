# -- on knl ---
rm CMakeCache.txt
/usr/bin/cmake -DCMAKE_ENV=knl -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - Unix Makefiles" ${HOME}/xplane_nw
make -j60
