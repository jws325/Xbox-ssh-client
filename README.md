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
MP3 player\
Key or Password based authentication\
Numpad drumming\
Custom .bmp wallpapers\
Custom screen margins\
GPU acceleration\
and more!

## Shortcuts

Control + f (switch font)\
Control + = (increase font size)\
Control + - (decrease font size)\
Control + t (switch theme)\
Control + p (Start or Stop MP3 player)\
Control + Left Arrow (go back one song)\
Control + Right Arrow (go forward one song)\
Control + Shift + Left Arrow (rewind song)\
Control + Shift + Right Arrow (fast forward song)\
Control + Spacebar (Enable or Disable numpad drumming)\
Numpad + (Increase drum volume)\
Numpad - (Decrease drum volume)\
Control + Shift + B (Switch wallpapers)\
Control + Shift + O (Change wallpaper opacity)\
Control + Shift + M (Change screen margins)

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

## Install by dragging all files in release to E:\Apps\XSSH

## If you are worried about security, please use https://github.com/jws325/Xbox-ssh-client/blob/main/tools/make-seed.py to generate a unique xssh-seed.bin

## 1080i performance is not yet optimized. 
All other resolutions work ok.\
480p 4:3 is my preferred resolution and is the most heavily tested.

## Screenshots

<img width="859" height="827" alt="Screenshot 2026-11-24 6 27 22 PM" src="https://github.com/user-attachments/assets/33311d2b-b9b4-4e9f-ae5b-83ebfa0b3e64" />

<img width="859" height="842" alt="Screenshot 2026-11-24 6 28 01 PM" src="https://github.com/user-attachments/assets/ec07af43-377d-4baa-984c-8be3ced5c939" />

<img width="858" height="817" alt="Screenshot 2026-11-24 6 28 26 PM" src="https://github.com/user-attachments/assets/c87e7045-7e8f-4ca9-98b5-5f392fbb21aa" />

<img width="865" height="781" alt="Screenshot 2026-11-24 6 29 33 PM" src="https://github.com/user-attachments/assets/3a2e2085-1deb-4d93-a828-1accd458a3a9" />

<img width="1170" height="1047" alt="cmatrix" src="https://github.com/user-attachments/assets/bcb856cd-ef7f-45f3-9531-910a482ec0c8" />

<img width="1170" height="1156" alt="btop" src="https://github.com/user-attachments/assets/a766eee2-4971-4ed4-a661-8380263ef60f" />

<img width="1170" height="1204" alt="vim" src="https://github.com/user-attachments/assets/1a77cd61-713b-4eb7-b55a-1019f175a275" />
