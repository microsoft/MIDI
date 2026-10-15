// PROTOTYPE. Behavior for the Windows MIDI Settings "Configurations" window comps.
//
// ?state=a,b    shows elements marked data-only="a" (or "b"), hides those marked data-not, and
//               with "noadmin" grays out everything marked data-admin
// ?check        writes the page's own layout problems into <pre id="report"> as JSON, so a
//               headless browser can report them without anyone looking at a picture

(function () {
    const params = new URLSearchParams(location.search);
    const states = (params.get("state") || "").split(",").map(s => s.trim()).filter(Boolean);

    function applyStates() {
        document.querySelectorAll("[data-only]").forEach(el => {
            const wanted = el.getAttribute("data-only").split(/\s+/);

            if (wanted.some(w => states.includes(w))) {
                el.removeAttribute("data-only");
            }
        });

        document.querySelectorAll("[data-not]").forEach(el => {
            const unwanted = el.getAttribute("data-not").split(/\s+/);

            if (unwanted.some(w => states.includes(w))) {
                el.style.display = "none";
            }
        });

        if (states.includes("noadmin")) {
            document.querySelectorAll("[data-admin]").forEach(el => el.classList.add("dis"));
        }

        // a variant can hide the last line of a list, which would leave a divider at the bottom
        document.querySelectorAll(".card").forEach(card => {
            const lines = [...card.querySelectorAll(":scope > .result")].filter(el => getComputedStyle(el).display !== "none");

            if (lines.length > 0) {
                lines[lines.length - 1].style.borderBottom = "none";
            }
        });
    }

    function checkLayout() {
        document.body.classList.add("measuring");

        const problems = [];
        const rectOf = el => el.getBoundingClientRect();
        const visible = el => el.offsetParent !== null && rectOf(el).width > 0;
        const label = el => el.textContent.trim().replace(/\s+/g, " ").slice(0, 60);
        const overlaps = (a, b) => a.left < b.right - 1 && b.left < a.right - 1 && a.top < b.bottom - 1 && b.top < a.bottom - 1;

        const textSelectors = ".t, .s, .name, .what, .btn, .combo, .chip, .nav-item, .textbox, .menu-item, .kv .k, .kv .v, .kv .d, .contents .c, .link, .flabel, .dlg-title, .h1, .label, .title, .tb-label";

        document.querySelectorAll(textSelectors).forEach(el => {
            if (visible(el) && el.scrollWidth > el.clientWidth + 1) {
                problems.push(`cut off: "${label(el)}"`);
            }
        });

        document.querySelectorAll(".rows, .contents, .act, .hstack, .choice-cards, .right, .exp-head, .hero-top, .item").forEach(group => {
            const kids = [...group.children].filter(visible);

            for (let i = 0; i < kids.length; i++) {
                for (let j = i + 1; j < kids.length; j++) {
                    if (overlaps(rectOf(kids[i]), rectOf(kids[j]))) {
                        problems.push(`overlap: "${label(kids[i]).slice(0, 30)}" / "${label(kids[j]).slice(0, 30)}"`);
                    }
                }
            }
        });

        document.querySelectorAll(".scroll").forEach(area => {
            const bounds = rectOf(area);

            [...area.querySelectorAll(".card, .row, .exp, .infobar, .drop, .choice, .result, .item, .section, .split")].filter(visible).forEach(el => {
                const r = rectOf(el);

                if (r.bottom > bounds.bottom + 1 || r.right > bounds.right + 1) {
                    problems.push(`past the scroll area by ${Math.ceil(Math.max(r.bottom - bounds.bottom, r.right - bounds.right))} px: "${label(el).slice(0, 50)}"`);
                }
            });
        });

        document.querySelectorAll(".window").forEach(win => {
            const bounds = rectOf(win);

            win.querySelectorAll(".flyout, .dialog").forEach(el => {
                const r = rectOf(el);

                if (visible(el) && (r.bottom > bounds.bottom + 1 || r.right > bounds.right + 1 || r.top < bounds.top - 1)) {
                    problems.push(`outside the window: "${label(el).slice(0, 50)}"`);
                }
            });
        });

        const page = document.querySelector(".page");
        const pageBounds = rectOf(page);

        document.querySelectorAll(".window, .frame").forEach(win => {
            if (rectOf(win).right > pageBounds.right + 1) {
                problems.push(`window wider than the page by ${Math.ceil(rectOf(win).right - pageBounds.right)} px`);
            }
        });

        const report = document.createElement("pre");
        report.id = "report";
        report.style.display = "none";
        report.textContent = JSON.stringify({ page: [Math.ceil(pageBounds.width), Math.ceil(pageBounds.bottom + 4)], problems });
        document.body.appendChild(report);
    }

    document.addEventListener("DOMContentLoaded", () => {
        applyStates();

        if (params.has("check")) {
            // after fonts, so measured widths are the real ones
            document.fonts.ready.then(() => setTimeout(checkLayout, 50));
        }
    });
})();
