# lora-workspace

PlatformIO firmware for two Heltec WiFi LoRa 32 V4 boards. See `README.md` and `docs/`.

## Firmware: only one to flash

Keep exactly one firmware available to flash, `lora_link`, unless Charles explicitly says
otherwise. Do not add PlatformIO environments, build variants or feature flags that produce
another flashable image. Put new features in `lora_link`.

## Git: commit regularly, only your own changes

- **Commit regularly**, and always at the end of every turn that changed files.
- **Several agents work in this repository at the same time.** The working tree
  will often contain other agents' uncommitted changes. Commit only your own:
  - Stage explicit paths (`git add path/to/file ...`). Never use `git add -A`,
    `git add .` or `git commit -a`.
  - Run `git status` and `git diff` on your files before committing, and check
    `git diff --cached` so only your files are staged.
  - If a file contains both your edits and someone else's, stage only your hunks
    (`git add -p`), or leave the file and mention it.
  - Never revert, reset, stash, or reformat changes you did not make.
- Write a clear message describing what you changed and why.

## Deployment

The web app (`web/`) is deployed automatically, outside this repository: a commit pushed
to `master` goes live within 2 minutes. There is no manual deploy step.

The firmware image is **built by the server** from `master` (`scripts/build_release.sh`, a few
minutes after the page updates) and is not committed: never add `.bin` files or `web/firmware/` to git.
The build is pinned in `platformio.ini`; keep it deterministic.
