#!/usr/bin/env node
// Flipper-specific sanity checks that TypeScript can't express.
// Walks JS scripts and C app manifests, fails non-zero on any error.

import { readFile, readdir, stat } from "node:fs/promises";
import { existsSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { dirname, join, basename, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const JS_ROOT = resolve(HERE, "..");
const REPO_ROOT = resolve(JS_ROOT, "..");

const errors = [];
const warn = (file, msg) => errors.push(`${relative(REPO_ROOT, file)}: ${msg}`);

// ---------------------------------------------------------------------------
// SDK module allowlist — derived from tsconfig.json path aliases so the two
// stay in sync.
// ---------------------------------------------------------------------------
async function loadSdkModules() {
  const tsconfigRaw = await readFile(join(JS_ROOT, "tsconfig.json"), "utf8");
  const tsconfig = JSON.parse(stripJsonComments(tsconfigRaw));
  const paths = tsconfig?.compilerOptions?.paths ?? {};
  const modules = new Set();
  for (const key of Object.keys(paths)) {
    // "gui/*" → allow any "gui/<sub>"; record both the prefix and the wildcard.
    if (key.endsWith("/*")) modules.add(key);
    else modules.add(key);
  }
  return modules;
}

function stripJsonComments(s) {
  // String-aware stripper: only treat // and /* as comments outside of strings.
  let out = "";
  let i = 0;
  let inStr = false;
  while (i < s.length) {
    const c = s[i];
    const n = s[i + 1];
    if (inStr) {
      out += c;
      if (c === "\\" && i + 1 < s.length) {
        out += s[i + 1];
        i += 2;
        continue;
      }
      if (c === '"') inStr = false;
      i++;
      continue;
    }
    if (c === '"') {
      inStr = true;
      out += c;
      i++;
      continue;
    }
    if (c === "/" && n === "/") {
      while (i < s.length && s[i] !== "\n") i++;
      continue;
    }
    if (c === "/" && n === "*") {
      i += 2;
      while (i < s.length && !(s[i] === "*" && s[i + 1] === "/")) i++;
      i += 2;
      continue;
    }
    out += c;
    i++;
  }
  return out;
}

function moduleAllowed(mod, allowlist) {
  if (allowlist.has(mod)) return true;
  for (const entry of allowlist) {
    if (entry.endsWith("/*")) {
      const prefix = entry.slice(0, -2);
      if (mod === prefix || mod.startsWith(prefix + "/")) return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// JS validation
// ---------------------------------------------------------------------------
async function walkJs(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const name of await readdir(dir)) {
    const full = join(dir, name);
    const s = await stat(full);
    if (s.isDirectory()) out.push(...(await walkJs(full)));
    else if (name.endsWith(".js")) out.push(full);
  }
  return out;
}

function parseCheck(file) {
  try {
    execFileSync(process.execPath, ["--check", file], { stdio: "pipe" });
  } catch (e) {
    const msg = (e.stderr?.toString() || e.message).split("\n")[0];
    warn(file, `parse error: ${msg}`);
  }
}

function scanRequires(source) {
  // Matches require("x") and require('x'). Templates use CommonJS-style require.
  const re = /\brequire\s*\(\s*["']([^"']+)["']\s*\)/g;
  const out = [];
  let m;
  while ((m = re.exec(source))) out.push(m[1]);
  return out;
}

async function validateJsFile(file, allowlist) {
  parseCheck(file);
  const src = await readFile(file, "utf8");
  const requires = scanRequires(src);
  for (const mod of requires) {
    if (!moduleAllowed(mod, allowlist)) {
      warn(file, `require("${mod}") is not in the Momentum SDK allowlist`);
    }
  }
}

// ---------------------------------------------------------------------------
// C app validation (application.fam)
// ---------------------------------------------------------------------------
async function findFams(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const name of await readdir(dir)) {
    const full = join(dir, name);
    const s = await stat(full);
    if (s.isDirectory()) out.push(...(await findFams(full)));
    else if (name === "application.fam") out.push(full);
  }
  return out;
}

function famField(src, field) {
  // FAM files use Python-literal syntax. Match field="value" loosely.
  const re = new RegExp(`\\b${field}\\s*=\\s*["']([^"']+)["']`);
  const m = src.match(re);
  return m ? m[1] : null;
}

async function validateFam(file) {
  const src = await readFile(file, "utf8");
  const appid = famField(src, "appid");
  const entry = famField(src, "entry_point");
  const folder = basename(dirname(file));
  const norm = (s) => s.toLowerCase().replace(/-/g, "_");
  if (!appid) warn(file, "missing appid=");
  else if (norm(appid) !== norm(folder)) {
    warn(
      file,
      `appid="${appid}" does not match folder name "${folder}" (after kebab→snake normalisation)`,
    );
  }
  if (!entry) warn(file, "missing entry_point=");
  else {
    const appdir = dirname(file);
    const cFiles = (await readdir(appdir)).filter((n) => n.endsWith(".c"));
    let found = false;
    for (const c of cFiles) {
      const csrc = await readFile(join(appdir, c), "utf8");
      const sigRe = new RegExp(`\\b${entry}\\s*\\(`);
      if (sigRe.test(csrc)) {
        found = true;
        break;
      }
    }
    if (!found) {
      warn(file, `entry_point "${entry}" not defined in any .c file in ${folder}/`);
    }
  }
  await validateStudioSidecar(file);
}

// Flipper GUI Studio apps carry a `*.flipper-gui.json` spec (schema
// "flipper-gui/v1") so they round-trip back into the editor. This is the
// fast first gate: a Studio app must have a parseable, correctly-schema'd
// sidecar. Deep "does the C match the spec" verification is regen-check.mjs.
async function validateStudioSidecar(file) {
  const appdir = dirname(file);
  const fam = await readFile(file, "utf8");
  const sidecars = (await readdir(appdir)).filter((n) => n.endsWith(".flipper-gui.json"));
  const weburl = famField(fam, "fap_weburl") || "";
  const looksLikeStudio = weburl.includes("flipper-gui");

  if (!sidecars.length) {
    if (looksLikeStudio) {
      warn(
        file,
        `looks like a Flipper GUI Studio app (fap_weburl) but has no *.flipper-gui.json spec — the editor round-trip needs it (Export → JSON next to application.fam)`,
      );
    }
    return;
  }
  for (const name of sidecars) {
    const sidecar = join(appdir, name);
    let parsed;
    try {
      parsed = JSON.parse(await readFile(sidecar, "utf8"));
    } catch (e) {
      warn(sidecar, `flipper-gui spec is not valid JSON: ${e.message}`);
      continue;
    }
    if (parsed?.schema !== "flipper-gui/v1") {
      warn(sidecar, `flipper-gui spec must declare "schema": "flipper-gui/v1"`);
    }
  }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
const allowlist = await loadSdkModules();

const jsFiles = [
  ...(await walkJs(join(JS_ROOT, "templates"))),
  ...(await walkJs(join(JS_ROOT, "examples"))),
];
for (const f of jsFiles) await validateJsFile(f, allowlist);

const famFiles = await findFams(join(REPO_ROOT, "C-Apps"));
for (const f of famFiles) await validateFam(f);

console.log(
  `validate: checked ${jsFiles.length} JS file(s), ${famFiles.length} FAM file(s)`,
);

if (errors.length) {
  console.error("\nvalidate: FAILED");
  for (const e of errors) console.error(`  - ${e}`);
  process.exit(1);
}
console.log("validate: OK");
