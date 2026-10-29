# Build from an nxdk shell: . /path/to/nxdk/bin/activate; make NXDK_DIR=/path/to/nxdk
# Dependencies: ./tools/fetch-deps.sh first. Uses the SDK compiler for all code.
# SSH terminal build intentionally excludes SCP/SFTP (requires POSIX filesystem APIs).
XBE_TITLE = XSSH\ SSH\ Terminal
# No automatic XISO: XSSH requires a writable seed beside the executable.
# GEN_XISO = XSSH.iso
NXDK_DIR ?= $(CURDIR)/../nxdk
NXDK_NET = y
NXDK_SDL = y
NXDK_USB_ENABLE_HID = y

SSHROOT = $(CURDIR)/third_party/libssh2
TLSROOT = $(CURDIR)/third_party/mbedtls

SSH_C = agent bcrypt_pbkdf channel comp chacha cipher-chachapoly crypt crypto global \
        hostkey keepalive kex knownhost mac mbedtls misc packet pem poly1305 publickey \
        session transport userauth userauth_kbd_packet version
TLS_C = aes asn1parse asn1write base64 bignum bignum_core bignum_mod bignum_mod_raw \
        block_cipher cipher cipher_wrap constant_time ctr_drbg des dhm ecdh ecdsa \
        ecp ecp_curves ecp_curves_new entropy entropy_poll error md md5 oid pem pk pk_ecc \
        pk_wrap pkcs12 pkcs5 pkparse pkwrite platform platform_util ripemd160 rsa \
        rsa_alt_helpers sha1 sha256 sha512

SRCS = $(CURDIR)/src/main.c $(CURDIR)/src/font_size.c $(CURDIR)/src/terminal.c $(CURDIR)/src/theme.c $(CURDIR)/src/rng.c \
       $(CURDIR)/src/usb_keyboard.c $(CURDIR)/src/hid_keys.c \
       $(CURDIR)/src/video_modes.c $(CURDIR)/src/video_xbox.c \
       $(CURDIR)/src/network_config.c $(CURDIR)/src/network_xbox.c
SRCS += $(addprefix $(SSHROOT)/src/,$(addsuffix .c,$(SSH_C)))
SRCS += $(addprefix $(TLSROOT)/library/,$(addsuffix .c,$(TLS_C)))

# libssh2/mbedtls must distinguish nxdk from Microsoft's CRT.  Clang for
# original Xbox may predefine _MSC_VER even when _WIN32 is undefined.
# All compatibility branches in tools/patch-libssh2-nxdk.py require NXDK=1.
NXDK_CFLAGS += -DNXDK=1 -U_WIN32 -std=gnu11 -O2 -I$(CURDIR)/include \
    -I$(SSHROOT)/include -I$(SSHROOT)/src \
    -I$(TLSROOT)/include -I$(TLSROOT)/library \
    -DHAVE_CONFIG_H -DLIBSSH2_MBEDTLS \
    -DSDL_DISABLE_JOYSTICK_INIT_DELAY \
    -DMBEDTLS_CONFIG_FILE='"mbedtls_config_xbox.h"' \
    -DLIBSSH2_NO_MD5_PEM -DLIBSSH2_NO_RSA_SHA1 \
    -DLIBSSH2_NO_3DES -DLIBSSH2_NO_RC4 -DLIBSSH2_NO_BLOWFISH

include $(NXDK_DIR)/Makefile


# A writable hard-drive release: make (or make release) assembles the three
# files to copy together into the Xbox application's directory.
# The secret seed is created once per release directory and is NEVER replaced
# on incremental builds. Do not distribute a seed across multiple consoles.
RELEASE_DIR := $(CURDIR)/release

.PHONY: release
all: release
release: $(OUTPUT_DIR)/default.xbe $(CURDIR)/xssh.cfg $(CURDIR)/tools/make-seed.py
	@mkdir -p "$(RELEASE_DIR)"
	@cp -f "$(OUTPUT_DIR)/default.xbe" "$(RELEASE_DIR)/default.xbe"
	@cp -f "$(CURDIR)/xssh.cfg" "$(RELEASE_DIR)/xssh.cfg"
	@if [ -e "$(RELEASE_DIR)/xssh-seed.bin" ]; then \
	    test -f "$(RELEASE_DIR)/xssh-seed.bin" && \
	    test "$$(wc -c < "$(RELEASE_DIR)/xssh-seed.bin")" -eq 64 || { \
	        echo "ERROR: Existing release/xssh-seed.bin is not a 64-byte regular file" >&2; exit 1; \
	    }; \
	else \
	    python3 "$(CURDIR)/tools/make-seed.py" "$(RELEASE_DIR)/xssh-seed.bin"; \
	fi
	@echo "[ RELEASE ] $(RELEASE_DIR) (default.xbe, xssh.cfg, xssh-seed.bin)"
