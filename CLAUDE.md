# Claude Code project brief — Momentum App Framework

This file gives Claude Code (and humans) the project-specific context it needs
so feature work doesn't restart cold every session.

## What this is

A starter framework for **Flipper Zero** apps running [Momentum
firmware](https://momentum-fw.dev). Two parallel app paths:

- **JS scripts** under `JS-Apps/` — run by the on-device mJS engine, deployed
  by copying `.js` files to `/ext/apps/Scripts/`. No build step. Phone-only
  development is supported.
- **C `.fap` apps** under `C-Apps/` — built locally with
  [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) against the
  pinned Momentum SDK, deployed to `/ext/apps/Examples/` (or any category).

Both halves ship together as release artifacts via GitHub Releases.

## Layout

```
JS-Apps/
  templates/         generic skeletons (basic-script, gui-example, …)
  examples/          concrete, opinionated scripts (balkon-markise-remote, …)
  scripts/           dev tooling: validate.mjs, smoke.mjs
  tsconfig.json      Momentum SDK module path aliases live here
  package.json       npm scripts: typecheck, lint, validate, smoke, format
C-Apps/
  templates/         hello-world skeleton; excluded from release builds
  <appid>/           production app: application.fam + <appid>.c + README
.github/workflows/   ci.yml, release-please.yml, release-artifacts.yml,
                     momentum-sync.yml, labeler.yml, automerge.yml
.momentum-sdk-version  Single source of truth for UFBT_CHANNEL + JS_SDK
release-please-config.json + .release-please-manifest.json
```

## SDK & types

Type stubs come from `@next-flip/fz-sdk-mntm` (pinned in
`JS-Apps/package.json`). Available modules are the `paths` aliases in
`JS-Apps/tsconfig.json`:

`badusb, blebeacon, event_loop, flipper, gpio, gui, gui/*, i2c, math,
notification, serial, spi, storage, subghz, usbdisk, vgm`.

**`validate.mjs` enforces this allowlist** — if a JS script `require()`s
something outside it, CI fails. Adding a new SDK module means updating
`tsconfig.json` `paths` in the same PR.

mJS quirks the JS scripts depend on: globals `print` and `delay`; no
closures (state is passed via `eventLoop.subscribe(view, fn, ...args)`); only
the SDK module list above is available via `require()`.

## Commands to use

From `JS-Apps/`:

```bash
npm ci                 # one-time install
npm run typecheck      # tsc --noEmit
npm run validate       # FAM appid/entry + SDK require allowlist
npm run smoke          # vm-sandbox dry-run of every script (1.5s timeout)
npm run format         # prettier --write
npm run format:check   # prettier --check (what CI runs)
npm run lint           # format:check + typecheck
```

For C apps:

```bash
pip install --upgrade ufbt
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=$(grep ^UFBT_CHANNEL= ../.momentum-sdk-version | cut -d= -f2)
cd C-Apps/<your-app> && ufbt
```

## Release process

1. Land Conventional Commits (`feat:`, `fix:`, `docs:`) on `main` via PR.
2. `release-please.yml` opens / updates a "release PR" with a generated
   `CHANGELOG.md` and version bump.
3. Merging the release PR creates a tag and a GitHub Release.
4. `release-artifacts.yml` fires on `release: published`, builds every
   `application.fam` (excluding `C-Apps/templates/`), bundles JS templates +
   examples, and uploads `.fap` / `.js` / `momentum-app-framework-bundle.zip`
   to the release.

**Don't push directly to `main`.** Don't hand-bump `package.json` `version`
or `.release-please-manifest.json`. release-please owns those.

## Momentum SDK upgrades

`momentum-sync.yml` runs weekly, checks `npm view @next-flip/fz-sdk-mntm
version` and the Momentum `directory.json`, and opens a PR labelled
`momentum-sync` updating `.momentum-sdk-version` and `package.json` if either
moved. CI re-runs against the new pin. Review and merge.

## When adding features

- Default to JS scripts for anything that doesn't strictly need C (LED, GUI,
  Sub-GHz, NFC are all available from JS).
- New JS template? Drop in `JS-Apps/templates/`, ensure `npm run validate`
  passes, no `require()` outside the SDK allowlist.
- New C app? Folder name kebab-case is fine; `appid` must be snake-case (it
  becomes the `.fap` filename). `entry_point` must be a real symbol in one
  of the `.c` files in the folder. `validate.mjs` checks both.
- GUI-heavy C app? Scaffold it visually with [Flipper GUI
  Studio](https://tools.ranzlappen.com/tools/flipper-gui/). Its **C app**
  export unzips into `C-Apps/<app>/` and already satisfies `validate.mjs`.
  Commit the `<appid>.flipper-gui.json` spec alongside the C — it's the
  source of truth and what makes the app re-editable (round-trip via **Load
  JSON**, never by parsing the `.c`). Treat `<ns>_scene.c/.h` and
  `application.fam` as build output regenerated from that spec; `npm run
  regen-check` (CI-enforced) byte-checks they're in sync, and
  `npm run regen-check -- --write` regenerates them. Put hand-written logic
  only in the `<ns>_on_event()` override in `<appid>.c` (excluded from the
  regen diff). The exporters used by the check are a pinned snapshot under
  `JS-Apps/vendor/flipper-gui/`. See `docs/gui-tool-integration.md` and the
  `C-Apps/templates/gui-studio/` example.
- Commit prefix matters — see `CONTRIBUTING.md`. A `feat:` commit triggers a
  minor bump; everything you want in the changelog needs the right prefix.

## Anti-patterns to avoid

- Hand-editing `CHANGELOG.md` (release-please overwrites).
- Pinning `@next-flip/fz-sdk-mntm: "latest"` again (was the old default — drift breaks CI silently).
- Adding `.fap`/`.bin` artifacts to git. Those come from release builds only.
- Adding eslint/jest. The tooling is deliberately minimal because Flipper
  scripts run under mJS, not Node — most lint rules don't apply and most
  test frameworks can't load the SDK.
- Hand-editing a Studio app's `<ns>_scene.c/.h` or `application.fam`, or
  deleting its `<appid>.flipper-gui.json` (regen-check overwrites/fails —
  edit the spec and regenerate instead).
- Reformatting `JS-Apps/vendor/flipper-gui/` (it's a pinned upstream mirror,
  prettier-ignored; fix upstream and re-vendor).
