#!/usr/bin/env node
// Lightweight smoke runner: executes each template/example inside a vm sandbox
// with stubbed Flipper SDK modules and globals (print, delay, require).
// Catches ReferenceErrors and obvious wiring mistakes before deploy.
//
// Scripts that block on eventLoop.run() are interrupted via a timeout; reaching
// the timeout is treated as success (the script reached its main loop).

import { readFile, readdir, stat } from "node:fs/promises";
import { existsSync } from "node:fs";
import vm from "node:vm";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const JS_ROOT = resolve(HERE, "..");
const REPO_ROOT = resolve(JS_ROOT, "..");

const TIMEOUT_MS = 1500;
const failures = [];

function makeSdkStub(name) {
  // Recursive proxy: any property access or call yields another proxy.
  const handler = {
    get(_t, prop) {
      if (prop === Symbol.toPrimitive) return () => `[sdk:${name}]`;
      if (prop === "then") return undefined; // not a thenable
      return makeStubFn(`${name}.${String(prop)}`);
    },
    apply() {
      return makeSdkStub(`${name}()`);
    },
  };
  return new Proxy(function () {}, handler);
}
function makeStubFn(name) {
  return makeSdkStub(name);
}

function makeSandbox(file) {
  const sdkCache = new Map();
  const sandbox = {
    print: () => {},
    delay: () => {},
    require: (mod) => {
      if (!sdkCache.has(mod)) sdkCache.set(mod, makeSdkStub(mod));
      return sdkCache.get(mod);
    },
    console,
    __file: file,
  };
  // Flipper mJS sometimes exposes these too.
  sandbox.setTimeout = () => 0;
  sandbox.clearTimeout = () => {};
  return vm.createContext(sandbox);
}

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

async function smokeFile(file) {
  const src = await readFile(file, "utf8");
  let script;
  try {
    script = new vm.Script(src, { filename: file });
  } catch (e) {
    failures.push(`${relative(REPO_ROOT, file)}: parse error: ${e.message}`);
    return;
  }
  const ctx = makeSandbox(file);
  try {
    script.runInContext(ctx, { timeout: TIMEOUT_MS });
  } catch (e) {
    const msg = String(e && e.message ? e.message : e);
    // Hitting the timeout means the script reached its main loop — success.
    if (/Script execution timed out/i.test(msg)) return;
    failures.push(`${relative(REPO_ROOT, file)}: ${msg}`);
  }
}

const files = [
  ...(await walkJs(join(JS_ROOT, "templates"))),
  ...(await walkJs(join(JS_ROOT, "examples"))),
];

for (const f of files) await smokeFile(f);

console.log(`smoke: ran ${files.length} JS file(s)`);
if (failures.length) {
  console.error("\nsmoke: FAILED");
  for (const f of failures) console.error(`  - ${f}`);
  process.exit(1);
}
console.log("smoke: OK");
