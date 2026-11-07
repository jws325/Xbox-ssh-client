# XSSH — original Xbox SSH2 terminal (nxdk)

## To build from source:

```sh
./tools/fetch-deps.sh    # only required if dependencies are not already present
./tools/build-verified-release.sh
```

For a manual build, the equivalent commands are:

```sh
eval "$("$HOME/nxdk/bin/activate" -s)"
make -B NXDK_DIR="$HOME/nxdk" -j4
python3 tools/verify-built-xbe.py release/default.xbe
sha256sum bin/default.xbe release/default.xbe
```

## Please see xssh.cfg for config options

## Install by dragging all files in release to E:/Apps/XSSH
