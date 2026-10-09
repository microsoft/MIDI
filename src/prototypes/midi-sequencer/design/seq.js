// PROTOTYPE. Drawing helpers for the Windows MIDI Sequencer design comps.
//
// The comps declare what goes where in bars and beats, and this file turns that into pixels, so a
// clip can be moved by changing data-at instead of doing coordinate maths by hand:
//   <div class="tclip" data-at="9" data-len="4" data-draw="bass">   a clip on the timeline
//   <div class="mini" data-draw="bass" data-bars="1">              a launcher cell preview
//   <div class="roll" data-roll="bassA">                            a piano roll
//   <div class="scells" data-steps='{...}'>                         a row of steps
//   <svg class="euclid" data-rings='[...]'>                         Euclidean rings
//
// Opened with ?check, a page also writes its own layout problems into <pre id="report"> so
// check.ps1 can read them with headless Edge and nobody has to look at a picture first.

const SVG_NS = "http://www.w3.org/2000/svg";

function mulberry32(seed) {
    return function () {
        seed |= 0;
        seed = (seed + 0x6D2B79F5) | 0;
        let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

function svgEl(name, attrs, parent) {
    const el = document.createElementNS(SVG_NS, name);

    for (const [k, v] of Object.entries(attrs || {})) {
        el.setAttribute(k, v);
    }

    if (parent) {
        parent.appendChild(el);
    }

    return el;
}

function newSvg(host) {
    host.querySelectorAll(":scope > svg.drawn").forEach(n => n.remove());
    const w = host.clientWidth;
    const h = host.clientHeight;
    const svg = svgEl("svg", { class: "drawn", viewBox: `0 0 ${w} ${h}`, preserveAspectRatio: "none" });
    host.appendChild(svg);
    return { svg, w, h };
}

function trackColor(el) {
    const c = getComputedStyle(el).getPropertyValue("--tc").trim();
    return c || "#9E9E9E";
}

// ------------------------------------------------------------------ music

// Notes are { t: start in quarter notes, d: length in quarter notes, p: note number, v: 0..1 }.

const BASS_BARS = [
    [40, 40, 52, 40, 43, 40, 47, 45],
    [40, 40, 52, 40, 43, 45, 47, 50],
    [38, 38, 50, 38, 41, 38, 45, 43],
    [36, 36, 48, 36, 43, 47, 48, 52],
];

const ACCENTS = [1, .55, .8, .55, .9, .55, .75, .65];

function bassNotes(bars, variant) {
    const notes = [];

    for (let b = 0; b < bars; b++) {
        const row = BASS_BARS[(b + (variant || 0)) % BASS_BARS.length];

        row.forEach((p, i) => {
            const long = (i === 7 && b % 2 === 1);
            notes.push({ t: b * 4 + i * 0.5, d: long ? 0.48 : 0.42, p, v: ACCENTS[i] });
        });
    }

    return notes;
}

const PAD_CHORDS = [[52, 55, 59, 64], [48, 52, 55, 60], [50, 55, 59, 62], [50, 54, 57, 62]];

function padNotes(bars, beatsPerChord) {
    const notes = [];
    const span = beatsPerChord || 4;

    for (let t = 0, i = 0; t < bars * 4; t += span, i++) {
        PAD_CHORDS[i % PAD_CHORDS.length].forEach(p => notes.push({ t, d: span - 0.1, p, v: .7 }));
    }

    return notes;
}

function stabNotes(bars) {
    const notes = [];

    for (let b = 0; b < bars; b++) {
        const chord = PAD_CHORDS[b % PAD_CHORDS.length].slice(1);
        [1.5, 3.5].forEach(o => chord.forEach(p => notes.push({ t: b * 4 + o, d: 0.3, p: p + 12, v: .85 })));
    }

    return notes;
}

const PENTA = [64, 67, 69, 71, 74, 76, 79];

function leadNotes(bars, seed) {
    const r = mulberry32(seed || 7);
    const notes = [];
    let t = 0;
    let i = 3;

    while (t < bars * 4) {
        const d = [0.5, 0.5, 1, 0.25, 0.25, 1.5][Math.floor(r() * 6)];

        if (r() > 0.22) {
            i = Math.max(0, Math.min(PENTA.length - 1, i + Math.floor(r() * 5) - 2));
            notes.push({ t, d: d * 0.9, p: PENTA[i], v: .6 + r() * .4 });
        }

        t += d;
    }

    return notes;
}

function chanceNotes(bars, seed) {
    const r = mulberry32(seed || 11);
    const scale = [52, 55, 57, 59, 62, 64, 67, 69, 71, 74, 76];
    const notes = [];

    for (let t = 0; t < bars * 4; t += 0.5) {
        if (r() < 0.38) {
            notes.push({ t, d: [0.25, 0.5, 1][Math.floor(r() * 3)], p: scale[Math.floor(r() * scale.length)], v: .4 + r() * .6, gen: true });
        }
    }

    return notes;
}

function euclid(k, n, rotate) {
    // Step i sounds when (i * k) mod n < k: the same spacing as Bjorklund's algorithm, up to rotation.
    const out = [];

    for (let i = 0; i < n; i++) {
        const j = (((i - (rotate || 0)) % n) + n) % n;
        out.push((j * k) % n < k);
    }

    return out;
}

function drumRows(bars, variant) {
    const rows = [[], [], []];

    for (let b = 0; b < bars; b++) {
        const fill = variant === "fill" && b === bars - 1;
        [0, 4, 8, 12].concat(variant === "b" ? [10] : []).forEach(s => rows[0].push(b * 16 + s));
        [4, 12].concat(fill ? [13, 14, 15] : []).forEach(s => rows[1].push(b * 16 + s));

        for (let s = 0; s < 16; s += (variant === "b" ? 1 : 2)) {
            rows[2].push(b * 16 + s);
        }
    }

    return rows;
}

function euclidRows(bars) {
    const specs = [[5, 16, 0], [7, 16, 2], [3, 16, 1]];
    return specs.map(([k, n, r]) => {
        const one = euclid(k, n, r);
        const hits = [];

        for (let b = 0; b < bars; b++) {
            one.forEach((on, i) => { if (on) { hits.push(b * 16 + i); } });
        }

        return hits;
    });
}

function notesFor(name, bars) {
    switch (name) {
        case "bass": return bassNotes(bars, 0);
        case "bassB": return bassNotes(bars, 2);
        case "pad": return padNotes(bars, 4);
        case "swell": return padNotes(bars, 8);
        case "stabs": return stabNotes(bars);
        case "lead": return leadNotes(bars, 7);
        case "lead2": return leadNotes(bars, 31);
        case "take": return leadNotes(bars, 19);
        case "chance": return chanceNotes(bars, 11);
        case "drift": return chanceNotes(bars, 23);
        default: return [];
    }
}

// --------------------------------------------------------- small previews

function drawNotesInto(svg, w, h, notes, totalBeats, color, opts) {
    if (!notes.length) {
        return;
    }

    let low = Math.min(...notes.map(n => n.p));
    let high = Math.max(...notes.map(n => n.p));

    if (high - low < 10) {
        low -= 4;
        high += 4;
    }

    const rows = high - low + 1;
    const rowH = Math.max(1.2, (h - 2) / rows);

    for (const n of notes) {
        const x = n.t / totalBeats * w;
        const nw = Math.max(1.4, n.d / totalBeats * w - 0.5);
        const y = 1 + (high - n.p) / rows * (h - 2);

        if (x > w) {
            continue;
        }

        const rect = svgEl("rect", {
            x: x.toFixed(2), y: y.toFixed(2), width: nw.toFixed(2), height: Math.max(1.4, rowH - 0.4).toFixed(2),
            fill: color, "fill-opacity": (0.55 + 0.45 * (n.v || .8)).toFixed(2), rx: 0.6,
        }, svg);

        if (opts && opts.dashed) {
            rect.setAttribute("fill-opacity", "0.25");
            rect.setAttribute("stroke", color);
            rect.setAttribute("stroke-width", "0.8");
            rect.setAttribute("stroke-dasharray", "1.6 1.2");
        }
    }
}

function drawHitsInto(svg, w, h, rows, totalSteps, color, round) {
    const rowH = (h - 2) / rows.length;

    rows.forEach((hits, r) => {
        for (const s of hits) {
            const x = s / totalSteps * w;
            const sw = Math.max(1.6, w / totalSteps - 0.8);

            if (x > w) {
                continue;
            }

            if (round) {
                const rad = Math.max(1.2, Math.min(rowH, sw) * 0.36);
                svgEl("circle", { cx: (x + sw / 2).toFixed(2), cy: (1 + r * rowH + rowH / 2).toFixed(2), r: rad.toFixed(2), fill: color, "fill-opacity": r === 0 ? 1 : 0.75 }, svg);
            } else {
                svgEl("rect", { x: x.toFixed(2), y: (1 + r * rowH + rowH * 0.18).toFixed(2), width: sw.toFixed(2), height: (rowH * 0.64).toFixed(2), fill: color, "fill-opacity": r === 0 ? 1 : 0.8, rx: 0.6 }, svg);
            }
        }
    });
}

function drawPreview(host, name, bars, color) {
    const { svg, w, h } = newSvg(host);

    if (name === "drums" || name === "drumsB" || name === "drumsFill") {
        const variant = name === "drumsB" ? "b" : name === "drumsFill" ? "fill" : "";
        drawHitsInto(svg, w, h, drumRows(bars, variant), bars * 16, color, false);
    } else if (name === "euclid") {
        drawHitsInto(svg, w, h, euclidRows(bars), bars * 16, color, true);
    } else {
        drawNotesInto(svg, w, h, notesFor(name, bars), bars * 4, color, { dashed: name === "chance" || name === "drift" });
    }
}

// --------------------------------------------------------------- timeline

function barWidth(el) {
    const v = getComputedStyle(el).getPropertyValue("--bar").trim();
    return parseFloat(v) || 34;
}

function layoutTimeline() {
    document.querySelectorAll(".arrange").forEach(arrange => {
        const bar = barWidth(arrange);
        const playheadAt = parseFloat(arrange.dataset.playhead || "0");
        const bars = parseInt(arrange.dataset.bars || "24", 10);

        arrange.querySelectorAll(".arow.ruler > .lane").forEach(lane => {
            for (let i = 1; i <= bars + 1; i++) {
                const num = document.createElement("div");
                num.className = "bar-num";
                num.style.left = ((i - 1) * bar) + "px";
                num.textContent = i;
                lane.appendChild(num);
            }
        });

        arrange.querySelectorAll("[data-at]").forEach(el => {
            const at = parseFloat(el.dataset.at);
            el.style.left = ((at - 1) * bar + (el.classList.contains("tclip") ? 1 : 0)) + "px";

            if (el.dataset.len) {
                el.style.width = (parseFloat(el.dataset.len) * bar - (el.classList.contains("tclip") ? 2 : 0)) + "px";
            }
        });

        if (playheadAt > 0) {
            arrange.querySelectorAll(".arow > .lane").forEach(lane => {
                const line = document.createElement("div");
                line.className = "playhead";
                line.style.left = ((playheadAt - 1) * bar) + "px";
                lane.appendChild(line);
            });
        }

        arrange.querySelectorAll(".tclip").forEach(clip => {
            const color = trackColor(clip);
            const body = clip.querySelector(".body");
            const len = parseFloat(clip.dataset.len);
            const loop = parseFloat(clip.dataset.loop || "0");

            if (body && clip.dataset.draw) {
                const { svg, w, h } = newSvg(body);
                const name = clip.dataset.draw;

                if (name === "drums" || name === "drumsB" || name === "drumsFill") {
                    const variant = name === "drumsB" ? "b" : name === "drumsFill" ? "fill" : "";
                    drawHitsInto(svg, w, h, drumRows(len, variant), len * 16, color, false);
                } else if (name === "euclid") {
                    drawHitsInto(svg, w, h, euclidRows(len), len * 16, color, true);
                } else {
                    drawNotesInto(svg, w, h, notesFor(name, len), len * 4, color, { dashed: name === "chance" || name === "drift" });
                }
            }

            if (loop > 0 && loop < len) {
                for (let at = loop; at < len; at += loop) {
                    const mark = document.createElement("div");
                    mark.className = "loopmark";
                    mark.style.left = (at * bar - 1) + "px";
                    clip.appendChild(mark);
                }
            }
        });

        arrange.querySelectorAll(".lane[data-auto]").forEach(lane => drawAutomation(lane, bar));
        arrange.querySelectorAll(".lane[data-tempo]").forEach(lane => drawTempo(lane, bar));
    });

    document.querySelectorAll(".mini[data-draw]").forEach(host => {
        drawPreview(host, host.dataset.draw, parseFloat(host.dataset.bars || "1"), trackColor(host));
    });
}

function drawAutomation(lane, bar) {
    const spec = JSON.parse(lane.dataset.auto);
    const color = trackColor(lane);
    const { svg, w, h } = newSvg(lane);
    svg.setAttribute("class", "drawn auto-svg");

    const pad = 6;
    const pts = spec.points.map(([b, v]) => [(b - 1) * bar, pad + (1 - v) * (h - pad * 2)]);

    const line = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");
    svgEl("path", { d: `${line} L ${pts[pts.length - 1][0]} ${h} L ${pts[0][0]} ${h} Z`, fill: color, "fill-opacity": ".12" }, svg);
    svgEl("path", { d: line, fill: "none", stroke: color, "stroke-width": "1.6" }, svg);

    pts.forEach((p, i) => {
        if (spec.handles === false) {
            return;
        }

        const selected = spec.selected === i;
        svgEl("circle", { cx: p[0], cy: p[1], r: selected ? 4 : 3, fill: selected ? "#FFF" : "#1E1E1E", stroke: selected ? "#FFF" : color, "stroke-width": "1.5" }, svg);
    });
}

function drawTempo(lane, bar) {
    const spec = JSON.parse(lane.dataset.tempo);
    const { svg, w, h } = newSvg(lane);
    svg.setAttribute("class", "drawn tempo-svg");

    const lo = spec.min;
    const hi = spec.max;
    const y = bpm => 23 + (1 - (bpm - lo) / (hi - lo)) * (h - 29);
    const pts = spec.points.map(([b, bpm]) => [(b - 1) * bar, y(bpm), bpm]);
    const d = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");

    svgEl("path", { d, fill: "none", stroke: "rgba(255,255,255,.75)", "stroke-width": "1.5" }, svg);

    (spec.labels || []).forEach(([b, text]) => {
        const p = pts.find(q => Math.abs(q[0] - (b - 1) * bar) < 0.5) || [(b - 1) * bar, y(lo)];
        const width = text.length * 6 + 8;
        svgEl("rect", { x: p[0] + 6, y: p[1] - 7, width, height: 14, rx: 3, fill: "#262626", stroke: "rgba(255,255,255,.35)", "stroke-width": ".8" }, svg);
        const t = svgEl("text", { x: p[0] + 10, y: p[1] + 3.5, fill: "rgba(255,255,255,.9)", "font-size": "10", "font-family": "Segoe UI" }, svg);
        t.textContent = text;
    });
}

// ------------------------------------------------------------ piano roll

const ROLLS = {
    bassA: () => {
        const notes = bassNotes(4, 0);
        notes[8].sel = true;
        notes[8].v = .63;
        notes[15].bend = [[0, 0], [0.45, 0], [1, 2]];
        notes[21].cents = 25;
        return { low: 33, high: 57, bars: 4, notes };
    },
    bassTall: () => {
        const notes = bassNotes(4, 0);
        notes[8].sel = true;
        notes[8].v = .63;
        notes[15].bend = [[0, 0], [0.45, 0], [1, 2]];
        notes[21].cents = 25;
        const ghosts = padNotes(4, 4).map(n => Object.assign({}, n, { ghost: true }));
        return { low: 28, high: 66, bars: 4, notes, ghosts, scale: [4, 6, 7, 9, 11, 0, 2] };
    },
    leadHook: () => {
        const vibrato = [[0, 0], [0.4, 0], [0.5, .3], [0.6, -.3], [0.7, .3], [0.8, -.3], [0.9, .3], [1, -.3]];
        const slow = [[0, 0], [0.3, 0], [0.4, .2], [0.5, -.2], [0.6, .25], [0.7, -.25], [0.8, .3], [0.9, -.3], [1, 0]];
        const notes = [
            { t: 0, d: 0.95, p: 64, v: .78 },
            { t: 1, d: 0.45, p: 67, v: .62 },
            { t: 1.5, d: 0.45, p: 69, v: .70 },
            { t: 2, d: 1.9, p: 71, v: .80, sel: true, bend: vibrato },
            { t: 4, d: 0.45, p: 74, v: .85 },
            { t: 4.5, d: 0.45, p: 71, v: .60 },
            { t: 5, d: 0.95, p: 69, v: .72, bend: [[0, 0], [0.55, 0], [1, -2]] },
            { t: 6, d: 0.95, p: 67, v: .66 },
            { t: 7, d: 0.9, p: 64, v: .70 },
            { t: 8, d: 0.45, p: 64, v: .60 },
            { t: 8.5, d: 0.45, p: 67, v: .66 },
            { t: 9, d: 0.95, p: 67, v: .82, cents: 50 },
            { t: 10, d: 0.45, p: 69, v: .70 },
            { t: 10.5, d: 0.45, p: 71, v: .64 },
            { t: 11, d: 0.95, p: 74, v: .76, bend: [[0, -1], [0.3, 0], [1, 0]] },
            { t: 12, d: 3.8, p: 76, v: .90, bend: slow },
        ];
        const ghosts = padNotes(4, 4).map(n => Object.assign({}, n, { ghost: true }));
        return { low: 52, high: 79, bars: 4, notes, ghosts, scale: [4, 6, 7, 9, 11, 0, 2] };
    },
    euclidOut: () => {
        const rings = [{ k: 5, n: 16, rot: 0, p: 52, v: .9 }, { k: 7, n: 16, rot: 2, p: 59, v: .7 }, { k: 3, n: 16, rot: 1, p: 67, v: .8 }];
        const notes = [];

        for (let bar = 0; bar < 2; bar++) {
            rings.forEach(r => euclid(r.k, r.n, r.rot).forEach((on, i) => {
                if (on) {
                    notes.push({ t: bar * 4 + i / 4, d: 0.22, p: r.p, v: r.v });
                }
            }));
        }

        return { low: 48, high: 71, bars: 2, notes };
    },
    wander1: () => ({ low: 62, high: 78, bars: 2, notes: wanderPasses()[0] }),
    wander2: () => ({ low: 62, high: 78, bars: 2, notes: wanderPasses()[1] }),
    wander3: () => ({ low: 62, high: 78, bars: 2, notes: wanderPasses()[2] }),
    wander4: () => ({ low: 62, high: 78, bars: 2, notes: wanderPasses()[3] }),
};

// A chance melody that keeps 75% of what it played last pass: each pass is reproducible from the
// clip's seed and the pass number, which is what lets playback jump around and export repeat.
function wanderPasses() {
    if (wanderPasses.cache) {
        return wanderPasses.cache;
    }

    const scale = [64, 67, 69, 71, 74, 76];
    const r = mulberry32(0x4F2A);
    let slots = [];

    for (let i = 0; i < 16; i++) {
        slots.push(r() < 0.5 ? { p: scale[Math.floor(r() * scale.length)], d: 0.45, v: .5 + r() * .45 } : null);
    }

    const passes = [];

    for (let pass = 0; pass < 4; pass++) {
        if (pass > 0) {
            const rp = mulberry32(0x4F2A + pass * 7919);
            slots = slots.map(s => {
                if (rp() < 0.25) {
                    return rp() < 0.5 ? { p: scale[Math.floor(rp() * scale.length)], d: 0.45, v: .5 + rp() * .45, changed: true } : null;
                }

                return s ? Object.assign({}, s, { changed: false }) : null;
            });
        }

        passes.push(slots.map((s, i) => (s ? { t: i * 0.5, d: s.d, p: s.p, v: s.v, changed: !!s.changed } : null)).filter(Boolean));
    }

    wanderPasses.cache = passes;
    return passes;
}

const rollCache = {};

function rollSpec(name) {
    if (!rollCache[name]) {
        rollCache[name] = ROLLS[name]();
    }

    return rollCache[name];
}

function isBlack(p) {
    return [1, 3, 6, 8, 10].includes(p % 12);
}

function noteName(p) {
    // Middle C (60) is C3 here, matching the SDK helper and MIDI Patchbay.
    return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][p % 12] + (Math.floor(p / 12) - 2);
}

function drawKeys(host) {
    const spec = rollSpec(host.dataset.keys);
    const { svg, w, h } = newSvg(host);
    const rows = spec.high - spec.low + 1;
    const rowH = h / rows;

    for (let p = spec.low; p <= spec.high; p++) {
        const y = (spec.high - p) * rowH;
        const black = isBlack(p);
        svgEl("rect", { x: 0, y: y.toFixed(2), width: black ? w * 0.62 : w, height: rowH.toFixed(2), fill: black ? "#1B1B1B" : "#D9D9D9", stroke: "#8A8A8A", "stroke-width": ".4" }, svg);

        if (p % 12 === 0 && rowH >= 6) {
            const t = svgEl("text", { x: w - 3, y: (y + rowH - 1.5).toFixed(2), "text-anchor": "end", fill: "#333", "font-size": Math.min(10, rowH).toFixed(1), "font-family": "Segoe UI" }, svg);
            t.textContent = noteName(p);
        }
    }
}

function drawRoll(host) {
    const spec = rollSpec(host.dataset.roll);
    const color = trackColor(host);
    const { svg, w, h } = newSvg(host);
    const rows = spec.high - spec.low + 1;
    const rowH = h / rows;
    const beats = spec.bars * 4;
    const x = t => t / beats * w;

    for (let p = spec.low; p <= spec.high; p++) {
        const y = (spec.high - p) * rowH;
        const inScale = !spec.scale || spec.scale.includes(p % 12);
        const fill = isBlack(p) ? "rgba(0,0,0,.20)" : "rgba(255,255,255,.025)";
        svgEl("rect", { x: 0, y: y.toFixed(2), width: w, height: rowH.toFixed(2), fill }, svg);

        if (spec.scale && inScale) {
            svgEl("rect", { x: 0, y: y.toFixed(2), width: w, height: rowH.toFixed(2), fill: "rgba(96,205,255,.045)" }, svg);
        }

        if (p % 12 === 0) {
            svgEl("line", { x1: 0, x2: w, y1: (y + rowH).toFixed(2), y2: (y + rowH).toFixed(2), stroke: "rgba(255,255,255,.14)", "stroke-width": "1" }, svg);
        }
    }

    for (let s = 0; s <= beats * 4; s++) {
        const xx = x(s / 4);
        const strong = s % 16 === 0 ? .22 : s % 4 === 0 ? .10 : .04;
        svgEl("line", { x1: xx, x2: xx, y1: 0, y2: h, stroke: `rgba(255,255,255,${strong})`, "stroke-width": "1" }, svg);
    }

    for (const n of spec.ghosts || []) {
        if (n.p < spec.low || n.p > spec.high) {
            continue;
        }

        svgEl("rect", { x: x(n.t) + 0.5, y: ((spec.high - n.p) * rowH + 1).toFixed(2), width: Math.max(2, x(n.d) - 1), height: Math.max(2, rowH - 2), fill: "none", stroke: "rgba(255,255,255,.22)", "stroke-dasharray": "3 2", rx: 2 }, svg);
    }

    spec.notes.forEach(n => {
        if (n.p < spec.low || n.p > spec.high) {
            return;
        }

        const nx = x(n.t) + 0.5;
        const nw = Math.max(3, x(n.d) - 1);
        const ny = (spec.high - n.p) * rowH + 1;
        const nh = Math.max(3, rowH - 2);
        const op = 0.45 + 0.55 * n.v;

        svgEl("rect", { x: nx.toFixed(2), y: ny.toFixed(2), width: nw.toFixed(2), height: nh.toFixed(2), rx: 2, fill: color, "fill-opacity": (n.changed ? 1 : op).toFixed(2), stroke: (n.sel || n.changed) ? "#FFF" : "rgba(0,0,0,.55)", "stroke-width": n.sel ? "1.6" : n.changed ? "1.3" : "1" }, svg);

        if (n.sel && nh >= 8) {
            svgEl("rect", { x: (nx + nw - 3).toFixed(2), y: (ny + 2).toFixed(2), width: 2, height: (nh - 4).toFixed(2), fill: "#FFF", rx: 1 }, svg);
        }

        if (n.bend) {
            const pts = n.bend.map(([f, semis]) => [nx + f * nw, ny + nh / 2 - semis * rowH]);
            const d = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");
            svgEl("path", { d, fill: "none", stroke: "#FFF", "stroke-width": "1.4" }, svg);
            pts.forEach(p => svgEl("circle", { cx: p[0], cy: p[1], r: 2, fill: "#FFF" }, svg));
        }

        if (n.cents && nh >= 8) {
            const t = svgEl("text", { x: (nx + 3).toFixed(1), y: (ny + nh - 2).toFixed(1), fill: "#FFF", "font-size": Math.min(10, nh - 1).toFixed(1), "font-family": "Segoe UI", "font-weight": "600" }, svg);
            t.textContent = "+" + n.cents + "\u00A2";
        }
    });
}

function drawVelocity(host) {
    const spec = rollSpec(host.dataset.vel);
    const color = trackColor(host);
    const { svg, w, h } = newSvg(host);
    const beats = spec.bars * 4;

    for (let b = 0; b <= spec.bars; b++) {
        svgEl("line", { x1: b * 4 / beats * w, x2: b * 4 / beats * w, y1: 0, y2: h, stroke: "rgba(255,255,255,.14)" }, svg);
    }

    svgEl("line", { x1: 0, x2: w, y1: h / 2, y2: h / 2, stroke: "rgba(255,255,255,.07)", "stroke-dasharray": "3 3" }, svg);

    spec.notes.forEach(n => {
        const xx = n.t / beats * w + 1.5;
        const top = 4 + (1 - n.v) * (h - 8);
        svgEl("line", { x1: xx, x2: xx, y1: h, y2: top, stroke: n.sel ? "#FFF" : color, "stroke-width": n.sel ? "2.4" : "2" }, svg);
        svgEl("circle", { cx: xx, cy: top, r: n.sel ? 3.4 : 2.6, fill: n.sel ? "#FFF" : color }, svg);
    });
}

function drawCurveLane(host) {
    const spec = JSON.parse(host.dataset.curve);
    const color = spec.color || trackColor(host);
    const { svg, w, h } = newSvg(host);
    const bars = spec.bars || 4;

    for (let b = 0; b <= bars; b++) {
        svgEl("line", { x1: b / bars * w, x2: b / bars * w, y1: 0, y2: h, stroke: "rgba(255,255,255,.14)" }, svg);
    }

    if (spec.center) {
        svgEl("line", { x1: 0, x2: w, y1: h / 2, y2: h / 2, stroke: "rgba(255,255,255,.18)", "stroke-dasharray": "3 3" }, svg);
    }

    (spec.segments || []).forEach(seg => {
        const pts = seg.map(([beat, v]) => [beat / (bars * 4) * w, 4 + (1 - v) * (h - 8)]);
        const d = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");

        if (!spec.center) {
            svgEl("path", { d: `${d} L ${pts[pts.length - 1][0]} ${h} L ${pts[0][0]} ${h} Z`, fill: color, "fill-opacity": ".13" }, svg);
        }

        svgEl("path", { d, fill: "none", stroke: color, "stroke-width": "1.6" }, svg);
        pts.forEach((p, i) => {
            if (seg.length <= 12 || i % 3 === 0) {
                svgEl("circle", { cx: p[0], cy: p[1], r: 2.6, fill: "#1E1E1E", stroke: color, "stroke-width": "1.4" }, svg);
            }
        });
    });
}

function drawBendLane(host) {
    const spec = rollSpec(host.dataset.bend);
    const color = trackColor(host);
    const { svg, w, h } = newSvg(host);
    const beats = spec.bars * 4;
    const range = 2;
    const y = semis => h / 2 - semis / range * (h / 2 - 6);

    for (let b = 0; b <= spec.bars; b++) {
        svgEl("line", { x1: b * 4 / beats * w, x2: b * 4 / beats * w, y1: 0, y2: h, stroke: "rgba(255,255,255,.14)" }, svg);
    }

    svgEl("line", { x1: 0, x2: w, y1: h / 2, y2: h / 2, stroke: "rgba(255,255,255,.18)", "stroke-dasharray": "3 3" }, svg);

    spec.notes.forEach(n => {
        const x0 = n.t / beats * w;
        const x1 = (n.t + n.d) / beats * w;
        svgEl("rect", { x: x0 + 0.5, y: h / 2 - 2, width: Math.max(2, x1 - x0 - 1), height: 4, rx: 2, fill: color, "fill-opacity": ".22" }, svg);

        if (!n.bend) {
            return;
        }

        const pts = n.bend.map(([f, semis]) => [x0 + f * (x1 - x0), y(semis)]);
        const d = pts.map((p, i) => (i ? "L" : "M") + p[0].toFixed(1) + " " + p[1].toFixed(1)).join(" ");
        svgEl("path", { d, fill: "none", stroke: n.sel ? "#FFF" : color, "stroke-width": n.sel ? "1.8" : "1.5" }, svg);
        pts.forEach(p => svgEl("circle", { cx: p[0], cy: p[1], r: 2.4, fill: "#1E1E1E", stroke: n.sel ? "#FFF" : color, "stroke-width": "1.3" }, svg));
    });
}

function drawRulerMarks(host) {
    const bars = parseInt(host.dataset.bars || "4", 10);
    const first = parseInt(host.dataset.first || "1", 10);
    const w = host.clientWidth;

    for (let b = 0; b < bars; b++) {
        for (let q = 0; q < 4; q++) {
            const s = document.createElement("span");
            s.style.left = ((b * 4 + q) / (bars * 4) * w) + "px";

            if (q === 0) {
                s.textContent = String(first + b);
            } else {
                s.style.borderLeftColor = "rgba(255,255,255,.07)";
                s.style.top = "12px";
            }

            host.appendChild(s);
        }
    }
}

// --------------------------------------------------------------- steps

function buildSteps(host) {
    const spec = JSON.parse(host.dataset.steps);
    const n = spec.n || 16;
    const len = spec.len || n;
    const on = new Set(spec.on || []);
    const vel = spec.vel || {};
    const ties = new Set(spec.tie || []);

    for (let i = 0; i < n; i++) {
        if (i > 0 && i % 4 === 0) {
            const gap = document.createElement("div");
            gap.className = "gap";
            host.appendChild(gap);
        }

        const step = document.createElement("div");
        step.className = "step" + (i % 4 === 0 ? " beat" : "");

        if (i >= len) {
            step.className = "step oob";
            host.appendChild(step);
            continue;
        }

        if (on.has(i)) {
            step.classList.add("on");
            const v = document.createElement("div");
            v.className = "v";
            v.style.height = Math.round((vel[i] !== undefined ? vel[i] : spec.v || .8) * 100) + "%";
            v.style.opacity = ".55";
            step.appendChild(v);
        }

        if (ties.has(i)) {
            step.classList.add("tie");
        }

        const badge = [];

        if (spec.note && spec.note[i]) { badge.push(spec.note[i]); }
        if (spec.prob && spec.prob[i] !== undefined) { badge.push(spec.prob[i] + "%"); }
        if (spec.cond && spec.cond[i]) { badge.push(spec.cond[i]); }

        if (badge.length) {
            const b = document.createElement("div");
            b.className = "badge";
            b.textContent = badge.join(" ");
            step.appendChild(b);
        }

        if (spec.rt && spec.rt[i]) {
            const rt = document.createElement("div");
            rt.className = "rt";

            for (let k = 0; k < spec.rt[i]; k++) {
                rt.appendChild(document.createElement("i"));
            }

            step.appendChild(rt);
        }

        if (spec.nudge && spec.nudge[i]) {
            const nd = document.createElement("div");
            nd.className = "nudge";
            nd.textContent = spec.nudge[i] > 0 ? "\u25B8" : "\u25C2";
            nd.style.right = spec.nudge[i] > 0 ? "3px" : "auto";
            nd.style.left = spec.nudge[i] > 0 ? "auto" : "3px";
            step.appendChild(nd);
        }

        if (spec.sel === i) { step.classList.add("sel"); }
        if (spec.cur === i) { step.classList.add("cur"); }

        host.appendChild(step);
    }
}

// --------------------------------------------------------------- rings

function drawRings(svg) {
    const rings = JSON.parse(svg.dataset.rings);
    const size = svg.clientWidth;
    const c = size / 2;
    svg.setAttribute("viewBox", `0 0 ${size} ${size}`);
    const playStep = parseInt(svg.dataset.play || "-1", 10);

    rings.forEach((ring, idx) => {
        const r = c - 14 - idx * (size * 0.14);
        const hits = euclid(ring.k, ring.n, ring.rot || 0);
        svgEl("circle", { cx: c, cy: c, r, fill: "none", stroke: "rgba(255,255,255,.10)", "stroke-width": "1" }, svg);

        const pts = [];

        hits.forEach((on, i) => {
            const a = -Math.PI / 2 + i / ring.n * Math.PI * 2;
            const x = c + r * Math.cos(a);
            const y = c + r * Math.sin(a);

            if (on) {
                pts.push([x, y]);
            }

            const isPlay = Math.floor(playStep * ring.n / 16) === i && playStep >= 0;
            svgEl("circle", {
                cx: x.toFixed(2), cy: y.toFixed(2), r: on ? 6 : 2.6,
                fill: on ? ring.color : "rgba(255,255,255,.22)",
                stroke: isPlay ? "#FFF" : "none", "stroke-width": isPlay ? 2 : 0,
            }, svg);
        });

        if (pts.length > 2) {
            svgEl("polygon", { points: pts.map(p => p.map(v => v.toFixed(1)).join(",")).join(" "), fill: ring.color, "fill-opacity": ".07", stroke: ring.color, "stroke-opacity": ".35", "stroke-width": "1" }, svg);
        }
    });

    if (playStep >= 0) {
        const a = -Math.PI / 2 + playStep / 16 * Math.PI * 2;
        svgEl("line", { x1: c, y1: c, x2: c + (c - 6) * Math.cos(a), y2: c + (c - 6) * Math.sin(a), stroke: "rgba(255,255,255,.55)", "stroke-width": "1.2" }, svg);
    }
}

// --------------------------------------------------------------- driver

function drawAll() {
    layoutTimeline();
    document.querySelectorAll(".keys[data-keys]").forEach(drawKeys);
    document.querySelectorAll(".roll[data-roll]").forEach(drawRoll);
    document.querySelectorAll(".vlane[data-vel]").forEach(drawVelocity);
    document.querySelectorAll(".tlane[data-bend]").forEach(drawBendLane);
    document.querySelectorAll("[data-curve]").forEach(drawCurveLane);
    document.querySelectorAll(".marks[data-bars]").forEach(drawRulerMarks);
    document.querySelectorAll(".scells[data-steps]").forEach(buildSteps);
    document.querySelectorAll("svg.euclid[data-rings]").forEach(drawRings);

    document.querySelectorAll("[data-count-changed]").forEach(el => {
        const count = rollSpec(el.dataset.countChanged).notes.filter(n => n.changed).length;
        el.textContent = count === 0 ? "First pass" : count === 1 ? "1 new note" : count + " new notes";
    });
}

function checkLayout() {
    const problems = [];
    const rectOf = el => el.getBoundingClientRect();
    const overlaps = (a, b) => a.left < b.right - 1 && b.left < a.right - 1 && a.top < b.bottom - 1 && b.top < a.bottom - 1;

    document.querySelectorAll(".lane").forEach(lane => {
        const clips = [...lane.querySelectorAll(":scope > .tclip")];

        for (let i = 0; i < clips.length; i++) {
            for (let j = i + 1; j < clips.length; j++) {
                if (overlaps(rectOf(clips[i]), rectOf(clips[j]))) {
                    problems.push(`clips overlap: "${clips[i].textContent.trim().slice(0, 24)}" / "${clips[j].textContent.trim().slice(0, 24)}"`);
                }
            }
        }
    });

    const textSelectors = ".tname, .tsub, .cap .t, .cell .nm, .chip, .btn, .field, .scene .nm, .slabel .n, .slabel .s, .lanehead .ln, .lanehead .ls, .kv .v, .lbl, .insp-head .t, .menu-item, .table td, .marker, .lcd .big";

    document.querySelectorAll(textSelectors).forEach(el => {
        if (el.offsetParent !== null && el.scrollWidth > el.clientWidth) {
            problems.push(`cut off: "${el.textContent.trim().replace(/\s+/g, " ").slice(0, 60)}"`);
        }
    });

    document.querySelectorAll(".window").forEach(win => {
        const bounds = rectOf(win);
        win.querySelectorAll(".dlg-body > *, .insp-body > *, .card, .infobar, .srow, .gcard").forEach(el => {
            const r = rectOf(el);

            if (r.height > 0 && (r.bottom > bounds.bottom + 1 || r.right > bounds.right + 1)) {
                problems.push(`outside the window: "${el.textContent.trim().replace(/\s+/g, " ").slice(0, 50)}"`);
            }
        });
    });

    document.querySelectorAll(".dialog").forEach(dialog => {
        const r = rectOf(dialog);
        const host = rectOf(dialog.parentElement);

        if (r.top < host.top || r.bottom > host.bottom) {
            problems.push(`dialog taller than its window by ${Math.ceil(r.height - host.height)} px`);
        }

        dialog.querySelectorAll(".dlg-body > *").forEach(el => {
            const er = rectOf(el);

            if (er.bottom > r.bottom + 1) {
                problems.push(`below the dialog: "${el.textContent.trim().slice(0, 40)}"`);
            }
        });
    });

    const page = document.querySelector(".page");
    const pageBounds = rectOf(page);

    document.querySelectorAll(".window").forEach(win => {
        if (rectOf(win).right > pageBounds.right + 1) {
            problems.push(`window wider than the page by ${Math.ceil(rectOf(win).right - pageBounds.right)} px`);
        }
    });

    const report = document.createElement("pre");
    report.id = "report";
    report.textContent = JSON.stringify({ page: [Math.ceil(pageBounds.width), Math.ceil(pageBounds.bottom + 4)], problems });
    document.body.appendChild(report);
}

let drawn = false;

function start() {
    if (drawn) {
        return;
    }

    drawn = true;
    drawAll();

    if (location.search.includes("check")) {
        checkLayout();
    }
}

if (document.fonts) {
    document.fonts.ready.then(start);
} else {
    window.addEventListener("load", start);
}
