#!/usr/bin/env node
// Bi-directional C-Apps ⇄ Flipper GUI Studio drift guard.
//
// Every Studio-generated C app commits its design as a `*.flipper-gui.json`
// spec (schema "flipper-gui/v1") next to application.fam. That spec is the
// source of truth: the FAM and the <ns>_scene.{c,h} are pure functions of it.
// This script re-runs the *real* Studio exporters (vendored under
// vendor/flipper-gui/) on each committed spec and byte-compares the result
// against the committed files. Any drift — a hand-edited scene, a stale spec,
// an exporter bump — fails CI.
//
// Excluded from the diff: <ns>.c (the entry point; holds hand-written
// on_event logic) and binary assets (icon.png, images/). Those are
// generated once, then owned by the author.
//
//   node scripts/regen-check.mjs            verify (CI mode), non-zero on drift
//   node scripts/regen-check.mjs --write    regenerate FAM + scene.{c,h} in place

import { readFile, readdir, stat, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { dirname, join, basename, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { exportFam, appMeta } from "../vendor/flipper-gui/exporters/fam.js";
import { exportScene } from "../vendor/flipper-gui/exporters/scene.js";
import { exportJson } from "../vendor/flipper-gui/exporters/json.js";
import { preloadFonts } from "../vendor/flipper-gui/lib/font-render.js";

const HERE = dirname(fileURLToPath(import.meta.url));
const JS_ROOT = resolve(HERE, "..");
const REPO_ROOT = resolve(JS_ROOT, "..");
const C_APPS = join(REPO_ROOT, "C-Apps");

const WRITE = process.argv.includes("--write");
const SCHEMA = "flipper-gui/v1";

const errors = [];
const fail = (file, msg) => errors.push(`${relative(REPO_ROOT, file)}: ${msg}`);

// Recursively find every *.flipper-gui.json spec under C-Apps/.
async function findSpecs(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const name of await readdir(dir)) {
    const full = join(dir, name);
    const s = await stat(full);
    if (s.isDirectory()) out.push(...(await findSpecs(full)));
    else if (name.endsWith(".flipper-gui.json")) out.push(full);
  }
  return out;
}

// Byte-compare `generated` against the committed file (or write it under --write).
// Returns nothing; records an error on mismatch.
async function checkFile(dir, filename, generated) {
  const target = join(dir, filename);
  if (WRITE) {
    await writeFile(target, generated);
    console.log(`  wrote ${relative(REPO_ROOT, target)}`);
    return;
  }
  if (!existsSync(target)) {
    fail(target, `missing — run \`npm run regen-check -- --write\` to generate it`);
    return;
  }
  const committed = await readFile(target, "utf8");
  if (committed !== generated) {
    fail(target, firstDiff(committed, generated));
  }
}

// A readable one-line pointer at the first differing line.
function firstDiff(a, b) {
  const al = a.split("\n");
  const bl = b.split("\n");
  const n = Math.max(al.length, bl.length);
  for (let i = 0; i < n; i++) {
    if (al[i] !== bl[i]) {
      return `out of sync with its spec at line ${i + 1}:\n      committed: ${JSON.stringify(al[i] ?? "<eof>")}\n      expected:  ${JSON.stringify(bl[i] ?? "<eof>")}\n      → regenerate with \`npm run regen-check -- --write\` (or re-export from Flipper GUI Studio)`;
    }
  }
  return "differs from its spec (length mismatch)";
}

async function checkSpec(specFile) {
  const dir = dirname(specFile);
  const raw = await readFile(specFile, "utf8");

  let state;
  try {
    state = JSON.parse(raw);
  } catch (e) {
    fail(specFile, `not valid JSON: ${e.message}`);
    return;
  }
  if (state.schema !== SCHEMA) {
    fail(specFile, `schema must be "${SCHEMA}" (got ${JSON.stringify(state.schema)})`);
    return;
  }
  if (!state.app || !Array.isArray(state.screens)) {
    fail(specFile, `missing required "app" / "screens"`);
    return;
  }

  const m = appMeta(state);

  // Identifier cross-check: the spec's namespace must resolve to this folder.
  const folder = basename(dir);
  if (m.folder !== folder) {
    fail(specFile, `app.namespace "${state.app.namespace}" → folder "${m.folder}" but the spec lives in "${folder}/"`);
  }

  // Idempotency: the committed spec must already be in canonical exporter form,
  // so re-importing and re-exporting it is a no-op (round-trip stable).
  const canonical = exportJson(state).text;
  if (!WRITE && raw !== canonical) {
    fail(specFile, firstDiff(raw, canonical).replace("out of sync with its spec", "not in canonical form"));
  }

  // Regenerate the deterministic outputs and compare (or write).
  const fam = exportFam(state);
  await checkFile(dir, fam.filename, fam.text);
  for (const f of exportScene(state)) {
    await checkFile(dir, f.filename, f.text);
  }
}

// ── Main ───────────────────────────────────────────────────────────
await preloadFonts(); // real glyph advances → pixel-exact button centering

const specs = await findSpecs(C_APPS);
for (const spec of specs) {
  console.log(`${WRITE ? "regenerating" : "checking"} ${relative(REPO_ROOT, spec)}`);
  await checkSpec(spec);
}

console.log(`\nregen-check: processed ${specs.length} spec(s)`);

if (errors.length) {
  console.error("\nregen-check: FAILED — committed C is out of sync with its flipper-gui spec");
  for (const e of errors) console.error(`  - ${e}`);
  process.exit(1);
}
console.log("regen-check: OK");
