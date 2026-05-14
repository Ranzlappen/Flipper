<!--
  Thanks for contributing! A few quick notes:
  - Commit messages use Conventional Commits (feat: / fix: / docs: / chore:).
    The title of this PR will become the squashed commit, so prefix it too.
  - CI (.github/workflows/ci.yml) runs prettier, typecheck, validate, smoke,
    and builds every C app under C-Apps/ (excluding templates/). All must
    pass before merge.
-->

## Summary

<!-- 1-3 bullets explaining what and why. -->

-

## Type of change

- [ ] `feat` — new JS script, new C app, or new feature
- [ ] `fix` — bug fix in existing code
- [ ] `docs` — documentation only
- [ ] `chore` / `ci` / `refactor` — internal change, no user-visible behaviour
- [ ] Breaking change (will become a major-version bump)

## Verification

- [ ] `npm run typecheck` passes in `JS-Apps/`
- [ ] `npm run validate` passes
- [ ] `npm run format:check` passes (run `npm run format` to fix)
- [ ] If C app: `ufbt` builds the app locally and the `.fap` runs on a
      Momentum-firmware Flipper Zero
- [ ] If JS script: deployed to `/ext/apps/Scripts/` and tested on-device

## Momentum compatibility

- Tested against `.momentum-sdk-version`: <!-- e.g. UFBT_CHANNEL=release / JS_SDK=1.0.0 -->
- [ ] No new SDK module used, OR new module is allowlisted in `tsconfig.json` paths
