# Agent notes

- Framework only. Never commit machine details (IPs, hostnames, user names, device serials, passwords): those belong in a private instance repo.
- `make check` must pass (shell syntax + `tests/test-config.sh` on `examples/instance`).
- Keep scripts POSIX `sh`.
- Do not start `sk-vdisplay` builds that drop the frame pump: without it Xorg freezes (flip_done timeouts).
