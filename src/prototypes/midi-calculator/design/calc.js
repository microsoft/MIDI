// PROTOTYPE. Draws the parts every Windows MIDI Calculator comp shares (title bar, navigation,
// page heading and session values) around a page's own calculators, so each page only holds what
// is different about it. Nothing here is product code.
//
//   <div class="window" data-calc="tempo" style="width:1240px;height:900px"> ...cards... </div>
//
// data-calc        which navigation item is selected, and the page heading
// data-nav         "narrow" folds the navigation to icons
// data-session     "hide" leaves the session values off
// data-mark-nav    a callout number for the navigation
// data-mark-session a callout number for the session values

(function () {
    const pages = [
        { id: 'tempo', glyph: '\uE916', name: 'Tempo and beats' },
        { id: 'pitch', glyph: '\uE8D6', name: 'Notes and pitch' },
        { id: 'chords', glyph: '\uEC4F', name: 'Chords and scales' },
        { id: 'timecode', glyph: '\uE823', name: 'Timecode and samples' },
        { id: 'midi2', glyph: '\uE943', name: 'MIDI 2.0 values' },
        { id: 'windows', glyph: '\uE770', name: 'Windows timestamps' }
    ];

    function el(tag, cls, html) {
        const e = document.createElement(tag);
        if (cls) e.className = cls;
        if (html !== undefined) e.innerHTML = html;
        return e;
    }

    function build(win) {
        const id = win.dataset.calc;
        const page = pages.find(p => p.id === id) || { name: win.dataset.title || '' };
        const content = Array.from(win.childNodes);

        const title = el('div', 'titlebar',
            '<span class="appglyph gl">\uE8EF</span>' +
            '<div class="title">Windows MIDI Calculator</div>' +
            '<div class="preview">PREVIEW</div>' +
            '<div class="spacer"></div>' +
            '<div class="cap" title="Compact overlay (Ctrl+Shift+O)">\uE73F</div>' +
            '<div class="cap">\uE713</div>' +
            '<div class="cap">\uE718</div>' +
            '<div class="sys"><div>\uE921</div><div>\uE922</div><div>\uE8BB</div></div>');

        const body = el('div', 'body');
        const nav = el('div', 'nav' + (win.dataset.nav === 'narrow' ? ' narrow' : ''));
        const narrow = win.dataset.nav === 'narrow';

        nav.appendChild(el('div', 'ni', '<span class="gl">\uE700</span>'));
        for (const p of pages) {
            nav.appendChild(el('div', 'ni' + (p.id === id ? ' sel' : ''),
                '<span class="gl">' + p.glyph + '</span>' + (narrow ? '' : p.name)));
        }
        nav.appendChild(el('div', 'spacer'));
        nav.appendChild(el('div', 'sep'));
        nav.appendChild(el('div', 'ni' + (id === 'history' ? ' sel' : ''),
            '<span class="gl">\uE81C</span>' + (narrow ? '' : 'History')));

        if (win.dataset.markNav) {
            nav.firstElementChild.nextElementSibling.dataset.mark = win.dataset.markNav;
        }

        const pane = el('div', 'content');
        const head = el('div', 'head', '<h1>' + page.name + '</h1><div class="spacer"></div>' +
            (win.dataset.headExtra || ''));
        pane.appendChild(head);

        if (win.dataset.session !== 'hide') {
            const s = el('div', 'session',
                '<span class="t">Session</span>' +
                '<span class="sv">Tempo <span class="v">120</span> BPM</span>' +
                '<span class="sv">Meter <span class="v">4/4</span></span>' +
                '<span class="sv">Sample rate <span class="v">48,000</span> Hz</span>' +
                '<span class="sv">A3 = <span class="v">440</span> Hz</span>' +
                '<span class="sv">Frame rate <span class="v">24</span> fps</span>' +
                '<span class="spacer"></span>' +
                '<span class="btn sm subtle"><span class="gl">\uE777</span>Reset</span>');
            if (win.dataset.markSession) s.dataset.mark = win.dataset.markSession;
            pane.appendChild(s);
        }

        const scroll = el('div', 'scroll');
        for (const n of content) scroll.appendChild(n);
        pane.appendChild(scroll);

        body.appendChild(nav);
        body.appendChild(pane);
        win.appendChild(title);
        win.appendChild(body);
    }

    document.addEventListener('DOMContentLoaded', () => {
        document.querySelectorAll('.window[data-calc]').forEach(build);

        // page.html?check lists what spills out of a window or is cut off inside a box, for
        // capture.ps1 -Check. Headless Edge prints it with --dump-dom.
        if (location.search.includes('check')) {
            setTimeout(() => {
                const out = [];
                document.querySelectorAll('.window').forEach(win => {
                    const w = win.getBoundingClientRect();
                    win.querySelectorAll('*').forEach(e => {
                        const r = e.getBoundingClientRect();
                        if (r.width === 0 || r.height === 0) return;
                        const text = (e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 50);
                        if (r.right > w.right + 1 || r.bottom > w.bottom + 1) {
                            out.push('OUTSIDE ' + e.className + ' "' + text + '" by ' + Math.round(Math.max(r.right - w.right, r.bottom - w.bottom)));
                        }
                        const style = getComputedStyle(e);
                        if (style.overflow === 'hidden' && (e.scrollWidth > e.clientWidth + 1) && e.children.length < 4) {
                            out.push('CLIPPED ' + e.className + ' "' + text + '" ' + e.scrollWidth + '>' + e.clientWidth);
                        }
                    });
                });
                const pre = document.createElement('pre');
                pre.id = 'check';
                pre.textContent = 'CHECK-BEGIN\n' + out.join('\n') + '\nCHECK-END';
                document.body.appendChild(pre);
            }, 300);
        }
    });
})();
