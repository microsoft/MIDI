// Shared note math for the comps. 60 = C3, the Windows MIDI Services convention.
const SHARP = ['C','C♯','D','E♭','E','F','F♯','G','A♭','A','B♭','B'];
const SCALES = { major:[0,2,4,5,7,9,11], minor:[0,2,3,5,7,8,10], penta:[0,2,4,7,9], chromatic:[0,1,2,3,4,5,6,7,8,9,10,11] };
function nm(n) { return SHARP[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 2); }
function inScale(n, root, sc) { return SCALES[sc].includes((((n - root) % 12) + 12) % 12); }
// Note at scale step k counted from a root note.
function stepNote(rootNote, sc, k) { const s = SCALES[sc]; const o = Math.floor(k / s.length); const i = ((k % s.length) + s.length) % s.length; return rootNote + 12 * o + s[i]; }
// rows x cols grid; row 0 is the bottom. Returns note for (row, col).
function gridNote(opt, row, col) {
  if (opt.inKey) return stepNote(opt.first, opt.scale, col * opt.right + row * opt.up);
  return opt.first + col * opt.right + row * opt.up;
}
function padClass(opt, n) {
  if (n < 0 || n > 127) return 'p x';
  if (((n - opt.root) % 12 + 12) % 12 === 0) return 'p r';
  if (!opt.inKey && !inScale(n, opt.root, opt.scale)) return 'p o';
  return 'p';
}
// Draws a grid. dir 'rows': rows stack bottom to top. dir 'cols': columns stack left to right, pads run bottom to top.
function drawGrid(el, opt) {
  const lines = opt.lines, along = opt.along;
  const isCols = opt.dir === 'cols';
  el.style.gridTemplateColumns = `repeat(${isCols ? lines : along}, 1fr)`;
  const cells = [];
  const R = isCols ? along : lines, C = isCols ? lines : along;
  for (let vr = R - 1; vr >= 0; vr--) for (let vc = 0; vc < C; vc++) {
    const line = isCols ? vc : vr, pos = isCols ? vr : vc;
    const n = gridNote(opt, line, pos);
    let cls = padClass(opt, n);
    if (opt.held && opt.held.some(h => h[0] === line && h[1] === pos)) cls = 'p h';
    else if (opt.held && opt.showDupes && opt.held.some(h => gridNote(opt, h[0], h[1]) === n)) cls += ' dupe';
    cells.push(`<div class="${cls}" style="font-size:${opt.fs || 13}px">${n >= 0 && n <= 127 ? nm(n) : ''}</div>`);
  }
  el.innerHTML = cells.join('');
}
