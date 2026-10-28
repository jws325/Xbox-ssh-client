# XSSH — original Xbox SSH2 terminal (nxdk)

A **source-code prototype** for an **original Xbox**, not Xbox 360 or Xbox One.
The client uses nxdk (lwIP sockets and SDL2), libssh2 1.11.1 for SSH2, and
Mbed TLS 3.6.5 for cryptography. It is built around a configurable display output and an initially 80×24 7-bit
terminal, **two configurable Xbox-green/black themes**, a controller-operated virtual keyboard, and **direct USB HID boot
keyboard input**. IPv4 networking supports **DHCP or static IP** and displays
the actual Xbox address on screen. SSH password auth and
interactive PTY shells are supported; no SFTP or port forwarding.

**Status:** Application source and dependency integration are included, but
**a real nxdk cross-build and Xbox hardware/emulator test were not possible in
this environment**. Treat as an initial port rather than a verified XBE.

**Full configuration reference:** `xssh.cfg.EXAMPLE.txt` lists every supported
`xssh.cfg` option, allowed values, and commented examples. Copy it to
`xssh.cfg` and edit it before deploying.

## Build

1. Set up a current [XboxDev/nxdk](https://github.com/XboxDev/nxdk), including
   its recursive submodules, and ensure its LLVM tools are on PATH (run
   `source /path/to/nxdk/bin/activate`, or build inside its activated shell).
2. In this XSSH folder, execute `./tools/fetch-deps.sh`. It fetches pinned
   versions of libssh2 and Mbed TLS. Their licenses remain with their source.
3. Run `make NXDK_DIR=/path/to/nxdk`. The Makefile enables nxdk's USB HID
   class driver (`NXDK_USB_ENABLE_HID=y`) and builds the SDL joystick backend
   with `SDL_DISABLE_JOYSTICK_INIT_DELAY` so the HID driver can be registered
   **before** first USB enumeration. **Build against an nxdk tree where SDL2
   is freshly built with that flag** (if the SDL library was compiled previously,
   clean/rebuild the nxdk SDL2 objects before running XSSH's build). The Makefile compiles both crypto and
   SSH2 source under the nxdk toolchain and creates `bin/default.xbe` and
   `XSSH.iso` if the port compiles successfully. If your nxdk revision differs,
   you may need to adapt C flags/feature checks or the third-party source list.
4. Deploy `bin/default.xbe` and an edited `xssh.cfg` into the *same directory*
   on the Xbox (`D:\xssh.cfg` from the executable). You need a homebrew-capable
   original Xbox connected by wired Ethernet.
5. On a trusted PC run `python3 tools/make-seed.py`. Transfer the resulting
   **secret** `xssh-seed.bin` into `E:\xssh-seed.bin` on the Xbox. **Do not
   distribute this file with an XBE or ISO, expose it publicly, or use the
   same file on multiple consoles.** `E:` must be writable; the application mounts the E: partition and
   replaces its seed with a SHA-512-derived successor before each session.

## Video output configuration (new)

Edit **`video_mode`** in `D:\xssh.cfg` and restart the XBE. This selects the
actual Xbox output mode **before SDL initializes**. Available modes are
queried through nxdk HAL `XVideoListModes()` (not simply assumed from a
preset table), then applied using `XVideoSetMode()`. The application creates
its SDL2 window at the chosen physical resolution. `SDL_RenderSetLogicalSize`
keeps the **640×480 logical UI** readable at every
output resolution, with letterboxing when necessary. The default font retains an
80×24 terminal grid; configurable font sizes change the grid.

| Value | Meaning |
|---|---|
| `video_mode=auto` | **Default: inherit the active display resolution at XSSH startup** (normally the dashboard/launcher resolution when preserved through XBE launch), subject to nxdk HAL support |
| `video_mode=480i` | 640×480 or 720×480 interlaced (subject to nxdk filtering) |
| `video_mode=480p` | 640×480 or 720×480 progressive |
| `video_mode=720p` | 1280×720 progressive |
| `video_mode=1080i` | 1920×1080 interlaced |
| `video_mode=800x600@60` | Exact arbitrary WxH at 60 Hz (e.g. VGA / custom BIOS) |
| `video_mode=1024x768p@60` | WxH with progressive scan requested |
| `video_mode=1920x1080@30` | Custom timing, only if **nxdk HAL** lists that resolution and refresh |

Additional forms: `720x480@60`, `640x480i@50`, `1440x900@60`, and any
`WxH[@Hz]` and `720p@60`, `1080i@60` for compatible HAL-advertised modes. For a custom `WxH@Hz` mode,
leave off `i` or `p` when its scan type is not disclosed by nxdk's public
`VIDEO_MODE` struct. Modes are probed at integer 24–240 Hz; the HAL does not
provide noninteger/fractional refresh enumeration.

**Automatic startup inheritance:** For `video_mode=auto`, XSSH reads the active
NV2A scanout width/height **before** calling `XVideoSetMode()` or initializing
SDL. It matches these dimensions to a mode enumerated by nxdk's HAL, using the
startup refresh rate when known. Thus a dashboard running at 720p/1080i will
start XSSH at that resolution **if the launch preserves the mode** and the HAL
supports it. `auto` does **not** select the lowest/640×480 mode by default.

If the inherited resolution cannot be read, is missing from the HAL's mode
list, or mode initialization fails, XSSH reports a warning and only then
attempts an enumerated safety fallback (normally 640×480). The selected output
resolution appears at the top of the screen. A fallback cannot guarantee that
a TV will display the signal if its video cable/adapter cannot carry it. Video
mode selection happens at startup; XSSH does not switch live.

**Launch-time limitation:** Some dashboards, custom BIOSes, or softmod launch
paths reset the video hardware *before* XSSH can read it. In that case the
original dashboard mode is no longer recoverable from the live NV2A registers;
`auto` can inherit only the mode actually active when XSSH starts. nxdk's
`XVideoGetMode()` alone cannot recover the previous dashboard mode because
stock nxdk tracks only modes set in the current process. Explicitly choose
`video_mode=720p`, `1080i`, or another supported mode if needed.

**Important limitations:**

- The stock nxdk HAL determines 480i versus 480p for HDTV/component output
  using the Xbox video settings. When 480p is enabled in the dashboard, its
  `XVideoListModes()` **hides** component 480i. To select component 480i,
  disable 480p in the Xbox video settings first; XSSH does not bypass HAL
  validation or reprogram GPU encoder registers directly.
- For 720p/1080i, use a compatible HD video adapter and enable those modes
  in the dashboard/firmware. A PAL system may require an appropriate BIOS
  or region/video configuration. 1080i and high-resolution custom modes
  consume substantially more of the original Xbox's 64 MB RAM and may be slow.
- **Custom BIOS modes are supported only when the installed nxdk HAL
  enumerates them.** A custom BIOS alone does not add modes to nxdk's
  stock hardcoded HAL mode table; a compatible fork/patch of `lib/hal/video.c`
  may be necessary. The public HAL enumeration does not report interlace flags
  for non-standard modes, so specify custom modes as `WxH@Hz`.
- `576i` is accepted as a named selection but only works if a HAL fork
  actually advertises 720×576 at 50 Hz. Stock nxdk may expose PAL-50 as
  640×480 or 720×480 instead, not 720×576.
- `auto` preserves the **starting scanout dimensions**, rather than choosing
  the smallest or largest supported mode. It does not infer a TV's capabilities.
  Custom timing/refresh may not be recoverable from the HAL's public API.

## Appearance themes (Xbox / Inverted Xbox)

Edit `theme` in `D:\xssh.cfg` and restart XSSH:

```ini
# Bright Xbox-green (#9BF00B) text on pure black (default)
theme=xbox
```

Or select the opposite colors:

```ini
# Pure black text on bright Xbox-green (#9BF00B)
theme=inverted_xbox
```

Both themes cover the terminal, title bar, connection/status messages, footer,
password prompt, and on-screen keyboard. The currently selected controller key
reverses its colors. ANSI foreground color sequences from the remote SSH shell
are rendered in the selected foreground for a consistent monochrome look;
ANSI cursor movements and clearing still work. The software keyboard still
hides when a USB HID keyboard is connected. An unknown theme value is reported
as a configuration error instead of silently being ignored.

## Terminal font size

Set `font_size=` in `D:\xssh.cfg` and restart XSSH:

```ini
font_size=12
```

`12` is the default and preserves the original **8×12 bitmap glyphs / 80×24 terminal**.
Supported integer values are **8–24** inclusive; for example `font_size=8`
displays smaller glyphs and more terminal rows/columns, while
`font_size=18` displays larger glyphs and fewer cells. Invalid values are
reported as configuration errors rather than silently clamped.

The setting changes **SSH terminal text only**, not the surrounding title,
status/footer, or controller-operated on-screen keyboard. Glyphs are scaled
from the same bitmap atlas (no additional font dependencies), and the
terminal renderer and SSH PTY request automatically use the new grid size.
The 640×480 logical canvas is still scaled to `video_mode`; changing
`font_size` does not change the actual Xbox video output resolution.

## Xbox network configuration

XSSH's `D:\xssh.cfg` defaults to:

```ini
network_mode=dhcp
```

- **`dhcp`** uses nxdk's DHCP client (`nxNetInit(NULL)`) and updates the IP
  readout when the lease arrives. The DHCP server may assign the dashboard's
  previous address, but this is **not guaranteed**. To keep the same address,
  create a DHCP reservation for the Xbox's Ethernet MAC on your router.
- **`static`** stops DHCP and applies the explicitly configured Xbox IPv4
  address, netmask and gateway to lwIP's existing Ethernet interface.

For a fixed Xbox address, set the following values to ones that fit your LAN:

```ini
network_mode=static
network_ip=192.168.1.50
network_netmask=255.255.255.0
network_gateway=192.168.1.1
```

`network_ip` is the **Xbox's own** IP address. `host=` still refers to the
**remote SSH server**. Do not assign an address that another device already
uses. `network_gateway=0.0.0.0` is permitted if the SSH server is reachable on
an isolated local subnet and no default gateway is needed. Static addresses
and masks are checked strictly at startup; invalid settings fail closed rather
than silently switching to DHCP.

The footer displays **`XBOX IP: 0.0.0.0`** while DHCP has not assigned an
address, then updates with the actual address (for example,
`XBOX IP: 192.168.1.73`) at roughly half-second intervals. SSH login is
blocked until an IP is assigned. Startup also logs the network mode and
initial IP; the footer remains visible while connected. The network interface
is initialized even if the SSH entropy seed is missing, so you can inspect
network status while troubleshooting.

**Important:** An XBE does not automatically inherit the homebrew or retail
Dashboard's *in-memory* IP settings. Some dashboards store their own settings
in proprietary files, not shared with nxdk. Therefore **`dhcp` requests an
address from the DHCP server; it does not import dashboard settings**. If the dashboard used a static IP,
copy its address/mask/gateway into the `static` settings above, or create a
DHCP reservation. The MAC address stays the Xbox Ethernet hardware's MAC;
XSSH does not change it. This client currently accepts numeric IPv4 server
addresses only, so DNS settings are not needed for its SSH connection.

No full nxdk cross-build/hardware validation has been performed here. The
portable network parser and mocked nxdk/lwIP adapter are tested natively; if
using a forked nxdk with different network APIs, adjust `src/network_xbox.c`
as needed.

## Host-key pinning and connections

The `xssh.cfg` contains `host` (numeric IPv4), `port`, `user`, and
`hostkey_sha256`. No password is stored on disk. Leave `hostkey_sha256` blank
for the first attempt: the console performs the SSH handshake, displays the
SHA-256 hex of the negotiated **raw host-key blob**, and **refuses to log in**.

**Verify that fingerprint independently** against the server you intended to
reach. For example, compare against an authenticated copy of the server's
`/etc/ssh/ssh_host_ecdsa_key.pub`, or another trusted SSH host-key source,
ensuring the same key algorithm is negotiated. The fingerprint on the console
is SHA-256 **hex**, not the OpenSSH `SHA256:` base64 presentation. For a trusted
OpenSSH public-key file on a PC, compute the hex fingerprint as:

```sh
python3 - /path/to/ssh_host_ecdsa_key.pub <<'PY'
import base64, hashlib, pathlib, sys
line = pathlib.Path(sys.argv[1]).read_text().strip().split()
print(hashlib.sha256(base64.b64decode(line[1])).hexdigest())
PY
```

Set the 64 hex characters in `hostkey_sha256=...`, replace `D:\xssh.cfg`,
and relaunch. The client will authenticate only if the pinned hash matches.
It does **not** trust changed keys automatically. RSA and NIST ECDSA keys
supported by the bundled libssh2/Mbed TLS configuration are expected; Ed25519
is **not** available in this chosen crypto backend.

## USB keyboard support (new)

Use a **wired USB HID Boot Protocol keyboard** with a **female USB-A to original
Xbox controller-port adapter**. The Xbox controller connector is electrically
USB, but not physically USB-A. The keyboard needs no PC and no hub if it fits
one Xbox port through the adapter. A USB 1.1-compatible keyboard is the safest
choice; some gaming/NKRO keyboards expose only proprietary report interfaces
and will not work.

The keyboard is read by nxdk's **OHCI USB HID class driver**, not by SDL's
keyboard events. The Xbox SDL2 backend does not turn HID keyboard reports into
`SDL_KEYDOWN`/`SDL_TEXTINPUT`, so XSSH registers `usbh_hid_init()` after SDL's
joystick initializes USB (and **before** SDL's first hub enumeration). The
Makefile sets `SDL_DISABLE_JOYSTICK_INIT_DELAY` for this. Connect the keyboard
before startup or hot-plug it while XSSH runs. The bottom of the screen says
`USB KB READY` when its interrupt-in pipe is active; `USB KB NOT FOUND`
otherwise. **The on-screen keyboard automatically disappears when a USB
keyboard is detected and returns when that keyboard is disconnected.** The
terminal, status bar, and network footer stay visible in either state. The
controller's virtual-key selection (D-pad, A, Y) is suspended while the USB
keyboard is active; B (Enter), X (Backspace), Start (Ctrl-C), and Back
(disconnect) continue to work.

The physical keyboard uses a **US ANSI / 7-bit ASCII layout**, including
Shift, Caps Lock, Space, Enter, Backspace, Tab, Esc, letters, numbers, and
punctuation. Supported SSH terminal keys also include:

- Ctrl+A through Ctrl+Z (including Ctrl+C, Ctrl+D, Ctrl+L), plus several
  Ctrl+punctuation combinations; left and right Ctrl/Shift/Alt.
- Alt+character as an ESC prefix (Meta), and Shift+Tab as `ESC [ Z`.
- Arrow keys, Home, End, Insert, Delete, Page Up/Down, F1–F12 using common
  xterm-compatible ANSI sequences; keypad numbers/operators/Enter.
- Key hold repeat after 440 ms, then approximately every 45 ms.

**Limitations:** USB HID Boot Protocol keyboard only; not arbitrary NKRO or
Bluetooth devices; US layout only, no Unicode or IME, CapsLock LED control,
keypad navigation mode, or layout selection. Only the first eligible attached
boot keyboard is active. Physical keyboard characters for the password prompt
are restricted to printable ASCII; password is still never saved in the config.

## Controller controls

- **D-pad:** choose an on-screen key; **A:** type selected key (only when no USB keyboard is connected).
- **B:** send Enter (or submit password); **X:** Backspace.
- **Y:** toggle upper/lowercase of the on-screen keyboard (when visible); **Start:** Ctrl-C; **Back:** disconnect.
- The on-screen keyboard has SPACE, ENTER, BACK, TAB, ESC, and CTRL-C shortcuts.
- The SDL key-event path is retained for emulator/backends that support it,
  but the real Xbox USB keyboard uses the direct HID driver above.

## Native unit tests

Test the portable HID report decoder, mocked USB host integration, and the
terminal parser on a PC with `./tools/test-keyboard.sh` and video mode logic
with `./tools/test-video.sh`. Network tests are in
`./tools/test-network.sh`. Font-size and terminal-grid tests are in
`./tools/test-font.sh`. Or manually:

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/hid_keys.c tools/test-hid-keys.c -o /tmp/xssh-hid-test
/tmp/xssh-hid-test
cc -std=c11 -Wall -Wextra -Werror -Itools/test-stubs -Iinclude \
  src/hid_keys.c src/usb_keyboard.c tools/test-usb-keyboard.c -o /tmp/xssh-usb-test
/tmp/xssh-usb-test
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/terminal.c tools/test-terminal.c -o /tmp/xssh-term-test
/tmp/xssh-term-test
./tools/test-video.sh
./tools/test-network.sh
./tools/test-font.sh
```

## Security and feature limitations

- The original Xbox does not expose a known OS cryptographic entropy API.
  Rather than silently seed a PRNG from `rand()`/clock, XSSH requires a
  64-byte secret generated by the PC OS CSPRNG. A persistent, SHA-512-derived
  seed ratchet reduces repeats on **normal** boots. Disk image rollback,
  plaintext seed disclosure, or state cloning can still compromise secrecy.
  This is a hobbyist design, **not a security-audited SSH implementation**.
- SSH host-key **pinning is mandatory**. No trust-on-first-use and no silent
  downgrade to plaintext Telnet. Password is entered at runtime and wiped
  after the auth attempt, but remains accessible in memory during entry.
- Only one IPv4 SSH connection at a time. Blocking connect/handshake/auth may
  pause the screen; interactive SSH data uses nonblocking reads/writes.
- The emulator handles common ASCII/ANSI cursor movements, color, clear,
  and scrolling. It is **not** a full xterm emulator: no Unicode, insert/delete
  line, alternate screen, bracketed paste, or live terminal resize. A grid
  size is chosen at startup from `font_size`. Full-screen
  applications like vim, htop, tmux may render incorrectly.
- Use a network you control; never enter your primary sensitive credentials
  into an unverified prototype. Some servers may reject the limited algorithm
  set or only offer public-key authentication.

## Files

- `src/main.c`: network configuration, libssh2 authentication, SDL2 screen/controller UI,
  input routing from HID to SSH.
- `src/network_config.c` / `include/network_config.h`: safe IPv4 and network-mode parsing.
- `src/network_xbox.c` / `include/network_xbox.h`: DHCP/static lwIP integration and live IP display.
- `tools/test-network.sh`: portable network parser and mocked nxdk network adapter tests.
- `src/theme.c` / `include/theme.h`: green/black monochrome themes and config parsing.
- `tools/test-theme.sh`: portable tests of both themes, defaults, and ANSI foreground override.
- `src/usb_keyboard.c` / `include/usb_keyboard.h`: nxdk USB HID connection,
  interrupt reports, lock-free queue for USB callback/main-loop handoff.
- `src/hid_keys.c` / `include/hid_keys.h`: US boot-keyboard report decoder,
  ANSI/ASCII translation, and key repeat, testable without nxdk.
- `src/terminal.c` / `include/terminal.h`: compact ANSI interpreter with a configurable grid.
- `src/font_size.c` / `include/font_size.h`: strict font-size setting and terminal geometry.
- `tools/test-font.sh`: native font-size/layout and terminal-grid tests.
- `src/video_modes.c` / `include/video_modes.h`: portable, tested mode parsing,
  startup-resolution matching, and emergency fallback.
- `src/video_xbox.c` / `include/video_xbox.h`: nxdk video HAL mode enumeration,
  mode application, 480i/480p filtering, and selected mode diagnostics.
- `tools/test-video.sh`: native tests using a mocked nxdk HAL.
- `src/rng.c` / `include/rng.h`: persisted seed-ratchet and Mbed TLS entropy hook.
- `include/font8x12.h`: compiled ASCII glyph masks derived from DejaVu Sans Mono
  (see [DejaVu fonts license](https://dejavu-fonts.github.io/License.html)).
- `include/mbedtls_config_xbox.h`: crypto-only Mbed TLS configuration.
- `include/libssh2_config.h`: libssh2 platform features.
- `tools/*`: pinned upstream fetcher, PC random-seed generator, font generator.
