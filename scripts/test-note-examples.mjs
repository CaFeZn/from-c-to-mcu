import { readdir, readFile, mkdir, access, writeFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = fileURLToPath(new URL('../', import.meta.url));
const manifest = JSON.parse(await readFile(path.join(root, 'docs/.vitepress/topic-manifest.json'), 'utf8'));
const groups = [...new Set(manifest.topics.map(topic => topic.group))];
const output = path.join(root, 'artifacts/note-examples');
await mkdir(output, { recursive: true });
async function exists(file) { try { await access(file); return true; } catch { return false; } }
async function projects(directory) {
  if (await exists(path.join(directory, 'CMakeLists.txt'))) return [directory];
  const result = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    if (entry.isDirectory() && !entry.name.startsWith('.') && !entry.name.startsWith('build')) result.push(...await projects(path.join(directory, entry.name)));
  }
  return result;
}
function command(binary, args) {
  const result = spawnSync(binary, args, { cwd: root, encoding: 'utf8', timeout: 120000, windowsHide: true });
  return { binary, args, exitCode: result.status, stdout: result.stdout || '', stderr: result.stderr || '', error: result.error?.message || null };
}
const report = { compilerPolicy: 'Host CMake default toolchain; Debug keeps assertions enabled. No hardware execution.', projects: [], failed: 0, totalTests: 0 };
for (const group of groups) {
  const sourceRoot = path.join(root, 'examples/notes', group);
  const sources = await projects(sourceRoot);
  if (!sources.length) throw new Error('No standalone CMake example for ' + group);
  for (const source of sources) {
    const name = path.relative(path.join(root, 'examples/notes'), source).replaceAll('\\', '/');
    const build = path.join(output, name);
    const steps = [];
    steps.push(command('cmake', ['-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug']));
    if (steps.at(-1).exitCode === 0) steps.push(command('cmake', ['--build', build]));
    if (steps.at(-1).exitCode === 0) steps.push(command('ctest', ['--test-dir', build, '--output-on-failure', '--no-tests=error']));
    const passed = steps.length === 3 && steps.every(step => step.exitCode === 0 && !step.error);
    const match = steps.at(-1).stdout.match(/tests failed out of\s+(\d+)/);
    const tests = match ? Number(match[1]) : 0;
    report.projects.push({ name, passed, tests, steps });
    if (!passed) report.failed++;
    report.totalTests += tests;
    console.log((passed ? 'PASS ' : 'FAIL ') + name + ' (' + tests + ' tests)');
    if (!passed) console.error(steps.at(-1).stdout + steps.at(-1).stderr + (steps.at(-1).error || ''));
  }
}
await writeFile(path.join(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify({ projects: report.projects.length, tests: report.totalTests, failed: report.failed }));
if (report.failed) process.exitCode = 1;
