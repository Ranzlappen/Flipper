# Contributing

Thanks for helping out. This repo is a starter framework for Flipper Zero apps
on [Momentum firmware](https://momentum-fw.dev). Releases happen on autopilot
once a PR merges to `main`, so the bar for getting your change in is "CI
passes + a reviewer approves."

## Quick start

```bash
git clone https://github.com/Ranzlappen/Flipper
cd Flipper/JS-Apps
npm ci
npm run typecheck      # tsc against the Momentum SDK type stubs
npm run validate       # FAM appid/entry checks + SDK require allowlist
npm run smoke          # vm-sandbox dry-run of every JS template/example
npm run format         # prettier --write (run before pushing)
```

For C apps you also need [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
pip install --upgrade ufbt
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
cd C-Apps/<your-app>
ufbt          # produces dist/<appid>.fap
```

## Conventional Commits

We use [Conventional Commits](https://www.conventionalcommits.org/). The PR
title becomes the squashed commit message, so prefix it correctly:

| Prefix | When to use | Version bump |
|--------|-------------|--------------|
| `feat:` | New script, new app, new framework feature | minor (pre-1.0: patch) |
| `fix:` | Bug fix in existing code | patch |
| `docs:` | Documentation only | patch (visible in changelog) |
| `perf:` | Performance improvement | patch |
| `refactor:` | Internal cleanup, no behaviour change | none (hidden) |
| `chore:` / `ci:` / `build:` / `test:` | Infra / tooling | none (hidden) |
| `feat!:` or any `…!:` | Breaking change | major |

[release-please](https://github.com/googleapis/release-please) reads these
prefixes to generate `CHANGELOG.md` and bump the version. Don't hand-edit
either.

## Workflow

1. Branch off `main` (or use a `claude/...` branch).
2. Make your change. Run `npm run validate && npm run typecheck && npm run format:check` locally.
3. Open a PR. Fill in the template. CI must go green:
   - `CI / JS sanity` — prettier, typecheck, validate, smoke
   - `CI / Build <fam>` — one matrix cell per non-template `application.fam`
4. Get a CODEOWNER approval. PRs labelled `automerge` (by a CODEOWNER) will
   self-merge once checks pass.
5. release-please picks up your merged commit, updates its open "release PR"
   with a CHANGELOG entry. Merging that PR cuts a tag and the
   `release-artifacts.yml` workflow attaches `.fap`/`.js`/bundle.zip to the
   GitHub Release.

## Required branch protection (configure once in repo Settings → Branches)

Apply to `main`:

- Require a pull request before merging
- Require approvals: 1
- Require review from Code Owners
- Require status checks to pass before merging:
  - `CI / JS sanity`
  - `CI / Discover C apps`
  - `CI / Build *` (all matrix cells; mark as required as they appear)
- Require branches to be up to date before merging
- Require linear history
- Restrict who can push to matching branches: keep empty (PR-only)

Don't enable "require signed commits" — release-please and dependabot don't
sign and you'll get stuck.

## Adding a new JS script

1. Put the file under `JS-Apps/templates/` (generic) or `JS-Apps/examples/`
   (concrete, opinionated).
2. Only `require()` modules listed in `JS-Apps/tsconfig.json` `paths`. Adding
   a new SDK module means updating that list in the same PR.
3. Add `// Tested against @next-flip/fz-sdk-mntm <version>` near the top so
   regressions on the next Momentum SDK bump are easy to bisect.
4. `npm run validate && npm run smoke` locally before pushing.

## Adding a new C app

1. Create `C-Apps/<your-app-name>/` (folder name kebab-case is fine, validate
   normalises before comparing to `appid`).
2. Add `application.fam` with `appid="<snake_name>"` matching the folder.
3. `appid` doubles as the `.fap` filename. `entry_point` must be a real
   function symbol in one of the `.c` files in the folder.
4. `cd C-Apps/<your-app-name> && ufbt` must produce `dist/<appid>.fap`.

## Momentum SDK upgrades

The pinned SDK version lives in `.momentum-sdk-version` and
`JS-Apps/package.json`. Don't bump these by hand — `momentum-sync.yml` runs
weekly, opens a `chore(deps): bump Momentum SDK …` PR, and CI verifies it
still builds. Review the diff and merge.
