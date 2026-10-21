/*  The audio thread. It owns the patch — the same C++ engine the plugin
    runs, compiled to wasm — and posts a snapshot of it to the page a few
    times a second, which is exactly what the plugin's panel does.
*/
class QuadratProcessor extends AudioWorkletProcessor
{
    constructor()
    {
        super();
        this.ready = false;
        this.playing = true;
        this.stopped = false;
        this.ppq = 0;
        this.bpm = 120;
        this.sinceSnapshot = 0;

        this.port.onmessage = (e) => this.handle (e.data);
    }

    async handle (msg)
    {
        if (msg.type === 'init')
        {
            const wasi = { fd_close: () => 0, fd_seek: () => 0, fd_write: () => 0 };
            const { instance } = await WebAssembly.instantiate (msg.wasm, { wasi_snapshot_preview1: wasi });

            this.e = instance.exports;
            this.e._initialize();
            this.e.qd_init (sampleRate, 128);
            this.L = this.e.qd_buffer_l();
            this.R = this.e.qd_buffer_r();
            this.ready = true;

            this.port.postMessage ({ type: 'ready', specs: this.str (this.e.qd_specs()), rate: sampleRate });
            return;
        }

        if (! this.ready) return;

        switch (msg.type)
        {
            case 'preset':  this.e.qd_preset (msg.i); break;
            case 'knob':    this.e.qd_set_knob (msg.id, msg.k, msg.v); break;
            case 'depth':   this.e.qd_set_depth (msg.id, msg.k, msg.v); break;
            case 'option':  this.e.qd_set_option (msg.id, msg.option); break;
            case 'drawer':  this.e.qd_set_drawer (msg.id, msg.open ? 1 : 0); break;
            case 'step':    this.e.qd_set_step (msg.id, msg.step, msg.lane, msg.v); break;
            case 'mode':    this.e.qd_set_mode (msg.id, msg.step, msg.mode); break;
            case 'bpreset': this.e.qd_block_preset (msg.id, msg.i); break;
            case 'add':     this.e.qd_add (msg.kind, msg.x, msg.y); break;
            case 'remove':  this.e.qd_remove (msg.id); break;
            case 'move':    this.e.qd_move (msg.id, msg.x, msg.y); break;
            case 'connect': this.e.qd_connect (msg.from, msg.fp, msg.to, msg.tp, msg.toKnob ? 1 : 0); break;
            case 'unplug':  this.e.qd_disconnect (msg.to, msg.tp, msg.toKnob ? 1 : 0); break;
            case 'clear':   this.e.qd_clear(); break;
            case 'roll':
                this.port.postMessage ({ type: 'rolled', name: this.str (this.e.qd_roll (msg.seed >>> 0)) });
                break;
            case 'note':    this.e.qd_note (msg.midi, msg.vel ?? 1, msg.on ? 1 : 0); break;
            case 'panic':   this.e.qd_panic(); break;
            // stop: reset + silence; start: carry on
            case 'stop':    this.stopped = true; this.e.qd_panic(); break;
            case 'start':   this.stopped = false; break;
            case 'playing': this.playing = msg.on; break;
            case 'bpm':     this.bpm = msg.bpm; break;
        }

        this.snapshot();            // edits show at once
    }

    /*  An audio worklet has no TextDecoder, so the C string is decoded by
        hand. Only two- and three-byte sequences turn up here (the block
        descriptions use an em dash), which is all this handles.            */
    str (ptr)
    {
        const m = new Uint8Array (this.e.memory.buffer);
        let out = '';

        for (let i = ptr; m[i]; )
        {
            const c = m[i];

            if (c < 0x80) { out += String.fromCharCode (c); i += 1; }
            else if (c < 0xe0) { out += String.fromCharCode (((c & 0x1f) << 6) | (m[i + 1] & 0x3f)); i += 2; }
            else { out += String.fromCharCode (((c & 0x0f) << 12) | ((m[i + 1] & 0x3f) << 6) | (m[i + 2] & 0x3f)); i += 3; }
        }

        return out;
    }

    process (inputs, outputs)
    {
        const out = outputs[0];
        if (! this.ready) return true;

        const n = out[0].length;
        if (this.stopped) { this.tick (n); return true; }

        this.e.qd_transport (this.bpm, this.ppq, this.playing ? 1 : 0);
        if (this.playing) this.ppq += (n / sampleRate) * (this.bpm / 60);

        this.e.qd_process (n);

        const l = new Float32Array (this.e.memory.buffer, this.L, n);
        const r = new Float32Array (this.e.memory.buffer, this.R, n);
        out[0].set (l);
        if (out.length > 1) out[1].set (r);

        this.tick (n);
        return true;
    }

    // 20 snapshots a second
    tick (n)
    {
        this.sinceSnapshot += n;
        if (this.sinceSnapshot >= sampleRate / 20) this.snapshot();
    }

    snapshot()
    {
        this.sinceSnapshot = 0;
        this.port.postMessage ({ type: 'state', state: this.str (this.e.qd_state()) });
    }
}

registerProcessor ('quadrat', QuadratProcessor);
