# lora-workspace

PlatformIO firmware for two Heltec WiFi LoRa 32 V4 boards. See `README.md` and `docs/`.

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
