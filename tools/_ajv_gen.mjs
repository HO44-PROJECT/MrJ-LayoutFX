// _ajv_gen.mjs — invoked by gen_config_validator.py.
//
// Compiles a JSON Schema file into a standalone, dependency-free JS validator
// module using Ajv's standalone code-generation mode (https://ajv.js.org/standalone.html).
// Ajv itself is dev-tooling only: it runs here, at build time, in Node — the
// generated output has no require()/import of Ajv (or anything else) and is
// safe to bundle into the browser-side WebUI.
//
// Ajv's standalone output is NOT fully self-contained by itself: some keywords
// (e.g. additionalProperties, array equality) emit `require("ajv/dist/runtime/...")`
// calls for small shared helpers instead of inlining them (documented behavior —
// Ajv expects a bundler to resolve these). esbuild does that resolution here,
// producing one flat IIFE with zero require()/import left in it.
//
// Usage: node _ajv_gen.mjs <schema.json> <out.js>

import Ajv from "ajv";
import standaloneCode from "ajv/dist/standalone/index.js";
import esbuild from "esbuild";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const [, , schemaPath, outPath] = process.argv;
if (!schemaPath || !outPath) {
  console.error("usage: node _ajv_gen.mjs <schema.json> <out.js>");
  process.exit(1);
}

const schema = JSON.parse(fs.readFileSync(schemaPath, "utf8"));
const ajv = new Ajv({ code: { source: true, esm: false }, allErrors: true });

// config.schema.json declares "$schema": "https://json-schema.org/draft-07/schema#"
// but Ajv v8 only self-registers the draft-07 meta-schema under the plain
// "http://json-schema.org/draft-07/schema" key (no scheme-s, no trailing '#').
// Alias the exact URI string the schema file uses to the meta-schema Ajv
// already has loaded, rather than fighting Ajv's internal registration.
const metaSchemaUri = schema.$schema;
if (metaSchemaUri && !ajv.getSchema(metaSchemaUri)) {
  const draft7 = ajv.getSchema("http://json-schema.org/draft-07/schema");
  if (draft7) ajv.addMetaSchema(draft7.schema, metaSchemaUri);
}

const validate = ajv.compile(schema);
const rawCode = standaloneCode(ajv, validate);

// esbuild needs a real .cjs file on disk to resolve the runtime require()s
// (ajv/dist/runtime/equal, /ucs2length, etc. — see comment above) against
// ajv's own node_modules. It must live under tools/ itself (NOT the OS
// tmpdir): esbuild resolves bare specifiers by walking up from the entry
// file's own directory, so anywhere outside tools/node_modules's ancestry
// fails to resolve "ajv/dist/runtime/...".
const _toolsDir = path.dirname(fileURLToPath(import.meta.url));
const tmpFile = path.join(_toolsDir, `._ajv_standalone_tmp_${process.pid}.cjs`);
fs.writeFileSync(tmpFile, rawCode);

let bundled;
try {
  const result = await esbuild.build({
    entryPoints: [tmpFile],
    bundle: true,
    platform: "browser",
    format: "iife",
    globalName: "__configValidatorExports",
    write: false,
    minify: false,
  });
  bundled = result.outputFiles[0].text;
} finally {
  fs.unlinkSync(tmpFile);
}

// The WebUI bundle is plain concatenated <script> code with no module system
// (see build_webui.py) — expose a plain global, matching every other WebUI
// module (var ICONS = ..., var TRANSLATIONS = ...), instead of esbuild's
// __configValidatorExports.default wrapper.
bundled += "\nvar validateConfig = __configValidatorExports.default;\n";

fs.writeFileSync(outPath, bundled);
console.log(`[_ajv_gen] ${schemaPath} -> ${outPath} (${bundled.length} B)`);
