{
  description = "Scraper for mumble UDP ping protocol";

  outputs = { self, nixpkgs }: {

    overlay = final: prev: {
      mumble-ping = with final; (stdenv.override {
        cc = pkgs.musl;
      }).mkDerivation {
        name = "mumble-ping";
        src = self;
        nativeBuildInputs = [ meson ninja ];
        buildInputs = with pkgs; [ musl gcc ];
        CC = "musl-gcc";
        CFLAGS = "-static";
      };
    };

    packages.x86_64-linux.mumble-ping = (import nixpkgs {
      system = "x86_64-linux";
      overlays = [ self.overlay ];
    }).mumble-ping;

    defaultPackage.x86_64-linux = self.packages.x86_64-linux.mumble-ping;
  };
}
