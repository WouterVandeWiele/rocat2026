Import("env")  # noqa: F821 - PlatformIO SCons global
import os


def patch_file(path, original, replacement, label):
    if not os.path.isfile(path):
        print(f"[patch] {label}: file not found, skipping")
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    if original not in content:
        print(f"[patch] {label}: already applied")
        return
    with open(path, "w", encoding="utf-8") as f:
        f.write(content.replace(original, replacement))
    print(f"[patch] {label}: applied")


def libdeps(*parts):
    return os.path.join(
        env.subst("$PROJECT_LIBDEPS_DIR"),  # noqa: F821
        env.subst("$PIOENV"),               # noqa: F821
        *parts,
    )


# ── audio-tools: remove setConnectionTimeout() — not a member of WiFiClient* ──
# ESP32 Arduino WiFiClientSecure has setHandshakeTimeout() (already called
# on the next line) but not setConnectionTimeout(). WiFiClient has setTimeout()
# (already called above) but not setConnectionTimeout().
_urlstream = libdeps(
    "audio-tools", "src", "AudioTools", "Communication", "HTTP", "URLStream.h",
)

patch_file(
    path=_urlstream,
    original="        client_secure->setConnectionTimeout(client_timeout);\n",
    replacement="",
    label="audio-tools URLStream remove WiFiClientSecure::setConnectionTimeout",
)

patch_file(
    path=_urlstream,
    original="      client_insecure->setConnectionTimeout(client_timeout);\n",
    replacement="",
    label="audio-tools URLStream remove WiFiClient::setConnectionTimeout",
)
