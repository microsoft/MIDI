// PROTOTYPE. Drawing helpers for the MIDI Patchbay Preview 11 comps.
//
// Connections are drawn from real DOM geometry, so moving a node or a block needs no coordinate
// maths. A connection is declared as
//   <i data-conn data-from="ref" data-to="ref" data-state="hot|muted"></i>
// where a ref is the id of an endpoint port row, or "<block id>:in" / "<block id>:out".
//
// Library thumbnails and the overview map are drawn from a small list of boxes and links.

const SVG_NS = "http://www.w3.org/2000/svg";

function portElement(ref) {
    const [id, side] = ref.split(":");
    const host = document.getElementById(id);

    if (!host) {
        return null;
    }

    if (side) {
        return host.querySelector(".bport." + side);
    }

    return host.querySelector(".dot") || host;
}

function centerIn(canvas, element) {
    const a = element.getBoundingClientRect();
    const b = canvas.getBoundingClientRect();
    return { x: a.left - b.left + a.width / 2, y: a.top - b.top + a.height / 2 };
}

function cordPath(a, b) {
    const reach = Math.max(30, Math.min(110, Math.abs(b.x - a.x) * 0.5));
    return `M ${a.x} ${a.y} C ${a.x + reach} ${a.y}, ${b.x - reach} ${b.y}, ${b.x} ${b.y}`;
}

function drawCords() {
    document.querySelectorAll(".ecanvas").forEach(canvas => {
        canvas.querySelectorAll("svg.cords").forEach(n => n.remove());

        const svg = document.createElementNS(SVG_NS, "svg");
        svg.setAttribute("class", "cords");
        canvas.appendChild(svg);

        canvas.querySelectorAll("[data-conn]").forEach(conn => {
            const fromElement = portElement(conn.dataset.from);
            const toElement = portElement(conn.dataset.to);

            if (!fromElement || !toElement) {
                console.warn("missing port", conn.dataset.from, conn.dataset.to);
                return;
            }

            const d = cordPath(centerIn(canvas, fromElement), centerIn(canvas, toElement));
            const state = conn.dataset.state || "";

            if (state === "hot") {
                const glow = document.createElementNS(SVG_NS, "path");
                glow.setAttribute("d", d);
                glow.setAttribute("fill", "none");
                glow.setAttribute("stroke", "rgba(96, 205, 255, .30)");
                glow.setAttribute("stroke-width", "8");
                svg.appendChild(glow);
            }

            const path = document.createElementNS(SVG_NS, "path");
            path.setAttribute("d", d);
            path.setAttribute("fill", "none");
            path.setAttribute("stroke",
                state === "hot" ? "rgba(255, 255, 255, .95)" :
                state === "muted" ? "rgba(255, 255, 255, .30)" :
                "rgba(255, 255, 255, .52)");
            path.setAttribute("stroke-width", state === "hot" ? "2.5" : "2");

            if (state === "muted") {
                path.setAttribute("stroke-dasharray", "6 5");
            }

            svg.appendChild(path);
        });
    });
}

const kindFill = {
    endpoint: "#5A5A5A",
    filter: "#FFB547",
    transform: "#B4A7FF",
    flow: "#5BE0B0",
    missing: "#FF99A4",
};

// boxes: [x, y, w, h, kind], links: [from index, to index]. Coordinates are in the patch's own
// canvas units; the drawing is fitted into the element with a margin.
function drawMap(element, boxes, links, viewport) {
    const width = element.clientWidth;
    const height = element.clientHeight;
    const margin = 10;

    const left = Math.min(...boxes.map(b => b[0]));
    const top = Math.min(...boxes.map(b => b[1]));
    const right = Math.max(...boxes.map(b => b[0] + b[2]));
    const bottom = Math.max(...boxes.map(b => b[1] + b[3]));

    const scale = Math.min((width - margin * 2) / (right - left), (height - margin * 2) / (bottom - top));
    const offsetX = (width - (right - left) * scale) / 2 - left * scale;
    const offsetY = (height - (bottom - top) * scale) / 2 - top * scale;

    const svg = document.createElementNS(SVG_NS, "svg");
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);

    for (const [from, to] of links) {
        const a = boxes[from];
        const b = boxes[to];
        const start = { x: (a[0] + a[2]) * scale + offsetX, y: (a[1] + a[3] / 2) * scale + offsetY };
        const end = { x: b[0] * scale + offsetX, y: (b[1] + b[3] / 2) * scale + offsetY };
        const reach = Math.max(6, Math.abs(end.x - start.x) * 0.5);

        const path = document.createElementNS(SVG_NS, "path");
        path.setAttribute("d", `M ${start.x} ${start.y} C ${start.x + reach} ${start.y}, ${end.x - reach} ${end.y}, ${end.x} ${end.y}`);
        path.setAttribute("fill", "none");
        path.setAttribute("stroke", "rgba(255, 255, 255, .42)");
        path.setAttribute("stroke-width", "1.4");
        svg.appendChild(path);
    }

    for (const [x, y, w, h, kind] of boxes) {
        const rect = document.createElementNS(SVG_NS, "rect");
        rect.setAttribute("x", x * scale + offsetX);
        rect.setAttribute("y", y * scale + offsetY);
        rect.setAttribute("width", w * scale);
        rect.setAttribute("height", h * scale);
        rect.setAttribute("rx", kind === "endpoint" || kind === "missing" ? 2.5 : 4);

        if (kind === "missing") {
            rect.setAttribute("fill", "rgba(255, 153, 164, .14)");
            rect.setAttribute("stroke", kindFill.missing);
            rect.setAttribute("stroke-dasharray", "3 2");
        } else if (kind === "endpoint") {
            rect.setAttribute("fill", kindFill.endpoint);
            rect.setAttribute("stroke", "rgba(255, 255, 255, .25)");
        } else {
            rect.setAttribute("fill", kindFill[kind]);
            rect.setAttribute("fill-opacity", ".85");
        }

        svg.appendChild(rect);
    }

    if (viewport) {
        const rect = document.createElementNS(SVG_NS, "rect");
        rect.setAttribute("x", viewport[0] * scale + offsetX);
        rect.setAttribute("y", viewport[1] * scale + offsetY);
        rect.setAttribute("width", viewport[2] * scale);
        rect.setAttribute("height", viewport[3] * scale);
        rect.setAttribute("fill", "none");
        rect.setAttribute("stroke", "rgba(255, 255, 255, .7)");
        rect.setAttribute("stroke-width", "1");
        svg.appendChild(rect);
    }

    element.innerHTML = "";
    element.appendChild(svg);
}

function drawThumbs() {
    document.querySelectorAll("[data-map]").forEach(element => {
        const spec = JSON.parse(element.dataset.map);
        drawMap(element, spec.boxes, spec.links, spec.viewport);
    });
}

// The overview map in the editor reads the real boxes off the surface.
function drawMinimaps() {
    document.querySelectorAll(".minimap[data-surface]").forEach(element => {
        const surface = document.getElementById(element.dataset.surface);
        const items = [...surface.querySelectorAll(".node, .block")];
        const ids = items.map(n => n.id);

        const boxes = items.map(n => [n.offsetLeft, n.offsetTop, n.offsetWidth, n.offsetHeight,
            n.classList.contains("block")
                ? (n.classList.contains("filter") ? "filter" : n.classList.contains("transform") ? "transform" : "flow")
                : "endpoint"]);

        const links = [];
        surface.parentElement.querySelectorAll("[data-conn]").forEach(conn => {
            const from = conn.dataset.from.split(":")[0];
            const to = conn.dataset.to.split(":")[0];
            const fromIndex = ids.findIndex(id => from === id || from.startsWith(id + "-"));
            const toIndex = ids.findIndex(id => to === id || to.startsWith(id + "-"));

            if (fromIndex >= 0 && toIndex >= 0) {
                links.push([fromIndex, toIndex]);
            }
        });

        drawMap(element, boxes, links);
    });
}

function drawAll() {
    drawCords();
    drawThumbs();
    drawMinimaps();
}

// Opened with ?check, the page writes what is wrong with its own layout into a <pre id="report">,
// so headless Edge --dump-dom can check a comp without anybody looking at a picture.
function checkLayout() {
    const problems = [];

    const rectOf = element => element.getBoundingClientRect();

    const overlaps = (a, b, gap) =>
        a.left < b.right + gap && b.left < a.right + gap && a.top < b.bottom + gap && b.top < a.bottom + gap;

    document.querySelectorAll(".ecanvas").forEach(canvas => {
        const bounds = rectOf(canvas);
        const items = [...canvas.querySelectorAll(".node, .block")];

        for (let i = 0; i < items.length; i++) {
            const a = rectOf(items[i]);

            if (a.right > bounds.right - 4 || a.bottom > bounds.bottom - 4 || a.left < bounds.left || a.top < bounds.top) {
                problems.push(`outside canvas: ${items[i].id}`);
            }

            for (let j = i + 1; j < items.length; j++) {
                if (overlaps(a, rectOf(items[j]), 12)) {
                    problems.push(`overlap: ${items[i].id} / ${items[j].id}`);
                }
            }

            canvas.querySelectorAll(".minimap, .zoombar").forEach(overlay => {
                if (overlaps(a, rectOf(overlay), 6)) {
                    problems.push(`under ${overlay.className}: ${items[i].id}`);
                }
            });
        }

        canvas.querySelectorAll("[data-conn]").forEach(conn => {
            if (!portElement(conn.dataset.from) || !portElement(conn.dataset.to)) {
                problems.push(`missing port: ${conn.dataset.from} -> ${conn.dataset.to}`);
            }
        });
    });

    document.querySelectorAll(".bname, .nm, .port-label, .lname, .ldesc, .chip, .btn, .ptile, .tab, .field").forEach(element => {
        if (element.scrollWidth > element.clientWidth + 1) {
            problems.push(`cut off: "${element.textContent.trim().slice(0, 48)}"`);
        }
    });

    document.querySelectorAll(".window").forEach(win => {
        const bounds = rectOf(win);
        win.querySelectorAll(".ibody > *, .dlg-body > *, .libgrid > *").forEach(element => {
            const r = rectOf(element);

            if (r.height > 0 && r.bottom > bounds.bottom + 1) {
                problems.push(`below the window: "${element.textContent.trim().slice(0, 40)}"`);
            }
        });
    });

    const pageBounds = rectOf(document.querySelector(".page"));

    document.querySelectorAll(".window, .panel").forEach(win => {
        const r = rectOf(win);

        if (r.right > pageBounds.right + 1) {
            problems.push(`window wider than the page by ${Math.ceil(r.right - pageBounds.right)} px`);
        }
    });

    document.querySelectorAll(".dialog").forEach(dialog => {
        const r = rectOf(dialog);
        const host = rectOf(dialog.parentElement);

        if (r.top < host.top || r.bottom > host.bottom) {
            problems.push(`dialog taller than its window by ${Math.ceil(r.height - host.height)} px`);
        }
    });

    const report = document.createElement("pre");
    report.id = "report";
    report.textContent = JSON.stringify({ page: [Math.ceil(pageBounds.width), Math.ceil(pageBounds.height)], problems });
    document.body.appendChild(report);
}

window.addEventListener("load", drawAll);
document.fonts && document.fonts.ready.then(() => {
    drawAll();

    if (location.search.includes("check")) {
        checkLayout();
    }
});
