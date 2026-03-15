import { useState, useEffect, useCallback } from "react";

const API = "http://localhost:8080/api";

// ── helpers ──────────────────────────────────────────────────────
const apiFetch = async (path, options = {}) => {
  const res = await fetch(`${API}${path}`, {
    headers: { "Content-Type": "application/json" },
    ...options,
  });
  const json = await res.json();
  if (!json.success) throw new Error(json.message || "Server error");
  return json.data;
};

const PAKET_COLOR = {
  Basic: "bg-slate-100 text-slate-700",
  Standard: "bg-amber-100 text-amber-800",
  Premium: "bg-violet-100 text-violet-800",
};

const JENIS_ICON = { Mobil: "🚗", Motor: "🏍️" };

// ── sub-components ────────────────────────────────────────────────
function Badge({ label }) {
  return (
    <span className={`text-xs font-semibold px-2 py-0.5 rounded-full ${PAKET_COLOR[label] ?? "bg-gray-100 text-gray-600"}`}>
      {label}
    </span>
  );
}

function VehicleCard({ item, index, dim }) {
  return (
    <div className={`flex items-center gap-3 px-4 py-3 rounded-xl border transition-all
        ${dim ? "opacity-40" : "opacity-100"}
        bg-white border-gray-100 hover:shadow-sm`}>
      <span className="text-2xl">{JENIS_ICON[item.jenisKendaraan] ?? "🚙"}</span>
      <div className="flex-1 min-w-0">
        <p className="font-semibold text-sm text-gray-800 truncate">{item.namaPelanggan}</p>
        <p className="text-xs text-gray-400">{item.nomorPlat} · {item.waktuMasuk}</p>
      </div>
      <div className="flex flex-col items-end gap-1">
        <Badge label={item.paketCuci} />
        {index === 0 && (
          <span className="text-xs text-emerald-600 font-medium">▶ Berikutnya</span>
        )}
      </div>
    </div>
  );
}

function StatCard({ label, value, accent }) {
  return (
    <div className={`rounded-2xl p-4 flex flex-col gap-1 ${accent}`}>
      <span className="text-xs font-medium opacity-70 uppercase tracking-wide">{label}</span>
      <span className="text-3xl font-bold">{value}</span>
    </div>
  );
}

function Alert({ msg, type, onClose }) {
  if (!msg) return null;
  const base = type === "error"
    ? "bg-red-50 border-red-200 text-red-700"
    : "bg-emerald-50 border-emerald-200 text-emerald-700";
  return (
    <div className={`flex items-center justify-between gap-3 px-4 py-3 rounded-xl border text-sm ${base}`}>
      <span>{msg}</span>
      <button onClick={onClose} className="opacity-50 hover:opacity-100 text-lg leading-none">×</button>
    </div>
  );
}

// ── main app ──────────────────────────────────────────────────────
export default function App() {
  const [tab, setTab] = useState("antrean");
  const [antrean, setAntrean] = useState([]);
  const [riwayat, setRiwayat] = useState([]);
  const [terakhir, setTerakhir] = useState(null);
  const [status, setStatus] = useState({ antrean: 0, riwayat: 0 });
  const [loading, setLoading] = useState(false);
  const [processing, setProcessing] = useState(false);
  const [alert, setAlert] = useState({ msg: "", type: "success" });

  // form state
  const [form, setForm] = useState({
    namaPelanggan: "",
    nomorPlat: "",
    jenisKendaraan: "Mobil",
    paketCuci: "Basic",
  });

  const notify = (msg, type = "success") => {
    setAlert({ msg, type });
    setTimeout(() => setAlert({ msg: "", type: "success" }), 3500);
  };

  const refresh = useCallback(async () => {
    try {
      const [st, q, r] = await Promise.all([
        apiFetch("/status"),
        apiFetch("/antrean"),
        apiFetch("/riwayat"),
      ]);
      setStatus(st);
      setAntrean(Array.isArray(q) ? q : []);
      setRiwayat(Array.isArray(r) ? r : []);
      if (r.length > 0) setTerakhir(r[0]);
    } catch (e) {
      // silently ignore poll errors
    }
  }, []);

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 3000);
    return () => clearInterval(id);
  }, [refresh]);

  const handleTambah = async () => {
    if (!form.namaPelanggan || !form.nomorPlat) {
      notify("Nama dan nomor plat wajib diisi.", "error");
      return;
    }
    setLoading(true);
    try {
      const d = await apiFetch("/antrean", { method: "POST", body: JSON.stringify(form) });
      notify(`${d.namaPelanggan} (${d.nomorPlat}) masuk antrean ke-${d.posisi}`);
      setForm({ namaPelanggan: "", nomorPlat: "", jenisKendaraan: "Mobil", paketCuci: "Basic" });
      refresh();
    } catch (e) {
      notify(e.message, "error");
    } finally {
      setLoading(false);
    }
  };

  const handleProses = async () => {
    if (antrean.length === 0) { notify("Antrean kosong!", "error"); return; }
    setProcessing(true);
    try {
      const d = await apiFetch("/proses", { method: "POST" });
      setTerakhir(d);
      notify(`✓ ${d.namaPelanggan} (${d.nomorPlat}) selesai dicuci!`);
      refresh();
      setTab("riwayat");
    } catch (e) {
      notify(e.message, "error");
    } finally {
      setProcessing(false);
    }
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 via-white to-indigo-50 font-sans">
      {/* ── header ── */}
      <header className="sticky top-0 z-20 bg-white/80 backdrop-blur border-b border-gray-100">
        <div className="max-w-2xl mx-auto px-4 py-3 flex items-center justify-between">
          <div className="flex items-center gap-2">
            <span className="text-2xl">🚿</span>
            <div>
              <h1 className="text-base font-bold text-gray-900 leading-none">Car Wash Queue</h1>
              <p className="text-xs text-gray-400">Sistem Antrean Pencucian</p>
            </div>
          </div>
          <div className="flex gap-2">
            <span className="text-xs bg-indigo-50 text-indigo-700 font-semibold px-2 py-1 rounded-lg">
              Antri: {status.antrean}
            </span>
            <span className="text-xs bg-emerald-50 text-emerald-700 font-semibold px-2 py-1 rounded-lg">
              Selesai: {status.riwayat}
            </span>
          </div>
        </div>
      </header>

      <div className="max-w-2xl mx-auto px-4 py-6 space-y-5">

        {/* ── alert ── */}
        <Alert msg={alert.msg} type={alert.type} onClose={() => setAlert({ msg: "" })} />

        {/* ── stat cards ── */}
        <div className="grid grid-cols-3 gap-3">
          <StatCard label="Antrean" value={status.antrean} accent="bg-indigo-500 text-white" />
          <StatCard label="Riwayat" value={status.riwayat} accent="bg-emerald-500 text-white" />
          <StatCard
            label="Terakhir"
            value={terakhir ? JENIS_ICON[terakhir.jenisKendaraan] ?? "—" : "—"}
            accent="bg-amber-400 text-white"
          />
        </div>

        {/* ── form tambah ── */}
        <div className="bg-white rounded-2xl border border-gray-100 shadow-sm p-5 space-y-4">
          <h2 className="font-semibold text-gray-800 text-sm">Tambah Kendaraan</h2>
          <div className="grid grid-cols-2 gap-3">
            <input
              className="col-span-2 border border-gray-200 rounded-xl px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-300"
              placeholder="Nama pelanggan"
              value={form.namaPelanggan}
              onChange={e => setForm(f => ({ ...f, namaPelanggan: e.target.value }))}
            />
            <input
              className="border border-gray-200 rounded-xl px-3 py-2 text-sm outline-none focus:ring-2 focus:ring-indigo-300"
              placeholder="No. plat (B 1234 XY)"
              value={form.nomorPlat}
              onChange={e => setForm(f => ({ ...f, nomorPlat: e.target.value.toUpperCase() }))}
            />
            <select
              className="border border-gray-200 rounded-xl px-3 py-2 text-sm bg-white outline-none focus:ring-2 focus:ring-indigo-300"
              value={form.jenisKendaraan}
              onChange={e => setForm(f => ({ ...f, jenisKendaraan: e.target.value }))}
            >
              <option>Mobil</option>
              <option>Motor</option>
            </select>
          </div>
          <div className="grid grid-cols-3 gap-2">
            {["Basic", "Standard", "Premium"].map(p => (
              <button
                key={p}
                onClick={() => setForm(f => ({ ...f, paketCuci: p }))}
                className={`py-2 rounded-xl text-sm font-medium border transition-all
                  ${form.paketCuci === p
                    ? "bg-indigo-600 text-white border-indigo-600 shadow-sm"
                    : "bg-white text-gray-500 border-gray-200 hover:border-indigo-300"}`}
              >
                {p}
              </button>
            ))}
          </div>
          <button
            onClick={handleTambah}
            disabled={loading}
            className="w-full bg-indigo-600 hover:bg-indigo-700 disabled:opacity-50 text-white
              font-semibold py-2.5 rounded-xl text-sm transition-all active:scale-95"
          >
            {loading ? "Menambahkan…" : "➕ Tambah ke Antrean"}
          </button>
        </div>

        {/* ── proses button ── */}
        <button
          onClick={handleProses}
          disabled={processing || antrean.length === 0}
          className="w-full bg-emerald-500 hover:bg-emerald-600 disabled:opacity-40 disabled:cursor-not-allowed
            text-white font-bold py-3.5 rounded-2xl text-sm transition-all active:scale-95 shadow-sm"
        >
          {processing
            ? "Memproses…"
            : antrean.length === 0
              ? "Antrean Kosong"
              : `▶ Proses Kendaraan Berikutnya  —  ${antrean[0]?.namaPelanggan ?? ""}`}
        </button>

        {/* ── tabs ── */}
        <div className="flex gap-1 bg-gray-100 p-1 rounded-xl">
          {[["antrean", `🚗 Antrean (${antrean.length})`], ["riwayat", `✅ Riwayat (${riwayat.length})`]].map(
            ([key, label]) => (
              <button
                key={key}
                onClick={() => setTab(key)}
                className={`flex-1 py-2 text-sm font-semibold rounded-lg transition-all
                  ${tab === key ? "bg-white shadow-sm text-gray-900" : "text-gray-400 hover:text-gray-700"}`}
              >
                {label}
              </button>
            )
          )}
        </div>

        {/* ── list ── */}
        <div className="space-y-2">
          {tab === "antrean" && (
            antrean.length === 0
              ? <p className="text-center text-gray-400 py-8 text-sm">Antrean kosong</p>
              : antrean.map((item, i) => (
                <VehicleCard key={item.id} item={item} index={i} dim={false} />
              ))
          )}
          {tab === "riwayat" && (
            riwayat.length === 0
              ? <p className="text-center text-gray-400 py-8 text-sm">Belum ada riwayat</p>
              : riwayat.map((item, i) => (
                <VehicleCard key={item.id} item={item} index={i} dim={i > 0} />
              ))
          )}
        </div>

        {/* ── terakhir diproses ── */}
        {terakhir && (
          <div className="bg-amber-50 border border-amber-100 rounded-2xl p-4">
            <p className="text-xs text-amber-600 font-semibold uppercase tracking-wide mb-2">
              Terakhir Diproses
            </p>
            <div className="flex items-center gap-3">
              <span className="text-3xl">{JENIS_ICON[terakhir.jenisKendaraan]}</span>
              <div>
                <p className="font-bold text-gray-800">{terakhir.namaPelanggan}</p>
                <p className="text-xs text-gray-500">{terakhir.nomorPlat} · {terakhir.waktuMasuk}</p>
              </div>
              <div className="ml-auto"><Badge label={terakhir.paketCuci} /></div>
            </div>
          </div>
        )}

        <p className="text-center text-xs text-gray-300 pb-4">
          Auto-refresh tiap 3 detik · Backend: Crow C++ @ :8080
        </p>
      </div>
    </div>
  );
}