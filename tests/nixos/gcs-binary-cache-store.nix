{
  lib,
  config,
  nixpkgs,
  ...
}:

let
  pkgs = config.nodes.client.nixpkgs.pkgs;

  pkgA = pkgs.cowsay;

  storeUrl = "gs://my-bucket";
  objectThatDoesNotExist = "gs://my-bucket/object-that-does-not-exist";
in
{
  name = "gcs-binary-cache-store";
  nodes = {
    server =
      {
        config,
        lib,
        pkgs,
        ...
      }:
      {
        virtualisation.writableStore = true;
        virtualisation.additionalPaths = [ pkgA ];
        environment.systemPackages = [ pkgs.fake-gcs-server pkgs.google-cloud-sdk ];
        nix.extraOptions = ''
          experimental-features = nix-command
          substituters =
        '';
        networking.firewall.allowedTCPPorts = [ 9000 ];

        systemd.services.fake-gcs-server = {
          enable = true;
          description = "Fake GCS Server";
          wantedBy = [ "multi-user.target" ];
          after = [ "network.target" ];
          serviceConfig = {
            ExecStart = "${pkgs.fake-gcs-server}/bin/fake-gcs-server -port 9000 -backend memory";
            Restart = "always";
          };
        };
      };

    client =
      { config, pkgs, ... }:
      {
        virtualisation.writableStore = true;
        nix.extraOptions = ''
          experimental-features = nix-command
          substituters =
        '';
      };
  };

  testScript =
    { nodes }:
    ''
      # fmt: off
      start_all()
      # Create a binary cache.
      server.wait_for_unit("fake-gcs-server.service")
      server.wait_for_open_port(9000)

      server.succeed("nix copy --to '${storeUrl}' ${pkgA}")

      #client.wait_for_unit("network-addresses-eth1.service")

      # TODO: make this work with GCS
      # Test fetchurl on s3:// URLs while we're at it.
      #client.succeed("nix eval --impure --expr 'builtins.fetchurl { name = \"foo\"; url = \"s3://my-cache/nix-cache-info?endpoint=http://server:9000&region=eu-west-1\"; }'")

      # Test that the format string in the error message is properly setup and won't display `%s` instead of the failed URI
      #msg = client.fail("nix eval --impure --expr 'builtins.fetchurl { name = \"foo\"; url = \"${objectThatDoesNotExist}\"; }' 2>&1")
      #if "S3 object '${objectThatDoesNotExist}' does not exist" not in msg:
        #print(msg) # So that you can see the message that was improperly formatted
        #raise Exception("Error message formatting didn't work")

      # Copy a package from the binary cache.
      #client.fail("nix path-info ${pkgA}")

      #client.succeed("nix store info --store '${storeUrl}' >&2")

      #client.succeed("nix copy --no-check-sigs --from '${storeUrl}' ${pkgA}")

      #client.succeed("nix path-info ${pkgA}")
    '';
}
