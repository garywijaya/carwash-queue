#define CROW_MAIN
#include "crow_all.h"
#include <string>
#include <mutex>
#include <sstream>

// ============================================================
//  STRUKTUR DATA
// ============================================================

struct Kendaraan
{
    std::string id;
    std::string namaPelanggan;
    std::string jenisKendaraan;
    std::string paketCuci;
    std::string nomorPlat;
    std::string waktuMasuk;
};

struct NodeQueue
{
    Kendaraan data;
    NodeQueue *next;
};

struct NodeStack
{
    Kendaraan data;
    NodeStack *next;
};

// ============================================================
//  KELAS QUEUE (Linked List) — FIFO
// ============================================================

class Queue
{
    NodeQueue *front;
    NodeQueue *rear;
    int ukuran;

public:
    Queue() : front(nullptr), rear(nullptr), ukuran(0) {}

    bool isEmpty() const { return front == nullptr; }
    int size() const { return ukuran; }

    void enqueue(const Kendaraan &k)
    {
        NodeQueue *baru = new NodeQueue{k, nullptr};
        if (isEmpty())
            front = rear = baru;
        else
        {
            rear->next = baru;
            rear = baru;
        }
        ukuran++;
    }

    Kendaraan dequeue()
    {
        NodeQueue *tmp = front;
        Kendaraan k = tmp->data;
        front = front->next;
        if (!front)
            rear = nullptr;
        delete tmp;
        ukuran--;
        return k;
    }

    crow::json::wvalue toJSON() const
    {
        crow::json::wvalue arr(crow::json::type::List);
        NodeQueue *cur = front;
        int i = 0;
        while (cur)
        {
            crow::json::wvalue item;
            item["id"] = cur->data.id;
            item["namaPelanggan"] = cur->data.namaPelanggan;
            item["jenisKendaraan"] = cur->data.jenisKendaraan;
            item["paketCuci"] = cur->data.paketCuci;
            item["nomorPlat"] = cur->data.nomorPlat;
            item["waktuMasuk"] = cur->data.waktuMasuk;
            arr[i++] = std::move(item);
            cur = cur->next;
        }
        return arr;
    }

    ~Queue()
    {
        while (!isEmpty())
            dequeue();
    }
};

// ============================================================
//  KELAS STACK (Linked List) — LIFO
// ============================================================

class Stack
{
    NodeStack *top;
    int ukuran;

public:
    Stack() : top(nullptr), ukuran(0) {}

    bool isEmpty() const { return top == nullptr; }
    int size() const { return ukuran; }

    void push(const Kendaraan &k)
    {
        NodeStack *baru = new NodeStack{k, top};
        top = baru;
        ukuran++;
    }

    Kendaraan pop()
    {
        NodeStack *tmp = top;
        Kendaraan k = tmp->data;
        top = top->next;
        delete tmp;
        ukuran--;
        return k;
    }

    const Kendaraan &peek() const { return top->data; }

    crow::json::wvalue toJSON() const
    {
        crow::json::wvalue arr(crow::json::type::List);
        NodeStack *cur = top;
        int i = 0;
        while (cur)
        {
            crow::json::wvalue item;
            item["id"] = cur->data.id;
            item["namaPelanggan"] = cur->data.namaPelanggan;
            item["jenisKendaraan"] = cur->data.jenisKendaraan;
            item["paketCuci"] = cur->data.paketCuci;
            item["nomorPlat"] = cur->data.nomorPlat;
            item["waktuMasuk"] = cur->data.waktuMasuk;
            arr[i++] = std::move(item);
            cur = cur->next;
        }
        return arr;
    }

    ~Stack()
    {
        while (!isEmpty())
            pop();
    }
};

// ============================================================
//  GLOBAL STATE + MUTEX
// ============================================================

Queue antrean;
Stack riwayat;
std::mutex mtx;
int idCounter = 0;

std::string buatID()
{
    return "KDR-" + std::to_string(++idCounter);
}

std::string timestamp()
{
    time_t now = time(nullptr);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

// ============================================================
//  HELPER — JSON response builder
// ============================================================

crow::response ok(crow::json::wvalue data)
{
    crow::json::wvalue res;
    res["success"] = true;
    res["data"] = std::move(data);

    crow::response r(200);
    r.set_header("Content-Type", "application/json");
    r.write(res.dump());
    return r;
}

crow::response err(int code, const std::string &msg)
{
    crow::json::wvalue res;
    res["success"] = false;
    res["message"] = msg;

    crow::response r(code);
    r.set_header("Content-Type", "application/json");
    r.write(res.dump());
    return r;
}

// ============================================================
//  MAIN
// ============================================================

int main()
{
    // ⬇️ GANTI SimpleApp dengan App<CORSHandler>
    crow::App<crow::CORSHandler> app;

    // ⬇️ Konfigurasi CORS global — izinkan semua origin
    auto &cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods(
            crow::HTTPMethod::Get,
            crow::HTTPMethod::Post,
            crow::HTTPMethod::Options)
        .headers("Content-Type", "Authorization")
        .max_age(86400);

    // ----------------------------------------------------------
    // GET /api/status
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/status")
        .methods(crow::HTTPMethod::Get)([]()
                                        {
            std::lock_guard<std::mutex> lock(mtx);
            crow::json::wvalue d;
            d["antrean"] = antrean.size();
            d["riwayat"] = riwayat.size();
            return ok(std::move(d)); });

    // ----------------------------------------------------------
    // POST /api/antrean — tambah kendaraan
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/antrean")
        .methods(crow::HTTPMethod::Post)([](const crow::request &req)
                                         {
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
            Kendaraan k{buatID(), nama, jenis, paket, plat, timestamp()};
            antrean.enqueue(k);

            crow::json::wvalue d;
            d["id"]            = k.id;
            d["namaPelanggan"] = k.namaPelanggan;
            d["nomorPlat"]     = k.nomorPlat;
            d["posisi"]        = antrean.size();
            return ok(std::move(d)); });

    // ----------------------------------------------------------
    // GET /api/antrean — tampilkan semua antrean
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/antrean")
        .methods(crow::HTTPMethod::Get)([]()
                                        {
            std::lock_guard<std::mutex> lock(mtx);
            return ok(antrean.toJSON()); });

    // ----------------------------------------------------------
    // POST /api/proses — proses kendaraan terdepan
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/proses")
        .methods(crow::HTTPMethod::Post)([]()
                                         {
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
            return ok(std::move(d)); });

    // ----------------------------------------------------------
    // GET /api/riwayat — tampilkan semua riwayat
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/riwayat")
        .methods(crow::HTTPMethod::Get)([]()
                                        {
            std::lock_guard<std::mutex> lock(mtx);
            return ok(riwayat.toJSON()); });

    // ----------------------------------------------------------
    // GET /api/riwayat/terakhir
    // ----------------------------------------------------------
    CROW_ROUTE(app, "/api/riwayat/terakhir")
        .methods(crow::HTTPMethod::Get)([]()
                                        {
            std::lock_guard<std::mutex> lock(mtx);
            if (riwayat.isEmpty())
                return err(404, "Belum ada kendaraan yang diproses");

            const Kendaraan &k = riwayat.peek();
            crow::json::wvalue d;
            d["id"]             = k.id;
            d["namaPelanggan"]  = k.namaPelanggan;
            d["jenisKendaraan"] = k.jenisKendaraan;
            d["paketCuci"]      = k.paketCuci;
            d["nomorPlat"]      = k.nomorPlat;
            d["waktuMasuk"]     = k.waktuMasuk;
            return ok(std::move(d)); });

    // ----------------------------------------------------------
    //  Start server
    // ----------------------------------------------------------
    CROW_LOG_INFO << "=== Car Wash Server berjalan di http://localhost:8080 ===";
    app.port(8080).multithreaded().run();
    return 0;
}