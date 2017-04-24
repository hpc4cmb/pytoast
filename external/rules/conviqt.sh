curl -SL https://www.dropbox.com/s/ecqjps0nwe132kt/libconviqt-1.0.1.tar.bz2?dl=1 \
    -o libconviqt-1.0.1.tar.bz2 \
    && tar -xjf libconviqt-1.0.1.tar.bz2 \
    && cd libconviqt-1.0.1 \
    && CC="@MPICC@" CXX="@MPICXX@" MPICC="@MPICC@" MPICXX="@MPICXX@" \
    CFLAGS="@CFLAGS@ -std=gnu99" CXXFLAGS="@CXXFLAGS@ -std=c++98" \
    ./configure @CROSS@ --with-cfitsio="@AUX_PREFIX@" --prefix="@AUX_PREFIX@" \
    && make -j 4 && make install \
    && cd .. \
    && rm -rf libconviqt*