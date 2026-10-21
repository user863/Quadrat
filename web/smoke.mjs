/*  A headless check that the wasm engine sounds — the same question the
    plugin's tests ask, asked of the web build.                            */
import { readFileSync } from 'fs';

const wasi = { fd_close: () => 0, fd_seek: () => 0, fd_write: () => 0 };
const { instance } = await WebAssembly.instantiate (readFileSync ('quadrat.wasm'),
                                                    { wasi_snapshot_preview1: wasi });
const e = instance.exports;
e._initialize();

const N = 128, SR = 48000;
e.qd_init (SR, N);

const str = (ptr) => {
    const m = new Uint8Array (e.memory.buffer);
    let end = ptr; while (m[end]) ++end;
    return new TextDecoder().decode (m.subarray (ptr, end));
};

const specs = JSON.parse (str (e.qd_specs()));
console.log (specs.kinds.length, 'kinds,', specs.presets.length, 'presets');

const L = e.qd_buffer_l(), R = e.qd_buffer_r();
let failed = 0;

for (let p = 0; p < specs.presets.length; ++p)
{
    e.qd_preset (p);
    let peak = 0, ppq = 0;

    for (let k = 0; k < 1000; ++k)                // ~2.7 seconds
    {
        e.qd_transport (120, ppq, 1);
        ppq += (N / SR) * (120 / 60);
        e.qd_process (N);
        const l = new Float32Array (e.memory.buffer, L, N);
        for (let i = 0; i < N; ++i) peak = Math.max (peak, Math.abs (l[i]));
    }

    if (! (peak > 0.005))                        // then try playing it, as the plugin's test does
    {
        e.qd_note (60, 1.0, 1);
        for (let k = 0; k < 200; ++k)
        {
            e.qd_transport (120, ppq, 1);
            ppq += (N / SR) * (120 / 60);
            e.qd_process (N);
            const l = new Float32Array (e.memory.buffer, L, N);
            for (let i = 0; i < N; ++i) peak = Math.max (peak, Math.abs (l[i]));
        }
        e.qd_note (60, 0.0, 0);
    }

    if (! (peak > 0.005) || ! isFinite (peak)) { console.log ('  SILENT', specs.presets[p], peak); ++failed; }
    if (peak > 1.001) { console.log ('  LOUD', specs.presets[p], peak.toFixed (3)); ++failed; }
}

const state = JSON.parse (str (e.qd_state()));
console.log ('last preset:', state.blocks.length, 'blocks,', state.cables.length, 'cables');
console.log (failed === 0 ? 'every preset sounds in the browser engine' : failed + ' failed');
process.exit (failed === 0 ? 0 : 1);
