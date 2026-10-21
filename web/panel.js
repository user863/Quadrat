// web panel: a canvas port of the plugin's editor (PluginEditor.cpp)
// same 1120x660 layout, scaled to the window

export const paint = {
    plate: '#d2d3d0', plateHi: '#dddedb', plateLo: '#c6c7c4',
    engrave: '#8d8f8b', relief: '#eaebe8', shelf: '#e9eae7',
    knobTop: '#3d3f40', knobBot: '#2a2c2d', knobRim: '#1d1f20', index: '#f4f4f1',
    capTop: '#edeee9', capBot: '#d2d3ce', capDown: '#cfd0cb',
    chrome: '#e8e9e6', chromeLo: '#a9aba7',
    field: '#c3c4c1', screen: '#141416', screenLn: '#2b2b2e', grid: '#b7b8b5',
    tile: '#dadbd8', tileEdge: '#8d8f8b',
    ink: '#2c2e2c', inkSoft: '#6d6f6b', red: '#c8362b', green: '#2f7d4f',
    sig: ['#2f7d4f', '#2d5d9e', '#c8362b', '#c79a1e']      // audio, pitch, gate, cv
};

const W = 1120, H = 660, M = 24, HEADER = 64, PALETTE_W = 210, CELL = 78;
const PALETTE = { x: M, y: HEADER, w: PALETTE_W, h: H - HEADER - M };
const FIELD = { x: M + PALETTE_W + 14, y: HEADER, w: W - (M + PALETTE_W + 14) - M, h: H - HEADER - M };
const SHELF = { x: 16, y: HEADER + 34, w: FIELD.x - 32, h: PALETTE.h - 34 };
export const SEARCH = { x: 16, y: HEADER, w: FIELD.x - 32, h: 24 };
const CAP_R = 11;
const SECTIONS = [['source', 'sources'], ['mi', 'voices'], ['shape', 'shaping'], ['mod', 'modulation'],
                  ['time', 'timing'], ['gen', 'generative'], ['out', 'output']];
// ids from Blocks.h
const K = { osc: 1, noise: 2, filter: 3, clock: 14, seq: 15, macro: 24, comp: 33, limit: 34,
            meter: 35, timbre: 36, ball: 37, midi: 38, input: 39 };
const SCOPED = new Set ([K.osc, K.timbre, K.ball, K.macro, K.noise, K.comp, K.limit, K.clock, K.filter, K.midi, K.input]);

const font = (px, weight = 400) => `${weight} ${px}px Inter, system-ui, sans-serif`;
const clamp = (v, a, b) => Math.max (a, Math.min (b, v));
const inside = (r, p) => r && p.x >= r.x && p.x <= r.x + r.w && p.y >= r.y && p.y <= r.y + r.h;
const reduce = (r, dx, dy = dx) => ({ x: r.x + dx, y: r.y + dy, w: r.w - dx * 2, h: r.h - dy * 2 });

function label (ctx, text, x, y, size, spacing = 0, align = 'left', width = 0, bold = false)
{
    ctx.font = font (size, bold ? 600 : 400);
    text = String (text);
    let w = ctx.measureText (text).width + spacing * Math.max (0, text.length - 1);

    // shrink to fit
    if (width > 0 && w > width) { size *= width / w; ctx.font = font (size, bold ? 600 : 400); w = width; }

    let cx = align === 'centre' ? x + (width - w) / 2 : align === 'right' ? x + width - w : x;

    if (spacing < 0.05) { ctx.fillText (text, cx, y); return; }

    for (const ch of text) { ctx.fillText (ch, cx, y); cx += ctx.measureText (ch).width + spacing; }
}

function rect (ctx, r) { ctx.fillRect (r.x, r.y, r.w, r.h); }
function rrect (ctx, r, rad) { ctx.beginPath(); ctx.roundRect (r.x, r.y, r.w, r.h, rad); }

function drawKnob (ctx, cx, cy, r, v, scale, mark)
{
    const a0 = Math.PI * 1.25, a1 = Math.PI * 2.75, a = a0 + clamp (v, 0, 1) * (a1 - a0);

    if (scale)
    {
        ctx.strokeStyle = paint.inkSoft;
        ctx.lineWidth = 0.8;
        ctx.beginPath();
        for (let i = 0; i <= 10; ++i)
        {
            const t = a0 + (i / 10) * (a1 - a0), s = Math.sin (t), c = Math.cos (t);
            ctx.moveTo (cx + s * (r + 2.5), cy - c * (r + 2.5));
            ctx.lineTo (cx + s * (r + 5), cy - c * (r + 5));
        }
        ctx.stroke();
    }

    ctx.fillStyle = 'rgba(0,0,0,0.16)';
    ctx.beginPath(); ctx.arc (cx, cy + 2, r, 0, 7); ctx.fill();
    ctx.fillStyle = paint.knobRim;
    ctx.beginPath(); ctx.arc (cx, cy + 1, r, 0, 7); ctx.fill();
    const g = ctx.createLinearGradient (cx, cy - r, cx, cy + r);
    g.addColorStop (0, paint.knobTop); g.addColorStop (1, paint.knobBot);
    ctx.fillStyle = g;
    ctx.beginPath(); ctx.arc (cx, cy, r, 0, 7); ctx.fill();

    ctx.strokeStyle = mark;
    ctx.lineWidth = Math.max (1, r * 0.14);
    ctx.beginPath();
    ctx.moveTo (cx + Math.sin (a) * r * 0.14, cy - Math.cos (a) * r * 0.14);
    ctx.lineTo (cx + Math.sin (a) * r * 0.84, cy - Math.cos (a) * r * 0.84);
    ctx.stroke();
}

export class Panel
{
    constructor (canvas, specs, send, ui)
    {
        this.canvas = canvas;
        this.ctx = canvas.getContext ('2d');
        this.specs = specs;
        this.sub = specs.sub || 4;
        this.send = send;
        this.ui = ui;                         // { search, menu(items, at, pick) }
        this.state = { blocks: [], cables: [], touching: [] };
        this.zoom = 1; this.panX = -40; this.panY = -40;
        this.palScroll = 0;
        this.drag = null;
        this.mouse = { x: -1, y: -1 };
        this.selected = -1;
        this.stopped = false;
        this.showAbout = false;
        this.notice = ''; this.noticeUntil = 0;
        this.frameNext = true;
        this.capDown = null;
        this.filter ('');

        canvas.addEventListener ('pointerdown', (e) => this.down (e));
        window.addEventListener ('pointermove', (e) => this.move (e));
        window.addEventListener ('pointerup', (e) => this.up (e));
        canvas.addEventListener ('wheel', (e) => this.wheel (e), { passive: false });
        canvas.addEventListener ('dblclick', (e) => e.preventDefault());
    }

    // ── geometry ────────────────────────────────────────────────────────────
    get scale() { return this.canvas.width / devicePixelRatio / W; }
    kind (b) { return this.specs.kinds[b.kind]; }
    cellSize() { return CELL * this.zoom; }
    subSize() { return this.cellSize() / this.sub; }
    find (id) { return this.state.blocks.find (b => b.id === id); }

    toScreen (cx, cy) { const s = this.subSize(); return { x: FIELD.x + cx * s - this.panX, y: FIELD.y + cy * s - this.panY }; }
    toCell (p) { const s = this.subSize(); return { x: Math.floor ((p.x - FIELD.x + this.panX) / s), y: Math.floor ((p.y - FIELD.y + this.panY) / s) }; }

    tileRect (b)
    {
        const o = this.toScreen (b.x, b.y), c = this.cellSize(), k = this.kind (b);
        return reduce ({ x: o.x, y: o.y, w: c * k.cells, h: c * k.rows }, this.subSize() * 0.16);
    }

    portPos (r, k, isIn, i)
    {
        const n = isIn ? k.in.length : k.out.length;
        return { x: isIn ? r.x : r.x + r.w, y: r.y + r.h * (i + 1) / (n + 1) };
    }

    optionStrip (r, k, kindId)
    {
        if (! k.options || kindId === K.timbre) return null;
        const cell = r.h / Math.max (1, k.rows), h = cell * 0.22, inset = cell * 0.07;
        const corner = k.knobs.length > 0 ? cell * 0.26 : inset;
        return { x: r.x + inset, y: r.y + r.h - h - inset * 0.6, w: r.w - inset - corner, h };
    }

    tabRect (r, k)
    {
        if (k.knobs.length === 0) return null;
        const cell = r.h / Math.max (1, k.rows), w = cell * 0.2;
        const cy = r.y + r.h - cell * 0.07 * 0.6 - cell * 0.22 * 0.5, cx = r.x + r.w - cell * 0.15;
        return { x: cx - w / 2, y: cy - w / 2, w, h: w };
    }

    controlRect (r, k, kindId, i)
    {
        const n = k.knobs.length;
        if (n <= 0 || i < 0 || i >= n) return null;

        const perRow = clamp (k.cells, 1, n), rows = Math.ceil (n / perRow);
        const cell = r.h / Math.max (1, k.rows), titleH = cell * 0.25;
        const strip = this.optionStrip (r, k, kindId);
        const bottom = strip ? r.y + r.h - strip.y + cell * 0.05 : cell * 0.09;
        let a = { x: r.x + cell * 0.09, y: r.y + titleH + cell * 0.03, w: r.w - cell * 0.18, h: r.h - titleH - bottom - cell * 0.06 };

        if (kindId === K.meter) { const t = a.h * 0.62; a = { ...a, y: a.y + t, h: a.h - t }; }

        if (SCOPED.has (kindId))
        {
            const f = kindId === K.midi ? 0.66 : kindId === K.input ? 0.6 : (kindId === K.clock || kindId === K.ball) ? 0.56 : 0.42;
            const t = a.h * f; a = { ...a, y: a.y + t, h: a.h - t };
        }

        if (kindId === K.seq)
        {
            const w = a.w * 0.22;
            a = { x: a.x + a.w - w, y: a.y + a.h * 0.12, w, h: a.h * 0.76 };
        }

        const ch = Math.min (cell * 0.78, a.h / rows), top = a.y + (a.h - ch * rows) / 2;
        const row = Math.floor (i / perRow), col = i % perRow, inRow = Math.min (perRow, n - row * perRow);
        const cw = a.w / inRow;
        return { x: a.x + cw * col, y: top + ch * row, w: cw, h: ch };
    }

    faceRect (r, k, kindId)
    {
        const unit = r.h / Math.max (1, k.rows);
        const strip = this.optionStrip (r, k, kindId);
        const bottom = strip ? r.y + r.h - strip.y + unit * 0.05 : unit * 0.09;
        const a = reduce ({ x: r.x, y: r.y + unit * 0.25, w: r.w, h: r.h - unit * 0.25 - bottom }, unit * 0.09, unit * 0.03);
        const f = kindId === K.midi ? 0.62 : kindId === K.input ? 0.56 : (kindId === K.clock || kindId === K.ball) ? 0.52 : 0.38;
        return { x: a.x, y: a.y, w: a.w, h: a.h * f };
    }

    seqArea (r, k)
    {
        const unit = r.h / Math.max (1, k.rows);
        const a = reduce ({ x: r.x, y: r.y + unit * 0.26, w: r.w, h: r.h - unit * 0.36 }, unit * 0.08, unit * 0.03);
        return { ...a, w: a.w - r.w * 0.24 };
    }

    seqCell (r, k, step, lane)
    {
        const a = this.seqArea (r, k), w = a.w / 8;
        const col = { x: a.x + w * step, y: a.y, w: w * 0.86, h: a.h };
        const sw = col.h * 0.17, laneH = (col.h - sw) / 2;
        if (lane === 2) return { x: col.x, y: col.y + col.h - sw, w: col.w, h: sw };
        return { x: col.x, y: col.y + laneH * lane, w: col.w, h: laneH * 0.94 };
    }

    drawerRect (b)
    {
        const k = this.kind (b);
        if (! b.drawer || k.knobs.length === 0) return null;
        const r = this.tileRect (b), cell = r.h / Math.max (1, k.rows);
        const perRow = clamp (k.cells, 1, k.knobs.length), rows = Math.ceil (k.knobs.length / perRow);
        return { x: r.x, y: r.y + r.h + cell * 0.06, w: r.w, h: cell * 0.30 * rows + cell * 0.16, cell, perRow };
    }

    drawerCell (b, i)
    {
        const d = this.drawerRect (b);
        if (! d) return null;
        const k = this.kind (b), inner = reduce (d, d.cell * 0.06, d.cell * 0.08);
        const row = Math.floor (i / d.perRow), col = i % d.perRow;
        const inRow = Math.min (d.perRow, k.knobs.length - row * d.perRow), cw = inner.w / inRow;
        return { x: inner.x + cw * col, y: inner.y + d.cell * 0.30 * row, w: cw, h: d.cell * 0.30 };
    }

    jack (b, i)
    {
        const c = this.drawerCell (b, i);
        return c ? { x: c.x + c.h * 0.46, y: c.y + c.h / 2, r: c.h * 0.26 } : null;
    }

    knobTarget (b, i)
    {
        const j = this.jack (b, i);
        if (j) return j;
        const r = this.tileRect (b), k = this.kind (b), cell = r.h / Math.max (1, k.rows);
        return { x: r.x + r.w - cell * 0.26, y: r.y + r.h };
    }

    cableTo (b, i, toKnob)
    {
        return this.state.cables.find (c => c.to === b.id && c.tp === i && !! c.toKnob === toKnob);
    }

    sigOf (blockId, port) { const b = this.find (blockId); return b ? (this.kind (b).out[port]?.sig ?? 0) : 0; }

    // palette
    filter (term)
    {
        term = term.trim().toLowerCase();
        this.items = [];

        for (const [group, title] of SECTIONS)
        {
            const kinds = [];
            this.specs.kinds.forEach ((k, i) =>
            {
                if (k.group !== group) return;
                if (! term || k.name.toLowerCase().includes (term) || (k.tags || '').includes (term)) kinds.push (i);
            });
            if (kinds.length) { this.items.push ({ heading: title }); kinds.forEach (i => this.items.push ({ kind: i })); }
        }

        this.palScroll = 0;
    }

    itemRects()
    {
        let y = PALETTE.y + 40 - this.palScroll;
        return this.items.map (it => { const h = it.heading ? 26 : 76; const r = { ...it, x: PALETTE.x, y, w: PALETTE.w, h }; y += h; return r; });
    }

    contentHeight() { return this.items.reduce ((s, it) => s + (it.heading ? 26 : 76), 0); }
    scrollTrack() { return { x: PALETTE.x + PALETTE.w - 5, y: PALETTE.y + 40, w: 4, h: PALETTE.h - 44 }; }

    scrollThumb()
    {
        const t = this.scrollTrack(), content = this.contentHeight();
        if (content <= t.h) return null;
        const h = Math.max (18, t.h * t.h / content);
        return { x: t.x, y: t.y + (t.h - h) * (this.palScroll / Math.max (1, content - t.h)), w: t.w, h };
    }

    scrollBy (d)
    {
        const room = Math.max (0, this.contentHeight() - this.scrollTrack().h);
        this.palScroll = clamp (this.palScroll + d, 0, room);
    }

    // header caps
    caps()
    {
        const stop = FIELD.x + FIELD.w - CAP_R;
        return [['clear', stop - 168], ['roll', stop - 112], ['start', stop - 56], ['stop', stop]]
            .map (([name, cx]) => ({ name, cx, cy: 13 + CAP_R }));
    }

    isFree (x, y, cells, rows, ignore = -1)
    {
        const s = this.sub;
        return ! this.state.blocks.some (b =>
        {
            if (b.id === ignore) return false;
            const k = this.kind (b);
            return x < b.x + k.cells * s && b.x < x + cells * s && y < b.y + k.rows * s && b.y < y + rows * s;
        });
    }

    say (text) { this.notice = text; this.noticeUntil = performance.now() + 2500; }

    // fit patch on open
    frameNow()
    {
        const bl = this.state.blocks;
        if (! bl.length) return;
        let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
        for (const b of bl)
        {
            const k = this.kind (b);
            x0 = Math.min (x0, b.x); y0 = Math.min (y0, b.y);
            x1 = Math.max (x1, b.x + k.cells * this.sub); y1 = Math.max (y1, b.y + k.rows * this.sub + (b.drawer ? this.sub : 0));
        }
        const unit = CELL / this.sub, sw = x1 - x0, sh = y1 - y0;
        this.zoom = clamp (Math.min (FIELD.w / (sw * unit), FIELD.h / (sh * unit)) * 0.9, 0.3, 1.6);
        const px = this.subSize();
        const slackX = Math.min (40, (FIELD.w - sw * px) / 2), slackY = Math.min (40, (FIELD.h - sh * px) / 2);
        this.panX = Math.max (-40, x0 * px - Math.max (8, slackX));
        this.panY = Math.max (-40, y0 * px - Math.max (8, slackY));
    }

    setState (s)
    {
        this.state = s;
        if (this.drag && this.drag.kind === 'knob') { const b = this.find (this.drag.id); if (b) b.knobs[this.drag.i] = this.drag.value; }
        if (this.frameNext && s.blocks.length) { this.frameNext = false; this.frameNow(); }
    }

    // ── drawing ─────────────────────────────────────────────────────────────
    frame()
    {
        const ctx = this.ctx, s = this.scale;
        ctx.setTransform (devicePixelRatio * s, 0, 0, devicePixelRatio * s, 0, 0);

        const bg = ctx.createLinearGradient (0, 0, 0, H);
        bg.addColorStop (0, paint.plateHi); bg.addColorStop (1, paint.plateLo);
        ctx.fillStyle = bg; ctx.fillRect (0, 0, W, H);

        ctx.fillStyle = this.showAbout ? paint.sig[0] : paint.ink;
        label (ctx, 'quadrat', M, 38, 17, 1.2);

        if (this.notice && performance.now() < this.noticeUntil)
        {
            ctx.fillStyle = paint.inkSoft;
            label (ctx, this.notice, 330, 37, 9.5, 0, 'left', 380);
        }

        this.drawCaps();

        // shelf
        ctx.fillStyle = paint.shelf; rrect (ctx, SHELF, 5); ctx.fill();
        ctx.strokeStyle = 'rgba(141,143,139,0.35)'; ctx.lineWidth = 1; ctx.stroke();

        this.drawPalette();
        this.drawField();

        if (this.drag && this.drag.kind === 'palette') this.drawGhost();
        this.drawTooltip();
        if (this.showAbout) this.drawAbout();
    }

    drawCaps()
    {
        const ctx = this.ctx;
        for (const c of this.caps())
        {
            const red = c.name === 'stop', green = c.name === 'start';
            const pressed = this.capDown === c.name || (red && this.stopped);
            const drop = pressed ? 2.4 : 0;
            const base = red ? paint.red : green ? paint.green : null;
            const top = base ? (pressed ? base : base) : (pressed ? paint.capDown : paint.capTop);
            const bot = base ? shade (base, -0.35) : paint.capBot;

            ctx.fillStyle = `rgba(0,0,0,${pressed ? 0.14 : 0.2})`;
            ctx.beginPath(); ctx.arc (c.cx, c.cy + (pressed ? 2 : 3.5), CAP_R, 0, 7); ctx.fill();
            ctx.fillStyle = bot;
            ctx.beginPath(); ctx.arc (c.cx, c.cy + drop + 1.2, CAP_R, 0, 7); ctx.fill();
            const g = ctx.createLinearGradient (0, c.cy - CAP_R + drop, 0, c.cy + CAP_R + drop);
            g.addColorStop (0, top); g.addColorStop (1, shade (bot, 0.1));
            ctx.fillStyle = g;
            ctx.beginPath(); ctx.arc (c.cx, c.cy + drop, CAP_R, 0, 7); ctx.fill();

            if (pressed)
            {
                ctx.strokeStyle = 'rgba(0,0,0,0.13)'; ctx.lineWidth = 1.6;
                ctx.beginPath(); ctx.arc (c.cx, c.cy + drop, CAP_R - 0.6, 0, 7); ctx.stroke();
            }

            ctx.fillStyle = pressed ? paint.ink : paint.inkSoft;
            label (ctx, c.name, c.cx - 26, c.cy + CAP_R + 12, 8, 0, 'centre', 52);
        }
    }

    drawPalette()
    {
        const ctx = this.ctx;
        ctx.save();
        ctx.beginPath(); ctx.rect (PALETTE.x - 8, PALETTE.y + 38, PALETTE.w + 16, PALETTE.h - 38); ctx.clip();

        for (const it of this.itemRects())
        {
            if (it.y + it.h < PALETTE.y + 38 || it.y > PALETTE.y + PALETTE.h) continue;

            if (it.heading)
            {
                ctx.fillStyle = paint.inkSoft;
                label (ctx, it.heading, it.x, it.y + it.h - 4, 8, 1.4);
                continue;
            }

            const k = this.specs.kinds[it.kind];
            const unit = Math.min ((76 - 13 - 6) / Math.max (1, k.rows), (it.w - 10) / 4);
            ctx.fillStyle = paint.ink;
            label (ctx, k.name, it.x + 4, it.y + 13 * 0.85, 10.5, 0.4);
            const tile = { x: it.x + 4, y: it.y + 13 + 1.8, w: unit * k.cells, h: unit * k.rows };
            this.drawTile (this.sample (it.kind), tile, false);
        }

        ctx.restore();

        const thumb = this.scrollThumb();
        if (thumb)
        {
            const t = this.scrollTrack();
            ctx.fillStyle = 'rgba(141,143,139,0.35)'; rrect (ctx, t, t.w * 0.35); ctx.fill();
            ctx.fillStyle = paint.inkSoft; rrect (ctx, thumb, thumb.w * 0.35); ctx.fill();
        }
    }

    sample (kindIdx)
    {
        const k = this.specs.kinds[kindIdx];
        return { id: -1, kind: kindIdx, x: 0, y: 0, option: 0, drawer: 0, meter: 0,
                 knobs: k.knobs.map (n => n.def), depth: k.knobs.map (() => 1),
                 steps: Array (8).fill (0.5), steps2: Array (8).fill (0.5), modes: Array (8).fill (0), playhead: -1 };
    }

    drawField()
    {
        const ctx = this.ctx, f = FIELD;
        ctx.save();
        ctx.beginPath(); ctx.rect (f.x, f.y, f.w, f.h); ctx.clip();

        ctx.fillStyle = paint.field; rect (ctx, f);

        const c = this.cellSize(), s = this.subSize();
        if (s > 6)
        {
            ctx.fillStyle = 'rgba(183,184,181,0.45)';
            for (let x = f.x - (this.panX % s + s) % s; x < f.x + f.w; x += s) ctx.fillRect (x, f.y, 1, f.h);
            for (let y = f.y - (this.panY % s + s) % s; y < f.y + f.h; y += s) ctx.fillRect (f.x, y, f.w, 1);
        }
        ctx.fillStyle = paint.grid;
        for (let x = f.x - (this.panX % c + c) % c; x < f.x + f.w; x += c) ctx.fillRect (x, f.y, 1, f.h);
        for (let y = f.y - (this.panY % c + c) % c; y < f.y + f.h; y += c) ctx.fillRect (f.x, y, f.w, 1);

        const moving = this.drag && this.drag.kind === 'move' ? this.drag.id : -1;

        for (const b of this.state.blocks)
            if (b.id !== moving) this.drawTile (b, this.tileRect (b), b.id === this.selected);

        for (const b of this.state.blocks) this.drawDrawer (b);
        this.drawCables();

        if (moving >= 0)
        {
            const b = this.find (moving);
            if (b) { const r = this.tileRect (b); this.drawTile (b, { ...r, x: this.mouse.x - this.drag.ox, y: this.mouse.y - this.drag.oy }, true); }
        }

        if (this.drag && this.drag.kind === 'cable')
        {
            const from = this.drag.from;
            ctx.strokeStyle = paint.sig[this.sigOf (this.drag.src, this.drag.port)];
            ctx.lineWidth = 2.2;
            this.cablePath (from, this.mouse); ctx.stroke();
        }

        ctx.restore();
    }

    drawGhost()
    {
        const ctx = this.ctx, d = this.drag, k = this.specs.kinds[d.kindIdx];
        const c = this.cellSize();
        let r = { x: this.mouse.x - c * k.cells / 2, y: this.mouse.y - c * k.rows / 2, w: c * k.cells, h: c * k.rows };
        let free = true;

        if (inside (FIELD, this.mouse))
        {
            const cell = this.toCell (r);
            const o = this.toScreen (Math.max (0, cell.x), Math.max (0, cell.y));
            r = reduce ({ x: o.x, y: o.y, w: r.w, h: r.h }, this.subSize() * 0.16);
            free = this.isFree (Math.max (0, cell.x), Math.max (0, cell.y), k.cells, k.rows);
        }

        ctx.globalAlpha = 0.85;
        this.drawTile (this.sample (d.kindIdx), r, false);
        ctx.globalAlpha = 1;
        ctx.strokeStyle = free ? paint.sig[0] : paint.sig[2];
        ctx.lineWidth = Math.max (1.5, c * 0.025);
        ctx.strokeRect (r.x, r.y, r.w, r.h);
    }

    drawTile (b, r, selected)
    {
        const ctx = this.ctx, k = this.kind (b), unit = r.h / Math.max (1, k.rows);

        ctx.fillStyle = 'rgba(0,0,0,0.12)'; ctx.fillRect (r.x, r.y + unit * 0.03, r.w, r.h);
        ctx.fillStyle = selected ? shade (paint.tile, 0.06) : paint.tile; rect (ctx, r);
        ctx.strokeStyle = selected ? paint.ink : paint.tileEdge;
        ctx.lineWidth = unit * (selected ? 0.022 : 0.013);
        ctx.strokeRect (r.x, r.y, r.w, r.h);

        ctx.fillStyle = paint.ink;
        label (ctx, k.name, r.x + unit * 0.09, r.y + unit * 0.21, unit * 0.15);

        if (k.bp && k.bp.length)
        {
            ctx.font = font (unit * 0.15);
            const cx = r.x + unit * 0.09 + ctx.measureText (k.name).width + unit * 0.08, cy = r.y + unit * 0.17, ts = unit * 0.035;
            ctx.fillStyle = paint.inkSoft;
            ctx.beginPath(); ctx.moveTo (cx - ts, cy - ts * 0.7); ctx.lineTo (cx + ts, cy - ts * 0.7); ctx.lineTo (cx, cy + ts * 0.8); ctx.fill();
        }

        const strip = this.optionStrip (r, k, b.kind);
        if (strip) this.drawStrip (strip, k, b.option);

        const tab = this.tabRect (r, k);
        if (tab)
        {
            const wired = this.state.cables.some (c => c.toKnob && c.to === b.id);
            const ts = tab.w * (wired ? 0.34 : 0.26), cx = tab.x + tab.w / 2, cy = tab.y + tab.h / 2;
            const th = b.drawer ? Math.PI / 2 : Math.PI, dx = Math.cos (th), dy = Math.sin (th);
            ctx.fillStyle = `rgba(109,111,107,${wired ? 0.85 : 0.55})`;
            ctx.beginPath();
            ctx.moveTo (cx + dx * ts, cy + dy * ts);
            ctx.lineTo (cx - dx * ts * 0.55 - dy * ts * 0.85, cy - dy * ts * 0.55 + dx * ts * 0.85);
            ctx.lineTo (cx - dx * ts * 0.55 + dy * ts * 0.85, cy - dy * ts * 0.55 - dx * ts * 0.85);
            ctx.fill();
        }

        if (SCOPED.has (b.kind)) this.drawFace (b, k, this.faceRect (r, k, b.kind), unit);

        for (let i = 0; i < k.knobs.length; ++i) this.drawControl (b, k, r, i, unit);

        if (b.kind === K.meter) this.drawMeter (b, r, unit);
        if (b.kind === K.seq) this.drawSeq (b, k, r, unit);

        // ports
        const pr = unit * 0.055, ls = unit * 0.095, names = unit > 46;
        const hunting = this.drag && this.drag.kind === 'cable' ? this.sigOf (this.drag.src, this.drag.port) : -1;

        k.in.forEach ((p, i) =>
        {
            const y = r.y + r.h * (i + 1) / (k.in.length + 1), col = paint.sig[p.sig];
            if (hunting >= 0)
            {
                const agrees = p.sig === hunting, halo = pr * (agrees ? 2.2 : 1.6);
                ctx.fillStyle = withAlpha (paint.sig[hunting], agrees ? 0.45 : 0.18);
                ctx.beginPath(); ctx.arc (r.x, y, halo, 0, 7); ctx.fill();
            }
            ctx.fillStyle = paint.field; ctx.beginPath(); ctx.arc (r.x, y, pr, 0, 7); ctx.fill();
            ctx.strokeStyle = col; ctx.lineWidth = unit * 0.022; ctx.stroke();
            if (names) { ctx.fillStyle = withAlpha (col, 0.85); label (ctx, p.name, r.x + pr * 1.6, y + ls * 0.36, ls, 0, 'left', r.w * 0.4); }
        });

        k.out.forEach ((p, i) =>
        {
            const y = r.y + r.h * (i + 1) / (k.out.length + 1), col = paint.sig[p.sig];
            ctx.fillStyle = col; ctx.beginPath(); ctx.arc (r.x + r.w, y, pr, 0, 7); ctx.fill();
            if (names) { ctx.fillStyle = withAlpha (col, 0.85); label (ctx, p.name, r.x + r.w - pr * 1.6 - r.w * 0.4, y + ls * 0.36, ls, 0, 'right', r.w * 0.4); }
        });

        // output lamp
        const lr = unit * 0.06, lx = r.x + r.w - lr * 2.2, ly = r.y + lr * 2.2;
        ctx.fillStyle = 'rgba(141,143,139,0.45)'; ctx.beginPath(); ctx.arc (lx, ly, lr, 0, 7); ctx.fill();
        ctx.fillStyle = withAlpha (paint.red, clamp ((b.meter || 0) * 2.2, 0.12, 1));
        ctx.beginPath(); ctx.arc (lx, ly, lr * 0.78, 0, 7); ctx.fill();
    }

    drawStrip (strip, k, option)
    {
        const ctx = this.ctx, opts = k.options.split ('|'), n = opts.length;
        let fs = strip.h * 0.55;

        if (n <= 6)
        {
            const w = strip.w / n;
            ctx.font = font (fs);
            const widest = Math.max (...opts.map (o => ctx.measureText (o).width)), room = w - strip.h * 0.3;
            if (widest > room) fs *= Math.max (0.55, room / widest);

            opts.forEach ((o, i) =>
            {
                const box = { x: strip.x + i * w, y: strip.y, w: w - 1.5, h: strip.h }, on = i === option;
                ctx.fillStyle = on ? paint.ink : paint.field; rect (ctx, box);
                ctx.strokeStyle = paint.engrave; ctx.lineWidth = strip.h * 0.05; ctx.strokeRect (box.x, box.y, box.w, box.h);
                ctx.fillStyle = on ? paint.plate : paint.inkSoft;
                label (ctx, o, box.x, box.y + box.h / 2 + fs * 0.36, fs, 0, 'centre', box.w);
            });
            return;
        }

        // >6: dropdown
        ctx.fillStyle = paint.field; rect (ctx, strip);
        ctx.strokeStyle = paint.engrave; ctx.lineWidth = 0.8; ctx.strokeRect (strip.x, strip.y, strip.w, strip.h);
        ctx.fillStyle = paint.ink;
        label (ctx, opts[option] || '', strip.x + strip.h * 0.4, strip.y + strip.h / 2 + fs * 0.36, fs, 0, 'left', strip.w - 18);
        const cx = strip.x + strip.w - strip.h * 0.5, cy = strip.y + strip.h / 2, ts = strip.h * 0.18;
        ctx.fillStyle = paint.inkSoft;
        ctx.beginPath(); ctx.moveTo (cx - ts, cy - ts * 0.6); ctx.lineTo (cx + ts, cy - ts * 0.6); ctx.lineTo (cx, cy + ts * 0.8); ctx.fill();
    }

    drawFace (b, k, f, unit)
    {
        const ctx = this.ctx;
        ctx.fillStyle = paint.screen; rect (ctx, f);

        if (b.kind === K.clock)
        {
            const cx = f.x + f.w / 2, cy = f.y + f.h / 2, rad = Math.min (f.w, f.h) * 0.42;
            ctx.strokeStyle = '#8d8f8b'; ctx.lineWidth = 1;
            ctx.beginPath();
            for (let i = 0; i < 16; ++i)
            {
                const a = i / 16 * Math.PI * 2, l = i % 4 === 0 ? 0.22 : 0.1;
                ctx.moveTo (cx + Math.sin (a) * rad, cy - Math.cos (a) * rad);
                ctx.lineTo (cx + Math.sin (a) * rad * (1 - l), cy - Math.cos (a) * rad * (1 - l));
            }
            ctx.stroke();
            const a = (b.phase || 0) * Math.PI * 2;
            ctx.strokeStyle = paint.red; ctx.lineWidth = Math.max (1.5, rad * 0.07);
            ctx.beginPath(); ctx.moveTo (cx, cy); ctx.lineTo (cx + Math.sin (a) * rad * 0.92, cy - Math.cos (a) * rad * 0.92); ctx.stroke();
            ctx.fillStyle = paint.red; ctx.beginPath(); ctx.arc (cx, cy, rad * 0.08, 0, 7); ctx.fill();
            return;
        }

        const msg = b.kind === K.midi ? 'paste notes in the plugin' : b.kind === K.input ? 'audio in: plugin only' : null;
        if (msg)
        {
            ctx.fillStyle = '#8d8f8b';
            label (ctx, msg, f.x, f.y + f.h / 2 + 3, Math.max (6, f.h * 0.14), 0, 'centre', f.w);
            return;
        }

        if (b.kind === K.timbre)
        {
            const models = (k.options || '').split ('|');
            const i = clamp (Math.round ((b.knobs[0] ?? 0) * (models.length - 1)), 0, models.length - 1);
            ctx.fillStyle = paint.sig[0];
            label (ctx, models[i] || '', f.x, f.y + f.h / 2 + f.h * 0.15, f.h * 0.4, 0, 'centre', f.w - 6);
            return;
        }

        ctx.fillStyle = paint.screenLn; ctx.fillRect (f.x, f.y + f.h / 2, f.w, 1);
        const sc = b.scope;
        if (! sc) return;

        // comp/limit: gain reduction hangs down
        const gr = b.kind === K.comp || b.kind === K.limit;
        const half = f.h * 0.42;
        ctx.beginPath();
        sc.forEach ((v, i) =>
        {
            const x = f.x + f.w * i / (sc.length - 1);
            const y = gr ? f.y + clamp (v, 0, 1) * f.h : f.y + f.h / 2 - clamp (v, -1.2, 1.2) * half;
            i ? ctx.lineTo (x, y) : ctx.moveTo (x, y);
        });
        ctx.strokeStyle = gr ? paint.sig[3] : paint.sig[k.out[0]?.sig ?? 0];
        ctx.lineWidth = Math.max (1, f.h * 0.05);
        ctx.stroke();
    }

    drawControl (b, k, r, i, unit)
    {
        const ctx = this.ctx, cell = this.controlRect (r, k, b.kind, i);
        if (! cell) return;

        const spec = k.knobs[i], v = clamp (b.knobs[i] ?? spec.def, 0, 1);
        let mark = paint.index, moved = false;
        const cb = b.id >= 0 && this.cableTo (b, i, true);
        if (cb && (b.depth?.[i] ?? 1) > 0.001) { mark = paint.sig[this.sigOf (cb.from, cb.fp)]; moved = true; }

        const labelH = cell.h * 0.3, face = { x: cell.x, y: cell.y, w: cell.w, h: cell.h - labelH };

        if (spec.type === 1)            // slider
        {
            const y = face.y + face.h / 2, x0 = face.x + unit * 0.03, x1 = face.x + face.w - unit * 0.03;
            ctx.fillStyle = paint.engrave; ctx.fillRect (x0, y, x1 - x0, unit * 0.014);
            ctx.fillStyle = paint.relief; ctx.fillRect (x0, y + unit * 0.014, x1 - x0, unit * 0.014);
            const hx = x0 + (x1 - x0) * v, hw = unit * 0.035, hh = unit * 0.16;
            ctx.fillStyle = moved ? shade (mark, -0.35) : paint.chromeLo; ctx.fillRect (hx - hw, y - hh / 2, hw * 2, hh);
            ctx.fillStyle = moved ? mark : paint.chrome; ctx.fillRect (hx - hw, y - hh / 2, hw * 2, hh * 0.55);
        }
        else if (spec.type === 2)       // toggle
        {
            const d = Math.min (face.h, face.w) * 0.62, box = { x: face.x + face.w / 2 - d / 2, y: face.y + face.h / 2 - d / 2, w: d, h: d };
            ctx.fillStyle = v > 0.5 ? paint.red : paint.capTop; rect (ctx, box);
            ctx.strokeStyle = moved ? mark : paint.engrave; ctx.lineWidth = moved ? Math.max (1.5, unit * 0.02) : 1;
            ctx.strokeRect (box.x, box.y, box.w, box.h);
        }
        else if (spec.type === 3)       // select
        {
            const box = reduce (face, face.w * 0.08, face.h * 0.2), opts = (spec.opts || '').split ('|');
            ctx.fillStyle = paint.field; rect (ctx, box);
            ctx.strokeStyle = moved ? mark : paint.engrave; ctx.lineWidth = 1; ctx.strokeRect (box.x, box.y, box.w, box.h);
            ctx.fillStyle = paint.ink;
            label (ctx, opts[Math.round (v * (opts.length - 1))] || '', box.x, box.y + box.h / 2 + unit * 0.045, Math.max (6, unit * 0.12), 0, 'centre', box.w);
        }
        else
        {
            const kr = Math.min (face.h * 0.40, face.w * 0.32);
            drawKnob (ctx, face.x + face.w / 2, face.y + face.h / 2, kr, v, kr > 9, mark);
        }

        if (spec.name && labelH > 5)
        {
            ctx.fillStyle = paint.inkSoft;
            label (ctx, spec.name, cell.x, cell.y + cell.h - labelH * 0.28, unit * 0.105, 0, 'centre', cell.w);
        }
    }

    drawMeter (b, r, unit)
    {
        const ctx = this.ctx, k = this.kind (b), strip = this.optionStrip (r, k, b.kind);
        let f = reduce ({ x: r.x, y: r.y + Math.min (22, unit * 0.26), w: r.w, h: strip.y - 6 - r.y - Math.min (22, unit * 0.26) }, 8, 2);
        f = { ...f, h: f.h * 0.6 };
        ctx.fillStyle = '#f1efe6'; rect (ctx, f);
        const cx = f.x + f.w / 2, cy = f.y + f.h * 1.35, rad = f.h * 1.15;
        const a = -0.75 + clamp ((b.meter || 0) * 1.5, 0, 1) * 1.5;
        ctx.save(); ctx.beginPath(); ctx.rect (f.x, f.y, f.w, f.h); ctx.clip();
        ctx.strokeStyle = paint.ink; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo (cx, cy); ctx.lineTo (cx + Math.sin (a) * rad, cy - Math.cos (a) * rad); ctx.stroke();
        ctx.restore();
    }

    drawSeq (b, k, r, unit)
    {
        const ctx = this.ctx;
        for (let i = 0; i < 8; ++i)
        {
            const mode = b.modes?.[i] ?? 0, on = mode !== 1;

            for (let lane = 0; lane < 2; ++lane)
            {
                const c = this.seqCell (r, k, i, lane), v = clamp ((lane ? b.steps2 : b.steps)?.[i] ?? 0.5, 0, 1);
                ctx.fillStyle = shade (paint.field, i === b.playhead ? -0.02 : -0.12); rect (ctx, c);
                ctx.fillStyle = 'rgba(141,143,139,0.5)';
                ctx.fillRect (c.x + c.w / 2 - Math.max (1, unit * 0.008), c.y + unit * 0.02, Math.max (2, unit * 0.016), c.h - unit * 0.04);
                const capH = Math.max (3, c.h * 0.17), travel = c.h - capH - unit * 0.04;
                const cy = c.y + c.h - unit * 0.02 - capH - travel * v;
                ctx.fillStyle = on ? paint.chromeLo : 'rgba(169,171,167,0.4)';
                ctx.fillRect (c.x + c.w * 0.1, cy, c.w * 0.8, capH);
                ctx.fillStyle = on ? (lane ? paint.sig[3] : paint.sig[1]) : 'rgba(141,143,139,0.5)';
                ctx.fillRect (c.x + c.w * 0.1, cy, c.w * 0.8, capH * 0.42);
            }

            const sw = this.seqCell (r, k, i, 2), box = reduce (sw, sw.w * 0.08, sw.h * 0.16);
            ctx.fillStyle = mode === 1 ? shade (paint.field, -0.06) : paint.ink; rect (ctx, box);
            const inn = reduce (box, box.w * 0.26, box.h * 0.3);
            ctx.strokeStyle = mode === 1 ? paint.engrave : paint.plateHi; ctx.lineWidth = Math.max (1.2, unit * 0.022);
            ctx.beginPath();
            if (mode === 0) { ctx.moveTo (inn.x, inn.y + inn.h); ctx.lineTo (inn.x + inn.w, inn.y); }
            else if (mode === 2) { ctx.moveTo (inn.x, inn.y); ctx.lineTo (inn.x + inn.w, inn.y + inn.h); }
            else { ctx.moveTo (inn.x, inn.y + inn.h / 2); ctx.lineTo (inn.x + inn.w, inn.y + inn.h / 2); }
            ctx.stroke();

            if (i === b.playhead)
            {
                const col = this.seqCell (r, k, i, 0);
                ctx.fillStyle = 'rgba(200,54,43,0.85)'; ctx.fillRect (col.x, col.y - unit * 0.045, col.w, unit * 0.025);
            }
        }
    }

    drawDrawer (b)
    {
        const d = this.drawerRect (b);
        if (! d) return;
        const ctx = this.ctx, k = this.kind (b), rad = d.cell * 0.05;
        ctx.fillStyle = 'rgba(0,0,0,0.13)'; rrect (ctx, { ...d, y: d.y + d.cell * 0.03 }, rad); ctx.fill();
        ctx.fillStyle = paint.plateHi; rrect (ctx, d, rad); ctx.fill();
        ctx.strokeStyle = 'rgba(141,143,139,0.7)'; ctx.lineWidth = 1; ctx.stroke();

        k.knobs.forEach ((spec, i) =>
        {
            const c = this.drawerCell (b, i), j = this.jack (b, i), jr = j.r, cb = this.cableTo (b, i, true);
            const col = cb ? paint.sig[this.sigOf (cb.from, cb.fp)] : paint.sig[3];

            ctx.fillStyle = 'rgba(0,0,0,0.18)'; ctx.beginPath(); ctx.arc (j.x, j.y + jr * 0.12, jr, 0, 7); ctx.fill();
            ctx.fillStyle = paint.chrome; ctx.beginPath(); ctx.arc (j.x, j.y, jr, 0, 7); ctx.fill();
            ctx.strokeStyle = paint.ink; ctx.lineWidth = jr * 0.22; ctx.stroke();
            ctx.fillStyle = paint.screen; ctx.beginPath(); ctx.arc (j.x, j.y, jr * 0.54, 0, 7); ctx.fill();

            if (cb)
            {
                ctx.fillStyle = col; ctx.beginPath(); ctx.arc (j.x, j.y, jr * 0.48, 0, 7); ctx.fill();
                ctx.strokeStyle = paint.ink; ctx.lineWidth = jr * 0.14; ctx.stroke();
                ctx.fillStyle = 'rgba(255,255,255,0.55)'; ctx.beginPath(); ctx.arc (j.x - jr * 0.16, j.y - jr * 0.18, jr * 0.14, 0, 7); ctx.fill();
            }

            // depth ring
            const rr = jr * 1.5, lw = Math.max (1.4, jr * 0.2), from = Math.PI * 1.25 - Math.PI / 2, span = Math.PI * 1.5;
            const depth = clamp (b.depth?.[i] ?? 1, 0, 1);
            ctx.lineCap = 'round';
            ctx.strokeStyle = 'rgba(141,143,139,0.35)'; ctx.lineWidth = lw;
            ctx.beginPath(); ctx.arc (j.x, j.y, rr, from, from + span); ctx.stroke();
            if (depth > 0.001)
            {
                ctx.strokeStyle = cb ? col : 'rgba(109,111,107,0.55)';
                ctx.beginPath(); ctx.arc (j.x, j.y, rr, from, from + span * depth); ctx.stroke();
            }
            ctx.lineCap = 'butt';

            ctx.fillStyle = cb ? paint.ink : paint.inkSoft;
            label (ctx, spec.name, j.x + jr * 1.95, c.y + c.h / 2 + c.h * 0.18, Math.max (6, d.cell * 0.115), 0, 'left', c.x + c.w - (j.x + jr * 1.95));
        });
    }

    cablePath (from, to)
    {
        const sag = Math.min (46, Math.hypot (to.x - from.x, to.y - from.y) * 0.22);
        this.ctx.beginPath();
        this.ctx.moveTo (from.x, from.y);
        this.ctx.bezierCurveTo (from.x + 30, from.y + sag, to.x - 30, to.y + sag, to.x, to.y);
    }

    drawCables()
    {
        const ctx = this.ctx, moving = this.drag && this.drag.kind === 'move' ? this.drag.id : -1;

        // adjacency links
        for (const t of this.state.touching || [])
        {
            if (t.from === moving || t.to === moving) continue;
            const a = this.find (t.from), b = this.find (t.to);
            if (! a || ! b) continue;
            const from = this.portPos (this.tileRect (a), this.kind (a), false, 0), to = this.portPos (this.tileRect (b), this.kind (b), true, 0);
            const reach = Math.max (this.cellSize() * 0.12, Math.abs (to.y - from.y) * 0.35), w = clamp (this.cellSize() * 0.03, 1.4, 2.2);
            ctx.beginPath(); ctx.moveTo (from.x, from.y); ctx.bezierCurveTo (from.x + reach, from.y, to.x - reach, to.y, to.x, to.y);
            ctx.strokeStyle = 'rgba(0,0,0,0.16)'; ctx.lineWidth = w + 0.8; ctx.stroke();
            ctx.strokeStyle = withAlpha (paint.sig[this.kind (a).out[0]?.sig ?? 0], 0.92); ctx.lineWidth = w; ctx.stroke();
        }

        for (const c of this.state.cables)
        {
            const a = this.find (c.from), b = this.find (c.to);
            if (! a || ! b) continue;
            let from = this.portPos (this.tileRect (a), this.kind (a), false, c.fp);
            let to = c.toKnob ? this.knobTarget (b, c.tp) : this.portPos (this.tileRect (b), this.kind (b), true, c.tp);

            if (moving >= 0)
            {
                const shiftOf = (blk) => { const r = this.tileRect (blk); return { x: this.mouse.x - this.drag.ox - r.x, y: this.mouse.y - this.drag.oy - r.y }; };
                if (a.id === moving) { const s = shiftOf (a); from = { x: from.x + s.x, y: from.y + s.y }; }
                if (b.id === moving) { const s = shiftOf (b); to = { x: to.x + s.x, y: to.y + s.y }; }
            }

            this.cablePath (from, to);
            ctx.save(); ctx.translate (0, 2); ctx.strokeStyle = 'rgba(0,0,0,0.16)'; ctx.lineWidth = 3; ctx.stroke(); ctx.restore();
            ctx.strokeStyle = withAlpha (paint.sig[this.kind (a).out[c.fp]?.sig ?? 0], 0.92); ctx.lineWidth = 2.2; ctx.stroke();
        }
    }

    drawTooltip()
    {
        if (this.drag || this.showAbout || this.hoverKind == null || performance.now() - this.hoverSince < 600) return;
        const k = this.specs.kinds[this.hoverKind];
        if (! k || ! k.about) return;

        const ctx = this.ctx, w = 260, pad = 10;
        ctx.font = font (9.5);
        const words = k.about.split (' '), lines = [];
        let line = '';
        for (const wd of words) { const t = line ? line + ' ' + wd : wd; if (ctx.measureText (t).width > w - pad * 2) { lines.push (line); line = wd; } else line = t; }
        if (line) lines.push (line);

        const h = pad * 2 + 14 + lines.length * 13;
        let x = this.mouse.x + 14, y = this.mouse.y + 18;
        if (x + w > W - 8) x = W - 8 - w;
        if (y + h > H - 8) y = this.mouse.y - h - 10;

        ctx.fillStyle = 'rgba(0,0,0,0.15)'; rrect (ctx, { x, y: y + 2, w, h }, 4); ctx.fill();
        ctx.fillStyle = paint.plateHi; rrect (ctx, { x, y, w, h }, 4); ctx.fill();
        ctx.strokeStyle = 'rgba(141,143,139,0.7)'; ctx.lineWidth = 1; ctx.stroke();
        ctx.fillStyle = paint.ink; label (ctx, k.name, x + pad, y + pad + 9, 10.5, 0.3, 'left', 0, true);
        ctx.fillStyle = paint.inkSoft;
        lines.forEach ((l, i) => label (ctx, l, x + pad, y + pad + 26 + i * 13, 9.5));
    }

    aboutPanel() { return { x: W / 2 - 250, y: 76, w: 500, h: 500 }; }
    githubLink() { return { x: W / 2 - 224, y: 76 + 500 - 22 - 17, w: 170, h: 17 }; }

    drawAbout()
    {
        const ctx = this.ctx;
        ctx.fillStyle = 'rgba(198,199,196,0.94)'; ctx.fillRect (0, 0, W, H);
        const p = this.aboutPanel();
        ctx.fillStyle = paint.plateHi; rrect (ctx, p, 3); ctx.fill();
        ctx.strokeStyle = 'rgba(109,111,107,0.4)'; ctx.lineWidth = 1; ctx.stroke();

        const lines = [
            'QUADRAT',
            'Quadrat is an open-source semi-modular synthesizer, with a fun,',
            'easy-to-understand interface that enables people to try out modular',
            'synthesis without having to develop a deep technical understanding of',
            'the underlying mechanisms. Enjoy!',
            'Free software under the GNU GPL v3, with no warranty; see LICENSE.',
            '',
            'THIS PAGE',
            'The plugin\'s own engine, compiled to WebAssembly. Play notes with A to K.',
            'The midi and in blocks, the tutorial and undo need the plugin.',
            '',
            'SOURCES',
            'macro, timbre, reson, mallet, xmod, scatter and drum are built on DSP',
            'code from the eurorack repository https://github.com/pichenettes/eurorack,',
            'by Emilie Gillet. Only code from STM32F projects is used, released',
            'under the MIT licence; see full text in third_party/mi/LICENSE-MIT.',
            '',
            'Type is Inter, by Rasmus Andersson, under the SIL Open Font License.',
            '',
            'Click anywhere to go back.'];

        let y = p.y + 22 + 14;
        for (const l of lines)
        {
            const heading = l && l === l.toUpperCase() && /[A-Z]/.test (l) && ! /[a-z]/.test (l);
            ctx.fillStyle = heading ? paint.sig[0] : paint.ink;
            label (ctx, l, p.x + 26, y, heading ? 8.5 : 9.5, heading ? 0.8 : 0);
            y += heading ? 19 : 15;
        }

        const g = this.githubLink(), hot = inside (g, this.mouse);
        ctx.fillStyle = hot ? paint.ink : paint.sig[1];
        label (ctx, 'github.com/user863/Quadrat', g.x, g.y + 12.5, 11, 0.2);
        ctx.fillRect (g.x + 1, g.y + 15, g.w - 3, 1);
    }

    // ── input ───────────────────────────────────────────────────────────────
    at (e)
    {
        const r = this.canvas.getBoundingClientRect(), s = r.width / W;
        return { x: (e.clientX - r.left) / s, y: (e.clientY - r.top) / s };
    }

    paletteItemAt (p)
    {
        if (! inside (PALETTE, p) || p.y < PALETTE.y + 38) return null;
        return this.itemRects().find (it => ! it.heading && inside (it, p)) || null;
    }

    blockAt (p)
    {
        const c = this.toCell (p);
        return this.state.blocks.find (b => { const k = this.kind (b); return c.x >= b.x && c.x < b.x + k.cells * this.sub && c.y >= b.y && c.y < b.y + k.rows * this.sub; });
    }

    down (e)
    {
        const p = this.at (e);
        this.mouse = p;
        this.ui.search.blur();

        if (this.showAbout)
        {
            if (inside (this.githubLink(), p)) window.open ('https://github.com/user863/Quadrat', '_blank', 'noopener');
            this.showAbout = false;
            return;
        }

        if (inside ({ x: M, y: 20, w: 110, h: 26 }, p)) { this.showAbout = true; return; }

        for (const c of this.caps())
            if (Math.hypot (p.x - c.cx, p.y - c.cy) <= CAP_R + 4) { this.capDown = c.name; this.pressCap (c.name); return; }

        const thumb = this.scrollThumb();
        if (thumb && inside ({ x: thumb.x - 4, y: thumb.y - 4, w: thumb.w + 8, h: thumb.h + 8 }, p))
        { this.drag = { kind: 'scroll', grab: p.y - thumb.y }; return; }

        const item = this.paletteItemAt (p);
        if (item) { this.drag = { kind: 'palette', kindIdx: item.kind }; return; }
        if (! inside (FIELD, p)) return;

        const grab = Math.max (6, this.cellSize() * 0.09);

        // ports first
        for (const b of this.state.blocks)
        {
            const k = this.kind (b), r = this.tileRect (b);
            for (let i = 0; i < k.out.length; ++i)
            {
                const pp = this.portPos (r, k, false, i);
                if (Math.hypot (p.x - pp.x, p.y - pp.y) <= grab) { this.drag = { kind: 'cable', src: b.id, port: i, from: pp }; return; }
            }
            for (let i = 0; i < k.in.length; ++i)
            {
                const pp = this.portPos (r, k, true, i);
                if (Math.hypot (p.x - pp.x, p.y - pp.y) <= grab)
                {
                    const c = this.cableTo (b, i, false);
                    if (c) { this.send ({ type: 'unplug', to: b.id, tp: i, toKnob: false }); this.dragCableFrom (c); }
                    return;
                }
            }
        }

        // tab, then drawers
        for (const b of this.state.blocks)
        {
            const k = this.kind (b), tab = this.tabRect (this.tileRect (b), k);
            if (tab && inside (reduce (tab, -grab * 0.35), p)) { b.drawer = ! b.drawer; this.send ({ type: 'drawer', id: b.id, open: b.drawer }); return; }

            const d = this.drawerRect (b);
            if (! d || ! inside (d, p)) continue;

            for (let i = 0; i < k.knobs.length; ++i)
            {
                const j = this.jack (b, i), dist = Math.hypot (p.x - j.x, p.y - j.y);
                if (dist > j.r * 1.1 && dist < j.r * 1.9) { this.drag = { kind: 'depth', id: b.id, i, from: b.depth[i] ?? 1, y: p.y }; return; }
                if (dist <= j.r * 1.1)
                {
                    const c = this.cableTo (b, i, true);
                    if (c) { this.send ({ type: 'unplug', to: b.id, tp: i, toKnob: true }); this.dragCableFrom (c); }
                    return;
                }
            }
            return;                     // drawer eats click
        }

        const b = this.blockAt (p);
        if (! b) { this.selected = -1; this.drag = { kind: 'pan', x: p.x, y: p.y, px: this.panX, py: this.panY }; return; }

        this.selected = b.id;
        const k = this.kind (b), r = this.tileRect (b);

        for (let i = 0; i < k.knobs.length; ++i)
        {
            const cell = this.controlRect (r, k, b.kind, i);
            if (! inside (cell, p)) continue;

            if (e.shiftKey && this.cableTo (b, i, true)) { this.send ({ type: 'unplug', to: b.id, tp: i, toKnob: true }); return; }

            const spec = k.knobs[i], v = b.knobs[i] ?? spec.def;
            if (spec.type === 2) { this.setKnob (b, i, v > 0.5 ? 0 : 1); return; }
            if (spec.type === 3)
            {
                const n = (spec.opts || '').split ('|').length, idx = Math.round (v * (n - 1));
                this.setKnob (b, i, n > 1 ? ((idx + 1) % n) / (n - 1) : 0);
                return;
            }
            if (spec.type === 1) this.setKnob (b, i, clamp ((p.x - cell.x) / Math.max (1, cell.w), 0, 1));
            this.drag = { kind: 'knob', id: b.id, i, slider: spec.type === 1, from: b.knobs[i] ?? spec.def, y: p.y, value: b.knobs[i] ?? spec.def };
            return;
        }

        if (b.kind === K.seq)
            for (let st = 0; st < 8; ++st)
                for (let lane = 0; lane < 3; ++lane)
                    if (inside (this.seqCell (r, k, st, lane), p))
                    {
                        if (lane === 2)
                        {
                            const m = ((b.modes[st] ?? 0) + 1) % 3;
                            b.modes[st] = m;
                            this.send ({ type: 'mode', id: b.id, step: st, mode: m });
                            return;
                        }
                        this.drag = { kind: 'step', id: b.id, lane };
                        this.setStep (b, st, lane, p.y);
                        return;
                    }

        // name: block presets
        const unit = r.h / Math.max (1, k.rows);
        if (k.bp && k.bp.length && inside ({ x: r.x, y: r.y, w: r.w / 2, h: unit * 0.28 }, p))
        {
            this.ui.menu (k.bp, e, (i) => this.send ({ type: 'bpreset', id: b.id, i }));
            return;
        }

        const strip = this.optionStrip (r, k, b.kind);
        if (strip && inside (strip, p))
        {
            const opts = k.options.split ('|');
            if (opts.length <= 6) { b.option = clamp (Math.floor ((p.x - strip.x) / (strip.w / opts.length)), 0, opts.length - 1); this.send ({ type: 'option', id: b.id, option: b.option }); }
            else this.ui.menu (opts, e, (i) => { b.option = i; this.send ({ type: 'option', id: b.id, option: i }); });
            return;
        }

        this.drag = { kind: 'move', id: b.id, ox: p.x - r.x, oy: p.y - r.y };
    }

    dragCableFrom (c)
    {
        const a = this.find (c.from);
        if (! a) return;
        this.drag = { kind: 'cable', src: c.from, port: c.fp, from: this.portPos (this.tileRect (a), this.kind (a), false, c.fp) };
    }

    pressCap (name)
    {
        if (name === 'clear') this.send ({ type: 'clear' });
        if (name === 'roll') { this.frameNext = true; this.send ({ type: 'roll', seed: (Math.random() * 1e9) | 0 }); }
        if (name === 'start') { this.stopped = false; this.send ({ type: 'start' }); }
        if (name === 'stop') { this.stopped = ! this.stopped; this.send ({ type: this.stopped ? 'stop' : 'start' }); }
    }

    setKnob (b, i, v) { b.knobs[i] = v; this.send ({ type: 'knob', id: b.id, k: i, v }); }

    setStep (b, st, lane, y)
    {
        const r = this.tileRect (b), k = this.kind (b), c = this.seqCell (r, k, st, lane), unit = r.h / Math.max (1, k.rows);
        const capH = Math.max (3, c.h * 0.17), travel = Math.max (1, c.h - capH - unit * 0.04);
        const v = clamp ((c.y + c.h - unit * 0.02 - capH / 2 - y) / travel, 0, 1);
        (lane ? b.steps2 : b.steps)[st] = v;
        this.send ({ type: 'step', id: b.id, step: st, lane, v });
    }

    move (e)
    {
        const p = this.at (e), prev = this.mouse;
        this.mouse = p;

        const hoverItem = this.paletteItemAt (p), hb = ! hoverItem && inside (FIELD, p) ? this.blockAt (p) : null;
        const hk = hoverItem ? hoverItem.kind : hb ? hb.kind : null;
        if (hk !== this.hoverKind) { this.hoverKind = hk; this.hoverSince = performance.now(); }

        const d = this.drag;
        if (! d) return;

        if (d.kind === 'scroll')
        {
            const t = this.scrollTrack(), th = this.scrollThumb();
            if (th) this.palScroll = clamp ((p.y - d.grab - t.y) / Math.max (1, t.h - th.h) * (this.contentHeight() - t.h), 0, this.contentHeight() - t.h);
        }
        else if (d.kind === 'pan') { this.panX = Math.max (-40, d.px - (p.x - d.x)); this.panY = Math.max (-40, d.py - (p.y - d.y)); }
        else if (d.kind === 'knob')
        {
            const b = this.find (d.id); if (! b) return;
            const r = this.tileRect (b), k = this.kind (b);
            if (d.slider) { const cell = this.controlRect (r, k, b.kind, d.i); d.value = clamp ((p.x - cell.x) / Math.max (1, cell.w), 0, 1); }
            else d.value = clamp (d.from - (p.y - d.y) / (170 * (e.shiftKey ? 4 : 1)), 0, 1);
            this.setKnob (b, d.i, d.value);
        }
        else if (d.kind === 'depth')
        {
            const b = this.find (d.id); if (! b) return;
            const v = clamp (d.from - (p.y - d.y) / (170 * (e.shiftKey ? 4 : 1)), 0, 1);
            b.depth[d.i] = v;
            this.send ({ type: 'depth', id: b.id, k: d.i, v });
        }
        else if (d.kind === 'step')
        {
            const b = this.find (d.id); if (! b) return;
            const r = this.tileRect (b), k = this.kind (b);
            for (let st = 0; st < 8; ++st) { const c = this.seqCell (r, k, st, d.lane); if (p.x >= c.x && p.x <= c.x + c.w) this.setStep (b, st, d.lane, p.y); }
        }
    }

    up (e)
    {
        const d = this.drag, p = this.mouse;
        this.drag = null;
        this.capDown = null;
        if (! d) return;

        if (d.kind === 'palette' && inside (FIELD, p))
        {
            const k = this.specs.kinds[d.kindIdx], c = this.cellSize();
            const cell = this.toCell ({ x: p.x - c * k.cells / 2, y: p.y - c * k.rows / 2 });
            this.send ({ type: 'add', kind: d.kindIdx, x: Math.max (0, cell.x), y: Math.max (0, cell.y) });
        }
        else if (d.kind === 'move')
        {
            if (inside (PALETTE, p)) { this.send ({ type: 'remove', id: d.id }); this.selected = -1; }
            else
            {
                const s = this.subSize(), cell = this.toCell ({ x: p.x - d.ox + s / 2, y: p.y - d.oy + s / 2 });
                this.send ({ type: 'move', id: d.id, x: Math.max (0, cell.x), y: Math.max (0, cell.y) });
            }
        }
        else if (d.kind === 'cable')
        {
            const grab = Math.max (9, this.cellSize() * 0.12);

            for (const b of this.state.blocks)
            {
                const k = this.kind (b), r = this.tileRect (b);
                for (let i = 0; i < k.in.length; ++i)
                {
                    const pp = this.portPos (r, k, true, i);
                    if (Math.hypot (p.x - pp.x, p.y - pp.y) <= grab) { this.send ({ type: 'connect', from: d.src, fp: d.port, to: b.id, tp: i, toKnob: false }); return; }
                }
            }

            for (const b of this.state.blocks)
                for (let i = 0; i < this.kind (b).knobs.length; ++i)
                {
                    const c = this.drawerCell (b, i);
                    if (c && inside (c, p)) { this.send ({ type: 'connect', from: d.src, fp: d.port, to: b.id, tp: i, toKnob: true }); return; }
                }

            for (const b of this.state.blocks)
            {
                const k = this.kind (b), r = this.tileRect (b);
                for (let i = 0; i < k.knobs.length; ++i)
                    if (inside (this.controlRect (r, k, b.kind, i), p)) { this.send ({ type: 'connect', from: d.src, fp: d.port, to: b.id, tp: i, toKnob: true }); return; }
            }
        }
    }

    wheel (e)
    {
        e.preventDefault();
        const p = this.at (e);

        if (inside (PALETTE, p)) { this.scrollBy (e.deltaY); return; }
        if (! inside (FIELD, p)) return;

        if (e.ctrlKey || e.metaKey)
        {
            const before = this.cellSize(), wx = (p.x - FIELD.x + this.panX) / before, wy = (p.y - FIELD.y + this.panY) / before;
            this.zoom = clamp (this.zoom * Math.pow (2, -e.deltaY * 0.004), 0.45, 2.2);
            const after = this.cellSize();
            this.panX = Math.max (-40, wx * after - (p.x - FIELD.x));
            this.panY = Math.max (-40, wy * after - (p.y - FIELD.y));
            return;
        }

        this.panX = Math.max (-40, this.panX + e.deltaX);
        this.panY = Math.max (-40, this.panY + e.deltaY);
    }

    deleteSelected()
    {
        if (this.selected >= 0) { this.send ({ type: 'remove', id: this.selected }); this.selected = -1; }
    }
}

// colour helpers
function hexToRgb (h) { const n = parseInt (h.slice (1), 16); return [n >> 16 & 255, n >> 8 & 255, n & 255]; }
function withAlpha (h, a) { const [r, g, b] = hexToRgb (h); return `rgba(${r},${g},${b},${a})`; }
function shade (h, amt)
{
    const [r, g, b] = hexToRgb (h), f = (v) => clamp (Math.round (amt >= 0 ? v + (255 - v) * amt : v * (1 + amt)), 0, 255);
    return '#' + [f (r), f (g), f (b)].map (v => v.toString (16).padStart (2, '0')).join ('');
}
