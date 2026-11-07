# XSSH — original Xbox SSH2 terminal (nxdk)

A **source-code prototype** for an **original Xbox**, not Xbox 360 or Xbox One.
The client uses nxdk (lwIP sockets and SDL2), libssh2 1.11.1 for SSH2, and
Mbed TLS 3.6.5 for cryptography. It is built around a configurable display output and an initially 80×24 7-bit
terminal, **two configurable Xbox-green/black themes**, a controller-operated virtual keyboard, and **direct USB HID boot
keyboard input**. IPv4 networking supports **DHCP or static IP** and displays
the actual Xbox address on screen. An interactive connection form asks for
server IPv4, port, username and password on launch, then opens the SSH session
when the password is submitted. First-use server keys need explicit verification.
Interactive PTY shells are supported; no SFTP or port forwarding.

**Status:** Application source and dependency integration are included, but
**a real nxdk cross-build and Xbox hardware/emulator test were not possible in
this environment**. Treat as an initial port rather than a verified XBE.

**Full configuration reference:** `xssh.cfg.EXAMPLE.txt` lists every supported
`xssh.cfg` option, allowed values, and commented examples. Copy it to
`xssh.cfg` and edit video/network/theme options before deploying.
`host`, `port` and `user` are optional login-form prefills, not requirements.
Passwords are never stored in the config.


## Verify the interactive login build on your Xbox

This revision displays **`XSSH LOGIN v3`** at the upper left at startup, and
**`XSSH-LG3-20261101`** beside the `CONNECT TO SSH SERVER` caption, followed
by SERVER IP / PORT / USERNAME / PASSWORD fields. The build ID is hardcoded
in the compiled binary and checked during release packaging. If your Xbox
does not display **both** the login form and that build ID, it is executing
**an older XBE or an XBE from a different installation path**.

To force a rebuild from **this source tree**, verify its embedded build ID,
and package a fresh XBE, use the single-command helper:

```sh
./tools/fetch-deps.sh    # only required if dependencies are not already present
./tools/build-verified-release.sh
```

It runs `make -B` to recompile all the sources and verifies that
`release/default.xbe` matches `bin/default.xbe` byte for byte. It **deletes
an old `release/default.xbe` before rebuilding**, so a failed build won't leave
a stale executable that could be copied accidentally. It never deletes
or regenerates an existing `release/xssh-seed.bin`.

For a manual build, the equivalent commands are:

```sh
eval "$("$HOME/nxdk/bin/activate" -s)"
make -B NXDK_DIR="$HOME/nxdk" -j4
python3 tools/verify-built-xbe.py release/default.xbe
sha256sum bin/default.xbe release/default.xbe
```

The last command must display identical SHA-256 hashes. The release Make rule refuses to package an executable missing the
release-specific build ID or login-screen markers. This prevents accidentally
copying an executable from the earlier, config-only-login client.
Install **`release/default.xbe`**, **not `main.exe` or an older `bin/default.xbe`
from a different source tree**, and keep your Xbox's existing secret seed when
upgrading. If your dashboard has an app cache or shortcut referencing another
folder, launch the new `default.xbe` directly from its file manager.

The marker test is a stale-binary check, not a substitute for an nxdk build
or runtime tests on physical Xbox hardware. If initialization fails, the
login form remains visible with an explanatory status message.

## Build

1. Set up a current [XboxDev/nxdk](https://github.com/XboxDev/nxdk), including
   its recursive submodules, and ensure its LLVM tools are on PATH (run
   `source /path/to/nxdk/bin/activate`, or build inside its activated shell).
2. In this XSSH folder, execute `./tools/fetch-deps.sh`. It fetches pinned
   versions of libssh2 and Mbed TLS, then patches libssh2 1.11.1 for nxdk's
   missing `<sys/stat.h>`. The patch preserves normal host builds and uses an
   opaque `struct stat` declaration on nxdk; XSSH does not build libssh2's
   SCP or SFTP features. Their licenses remain with their source.
3. Run `make NXDK_DIR=/path/to/nxdk`. The Makefile enables nxdk's USB HID
   class driver (`NXDK_USB_ENABLE_HID=y`) and builds SDL's joystick backend
   with `SDL_DISABLE_JOYSTICK_INIT_DELAY`. If SDL was previously compiled
   without that flag, clean/rebuild the nxdk SDL2 objects first. Compilation
   generates `bin/default.xbe`; the new packaging step also creates `release/`.
4. **Copy all three files from `release/` together** to a writable hard-drive
   folder on your Xbox, such as `F:\Apps\XSSH\`:

   ```text
   release/
       default.xbe
       xssh.cfg
       xssh-seed.bin
   ```

   The native executable is named **`default.xbe`** (not `default.bin`).
   At runtime, the launcher's `D:` mapping points at the XBE's directory;
   XSSH reads `D:\xssh.cfg` and `D:\xssh-seed.bin` there. The 64-byte
   secret is generated automatically on the build PC using `secrets.token_bytes`
   **only when the `release/` seed is missing**. Subsequent `make` runs keep the
   existing release seed instead of silently replacing it.
5. **Keep `xssh-seed.bin` private.** Use a unique build seed per console, never
   publish it in a source or release archive, and do not copy one seed onto
   multiple Xboxes. When updating XSSH on an existing console, copy the new
   `default.xbe` and `xssh.cfg` but **do not overwrite the console's seed**.
   XSSH rewrites its seed during startup as part of the cryptographic ratchet.
   Therefore the application directory must be **writable**: running XSSH from
   a read-only DVD/ISO is unsupported. If your dashboard does not map `D:` to
   the writable launch folder, XSSH reports a seed read/update failure. This
   behavior has not yet been verified on original Xbox hardware.

To assemble the folder again after an incremental build, run:

```sh
make release NXDK_DIR="$HOME/nxdk"
```

To start a *new console installation*, remove the local `release/xssh-seed.bin`
before building to generate a new unique secret; never do this on an Xbox that
has already been using the seed without considering the security consequences.



### Mbed TLS linker fix (`mbedtls_strerror`)

If the nxdk linker reports `undefined symbol: _mbedtls_strerror`, XSSH
needs Mbed TLS's error-reporting implementation as part of its crypto build.
The included Makefile compiles `third_party/mbedtls/library/error.c` and
`include/mbedtls_config_xbox.h` enables `MBEDTLS_ERROR_C`. This provides
Mbed TLS's actual error messages instead of a dummy implementation.

To patch an older source checkout without replacing it:

```sh
python3 tools/fix-mbedtls-strerror.py .
make NXDK_DIR="$HOME/nxdk" -j4
```

The `lld: warning: .edata=.rdata: already merged into .edataxb` warning
is separate and does not prevent linking.

### nxdk / Mbed TLS `sal.h` compatibility

If Mbed TLS 3.6.5 fails on `#include <sal.h>` from
`mbedtls/platform_util.h`, the source config already defines
`MBEDTLS_CHECK_RETURN` as Clang's `warn_unused_result` attribute.
This prevents the Microsoft SDK-specific branch, without disabling
important return-value warnings or adding dummy Microsoft headers.
For older XSSH checkouts, run `python3 tools/fix-mbedtls-sal-config.py`.
No edit to the downloaded third-party Mbed TLS source is required.

## libssh2 nxdk build compatibility (`sys/stat.h`)

If the previous XSSH build failed with:

```text
third_party/libssh2/include/libssh2.h:100:10: fatal error: 'sys/stat.h' file not found
```

run the patched dependency script again from the **project root** (it is safe
on an existing clone):

```sh
./tools/fetch-deps.sh
make NXDK_DIR=/path/to/nxdk
```

Or apply just the compatibility patch without fetching anything:

```sh
python3 tools/patch-libssh2-nxdk.py
```

The patch makes the POSIX `struct stat` type **opaque for nxdk only**. This is
valid for the SSH terminal, which does not implement file transfers. It is
**not** a working `stat()` implementation, and SCP/SFTP remain deliberately
excluded from the Makefile. The Makefile also compiles libssh2's `mbedtls.c`
backend required by the configured TLS implementation.

Run the native regression test with `python3 tools/test-libssh2-patch.py`.
This is a focused header fix, **not proof of a successful nxdk cross-build**;
other nxdk portability issues may still be present.

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
login form, and on-screen keyboard. The currently selected controller key
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

`12` is the default; the terminal grid uses the available height (larger when a USB keyboard is attached).
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

`network_ip` is the **Xbox's own** IP address. The optional `host=` prefill
still refers to the **remote SSH server**. Do not assign an address that another device already
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

## Interactive SSH login and host verification

XSSH starts with a connection form in this order:

1. **SERVER IP** — numeric IPv4 address of the SSH server.
2. **PORT** — decimal TCP port (default `22`).
3. **USERNAME** — login account on the remote SSH server.
4. **PASSWORD** — masked while typing and wiped after authentication.

Use a USB HID keyboard or the controller's on-screen keyboard. **Enter** (or
controller **B**) advances to the next field. Submitting the password
immediately attempts a TCP connection and SSH handshake; for a previously
verified host key the shell opens after password authentication. Press
**Shift+Tab**, **Up**, or controller **Start** to go back and edit a field.
Prepopulated fields from `xssh.cfg` are replaced on the first typed character,
or can be edited using Backspace. The login fields can also be left out of
`xssh.cfg` entirely; `port` then defaults to `22`.

### First-time SSH server keys

For a new server/port pair XSSH shows its SSH host key SHA-256 fingerprint as
64 hexadecimal characters and requires explicit **Enter/B to trust** or
**Esc/X to reject**. The password is **not sent until after host-key approval**.
Verify the fingerprint from a separate trusted source first; simply accepting
an unverified first-use key is vulnerable to man-in-the-middle attacks.
Approved keys are stored in `D:\xssh-known-hosts.txt`, an optional fourth
file created in the XSSH folder when needed. Future connections with the same
server IP and port use the saved key automatically; if it changes, XSSH
refuses the connection. Preserve this file when updating XSSH. The release
still contains only the **three required installation files**.

You can optionally pin a host key using `hostkey_sha256` in `xssh.cfg`.
That setting is applied **only when the form's entered host and port match**
`host` and `port` from the config; for other hosts XSSH uses the trusted-hosts
file. When a config pin is present for a matching host, a mismatch is rejected
without presenting a trust prompt. No SSH password is stored on disk.

XSSH computes SHA-256 hex of the negotiated **raw host-key blob**.

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

For an optional config-level pin, set the 64 hex characters in
`hostkey_sha256=...`. The client authenticates only when the applicable
pin matches. It does **not** silently trust changed keys. RSA and NIST ECDSA keys
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
keyboard is active; B (Enter), X (Backspace), Start (previous field on the
login form, Ctrl-C in the shell), and Back (disconnect) continue to work.

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
- **B:** advance the login field, connect after entering a password, or send Enter in the shell; **X:** Backspace (reject an unknown host key).
- **Y:** toggle upper/lowercase of the on-screen keyboard (when visible); **Start:** previous login field / Ctrl-C in the shell; **Back:** disconnect.
- For an unknown host key, **B** accepts after verification and **X** cancels without sending the password.
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
- SSH host-key verification is mandatory: explicit first-use confirmation or a
  previously saved/configured pin. No silent TOFU and no downgrade to plaintext
  Telnet. Password is entered at runtime and wiped
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

## nxdk build compatibility: missing `sys/types.h` / `sys/stat.h`

The standard nxdk PDCLib environment does not provide the complete POSIX
headers expected by upstream libssh2 1.11.1. After fetching dependencies,
`tools/fetch-deps.sh` runs `tools/patch-libssh2-nxdk.py` automatically.
This patches libssh2 for XSSH's **SSH-terminal-only** use:

- Avoids unprovided `sys/stat.h` / `sys/types.h` and supplies the necessary
  public API typedefs without creating fake POSIX filesystem implementations.
- Uses the existing nxdk/lwIP socket headers instead of assuming that
  `<sys/socket.h>` and `<sys/select.h>` exist.
- Avoids `<unistd.h>` in the SFTP public header. SCP/SFTP source files are
  still not included in the XSSH build.

To re-apply the patch to an existing checkout:

```sh
cd /path/to/xbox-nxdk-ssh
python3 tools/patch-libssh2-nxdk.py
make NXDK_DIR="$HOME/nxdk" -j4
```

The compatibility patch is tested with native header compilation but does
**not** prove a complete Xbox cross-build. Other POSIX portability issues may
still arise depending on nxdk and libssh2 versions.

## Mbed TLS build compatibility (`MBEDTLS_RSA_C`)

If nxdk compilation reports:

```text
error: "MBEDTLS_RSA_C defined, but none of the PKCS1 versions enabled"
```

this source archive enables `MBEDTLS_PKCS1_V15` in
`include/mbedtls_config_xbox.h`. This is required for PKCS#1 v1.5 RSA
signatures used with SSH RSA/SHA-2. It does not activate legacy SSH
`ssh-rsa`/SHA-1 signatures (that algorithm remains disabled separately).

To patch an older checkout in place, run from the XSSH project root:

```sh
python3 tools/fix-mbedtls-rsa-config.py
make NXDK_DIR="$HOME/nxdk" -j4
```

Run `python3 tools/test-mbedtls-config.py` for a native configuration check.
Full nxdk cross-compilation remains unverified.

### nxdk libssh2 Windows-compatibility note

Some nxdk compiler setups define `_MSC_VER` although the Xbox does not
include MSVC CRT types such as `SSIZE_T` or headers such as `<io.h>`.
`tools/patch-libssh2-nxdk.py` automatically excludes the MSVC-specific
branches in `libssh2.h` whenever `NXDK` is defined; the SSH terminal uses
portable libssh2 types and nxdk's lwIP socket implementation.
If upgrading an older checkout, run `tools/patch-libssh2-nxdk.py` again.

## nxdk-specific libssh2 portability

The nxdk Clang toolchain may define `_MSC_VER` despite not shipping Microsoft
CRT headers such as `io.h` or `sal.h`. XSSH defines `NXDK=1` for **every**
source file in `Makefile` and applies compatibility guards to libssh2 1.11.1
with `tools/patch-libssh2-nxdk.py`. Do not remove `-DNXDK=1` from
`NXDK_CFLAGS`, or libssh2 may select the Microsoft-only `SSIZE_T` / `io.h`
branches. The SSH-only port does not provide SCP/SFTP file-stat support.

On a clean checkout run:

```sh
./tools/fetch-deps.sh
make NXDK_DIR="$HOME/nxdk" -j4
```

To rerun all independent compile steps and capture diagnostic output:

```sh
make NXDK_DIR="$HOME/nxdk" -k -j1 V=1 2>&1 | tee xssh-build.log
```

**Build verification:** Native tests do not substitute for a full nxdk
cross-build and actual Xbox hardware testing.

### nxdk `time_t` / libssh2 public header compatibility (October 2026)

The libssh2 1.11.1 public header declares `libssh2_scp_send64` with
`time_t` parameters even in SSH-terminal-only builds. nxdk's PDCLib
provides `time_t` via the standard `<time.h>` header; POSIX
`<sys/types.h>` is not available. The dependency patch now includes
`<time.h>` from the nxdk compatibility section of `libssh2.h`.

If you already downloaded dependencies before this release, run:

```sh
python3 tools/patch-libssh2-nxdk.py
make NXDK_DIR="$HOME/nxdk" -j4
```

The full Xbox cross-build remains unverified until tested with nxdk.

### nxdk libssh2 portability

The dependency-fetch script applies Xbox-specific libssh2 1.11.1 header fixes,
including `time_t` from `<time.h>` for public SCP declarations (SCP is not built).
Run `./tools/fetch-deps.sh` after extracting this source and before building.
These fixes are tested with host-compiler smoke tests; a complete nxdk `.xbe`
build has not yet been verified.

### libssh2 ChaCha20/Poly1305 BSD type compatibility (nxdk)

libssh2 1.11.1 uses `u_int` and `u_char` in its OpenBSD ChaCha20 and
Poly1305 code. nxdk's PDCLib does not define those BSD aliases. When
`tools/fetch-deps.sh` runs, `tools/patch-libssh2-nxdk.py` now injects
`include/xssh_nxdk_bsd_types.h` into the two libssh2 headers. The aliases
are fixed-width (`uint32_t` and `uint8_t`), only active for `NXDK=1`.
ChaCha20-Poly1305 is **not disabled**.

If dependencies were previously fetched, reapply the patch without
re-downloading them:

```sh
python3 tools/patch-libssh2-nxdk.py
make NXDK_DIR="$HOME/nxdk" -j4
```

Run the targeted native regression test with:

```sh
python3 tools/test-chacha-bsd-types.py
```

### Session socket / Mbed TLS key-file portability

`tools/patch-libssh2-nxdk.py` removes the POSIX-only `fcntl.h` requirement
from libssh2 `session.c` for nxdk. It uses nxdk's lwIP
`ioctlsocket(FIONBIO)` for nonblocking SSH connections. This port assumes
XSSH passes a freshly created, initially blocking TCP socket.

`MBEDTLS_FS_IO` is enabled so the key-file parsing functions referenced
by libssh2 are present at link time. This does not enable SSH key login in
the XSSH UI; XSSH still prompts for a password.

Run `python3 tools/patch-libssh2-nxdk.py` after fetching dependencies.

## Interactive login regression tests

Run `./tools/test-connect-form.sh` on a desktop C compiler. This tests dotted
IPv4 validation, editable port/username/password, password memory scrubbing,
replacement of config prefill values, first-use key storage, and rejection of
changed fingerprints. These are native tests; nxdk compilation and hardware
input/security validation still require testing on a real Xbox.

## 2026-11-02 terminal-performance release

- VT100/ANSI scrolling margins, delayed line wrap, insert/delete characters and lines,
  alternate screen, reverse video, cursor-position responses, UTF-8 box lines.
- USB-keyboard layout reclaims the on-screen keyboard area, requesting a larger
  SSH PTY on connection/disconnection. At size 10 the USB grid is about 91x42.
- The terminal renders through a cached texture where the nxdk SDL2 renderer
  supports target textures, with a direct renderer fallback otherwise.
- Network receive is pumped independently of the 30 FPS draw cap.
- This source package is not a precompiled XBE. Cross-build it with nxdk and test
  terminal correctness and latency on original Xbox hardware.

## Readable font / live zoom (v5)

The terminal now defaults to `font_size=12`, displaying its bold 8x12 bitmap
at native logical resolution instead of shrinking it to 7x10. Edit `xssh.cfg`
to choose a startup font size from 8 through 24.

With a USB keyboard, press **Ctrl+=** (no Shift required)
to increase font size by one, or **Ctrl+-** to decrease it. Keypad
Ctrl++ / Ctrl+- also work. Shortcuts are intercepted locally and not
forwarded to the SSH server. Zoom is session-only; it does not rewrite
`xssh.cfg`. The terminal grid and SSH PTY size update together, so `htop`,
`vim` and similar full-screen programs can redraw at the new dimensions.
The top bar displays `F12`, `F13`, etc. for the current font size.

The source bitmap is generated from DejaVu Sans Mono Bold (Bitstream Vera
license), rasterized as high-contrast 1-bit glyphs with nearest-neighbor
scaling. This is not a vector/text-antialiased font renderer.


## Font atlas hotfix (v6, 2026-11-04)

The previous bold bitmap generator incorrectly used intensity `1` while
requiring pixel intensity >=120, creating an all-zero glyph atlas and an
unusable interface. This release regenerates the actual font data with
opaque pixels (`255`) and corrects positioning for the underscore/descenders.
`tools/check-font-atlas.py` now validates every printable glyph before the
verified build runs. Startup label: `XSSH TERM v6`; build ID:
`XSSH-FONT6-20261104`. The font zoom shortcuts are now Ctrl+= (no Shift) and Ctrl+-.

## 2026-11-04 selectable font release (v7)

`xssh.cfg` supports `font_style=bold`, `font_style=regular`,
`font_style=cousine` (default) and `font_style=cousine_bold`. All four styles are
compiled as 8x12 fixed bitmaps with a 94-glyph nonblank font check. No TTF
files are required on the Xbox. Change the setting and rebuild/restart XSSH.

**Ctrl+=** increases the terminal font size without Shift. **Ctrl+-** reduces
it. Keypad Ctrl+plus/minus are also supported. Ctrl+Shift+= still works.
Resizing updates the remote PTY; it doesn't change the configured font style.

The bundled bitmap faces derive from DejaVu Sans Mono (Bitstream Vera license)
and Cousine (SIL Open Font License). Only rasterized glyph masks are shipped.

## Unicode support (v8)

XSSH now retains decoded UTF-8 code points in its terminal cells rather than
converting unknown characters into `?`. The renderer implements Unicode
Braille U+2800..U+28FF (used by btop graphs), block elements U+2580..U+259F,
common box-drawing characters, arrows and symbols. Four selectable bitmap
ASCII fonts remain available. Other Unicode glyphs may still use a fallback.

Build ID: `XSSH-UNICODE8-20261104`. This is an Xbox source release requiring
a full nxdk build; the actual on-console behavior is not yet verified.


## V9: Startup actions, live font switching and ANSI color

- Default font: `font_style=cousine` (Cousine Regular, not bold).
- `Ctrl+F`: cycle Bold, Regular, Cousine, Cousine Bold, without reconnecting.
- `Ctrl+=` / `Ctrl+-`: increase/decrease size, with live PTY resize.
- The login screen now has **CONNECT**, **EXIT**, **RESTART**, and **SHUTDOWN** action buttons.
  Press `Tab` or `Esc` to focus the button row, use Left/Right to choose, then Enter.
  USB F9/F10/F11 can also select Exit/Restart/Shutdown; gamepad Back opens
  the action row, D-pad Left/Right chooses, and A/B activates.
- Restart and shutdown always require confirmation (Y/Enter, or controller B; N/Esc or X cancels).
- Exit ends XSSH; returning to a dashboard depends on the launching dashboard/BIOS.
- The original Xbox kernel firmware power calls implement reboot and shutdown.
  **Please test on your hardware**; power actions may vary by firmware.
- The terminal accepts 16-color ANSI SGR, 256-color `38;5`/`48;5`,
  and 24-bit RGB `38;2`/`48;2` sequences, including background colors.
  The Xbox themes still control default foreground/background and app chrome,
  but remote explicit ANSI colors take precedence.
- `xssh.cfg` is not overwritten by Ctrl+F; set `font_style=` to persist choice.

Run native checks:

```sh
./tools/test-keyboard.sh
./tools/test-theme.sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/terminal.c src/theme.c tools/test-ansi-color.c -o /tmp/xssh-ansi-test && /tmp/xssh-ansi-test
```


### Bitmap glyph baseline hotfix (v9.1)

Fixed Cousine Regular/Bold and DejaVu Regular glyph clipping: the bitmap
font generator previously applied a `-3` pixel vertical offset, cutting off
the tops of `f`, `i` and `l`. All generated fonts now use a consistent
9-pixel baseline in the 8x12 cell. The build validation checks the dot on
`i`, ascenders on `f`/`l`, distinct glyph forms and descenders before
compilation. The pre-generated bitmap atlas is checked into source.

Existing `font_style=cousine`, Ctrl+F, Ctrl+= and ANSI color support remain.


### v9.2 build fix (2026-11-05)

Added a forward declaration for `disconnect(App *)` before `confirm_power()` to fix the nxdk C99 conflicting-types build error. No changes to shutdown behavior.


### XSSH v9.3: power-off and interactive latency

- **Power off**: confirmation now writes the original Xbox SMC power-off
  value `0x80` to register `0x02` at SMBus address `0x20` via
  `HalWriteSMBusValue`; it does **not** pass mode `0` to
  `HalReturnToFirmware` (which halted or restarted on this console).
  A failed or unfulfilled shutdown will not trigger a fallback reboot.
- **Restart** retains `HalReturnToFirmware(1)`.
- **Vim / low-latency SSH**: enables `TCP_NODELAY` on the SSH TCP socket;
  flushes queued keystrokes *before* reading server output; caps each
  receive pass to eight 4096-byte reads or about four milliseconds; and
  avoids full redraws when no terminal content changed.  Output is not
  discarded; unread data remains queued for the next iteration.
- Previous SSH, Unicode, font, and login behavior is retained.

Cross-compilation and actual Xbox hardware shutdown/latency testing are
still required.  Original Xbox power APIs can be firmware-dependent; test
shutdown only when no important disk writes are in progress.


## Front LED and video screensaver (v10)

The original Xbox front-panel LED has **green and red LEDs**, which can combine
for orange. In `xssh.cfg` set:

```ini
led_mode=auto
screensaver_enabled=yes
screensaver_seconds=120
screensaver_file=D:\screensaver.xsv
```

`led_mode` values: `auto`, `green`, `red`, `orange`, `off`, `blink_green`,
`blink_red`, `alternate`. `auto` means orange in the login/verification screen,
green while SSH is connected, red on failure and off during the screensaver.
When XSSH exits normally, Xbox firmware regains LED control. These LED modes
are limited to the **original Xbox hardware's green/red/orange** LED; the
stock console does not have a programmable RGB LED.

After `screensaver_seconds` with no keyboard/controller activity, XSSH starts
playing `D:\screensaver.xsv`. Any input wakes the UI; the SSH connection
remains established and network traffic continues to be processed. The screensaver
will not start during a power confirmation or SSH host-key verification.

The release directory includes a generated animated demo `screensaver.xsv`.
Copy it alongside `default.xbe`, `xssh.cfg`, and `xssh-seed.bin`:

```text
E:\Apps\XSSH\default.xbe
E:\Apps\XSSH\xssh.cfg
E:\Apps\XSSH\xssh-seed.bin
E:\Apps\XSSH\screensaver.xsv
```

The demo is only regenerated if missing; your replacement video in `release/`
is preserved between builds. **Do not overwrite an existing Xbox seed on upgrade.**

### Convert your own MP4 / MOV / MKV to the fast Xbox format

On the development PC (requires FFmpeg):

```bash
python3 tools/convert-screensaver.py my-video.mp4 release/screensaver.xsv \
    --width 320 --height 240 --fps 12
```

The `.xsv` container stores uncompressed RGB565 frames, each 16 bits/pixel.
There is **no video codec decoding on Xbox** and no audio support. This is
CPU-efficient on a 733 MHz original Xbox, but uses HDD bandwidth:
320x240x2x12 = 1,843,200 bytes/second (approximately 1.76 MiB/s).
Higher frame rate/resolution will consume more storage and data bandwidth.
Recommended 320x240 at 12 FPS; 160x120 for slower hard drives. Files may
contain up to 640x480, 30 FPS frames but are not recommended at that size.

The original nxdk SDL renderer is asked for a native RGB565 streaming texture;
if that texture is not supported, XSSH falls back to 32-bit texture upload.
The video is stretched to the application's 640x480 logical display; physical
480i/480p/720p/1080i/custom BIOS video output remains controlled by `video_mode`.

Video format specification (little-endian): 8 bytes magic `XSSHRGB5`, then
4 x uint32 (`width`, `height`, `fps`, `frame_count`), followed by exactly
`frame_count` contiguous `width * height * 2`-byte RGB565 frames.

`./tools/test-led-saver.sh` validates LED modes and sample video playback.
The Xbox LED and SDL texture paths require verification on physical hardware.


## Optional antialiased text (v10.1)

`xssh.cfg` supports `text_antialiasing=no` (default, original pixel-sharp
font) and `text_antialiasing=yes` (grayscale alpha-smoothed glyphs).
Both modes are embedded in the XBE and apply to terminal and startup UI
text; existing `font_style` and `font_size` settings still work. Restart
XSSH after changing this setting. The terminal grid and SSH PTY dimensions
are unchanged. Smoothed text may improve legibility on some displays but
adds alpha blending work, especially in rapidly updating terminals.
The built-in Unicode line and block drawing remains pixel-sharp.
The checked-in atlas can be regenerated by running `python3 tools/gen-font-aa.py`
with Pillow and the installed source fonts. No font files are included.

### Runtime text antialiasing shortcut (v10.2)

Press **Ctrl+A** on a connected USB keyboard to toggle text antialiasing
immediately. The keyboard shortcut is handled locally by XSSH, so **Ctrl+A
is not sent to the remote SSH session** (including Vim/Screen). The terminal
texture cache is invalidated to redraw text in the selected mode.
`text_antialiasing=yes|no` in `xssh.cfg` continues to control the initial state;
Ctrl+A does not modify that file.
