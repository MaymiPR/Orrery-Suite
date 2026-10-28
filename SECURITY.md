# Security

The Orrery Suite plug-ins read their own preset files (`.antp`, `.scorpius`) and the state a host saves with a
project, so a crafted file that crashes them or does worse is a security bug.

Please report one privately with GitHub's **Report a vulnerability** button (Security tab > Advisories), not in a
public issue. Include the file or steps that trigger it and the plug-in's version, host and OS.

Fixes go into the next release; only the latest release is supported.

Release downloads are unsigned. Check them against the `SHA256SUMS.txt` that comes with each
[release](https://github.com/MaymiPR/OrrerySuite/releases).
