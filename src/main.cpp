#define CROW_STATIC_DIRECTORY "public/"
#define CROW_STATIC_ENDPOINT "/public/<path>"

#include <mutex>
#include <string>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include "crow_all.h"
#include "minesweeper.hpp"

int main(int argc, char *argv[]) {
    using namespace crow;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        std::cerr << "Invalid port number. Port must be between 1 and 65535." << std::endl;
        return 1;
    }

    SimpleApp app;

    std::unordered_map<std::string, minesweeper> active_session;
    std::unordered_map<std::string, std::mutex> session_mutex;

    std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());

    auto generate_new_session = [&]() -> std::string {
        constexpr int session_length = 64;
        constexpr int GENERATE_SESSION_MAX_TRIES = 100;

        static const std::string alphanum =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        int tries = 0;
        std::string session(session_length, 0);
        do {
            for (int i = 0; i < session_length; ++i) 
                session[i] = alphanum[generator() % alphanum.length()];
        } while (active_session.find(session) != active_session.end() && tries++ < GENERATE_SESSION_MAX_TRIES);

        return session;
    };

    CROW_ROUTE(app, "/new_session").methods(HTTPMethod::POST)([&](const request& req, response& res){
        auto json = json::load(req.body);
        std::string session = generate_new_session();

        if (session != "-1") {
            json::wvalue body{};
            active_session.insert({session, minesweeper(json["rows"].i(), json["cols"].i(), json["mine_density"].d())});
            body["session_id"] = session;
            body["bomb_remaining"] = active_session.find(session)->second.get_bomb_remaining();
            res.body = body.dump();
        } else {
            res.code = 500;
        }

        res.end();
    });

    CROW_ROUTE(app, "/end_session").methods(HTTPMethod::POST) ([&](const request& req, response& res){
        auto json = json::load(req.body);
        std::string session = json["session_id"].s();
        auto it = active_session.find(session);
        if (it != active_session.end()) {
            std::lock_guard<std::mutex> lg(session_mutex[session]);
            active_session.erase(it);
            res.code = 200;
        } else res.code = 400;
        res.end();
    });
    
    CROW_ROUTE(app, "/left_click").methods(HTTPMethod::POST)([&](const request& req, response& res){
        auto json = json::load(req.body);
        int x = json["x"].i();
        int y = json["y"].i();
        std::string session = json["session_id"].s();

        auto it = active_session.find(session);
        if (it != active_session.end() && it->second.is_valid({x, y}) && it->second.get_game_status() == minesweeper::GAME_STATUS::NEUTRAL) {
            json::wvalue body{};
            std::lock_guard<std::mutex> lg(session_mutex[session]);

            auto arr = it->second.reveal_all({x, y});
            std::vector<json::wvalue> display_id;
            std::vector<json::wvalue> updated_cell;
            for (size_t i = 0; i < arr.size(); ++i) {
                if (it->second.is_bomb(arr[i])) {
                    display_id.emplace_back(json::wvalue{9}); 
                } else {
                    display_id.emplace_back(json::wvalue{0 + it->second.get_adjacent_bomb_count(arr[i])}); 
                }
                updated_cell.emplace_back(std::vector<json::wvalue>{arr[i].first, arr[i].second});
            }

            body["updated_cell"] = std::move(updated_cell);
            body["display_id"] = std::move(display_id);

            switch (it->second.get_game_status()) {
                case minesweeper::GAME_STATUS::WIN:
                    body["game_status"] = "WIN";
                    break;
                case minesweeper::GAME_STATUS::LOSE:
                    body["game_status"] = "LOSE";
                    break;
                default: 
                    body["game_status"] = "NEUTRAL";
            }

            body["bomb_remaining"] = it->second.get_bomb_remaining();

            res.body = body.dump();

        } else {
            res.code = 400;
        }
        
        res.end();
    });
    
    CROW_ROUTE(app, "/right_click").methods(HTTPMethod::POST)([&](const request& req, response& res){
        auto json = json::load(req.body);
        int x = json["x"].i();
        int y = json["y"].i();
        std::string session = json["session_id"].s();

        auto it = active_session.find(session);
        if (it != active_session.end() && it->second.is_valid({x, y}) && it->second.get_game_status() == minesweeper::GAME_STATUS::NEUTRAL) {
            std::lock_guard<std::mutex> lg(session_mutex[session]);
            json::wvalue body{};
            
            if (!it->second.is_revealed({x, y})) {
                it->second.toggle_flag({x, y});
                body["updated_cell"] = std::move(std::vector<json::wvalue> {std::vector<json::wvalue>{x, y}});
                body["display_id"] = std::move(std::vector<json::wvalue>{it->second.is_flagged({x, y}) ? 11 : 10});
            } else {
                body["updated_cell"] = {} ;
                body["display_id"] = {};
            }
 
            switch (it->second.get_game_status()) {
                case minesweeper::GAME_STATUS::WIN:
                    body["game_status"] = "WIN";
                    break;
                case minesweeper::GAME_STATUS::LOSE:
                    body["game_status"] = "LOSE";
                    break;
                default: 
                    body["game_status"] = "NEUTRAL";
            }

            body["bomb_remaining"] = it->second.get_bomb_remaining();
            res.body = body.dump();

        } else {
            res.code = 400;
        }
        res.end();
    });

    CROW_ROUTE(app, "/get_hint").methods(HTTPMethod::POST)([&](const request& req, response& res){
        auto json = json::load(req.body);
        std::string session = json["session_id"].s();

        auto it = active_session.find(session);
        if (it != active_session.end()) {
            json::wvalue body{};

            std::lock_guard<std::mutex> lg(session_mutex[session]);
            std::vector<json::wvalue> arr;
            for (auto& i : it->second.get_hint()) {
                std::string type;
                switch(i.hint) {
                    case minesweeper::HINT_TYPE::SAFE:
                        type = "SAFE";
                        break;
                    case minesweeper::HINT_TYPE::MINE:
                        type = "MINE";
                        break;
                    default:
                        type = "HIGH_PROBABILITY";
                }
                arr.emplace_back(json::wvalue{{"x", i.x}, {"y", i.y}, {"HINT_TYPE", type}});
            }
            body["hints"] = std::move(arr);
            res.body = body.dump();

        } else {
            res.code = 400;
        }
        res.end();
    });

    CROW_ROUTE(app, "/")([&](response& res){
        res.set_static_file_info("./public/index.html");
        res.end();
    });

    std::cout << "Server running on port " << port << '\n';
    app.port(port).multithreaded().bindaddr("::").run();
}
