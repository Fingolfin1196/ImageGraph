with import <nixpkgs> {};


(overrideCC stdenv clang_9).mkDerivation {
  name = "ImageGraph";

  src = ./.;

  nativeBuildInputs = [
    cmake
    pkg-config
  ];

  cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];

  postInstall = ''
    cp -R ../dependencies $out/include/
  '';

  buildInputs = [
    abseil-cpp
    boost.dev
    fftw.dev
    glib.dev
    gsl
    imagemagick.dev
    libexif
    libgsf.dev
    openexr.dev
    orc.dev
    pcg_c
    pcre.dev
    tbb
    vips
  ];
}
