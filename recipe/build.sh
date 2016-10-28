./bootstrap

if [ "$(uname)" == Darwin ]; then
    export CFLAGS="-headerpad_max_install_names $CFLAGS"
fi

export CFLAGS="-I$PREFIX/include $CFLAGS"
export LDFLAGS="-L$PREFIX/lib $LDFLAGS"

./configure --prefix="$PREFIX" \
            --enable-python PYTHON="$PREFIX/bin/python"

make
make install
