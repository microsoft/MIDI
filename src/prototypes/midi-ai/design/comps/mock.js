// Draws the patch connections from real DOM geometry so the mockups stay editable:
// a connection is declared as <i data-conn data-from="portId" data-to="portId" data-label="1 -> 1">
// and the curve plus its pill are generated here.

const SVG_NS = "http://www.w3.org/2000/svg";

function dotCenter(canvas, portId) {
    const port = document.getElementById(portId);
    const dot = port.querySelector(".dot");
    const a = dot.getBoundingClientRect();
    const b = canvas.getBoundingClientRect();
    return { x: a.left - b.left + a.width / 2, y: a.top - b.top + a.height / 2 };
}

function draw() {
    document.querySelectorAll(".canvas").forEach(canvas => {
        canvas.querySelectorAll("svg.connections").forEach(n => n.remove());
        canvas.querySelectorAll(".conn-pill.gen").forEach(n => n.remove());

        const svg = document.createElementNS(SVG_NS, "svg");
        svg.setAttribute("class", "connections");

        const defs = document.createElementNS(SVG_NS, "defs");
        defs.innerHTML =
            '<marker id="hd" markerWidth="7" markerHeight="7" refX="6.2" refY="3.5" orient="auto">' +
            '<polygon points="0 0, 7 3.5, 0 7" fill="#60CDFF"/></marker>' +
            '<marker id="hdErr" markerWidth="7" markerHeight="7" refX="6.2" refY="3.5" orient="auto">' +
            '<polygon points="0 0, 7 3.5, 0 7" fill="#FF99A4"/></marker>';
        svg.appendChild(defs);
        canvas.appendChild(svg);

        canvas.querySelectorAll("[data-conn]").forEach(conn => {
            const a = dotCenter(canvas, conn.dataset.from);
            const b = dotCenter(canvas, conn.dataset.to);
            const reach = Math.max(38, Math.min(120, Math.abs(b.x - a.x) * 0.55));

            const path = document.createElementNS(SVG_NS, "path");

            if (b.x < a.x + 20) {
                // feedback edge: bow it under everything rather than dragging it back through the nodes
                const bow = Math.max(a.y, b.y) + (conn.dataset.bow ? Number(conn.dataset.bow) : 150);
                const mid = (a.x + b.x) / 2;
                path.setAttribute("d",
                    `M ${a.x} ${a.y} C ${a.x + 90} ${a.y}, ${mid + 80} ${bow}, ${mid} ${bow}` +
                    ` C ${mid - 80} ${bow}, ${b.x - 90} ${b.y}, ${b.x - 5} ${b.y}`);
            } else {
                path.setAttribute("d",
                    `M ${a.x} ${a.y} C ${a.x + reach} ${a.y}, ${b.x - reach} ${b.y}, ${b.x - 5} ${b.y}`);
            }
            path.setAttribute("fill", "none");

            const state = conn.dataset.state || "";
            if (state === "selected") {
                path.setAttribute("stroke", "#60CDFF");
                path.setAttribute("stroke-width", "3");
                path.setAttribute("marker-end", "url(#hd)");
            } else if (state === "error") {
                path.setAttribute("stroke", "#FF99A4");
                path.setAttribute("stroke-width", "2.5");
                path.setAttribute("stroke-dasharray", "7 5");
                path.setAttribute("marker-end", "url(#hdErr)");
            } else if (state === "idle") {
                path.setAttribute("stroke", "#60CDFF");
                path.setAttribute("stroke-opacity", ".30");
                path.setAttribute("stroke-width", "2");
                path.setAttribute("marker-end", "url(#hd)");
            } else if (state === "draft") {
                // A route that exists only in a draft: dashed, so it can never be mistaken for live.
                path.setAttribute("stroke", "#60CDFF");
                path.setAttribute("stroke-opacity", ".85");
                path.setAttribute("stroke-width", "2");
                path.setAttribute("stroke-dasharray", "6 5");
                path.setAttribute("marker-end", "url(#hd)");
            } else {
                path.setAttribute("stroke", "#60CDFF");
                path.setAttribute("stroke-opacity", ".62");
                path.setAttribute("stroke-width", "2");
                path.setAttribute("marker-end", "url(#hd)");
            }
            svg.appendChild(path);

            if (conn.dataset.label) {
                const mid = path.getPointAtLength(path.getTotalLength() * 0.5);
                const pill = document.createElement("div");
                pill.className = "conn-pill gen" +
                    (state === "selected" ? " sel" : state === "error" ? " err" : state === "draft" ? " draft" : "");
                pill.style.left = mid.x + "px";
                pill.style.top = mid.y + "px";
                pill.innerHTML =
                    (conn.dataset.glyph ? `<span class="gl">${conn.dataset.glyph}</span>` : "") +
                    conn.dataset.label;
                canvas.appendChild(pill);
            }
        });
    });
}

window.addEventListener("load", draw);
document.fonts && document.fonts.ready.then(draw);
