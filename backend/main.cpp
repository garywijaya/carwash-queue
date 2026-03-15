/*
 * ============================================================
 *  SISTEM ANTREAN CAR WASH — C++ Backend (Crow HTTP Server)
 *  Struktur: Queue + Stack berbasis Linked List
 *  Port: 8080
 * ============================================================
 *
 *  Dependensi:
 *    - Crow  : https://github.com/CrowCpp/Crow
 *    - Asio  : header-only (bundled dengan Crow)
 *
 *  Compile:
 *    g++ -std=c++17 -O2 -I./crow/include main.cpp -o server -lpthread
 *
 *  Run:
 *    ./server
 * ============================================================
 */

#define CROW_MAIN
#include "crow_all.h"
#include <string>
#include <mutex>
#include <sstream>

// ============================================================
//  STRUKTUR DATA
// ============================================================

struct Kendaraan {
    std::string id;             // UUID sederhana (timestamp + counter)
    std::string namaPelanggan;
    std::string jenisKendaraan; // Mobil / Motor
    std::string paketCuci;      // Basic / Standard / Premium
    std::string nomorPlat;
    std::string waktuMasuk;     // timestamp string
};

// ---------- NODE ----------

struct NodeQueue {
    Kendaraan data;
    NodeQueue* next;
};

struct NodeStack {
    Kendaraan data;
    NodeStack* next;
};

// ============================================================
//  KELAS QUEUE (Linked List) — FIFO
// ============================================================

class Queue {
    NodeQueue* front;
    NodeQueue* rear;
    int ukuran;
public:
    Queue() : front(nullptr), rear(nullptr), ukuran(0) {}

    bool isEmpty() const { return front == nullptr; }
    int size()     const { return ukuran; }

    // O(1)
    void enqueue(const Kendaraan& k) {
        NodeQueue* baru = new NodeQueue{k, nullptr};
        if (isEmpty()) { front = rear = baru; }
        else           { rear->next = baru; rear = baru; }
        ukuran++;
    }

    // O(1)
    Kendaraan dequeue() {
        NodeQueue* tmp = front;
        Kendaraan k = tmp->data;
        front = front->next;
        if (!front) rear = nullptr;
        delete tmp;
        ukuran--;
        return k;
    }

    // O(n) — serialise ke JSON array
    crow::json::wvalue toJSON() const {
        crow::json::wvalue arr(crow::json::type::List);
        NodeQueue* cur = front;
        int i = 0;
        while (cur) {
            crow::json::wvalue item;
            item["id"]             = cur->data.id;
            item["namaPelanggan"]  = cur->data.namaPelanggan;
            item["jenisKendaraan"] = cur->data.jenisKendaraan;
            item["paketCuci"]      = cur->data.paketCuci;
            item["nomorPlat"]      = cur->data.nomorPlat;
            item["waktuMasuk"]     = cur->data.waktuMasuk;
            arr[i++] = std::move(item);
            cur = cur->next;
        }
        return arr;
    }

    ~Queue() { while (!isEmpty()) dequeue(); }
};

// ============================================================
//  KELAS STACK (Linked List) — LIFO
// ============================================================

class Stack {
    NodeStack* top;
    int ukuran;
public:
    Stack() : top(nullptr), ukuran(0) {}

    bool isEmpty() const { return top == nullptr; }
    int size()     const { return ukuran; }

    // O(1)
    void push(const Kendaraan& k) {
        NodeStack* baru = new NodeStack{k, top};
        top = baru;
        ukuran++;
    }

    // O(1)
    Kendaraan pop() {
        NodeStack* tmp = top;
        Kendaraan k = tmp->data;
        top = top->next;
        delete tmp;
        ukuran--;
        return k;
    }

    // O(1)
    const Kendaraan& peek() const { return top->data; }

    // O(n) — serialise ke JSON array
    crow::json::wvalue toJSON() const {
        crow::json::wvalue arr(crow::json::type::List);
        NodeStack* cur = top;
        int i = 0;
        while (cur) {
            crow::json::wvalue item;
            item["id"]             = cur->data.id;
            item["namaPelanggan"]  = cur->data.namaPelanggan;
            item["jenisKendaraan"] = cur->data.jenisKendaraan;
            item["paketCuci"]      = cur->data.paketCuci;
            item["nomorPlat"]      = cur->data.nomorPlat;
            item["waktuMasuk"]     = cur->data.waktuMasuk;
            arr[i++] = std::move(item);
            cur = cur->next;
        }
        return arr;
    }

    ~Stack() { while (!isEmpty()) pop(); }
};

// ============================================================
//  GLOBAL STATE + MUTEX
// ============================================================

Queue   antrean;
Stack   riwayat;
std::mutex mtx;
int     idCounter = 0;

std::string buatID() {
    return "KDR-" + std::to_string(++idCounter);
}

std::string timestamp() {
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

// ============================================================
//  HELPER — JSON response builder
// ============================================================

crow::response ok(crow::json::wvalue data) {
    crow::json::wvalue res;
    res["success"] = true;
    res["data"]    = std::move(data);
    auto r = crow::response(200, res.dump());
    r.add_header("Content-Type",                "application/json");
    r.add_header("Access-Control-Allow-Origin", "*");
    r.add_header("Access-Control-Allow-Methods","GET,POST,OPTIONS");
    r.add_header("Access-Control-Allow-Headers","Content-Type");
    return r;
}

crow::response err(int code, const std::string& msg) {
    crow::json::wvalue res;
    res["success"] = false;
    res["message"] = msg;
    auto r = crow::response(code, res.dump());
    r.add_header("Content-Type",                "application/json");
    r.add_header("Access-Control-Allow-Origin", "*");
    return r;
}

crow::response corsOK() {
    crow::response r(204);
    r.add_header("Access-Control-Allow-Origin",  "*");
    r.add_header("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    r.add_header("Access-Control-Allow-Headers", "Content-Type");
    return r;
}

// ============================================================
//  MAIN — Route Definitions
// ============================================================

int main() {
    crow::SimpleApp app;

    // ----------------------------------------------------------
    // OPTIONS preflight (CORS) — semua route
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/<path>").methods(crow::HTTPMethod::Options)
    ([](const crow::request&, const std::string&) {
        return corsOK();
    });

    // ----------------------------------------------------------
    // GET /api/status — ringkasan jumlah antrean & riwayat
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/status").methods(crow::HTTPMethod::Get)
    ([]() {
        std::lock_guard<std::mutex> lock(mtx);
        crow::json::wvalue d;
        d["antrean"] = antrean.size();
        d["riwayat"] = riwayat.size();
        return ok(std::move(d));
    });

    // ----------------------------------------------------------
    // POST /api/antrean — tambah kendaraan ke antrean
    // Body JSON: { namaPelanggan, jenisKendaraan, paketCuci, nomorPlat }
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/antrean").methods(crow::HTTPMethod::Post)
    ([](const crow::request& req) {
        auto body = crow::json::load(req.body);
        if (!body)
            return err(400, "Body JSON tidak valid");

        std::string nama  = body["namaPelanggan"].s();
        std::string jenis = body["jenisKendaraan"].s();
        std::string paket = body["paketCuci"].s();
        std::string plat  = body["nomorPlat"].s();

        if (nama.empty() || jenis.empty() || paket.empty() || plat.empty())
            return err(400, "Semua field wajib diisi");

        std::lock_guard<std::mutex> lock(mtx);
        Kendaraan k{ buatID(), nama, jenis, paket, plat, timestamp() };
        antrean.enqueue(k);

        crow::json::wvalue d;
        d["id"]            = k.id;
        d["namaPelanggan"] = k.namaPelanggan;
        d["nomorPlat"]     = k.nomorPlat;
        d["posisi"]        = antrean.size();
        return ok(std::move(d));
    });

    // ----------------------------------------------------------
    // GET /api/antrean — tampilkan semua antrean
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/antrean").methods(crow::HTTPMethod::Get)
    ([]() {
        std::lock_guard<std::mutex> lock(mtx);
        return ok(antrean.toJSON());
    });

    // ----------------------------------------------------------
    // POST /api/proses — proses kendaraan terdepan
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/proses").methods(crow::HTTPMethod::Post)
    ([]() {
        std::lock_guard<std::mutex> lock(mtx);
        if (antrean.isEmpty())
            return err(404, "Antrean kosong, tidak ada kendaraan untuk diproses");

        Kendaraan k = antrean.dequeue();
        riwayat.push(k);

        crow::json::wvalue d;
        d["id"]             = k.id;
        d["namaPelanggan"]  = k.namaPelanggan;
        d["jenisKendaraan"] = k.jenisKendaraan;
        d["paketCuci"]      = k.paketCuci;
        d["nomorPlat"]      = k.nomorPlat;
        d["waktuMasuk"]     = k.waktuMasuk;
        d["sisaAntrean"]    = antrean.size();
        return ok(std::move(d));
    });

    // ----------------------------------------------------------
    // GET /api/riwayat — tampilkan semua riwayat (terbaru di atas)
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/riwayat").methods(crow::HTTPMethod::Get)
    ([]() {
        std::lock_guard<std::mutex> lock(mtx);
        return ok(riwayat.toJSON());
    });

    // ----------------------------------------------------------
    // GET /api/riwayat/terakhir — kendaraan terakhir diproses
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/riwayat/terakhir").methods(crow::HTTPMethod::Get)
    ([]() {
        std::lock_guard<std::mutex> lock(mtx);
        if (riwayat.isEmpty())
            return err(404, "Belum ada kendaraan yang diproses");

        const Kendaraan& k = riwayat.peek();
        crow::json::wvalue d;
        d["id"]             = k.id;
        d["namaPelanggan"]  = k.namaPelanggan;
        d["jenisKendaraan"] = k.jenisKendaraan;
        d["paketCuci"]      = k.paketCuci;
        d["nomorPlat"]      = k.nomorPlat;
        d["waktuMasuk"]     = k.waktuMasuk;
        return ok(std::move(d));
    });

    // ----------------------------------------------------------
    //  Start server
    // ----------------------------------------------------------
    CROW_LOG_INFO << "=== Car Wash Server berjalan di http://localhost:8080 ===";
    app.port(8080).multithreaded().run();
    return 0;
}