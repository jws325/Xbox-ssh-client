# XSSH — original Xbox SSH2 terminal (nxdk)

## Features:

Full color support\
Unicode support\
Native Xbox resolution supported\
Antialiased text\
Multiple fonts\
Video screensaver support\
Xbox LED control supported\
Exit, reboot, or shutdown from app\
USB keyboard supported\
and more!

Currently, only password authentication is supported.

## Shortcuts

Control + a (toggle antialiasing)\
Control + f (switch font)\
Control + = (increase font size)\
Control + - (decrease font size)

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

## If you are worried about security, please use https://github.com/jws325/Xbox-ssh-client/blob/main/tools/make-seed.py to generate a unique xssh-seed.bin

## Screenshots

<img width="1170" height="1047" alt="cmatrix" src="https://github.com/user-attachments/assets/bcb856cd-ef7f-45f3-9531-910a482ec0c8" />

<img width="1170" height="1156" alt="btop" src="https://github.com/user-attachments/assets/a766eee2-4971-4ed4-a661-8380263ef60f" />

<img width="1170" height="1204" alt="vim" src="https://github.com/user-attachments/assets/1a77cd61-713b-4eb7-b55a-1019f175a275" />



