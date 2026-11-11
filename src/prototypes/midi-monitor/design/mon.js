// PROTOTYPE. Fills the message tables in the Windows MIDI Monitor comps. Nothing here is product code.
//
// A table is declared as <div class="msgs" data-rows="simple|advanced|cord|cordport" data-count="N">
// and gets its header and N rows from the sample list below.

const SOURCES = {
    a: { ep: "MiniLab 3", gp: "1 \u00b7 MIDI", color: "var(--src1)" },
    b: { ep: "MiniLab 3", gp: "2 \u00b7 DIN Thru", color: "var(--src2)" },
    c: { ep: "Studio Mac (Network)", gp: "1 \u00b7 Logic Out", color: "var(--src3)" },
    d: { ep: "DAW Loopback (A)", gp: "1", color: "var(--src4)" },
    e: { ep: "Roland TR-8S", gp: "1 \u00b7 TR-8S", color: "var(--src1)" },
    f: { ep: "Roland TR-8S", gp: "2 \u00b7 TR-8S CTRL", color: "var(--src2)" },
};

// data-map="a:e,b:f" shows source a's rows as coming from e
let sourceMap = {};

const TYPE = {
    m1: { name: "MIDI 1.0 Note On", color: "var(--mt-m1)" },
    m1off: { name: "MIDI 1.0 Note Off", color: "var(--mt-m1)" },
    m1cc: { name: "MIDI 1.0 Control Change", color: "var(--mt-m1)" },
    m1pb: { name: "MIDI 1.0 Pitch Bend", color: "var(--mt-m1)" },
    m2: { name: "MIDI 2.0 Note On", color: "var(--mt-m2)" },
    m2cc: { name: "MIDI 2.0 Control Change", color: "var(--mt-m2)" },
    sys: { name: "Song Position Pointer", color: "var(--mt-sys)" },
    start: { name: "Start", color: "var(--mt-sys)" },
    sx7: { name: "SysEx 7 Complete", color: "var(--mt-sx7)" },
    flex: { name: "Flex Data Set Tempo", color: "var(--mt-flex)" },
};

// time, source, type, words, group, channel, details, since last
const SAMPLE = [
    ["2,481.204", "a", "m1", "20903C64", 1, 1, "<b>Note</b> <i>C4 (60)</i> &nbsp;<b>Velocity</b> <i>100</i>", "0.000"],
    ["2,481.211", "c", "m2", "41933C00 C8000000", 2, 4, "<b>Note</b> <i>C4 (60)</i> &nbsp;<b>Velocity</b> <i>51200 (78.1%)</i>", "0.007"],
    ["2,481.530", "a", "m1cc", "20B00140", 1, 1, "<b>Controller</b> <i>1 Modulation</i> &nbsp;<b>Value</b> <i>64</i>", "0.319"],
    ["2,481.533", "b", "m1", "21903E5A", 2, 1, "<b>Note</b> <i>D4 (62)</i> &nbsp;<b>Velocity</b> <i>90</i>", "0.003"],
    ["2,481.902", "c", "flex", "D1100000 07A12000 00000000 00000000", 2, "", "<b>Tempo</b> <i>120.00 BPM</i>", "0.369"],
    ["2,482.004", "a", "m1off", "20803C00", 1, 1, "<b>Note</b> <i>C4 (60)</i> &nbsp;<b>Velocity</b> <i>0</i>", "0.102"],
    ["2,482.010", "d", "sx7", "30167E7F 06010000", 1, "", "<b>Bytes</b> <i>F0 7E 7F 06 01 F7</i> &nbsp;Identity request", "0.006"],
    ["2,482.240", "a", "m1pb", "20E00048", 1, 1, "<b>Bend</b> <i>+1024 (9216)</i>", "0.230"],
    ["2,482.251", "c", "m2cc", "41B30700 80000000", 2, 4, "<b>Controller</b> <i>7 Volume</i> &nbsp;<b>Value</b> <i>50.0%</i>", "0.011"],
    ["2,482.660", "b", "m1off", "21803E00", 2, 1, "<b>Note</b> <i>D4 (62)</i> &nbsp;<b>Velocity</b> <i>0</i>", "0.409"],
    ["2,483.001", "d", "start", "11FA0000", 1, "", "", "0.341"],
    ["2,483.002", "d", "sys", "11F20000", 1, "", "<b>Position</b> <i>0 (bar 1)</i>", "0.001"],
    ["2,483.115", "a", "m1", "20904064", 1, 1, "<b>Note</b> <i>E4 (64)</i> &nbsp;<b>Velocity</b> <i>100</i>", "0.113"],
    ["2,483.118", "a", "m1", "20904364", 1, 1, "<b>Note</b> <i>G4 (67)</i> &nbsp;<b>Velocity</b> <i>100</i>", "0.003"],
    ["2,483.400", "c", "m2", "41934000 FFFF0000", 2, 4, "<b>Note</b> <i>E4 (64)</i> &nbsp;<b>Velocity</b> <i>65535 (100%)</i>", "0.282"],
    ["2,483.722", "a", "m1cc", "20B04000", 1, 1, "<b>Controller</b> <i>64 Sustain</i> &nbsp;<b>Value</b> <i>0</i>", "0.322"],
];

const LAYOUTS = {
    simple: {
        cols: "54px 104px 230px 64px 70px 1fr 84px",
        head: ["#", "Message Time", "Message", "Group", "Channel", "Details", "Since last"],
    },
    advanced: {
        cols: "50px 96px 196px 222px 56px 1fr 78px",
        head: ["#", "Message Time", "From", "Message", "Channel", "Details", "Since last"],
    },
    cord: {
        cols: "78px 196px 1fr",
        head: ["Time", "Message", "Details"],
    },
    cordport: {
        cols: "70px 112px 190px 1fr",
        head: ["Time", "Went to", "Message", "Details"],
    },
};

const CORD_TARGETS = ["Note filter (high)", "Note filter (low)"];

function cell(html, cls) {
    const div = document.createElement("div");
    if (cls) div.className = cls;
    div.innerHTML = html;
    return div;
}

function wordsCell(row, showName) {
    const type = TYPE[row[2]];
    let html = `<span class="w">${row[3].split(" ").slice(0, 2).join(" ")}${row[3].split(" ").length > 2 ? " \u2026" : ""}</span>`;
    if (showName) html += `<span class="nm" style="background:${type.color}">${type.name}</span>`;
    return cell(html, "words");
}

function sourceCell(row) {
    const s = SOURCES[sourceMap[row[1]] || row[1]];
    return cell(`<span class="bar" style="background:${s.color}"></span><span class="ep">${s.ep}</span><span class="gp">${s.gp}</span>`, "src");
}

function fill(table) {
    const kind = table.dataset.rows;
    const layout = LAYOUTS[kind];
    const count = parseInt(table.dataset.count || "12", 10);
    const only = table.dataset.only ? table.dataset.only.split(",") : null;
    const selected = table.dataset.sel ? table.dataset.sel.split(",").map(Number) : [];
    const names = table.dataset.names !== "off";

    // data-repeat="1:234" puts a "234 repeats" count on the second row
    const repeats = {};
    (table.dataset.repeat || "").split(",").filter(Boolean).forEach(pair => {
        const [row, count] = pair.split(":");
        repeats[Number(row)] = count;
    });

    sourceMap = {};
    (table.dataset.map || "").split(",").filter(Boolean).forEach(pair => {
        const [from, to] = pair.split(":");
        sourceMap[from] = to;
    });

    const head = document.createElement("div");
    head.className = "th";
    head.style.gridTemplateColumns = layout.cols;
    layout.head.forEach((h, i) => head.appendChild(cell(h, i === 0 && kind !== "cord" && kind !== "cordport" ? "n" : "")));
    table.appendChild(head);

    let shown = 0;
    let index = 1180;

    for (const row of SAMPLE) {
        index++;
        if (only && !only.includes(row[1])) continue;
        if (shown >= count) break;

        const tr = document.createElement("div");
        tr.className = "tr" + (names ? " tall" : "") + (shown % 2 ? " alt" : "") + (selected.includes(shown) ? " sel" : "");
        tr.style.gridTemplateColumns = layout.cols;

        if (kind === "simple") {
            tr.append(cell(index, "n"), cell(row[0], "t"), wordsCell(row, names), cell(row[4]), cell(row[5]), cell(row[6], "d"), cell(row[7], "dl"));
        } else if (kind === "advanced") {
            tr.append(cell(index, "n"), cell(row[0], "t"), sourceCell(row), wordsCell(row, names), cell(row[5]), cell(row[6], "d"), cell(row[7], "dl"));
        } else if (kind === "cord") {
            tr.append(cell(row[0].replace("2,", ""), "t"), wordsCell(row, names), cell(row[6] || TYPE[row[2]].name, "d"));
        } else {
            tr.append(cell(row[0].replace("2,", ""), "t"), cell(CORD_TARGETS[shown % 2 === 0 ? 0 : 1], "d"), wordsCell(row, names), cell(row[6] || TYPE[row[2]].name, "d"));
        }

        if (repeats[shown] !== undefined) {
            const details = tr.querySelectorAll(".d");
            const target = details[details.length - 1];
            target.innerHTML += ` <span class="rep">&times;${repeats[shown]} repeats</span>`;
        }

        table.appendChild(tr);
        shown++;
    }
}

document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll("[data-rows]").forEach(fill);
});
