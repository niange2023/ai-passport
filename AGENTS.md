<p align="right">
  <a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Repository Guidelines for AI Agents

Mandatory entry point for AI work in `ai-passport`. Read only task-routed documents below; do not preload all READMEs. Detailed workflow is in [docs/development/ai-guide.md](docs/development/ai-guide.md).

## Required AI skills

Required skills (`skills/`): `passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`, `passport-debug`. Before development, check that all five are installed; if any are missing, install and verify them automatically while respecting environment approvals and existing user config (report blockers without claiming success). Load only the skill matching the current task; flashing, Git writes, and publishing still require separate user authorization.

## Project and safety baseline

- **Target & partitions**: ESP32-C3, 8 MB Flash, no PSRAM, ESP-IDF 5.5.3. Default `partitions.csv` keeps NVS, PHY, and one factory app; validate custom tables.
- **Worktree & flashing**: Run `git status --short --branch` first and preserve user changes. Flashing does not require a prior Flash readback (see [flashing policy](docs/development/engineering/firmware-layout.md#flashing-and-stored-data)).
- **Source priority**: Product specs/measurements → `components/bsp/include/bsp_pins.h` → BSP code → hardware guide → README/demos. Ask rather than guess missing hardware facts.
- **Architecture & UI**: Reusable board logic in `components/bsp`; app/UI/state in `main`. Derivative apps must implement a redesigned UI (see [mandatory UI redesign](docs/development/ai-guide.md#mandatory-ui-redesign-for-derivative-applications)).
- **Runtime & fonts**: Hold `bsp_lvgl_lock()` outside the LVGL task; keep button callbacks non-blocking; stop all tasks/timers/handlers before deleting a screen; decouple testable logic for host tests; verify Chinese glyph coverage per [font checklist](docs/development/engineering/coding-conventions.md#chinese-fonts-and-missing-glyphs).
- **Security & docs**: Never commit secrets, device QR links, or unsanitized logs. Keep English `.md` and Simplified Chinese `.zh_CN.md` peers aligned with reciprocal links.

## Task-specific context routing

| Task | Read before editing |
| --- | --- |
| Any code change | `docs/development/ai-guide.md`, relevant headers and neighboring code |
| Skill setup / app workflow | `skills/README.md`, then only the matching skill |
| Environment / toolchain | `docs/development/engineering/environment-setup.md` |
| BSP / hardware | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`, `components/bsp/include/bsp_pins.h` |
| Demo / menu | `main/demo.h`, `main/main.c`, nearest `main/demo_*.c` |
| Chinese UI / fonts | `docs/development/engineering/lvgl-chinese-fonts.md` |
| Wi-Fi / BluFi | `docs/development/engineering/wifi-provisioning.md` |
| Build / test / partitions | `docs/development/engineering/build-and-test.md`, `docs/development/engineering/firmware-layout.md`, `sdkconfig.defaults`, `partitions.csv` |
| CI / release / completion | `docs/development/ci/CI-*.md`, `.github/workflows/`, `docs/development/release/project-completion.md` |
| Docs / Commit / PR | `docs/README.md`, `docs/contribution/doc-conventions.md`, `docs/contribution/commit-and-pr.md` |

## Required validation and delivery

Run `./tools/validate.sh --static`, `--firmware`, and complete `./tools/validate.sh` (ESP-IDF 5.5.3) before delivery. Report `Build`, `Host tests`, `Device tests` (`PASS / FAIL / NOT RUN`), and `Unverified` separately. After completing a firmware change, proactively offer on-device testing per [handoff guide](docs/development/ai-guide.md#offer-on-device-testing) and obtain approval before flashing. Commit/push only when requested; feature/doc PRs must not edit `docs/CHANGELOG*.md`. Community docs live in `.github/`.
