import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';

const root = fileURLToPath(new URL('../../../', import.meta.url));
const source = path.join(root, 'examples/notes/build');
const out = path.join(root, 'artifacts/course-completion/build/verification');
fs.mkdirSync(out, { recursive: true });
const cc = process.env.CC || 'gcc';
const cmake = process.env.CMAKE || 'cmake';
const ninja = process.env.NINJA || 'ninja';
const arm = process.env.ARM_PREFIX || 'arm-none-eabi-';
const exe = process.platform === 'win32' ? '.exe' : '';
const records = [];
function run(label, command, args, failure = false) {
  const result = spawnSync(command, args, { cwd: root, encoding: 'utf8' });
  if (result.error) throw result.error;
  const text = (result.stdout || '') + (result.stderr || '');
  fs.writeFileSync(path.join(out, label + '.log'),
    JSON.stringify({ command, args, cwd: root, exit: result.status }) + '\n' + text);
  if (failure) assert.notEqual(result.status, 0, label + ' must fail');
  else assert.equal(result.status, 0, label + '\n' + text);
  records.push({ label, exit: result.status, expectedFailure: failure });
  return text;
}
function configure(label, src, build) {
  return run(label, cmake, ['-S', src, '-B', build, '-G', 'Ninja',
    '-DCMAKE_C_COMPILER=' + cc, '-DCMAKE_MAKE_PROGRAM=' + ninja,
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCMAKE_BUILD_TYPE=Debug']);
}
function build(label, dir) {
  return run(label, cmake, ['--build', dir, '--verbose']);
}
run('host-version', cc, ['--version']);
run('host-target', cc, ['-dumpmachine']);
run('cmake-version', cmake, ['--version']);
run('ninja-version', ninja, ['--version']);
run('arm-version', arm + 'gcc', ['--version']);
const pc = path.join(out, 'pc');
configure('pc-configure', source, pc);
build('pc-build', pc);
run('pc-tests', path.join(path.dirname(cmake), 'ctest' + exe),
    ['--test-dir', pc, '--output-on-failure']);
assert.match(run('pc-clamp', path.join(pc, 'clamp_demo' + exe), []), /clamp=0,10 calls=2 mode=1/);
assert.match(run('pc-probe', path.join(pc, 'toolchain_probe' + exe), []), /c99 ok: sum=55/);
assert.match(run('pc-generated', path.join(pc, 'generated_demo' + exe), []), /records=7/);

const prep = path.join(out, 'stages');
fs.mkdirSync(prep, { recursive: true });
const flags = ['-std=c99', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-fno-common',
               '-I', path.join(source, 'include')];
run('preprocess', cc, [...flags, '-E', '-P', path.join(source, 'src/clamp.c'), '-o', path.join(prep, 'clamp.i')]);
assert.match(fs.readFileSync(path.join(prep, 'clamp.i'), 'utf8'), /value > 10/);
run('assembly', cc, [...flags, '-S', path.join(source, 'src/clamp.c'), '-o', path.join(prep, 'clamp.s')]);
for (const name of ['main', 'clamp']) {
  run('compile-' + name, cc, [...flags, '-c', path.join(source, 'src/' + name + '.c'), '-o', path.join(prep, name + '.o')]);
}
const mainO = path.join(prep, 'main.o');
const clampO = path.join(prep, 'clamp.o');
run('link', cc, [mainO, clampO, '-o', path.join(prep, 'manual' + exe)]);
assert.match(run('manual-run', path.join(prep, 'manual' + exe), []), /clamp=0,10 calls=2/);
assert.match(run('missing-definition', cc, [mainO, '-o', path.join(prep, 'missing' + exe)], true), /undefined reference/);
assert.match(run('duplicate-definition', cc, [mainO, clampO, clampO, '-o', path.join(prep, 'duplicate' + exe)], true), /multiple definition/);
const pe = fs.readFileSync(path.join(pc, 'toolchain_probe' + exe));
if (process.platform === 'win32') {
  assert.equal(pe.toString('ascii', 0, 2), 'MZ');
  const peOffset = pe.readUInt32LE(0x3c);
  assert.equal(pe.readUInt32LE(peOffset), 0x00004550);
}

const incSource = path.join(out, 'incremental-source');
fs.cpSync(source, incSource, { recursive: true, force: true });
const inc = path.join(out, 'incremental');
configure('inc-configure', incSource, inc);
build('inc-initial', inc);
assert.match(build('inc-noop', inc), /no work to do/);
fs.appendFileSync(path.join(incSource, 'src/main.c'), '\n/* Main-only edit. */\n');
let text = build('inc-main', inc);
assert.match(text, /main\.c/);
assert.doesNotMatch(text, / -c .*clamp\.c/);
fs.appendFileSync(path.join(incSource, 'include/config.h'), '\n/* Private-header edit. */\n');
text = build('inc-private-header', inc);
assert.match(text, /clamp\.c/);
assert.doesNotMatch(text, / -c .*main\.c/);
fs.appendFileSync(path.join(incSource, 'include/clamp.h'), '\n/* Public-header edit. */\n');
text = build('inc-public-header', inc);
assert.match(text, / -c .*main\.c/);
assert.match(text, / -c .*clamp\.c/);
run('inc-mode-configure', cmake, ['-S', incSource, '-B', inc, '-DREPORT_MODE=OFF']);
text = build('inc-mode', inc);
assert.match(text, / -c .*main\.c/);
assert.doesNotMatch(text, / -c .*clamp\.c/);
assert.match(run('inc-mode-run', path.join(inc, 'clamp_demo' + exe), []), /mode=0/);
fs.writeFileSync(path.join(incSource, 'schema.txt'), '9\n');
text = build('inc-generator', inc);
assert.match(text, /generate\.cmake/);
assert.match(run('inc-generator-run', path.join(inc, 'generated_demo' + exe), []), /records=9/);
assert.match(build('inc-final-noop', inc), /no work to do/);

const armOut = path.join(out, 'arm');
fs.mkdirSync(armOut, { recursive: true });
const armFlags = ['-mcpu=cortex-m4', '-mthumb', '-O0', '-g', '-ffreestanding',
 '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-ffunction-sections', '-fdata-sections'];
for (const [name, file] of [['startup', 'arm/startup.S'], ['image', 'arm/image.c'],
 ['weak-main', 'weak/main.c'], ['default', 'weak/default.c'], ['override', 'weak/override.c']]) {
  run('arm-compile-' + name, arm + 'gcc', [...armFlags,
   ...(file.endsWith('.c') ? ['-std=c99', '-Wall', '-Wextra', '-Werror'] : []),
   '-c', path.join(source, file), '-o', path.join(armOut, name + '.o')]);
}
function armLink(name, extra) {
  const file = path.join(armOut, name + '.elf');
  run('arm-link-' + name, arm + 'gcc', [...armFlags, '-nostdlib',
   '-T', path.join(source, 'arm/layout.ld'), '-Wl,--gc-sections',
   '-Wl,-Map=' + path.join(armOut, name + '.map'), path.join(armOut, 'startup.o'), ...extra, '-o', file]);
  return file;
}
const image = armLink('image', [path.join(armOut, 'image.o')]);
run('elf-header-sections', arm + 'readelf', ['-h', '-S', '-l', image]);
run('elf-sections-vma-lma', arm + 'objdump', ['-h', image]);
run('elf-symbols', arm + 'nm', ['-n', image]);
run('elf-relocations', arm + 'readelf', ['-r', path.join(armOut, 'image.o')]);
run('elf-disassembly', arm + 'objdump', ['-d', image]);
const bin = path.join(armOut, 'image.bin');
run('elf-to-bin', arm + 'objcopy', ['-O', 'binary', image, bin]);
run('elf-to-hex', arm + 'objcopy', ['-O', 'ihex', image, path.join(armOut, 'image.hex')]);
const elf = fs.readFileSync(image);
assert.deepEqual([...elf.subarray(0, 4)], [0x7f, 0x45, 0x4c, 0x46]);
assert.equal(elf[4], 1);
assert.equal(elf[5], 1);
assert.equal(elf.readUInt16LE(18), 40);
const binary = fs.readFileSync(bin);
assert.equal(binary.readUInt32LE(0), 0x20020000);
assert.equal(binary.readUInt32LE(4) & 1, 1);
const weakBase = [path.join(armOut, 'weak-main.o'), path.join(armOut, 'default.o')];
const override = path.join(armOut, 'override.o');
const archive = path.join(armOut, 'liboverride.a');
run('arm-archive', arm + 'ar', ['rcs', archive, override]);
for (const [name, objects, binding] of [
 ['weak-default', weakBase, 'W'],
 ['weak-direct', [...weakBase, override], 'T'],
 ['weak-archive', [...weakBase, archive], 'W'],
 ['weak-whole', [...weakBase, '-Wl,--whole-archive', archive, '-Wl,--no-whole-archive'], 'T']]) {
  const file = armLink(name, objects);
  const symbols = run(name + '-symbols', arm + 'nm', [file]);
  assert.match(symbols, new RegExp('\\b' + binding + ' on_event\\b'));
  run(name + '-disassembly', arm + 'objdump', ['-d', file]);
}
fs.writeFileSync(path.join(out, 'results.json'), JSON.stringify({
  scope: 'PC executed; ARM offline only; no board or fresh MSYS2 installation',
  binaryBytes: binary.length, elfBytes: elf.length, records
}, null, 2) + '\n');
console.log('PASS: ' + records.length + ' commands; PC tests, incremental cases, PE/ELF, weak bindings.');
