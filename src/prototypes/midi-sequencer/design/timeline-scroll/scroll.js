// PROTOTYPE. The timeline's own scroll bar, for the comps in this folder (index.html): a plain zoom
// bar or an overview of the whole sequence, under the timeline or above it, with zoom controls.
// The page says which on its html element:
//   data-hbar="plain|overview"           the kind of bar
//   data-place="bottom|top"              where it is
//   data-view="5 17"                     the bars in view, from and to
//   data-zoom="buttons|slider|toolbar"   the zoom controls: keys at the bar's end, a slider with
//                                        triangles there, or a slider in the track list's toolbar
//   data-menu                            the bar's right-click menu, open
//   data-hint                            a tooltip on the view's right end
// It runs before seq.js, so the timeline is drawn at the view's scale, and builds the bar after.

(() => {
    const root = document.documentElement;
    const arrange = document.querySelector(".arrange");

    if (!arrange) {
        return;
    }

    const mode = root.dataset.hbar || "overview";
    const place = root.dataset.place || "bottom";
    const zoomStyle = root.dataset.zoom || "buttons";
    const [from, to] = (root.dataset.view || "1 27").trim().split(/\s+/).map(Number);

    root.classList.add("hbar-" + mode, "hbar-" + place, "zoom-" + zoomStyle);

    // The bar's row, in the arrange's columns, so the lanes are measured at their final width. Its
    // timeline cell isn't a .lane, which seq.js would draw a playhead and a grid in.
    const row = document.createElement("div");
    row.className = "arow tlbar";
    row.innerHTML = '<div class="hdr"></div><div class="launch"></div><div class="hb-lane"></div>';

    if (place === "top") {
        arrange.insertBefore(row, arrange.querySelector(".arow.ruler"));
    } else {
        arrange.appendChild(row);
    }

    // The view fills the timeline: its scale and how far it's scrolled.
    const lane = arrange.querySelector(".arow.track > .lane");
    const width = lane.getBoundingClientRect().width;
    const bar = width / (to - from);
    arrange.style.setProperty("--bar", bar + "px");
    arrange.style.setProperty("--scroll", ((from - 1) * bar) + "px");
    arrange.dataset.bars = String(Math.max(parseInt(arrange.dataset.bars || "25", 10), Math.ceil(to)));

    // How fine the grid and the ruler are at that scale: bars in fours far out, beats in between,
    // and sixteenths close in.
    const grain = bar < 44 ? "coarse" : bar > 150 ? "fine" : "beats";
    root.classList.add("grid-" + grain);

    const after = () => setTimeout(build, 0);

    if (document.fonts) {
        document.fonts.ready.then(after);
    } else {
        window.addEventListener("load", after);
    }

    function div(className, parent) {
        const el = document.createElement("div");
        el.className = className;

        if (parent) {
            parent.appendChild(el);
        }

        return el;
    }

    function svgEl(name, attrs, parent) {
        const el = document.createElementNS("http://www.w3.org/2000/svg", name);

        for (const [k, v] of Object.entries(attrs)) {
            el.setAttribute(k, v);
        }

        if (parent) {
            parent.appendChild(el);
        }

        return el;
    }

    // ------------------------------------------------------------- the ruler at this scale

    function ruler() {
        const rulerLane = arrange.querySelector(".arow.ruler > .lane");

        if (!rulerLane) {
            return;
        }

        if (grain === "coarse") {
            rulerLane.querySelectorAll(".bar-num").forEach(num => {
                if ((parseInt(num.textContent, 10) - 1) % 2 !== 0) {
                    num.classList.add("skip");
                }
            });
        }

        if (grain === "fine") {
            for (let b = Math.floor(from); b <= Math.ceil(to); b++) {
                for (let beat = 2; beat <= 4; beat++) {
                    const num = div("bar-num beat", rulerLane);
                    num.style.left = ((b - 1 + (beat - 1) / 4) * bar) + "px";
                    num.textContent = b + "." + beat;
                }
            }
        }
    }

    // ------------------------------------------------------------- what the bar shows

    function clipName(clip) {
        const t = clip.querySelector(".cap .t");
        return t ? t.textContent.replace(/\s*\u00D7\d+$/, "").trim() : "";
    }

    function readModel() {
        const rows = [];
        let lastEnd = 1;

        arrange.querySelectorAll(".arow").forEach(r => {
            if (r.classList.contains("ruler") || r.classList.contains("tlbar")) {
                return;
            }

            const clips = [...r.querySelectorAll(":scope > .lane > .tclip")];

            if (clips.length === 0) {
                return;
            }

            // An open folder's tracks show their own; a closed one shows its outline.
            if (r.classList.contains("folder")) {
                const chev = r.querySelector(".chev");

                if (!chev || chev.textContent !== "\uE76C") {
                    return;
                }
            }

            const color = getComputedStyle(r).getPropertyValue("--tc").trim() || "#888888";

            rows.push({
                color,
                clips: clips.map(c => {
                    const at = parseFloat(c.dataset.at);
                    const len = parseFloat(c.dataset.len);
                    lastEnd = Math.max(lastEnd, at + len);
                    return { at, len, name: clipName(c), sel: c.classList.contains("sel"), linked: !!c.querySelector(".lnk"), muted: c.classList.contains("muted") };
                })
            });
        });

        const end = Math.max(lastEnd + 2, to);

        // The sections are the tempo track's tags; a clip's own tags aren't sections.
        const tags = [...arrange.querySelectorAll(".arow.tempo .tag[data-at]")].map(t => {
            const span = t.querySelector("span") || t;
            return { at: parseFloat(t.dataset.at), name: t.textContent.trim(), color: getComputedStyle(span).backgroundColor };
        }).sort((a, b) => a.at - b.at);

        const sections = tags.map((t, i) => ({ ...t, until: i + 1 < tags.length ? tags[i + 1].at : end }));

        const loopEl = arrange.querySelector(".loop-range");
        const loop = loopEl ? { at: parseFloat(loopEl.dataset.at), len: parseFloat(loopEl.dataset.len) } : null;

        // Where else the selected clip plays: its linked copies, as a search marks what it found.
        const selected = rows.flatMap(r => r.clips).find(c => c.sel);
        const linked = selected ? rows.flatMap(r => r.clips).filter(c => c.linked && c.name === selected.name) : [];

        return { rows, end, sections, loop, linked, playhead: parseFloat(arrange.dataset.playhead || "0") };
    }

    // ------------------------------------------------------------- the lanes drawn at full length

    // seq.js draws the tempo and automation lanes only as wide as the lane, which the view's scroll
    // slides out of sight, so they're drawn again as long as the sequence.
    function widen(model) {
        const full = (model.end - 1) * bar;

        arrange.querySelectorAll(".arow > .lane[data-tempo], .arow > .lane[data-auto]").forEach(l => {
            l.querySelectorAll(":scope > svg.drawn").forEach(n => n.remove());
            const wide = div("hb-wide", l);
            wide.style.cssText = `position:absolute;left:0;top:0;bottom:0;width:${full}px;z-index:4`;

            if (l.dataset.tempo) {
                wide.dataset.tempo = l.dataset.tempo;
                drawTempo(wide, bar);
            } else {
                wide.dataset.auto = l.dataset.auto;
                drawAutomation(wide, bar);
            }

            if (root.classList.contains("light")) {
                lighten(wide);
            }
        });
    }

    // seq.js's applyLightTheme, for the drawings made after it ran (running it again would turn
    // what it already turned white to black).
    function lighten(host) {
        const remap = value => {
            if (!value) {
                return value;
            }

            const v = value.replace(/\s+/g, "").toUpperCase();
            const ink = v.match(/^RGBA\(255,255,255,([\d.]+)\)$/);

            if (ink) {
                return `rgba(0,0,0,${Math.min(1, parseFloat(ink[1]) * 1.2).toFixed(3)})`;
            }

            if (v === "#FFF" || v === "#FFFFFF") {
                return "#1A1A1A";
            }

            if (v === "#1E1E1E" || v === "#262626") {
                return "#FFFFFF";
            }

            return value;
        };

        host.querySelectorAll("svg.drawn *").forEach(el => {
            for (const name of ["fill", "stroke"]) {
                const before = el.getAttribute(name);
                const after = remap(before);

                if (after !== before) {
                    el.setAttribute(name, after);
                }
            }
        });
    }

    // ------------------------------------------------------------- the two kinds of bar

    function overview(track, model, x, W, H) {
        // Drawn in a layer the well's shape cuts, so the view's grips can reach past it.
        const clip = div("hb-clip", track);
        const svg = svgEl("svg", { class: "hb-map", width: W, height: H, viewBox: `0 0 ${W} ${H}` }, clip);
        const top = 14;
        const bottom = H - 5;

        // The sections: a band each, every other one a shade deeper, a mark where each starts, and
        // its name.
        model.sections.forEach((s, i) => {
            const x0 = x(s.at);
            const x1 = x(s.until);

            if (i % 2 === 1) {
                svgEl("rect", { class: "sec-band", x: x0, y: 0, width: Math.max(0, x1 - x0), height: H }, svg);
            }

            svgEl("rect", { class: "sec-mark", x: x0, y: 0, width: 1.5, height: H, style: `fill:color-mix(in oklch, ${s.color} 55%, var(--hb-ink))` }, svg);

            const name = div("hb-sec", clip);
            name.style.left = (x0 + 5) + "px";
            name.style.maxWidth = Math.max(0, x1 - x0 - 8) + "px";
            name.textContent = s.name;
        });

        if (model.loop) {
            svgEl("rect", { class: "loop", x: x(model.loop.at), y: top - 2, width: x(model.loop.at + model.loop.len) - x(model.loop.at), height: bottom - top + 4, rx: 2 }, svg);
        }

        // A line for each track, where its clips are, in its color.
        const n = model.rows.length;
        const gap = n > 1 ? Math.min(1.2, (bottom - top) / (n * 4)) : 0;
        const lineH = Math.max(1.2, (bottom - top - gap * (n - 1)) / n);

        model.rows.forEach((r, i) => {
            const y = top + i * (lineH + gap);

            r.clips.forEach(c => {
                svgEl("rect", {
                    class: "clip" + (c.muted ? " muted" : ""),
                    x: x(c.at) + 0.5,
                    y,
                    width: Math.max(1, x(c.at + c.len) - x(c.at) - 1),
                    height: lineH,
                    rx: Math.min(1, lineH / 2),
                    style: `fill:${r.color}`
                }, svg);
            });
        });

        // Under the lines, the selected clip and its linked copies.
        model.linked.forEach(c => {
            svgEl("rect", { class: "found" + (c.sel ? " sel" : ""), x: x(c.at) + 0.5, y: H - 3.5, width: Math.max(2, x(c.at + c.len) - x(c.at) - 1), height: 2.5, rx: 1.25 }, svg);
        });

        if (model.playhead > 0) {
            svgEl("rect", { class: "ph", x: x(model.playhead) - 0.75, y: 0, width: 1.5, height: H }, svg);
        }

        // Outside the view, veiled.
        div("hb-veil", clip).style.cssText = `left:0;width:${x(from)}px`;
        div("hb-veil", clip).style.cssText = `left:${x(to)}px;right:0`;
    }

    function plain(track, model, x) {
        if (model.playhead > 0) {
            div("hb-tick", track).style.left = (x(model.playhead) - 1) + "px";
        }
    }

    // The view: in the overview a frame you drag, in the plain bar its thumb. Either way, an end
    // dragged zooms.
    function view(track, x) {
        const v = div(mode === "overview" ? "hb-view" : "hb-thumb", track);
        v.style.left = x(from) + "px";
        v.style.width = (x(to) - x(from)) + "px";
        div("grip l", v);
        div("grip r", v);

        // In the plain bar the playhead's tick shows over the thumb.
        const tick = track.querySelector(".hb-tick");

        if (tick) {
            track.appendChild(tick);
        }

        if (root.hasAttribute("data-hint")) {
            v.classList.add("hover-r");
            const hint = div("hb-hint", arrange);
            hint.innerHTML = "<b>Drag an end to zoom.</b> Ctrl + wheel zooms where you point, and so does a pinch on a touchpad or a touch screen.";
            const t = track.getBoundingClientRect();
            const a = arrange.getBoundingClientRect();
            const right = t.left - a.left + x(to);
            hint.style.left = Math.max(8, right - 300) + "px";

            if (place === "top") {
                hint.style.top = (t.bottom - a.top + 10) + "px";
            } else {
                hint.style.bottom = (a.bottom - t.top + 10) + "px";
            }
        }
    }

    // ------------------------------------------------------------- around the bar

    function toggle() {
        const t = div("hb-toggle");
        t.title = "Show the overview or a plain scroll bar";
        const plainSeg = div("seg plain" + (mode === "plain" ? " on" : ""), t);
        plainSeg.innerHTML = '<svg viewBox="0 0 16 16"><rect class="o" x="1.5" y="5" width="13" height="6" rx="3"/><rect class="f" x="5" y="6.5" width="5.5" height="3" rx="1.5"/></svg>';
        const mapSeg = div("seg map" + (mode === "overview" ? " on" : ""), t);
        mapSeg.innerHTML = '<svg viewBox="0 0 16 16"><rect class="c1" x="1" y="3.6" width="8" height="1.8" rx=".9"/><rect class="c2" x="5" y="7.1" width="9.5" height="1.8" rx=".9"/><rect class="c3" x="2.5" y="10.6" width="6" height="1.8" rx=".9"/></svg>';
        return t;
    }

    function key(parent, kind, glyph, title) {
        const k = div("k " + kind, parent);
        k.textContent = glyph;
        k.title = title;
        return k;
    }

    // Where the zoom is between the whole sequence and one bar across the timeline, on a log scale.
    function zoomPosition(model) {
        const least = width / (model.end - 1);
        const most = width;
        return Math.max(0, Math.min(1, Math.log(bar / least) / Math.log(most / least)));
    }

    function zoomTools(style, model) {
        const z = div("hb-zoom " + style);

        if (style === "buttons") {
            key(z, "out", "\uE71F", "Zoom out");
            key(z, "in", "\uE8A3", "Zoom in");
            div("zsep", z);
            key(z, "fit", "\uE9A6", "Zoom to fit the sequence");
            return z;
        }

        if (style === "toolbar") {
            const label = div("zlabel", z);
            label.textContent = "\u2194";
            label.title = "Time zoom";
        }

        div("tri small", z).title = "Zoom out";
        const rail = div("rail", z);
        div("knob", rail).style.left = (zoomPosition(model) * 100) + "%";
        div("tri big", z).title = "Zoom in";

        if (style === "toolbar") {
            key(z, "fit", "\uE9A6", "Zoom to fit the sequence");
        }

        return z;
    }

    function menu() {
        const m = div("hb-menu", arrange);
        const check = on => `<span class="ck">${on ? "\uE73E" : ""}</span>`;
        m.innerHTML =
            `<div class="mi">${check(mode === "overview")}Overview<span class="sub">tracks and sections</span></div>` +
            `<div class="mi">${check(mode === "plain")}Plain scroll bar</div>` +
            `<div class="ms"></div>` +
            `<div class="mi">${check(place === "bottom")}Under the timeline</div>` +
            `<div class="mi">${check(place === "top")}Above the ruler</div>` +
            `<div class="ms"></div>` +
            `<div class="mi">${check(false)}Zoom to fit the sequence<span class="key">Ctrl+0</span></div>` +
            `<div class="mi">${check(false)}Zoom to the selection<span class="key">Ctrl+Shift+0</span></div>`;

        const t = row.querySelector(".hb-toggle").getBoundingClientRect();
        const a = arrange.getBoundingClientRect();
        m.style.left = (t.left - a.left) + "px";

        if (place === "top") {
            m.style.top = (t.bottom - a.top + 6) + "px";
        } else {
            m.style.bottom = (a.bottom - t.top + 6) + "px";
        }
    }

    // ------------------------------------------------------------- building it

    function build() {
        ruler();

        const model = readModel();
        widen(model);
        const host = row.querySelector(".hb-lane");

        if (zoomStyle === "toolbar") {
            const tools = row.querySelector(".hdr");
            div("spacer", tools).style.flex = "1";
            tools.appendChild(toggle());
            tools.appendChild(zoomTools("toolbar", model));
        } else {
            host.appendChild(toggle());
        }

        const track = div("hb-track", host);

        if (zoomStyle !== "toolbar") {
            host.appendChild(zoomTools(zoomStyle, model));
        }

        const W = track.clientWidth;
        const H = track.clientHeight;
        const x = b => (b - 1) / (model.end - 1) * W;

        if (mode === "overview") {
            overview(track, model, x, W, H);
        } else {
            plain(track, model, x);
        }

        view(track, x);

        if (root.hasAttribute("data-menu")) {
            menu();
        }

        if (location.search.includes("diag")) {
            diagnose(model);
        }
    }

    // ?diag: where everything ended up, as JSON in the page, for a check without a screenshot.
    function diagnose(model) {
        const box = el => {
            if (!el) {
                return null;
            }

            const r = el.getBoundingClientRect();
            return [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)];
        };

        const out = {
            order: [...arrange.children].map(c => c.className + "@" + box(c)[1]),
            bar: Math.round(bar * 100) / 100,
            grain,
            end: model.end,
            rows: model.rows.length,
            sections: model.sections.map(s => s.name),
            linked: model.linked.length,
            window: box(document.querySelector(".window")),
            arrange: box(arrange),
            row: box(row),
            lane: box(lane),
            track: box(row.querySelector(".hb-track")),
            view: box(row.querySelector(".hb-view, .hb-thumb")),
            toggle: box(row.querySelector(".hb-toggle")),
            zoom: box(document.querySelector(".hb-zoom")),
            menu: box(document.querySelector(".hb-menu")),
            hint: box(document.querySelector(".hb-hint")),
            tempo: box(arrange.querySelector(".hb-wide .tempo-svg path")),
            auto: box(arrange.querySelector(".hb-wide .auto-svg path")),
            field: getComputedStyle(arrange, "::before").bottom,
            rulerNums: [...arrange.querySelectorAll(".arow.ruler .bar-num:not(.skip)")].filter(n => {
                const r = n.getBoundingClientRect();
                const l = box(arrange.querySelector(".arow.ruler > .lane"));
                return r.left >= l[0] - 1 && r.left <= l[0] + l[2];
            }).map(n => n.textContent),
            clipsInView: [...arrange.querySelectorAll(".tclip")].filter(c => {
                const r = c.getBoundingClientRect();
                const l = box(lane);
                return r.right > l[0] && r.left < l[0] + l[2];
            }).length
        };

        const pre = document.createElement("pre");
        pre.id = "hb-diag";
        pre.textContent = JSON.stringify(out);
        document.body.appendChild(pre);
    }
})();
