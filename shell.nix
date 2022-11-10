with import <nixpkgs> {};
let coda = stdenv.mkDerivation {
  pname = "coda";
  version = "2.24";
  src = fetchurl {
    url = "https://github.com/stcorp/coda/releases/download/2.24/coda-2.24.tar.gz";
    sha256 = "b27073b645b6334a6bd9c5e74912137bfd4bdfd96d475c2de7fc39ef7935128a";
  };
  buildInputs = [ python ];
};
in
stdenv.mkDerivation {
  name = "harpenv";
  buildInputs = [ coda libtool flex bison hdf4 hdf5 zip python39 python39Packages.numpy python39Packages.cffi ];
}

