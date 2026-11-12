#include "dungeon/config.hpp"
#include "dungeon/generate.hpp"
#include "dungeon/query.hpp"
#include "dungeon/session.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace {

SDL_Color colorFor(const dungeon::Room& room, int player, bool locked) {
    if (room.id == player) {
        return SDL_Color{80, 180, 255, 255};
    }
    if (room.id == 1) {
        return SDL_Color{90, 200, 90, 255};
    }
    if (room.enemy == dungeon::EnemyKind::Boss) {
        return SDL_Color{210, 70, 70, 255};
    }
    if (room.event == dungeon::EventKind::Door) {
        return locked ? SDL_Color{160, 90, 40, 255} : SDL_Color{210, 170, 70, 255};
    }
    if (room.keyOpens > 0) {
        return SDL_Color{230, 210, 80, 255};
    }
    if (room.event == dungeon::EventKind::SuperChest) {
        return SDL_Color{180, 90, 220, 255};
    }
    if (room.event == dungeon::EventKind::MiddleChest || room.event == dungeon::EventKind::SmallChest) {
        return SDL_Color{150, 110, 200, 255};
    }
    if (room.enemy == dungeon::EnemyKind::Elite) {
        return SDL_Color{200, 110, 60, 255};
    }
    if (room.enemy == dungeon::EnemyKind::Normal) {
        return SDL_Color{180, 80, 80, 255};
    }
    if (room.branch == 0) {
        return SDL_Color{70, 90, 120, 255};
    }
    return SDL_Color{55, 70, 90, 255};
}

const char* labelFor(const dungeon::Room& room) {
    if (room.id == 1) {
        return "S";
    }
    if (room.enemy == dungeon::EnemyKind::Boss) {
        return "B";
    }
    if (room.event == dungeon::EventKind::Door) {
        return "D";
    }
    if (room.keyOpens > 0) {
        return "K";
    }
    if (room.event == dungeon::EventKind::SuperChest) {
        return "C";
    }
    if (room.event == dungeon::EventKind::MiddleChest || room.event == dungeon::EventKind::SmallChest) {
        return "c";
    }
    if (room.enemy == dungeon::EnemyKind::Elite) {
        return "E";
    }
    if (room.enemy == dungeon::EnemyKind::Normal) {
        return "m";
    }
    return room.branch == 0 ? "M" : "o";
}

}  // namespace

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Dungeon Play", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1280, 800, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!window || !renderer) {
        SDL_Log("SDL window/renderer failed: %s", SDL_GetError());
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    dungeon::Config cfg = dungeon::defaultConfig();
    int seed = 12345;
    int mainMin = cfg.mainRooms.min;
    int mainMax = cfg.mainRooms.max;
    int totalMin = cfg.totalRooms.min;
    int totalMax = cfg.totalRooms.max;
    int subMin = cfg.subPaths.min;
    int subMax = cfg.subPaths.max;
    int monsterMin = cfg.monsterNums.min;
    int monsterMax = cfg.monsterNums.max;
    int doors = cfg.doors;
    int superChests = cfg.superChests;
    int middleChests = cfg.middleChests;
    float shortcut = static_cast<float>(cfg.shortcutRatio);
    int sizeRatio = cfg.sizeRatioLimit;

    std::optional<dungeon::Session> session;
    std::string status = "Set parameters and click Generate.";
    std::string lastError;

    auto rebuild = [&]() {
        if (seed < 0) {
            seed = 0;
        }
        if (mainMin > mainMax) {
            std::swap(mainMin, mainMax);
        }
        if (totalMin > totalMax) {
            std::swap(totalMin, totalMax);
        }
        if (totalMin < mainMin) {
            totalMin = mainMin;
        }
        if (totalMax < mainMax) {
            totalMax = mainMax;
        }
        cfg.mainRooms = {mainMin, mainMax};
        cfg.totalRooms = {totalMin, totalMax};
        cfg.subPaths = {subMin, subMax};
        cfg.monsterNums = {monsterMin, monsterMax};
        cfg.doors = doors;
        cfg.superChests = superChests;
        cfg.middleChests = middleChests;
        cfg.shortcutRatio = shortcut;
        cfg.sizeRatioLimit = sizeRatio;
        const auto result = dungeon::generate(cfg, static_cast<uint32_t>(seed));
        if (!result.dungeon) {
            session.reset();
            lastError = dungeon::errorMessage(result.error);
            status = "Generation failed: " + lastError;
            return;
        }
        session.emplace(*result.dungeon);
        const auto& d = session->dungeon();
        status = "Seed " + std::to_string(d.seed) + "  rooms=" + std::to_string(dungeon::roomCount(d)) +
                 "  solvable=" + std::string(dungeon::isSolvable(d) ? "yes" : "no") +
                 "  Walk from S to B. WASD / click a neighbor.";
    };
    auto applyMove = [&](int dest) {
        if (!session || session->won()) {
            return;
        }
        const auto& d = session->dungeon();
        if (dest <= 0 || dest >= static_cast<int>(d.rooms.size())) {
            return;
        }
        const bool locked =
            d.rooms[static_cast<size_t>(dest)].event == dungeon::EventKind::Door &&
            !session->hasKeyFor(dest);
        const auto& cur = d.rooms[static_cast<size_t>(session->player())];
        const bool adjacent =
            std::find(cur.neighbors.begin(), cur.neighbors.end(), dest) != cur.neighbors.end();
        if (!session->tryMove(dest)) {
            if (locked) {
                status = "Locked door. Find the key first.";
            } else if (!adjacent) {
                status = "Move to an adjacent room.";
            }
            return;
        }
        if (session->won()) {
            status = "Cleared! Steps=" + std::to_string(session->steps()) + "  Press N for a new seed.";
        } else {
            status = "Room " + std::to_string(session->player());
        }
    };

    auto syncLogicalSize = [&]() {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if (w > 0 && h > 0) {
            SDL_RenderSetLogicalSize(renderer, w, h);
        }
    };
    syncLogicalSize();
    rebuild();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                syncLogicalSize();
            }
            if (event.type == SDL_KEYDOWN && !ImGui::GetIO().WantCaptureKeyboard && session) {
                dungeon::Pos delta{0, 0};
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                    case SDLK_UP: delta = {0, 1}; break;
                    case SDLK_s:
                    case SDLK_DOWN: delta = {0, -1}; break;
                    case SDLK_a:
                    case SDLK_LEFT: delta = {-1, 0}; break;
                    case SDLK_d:
                    case SDLK_RIGHT: delta = {1, 0}; break;
                    case SDLK_r: rebuild(); break;
                    case SDLK_n:
                        ++seed;
                        rebuild();
                        break;
                    default: break;
                }
                if ((delta.x != 0 || delta.y != 0) && session) {
                    const int player = session->player();
                    if (player > 0 && player < static_cast<int>(session->dungeon().rooms.size())) {
                        const auto& cur = session->dungeon().rooms[static_cast<size_t>(player)];
                        const dungeon::Pos want{cur.pos.x + delta.x, cur.pos.y + delta.y};
                        for (int nb : cur.neighbors) {
                            if (session->dungeon().rooms[static_cast<size_t>(nb)].pos == want) {
                                applyMove(nb);
                                break;
                            }
                        }
                    }
                }
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(340, 760), ImGuiCond_Once);
        ImGui::Begin("Generator");
        ImGui::InputInt("Seed", &seed);
        ImGui::SeparatorText("Rooms");
        ImGui::DragIntRange2("Main", &mainMin, &mainMax, 1, 4, 24);
        ImGui::DragIntRange2("Total", &totalMin, &totalMax, 1, 8, 40);
        ImGui::DragIntRange2("Branches", &subMin, &subMax, 1, 0, 8);
        ImGui::DragIntRange2("Monsters", &monsterMin, &monsterMax, 1, 0, 20);
        ImGui::SliderInt("Doors", &doors, 0, 6);
        ImGui::SliderInt("Super chests", &superChests, 0, 4);
        ImGui::SliderInt("Middle chests", &middleChests, 0, 6);
        if (ImGui::CollapsingHeader("Advanced")) {
            ImGui::SliderFloat("Shortcut ratio", &shortcut, 0.3f, 1.0f);
            ImGui::SliderInt("Size ratio limit", &sizeRatio, 2, 8);
        }
        if (ImGui::Button("Generate", ImVec2(150, 32))) {
            rebuild();
        }
        ImGui::SameLine();
        if (ImGui::Button("New seed", ImVec2(150, 32))) {
            ++seed;
            rebuild();
        }
        ImGui::TextWrapped("%s", status.c_str());
        if (session) {
            const auto& d = session->dungeon();
            ImGui::Separator();
            ImGui::Text("Player room: %d", session->player());
            ImGui::Text("Steps: %d", session->steps());
            ImGui::Text("Start -> boss hops: %d", dungeon::shortestHops(d, 1, d.lastMainId));
            ImGui::TextUnformatted("S start  B boss  D door  K key");
            ImGui::TextUnformatted("WASD move  R regenerate  N next seed");
            if (session->won()) {
                ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1.f), "Dungeon cleared");
            }
        }
        ImGui::End();

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const int ww = static_cast<int>(display.x);
        const int hh = static_cast<int>(display.y);
        const float panel = 360.f;
        const float mapX0 = panel;
        const float mapY0 = 20.f;
        const float mapW = static_cast<float>(ww) - panel - 20.f;
        const float mapH = static_cast<float>(hh) - 40.f;

        int minX = 0, maxX = 0, minY = 0, maxY = 0;
        float cell = 0, ox = 0, oy = 0;
        const bool showMap = session && mapW > 50 && mapH > 50;
        if (showMap) {
            const auto& d = session->dungeon();
            minX = maxX = d.rooms[1].pos.x;
            minY = maxY = d.rooms[1].pos.y;
            for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
                minX = std::min(minX, d.rooms[static_cast<size_t>(i)].pos.x);
                maxX = std::max(maxX, d.rooms[static_cast<size_t>(i)].pos.x);
                minY = std::min(minY, d.rooms[static_cast<size_t>(i)].pos.y);
                maxY = std::max(maxY, d.rooms[static_cast<size_t>(i)].pos.y);
            }
            const int gw = std::max(1, maxX - minX + 1);
            const int gh = std::max(1, maxY - minY + 1);
            cell = std::min(mapW / static_cast<float>(gw), mapH / static_cast<float>(gh)) * 0.82f;
            ox = mapX0 + (mapW - cell * static_cast<float>(gw)) * 0.5f;
            oy = mapY0 + (mapH - cell * static_cast<float>(gh)) * 0.5f;

            auto screen = [&](dungeon::Pos p) {
                return ImVec2(ox + (static_cast<float>(p.x - minX) + 0.5f) * cell,
                              oy + (static_cast<float>(maxY - p.y) + 0.5f) * cell);
            };
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
                const auto& room = d.rooms[static_cast<size_t>(i)];
                const ImVec2 c = screen(room.pos);
                const char* text = labelFor(room);
                const ImVec2 ts = ImGui::CalcTextSize(text);
                fg->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(245, 245, 245, 255), text);
            }
        }

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 18, 20, 26, 255);
        SDL_RenderClear(renderer);

        if (showMap) {
            const auto& d = session->dungeon();
            auto screenSdl = [&](dungeon::Pos p) {
                const float x = ox + (static_cast<float>(p.x - minX) + 0.5f) * cell;
                const float y = oy + (static_cast<float>(maxY - p.y) + 0.5f) * cell;
                return SDL_FPoint{x, y};
            };

            SDL_SetRenderDrawColor(renderer, 90, 100, 120, 255);
            for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
                const auto& room = d.rooms[static_cast<size_t>(i)];
                const auto a = screenSdl(room.pos);
                for (int nb : room.neighbors) {
                    if (nb < room.id) {
                        continue;
                    }
                    const auto b = screenSdl(d.rooms[static_cast<size_t>(nb)].pos);
                    SDL_RenderDrawLine(renderer, static_cast<int>(a.x), static_cast<int>(a.y),
                                       static_cast<int>(b.x), static_cast<int>(b.y));
                }
            }

            const int mx = ImGui::GetIO().MouseClicked[0] ? static_cast<int>(ImGui::GetIO().MouseClickedPos[0].x) : -1;
            const int my = ImGui::GetIO().MouseClicked[0] ? static_cast<int>(ImGui::GetIO().MouseClickedPos[0].y) : -1;
            const bool clickMap = mx >= static_cast<int>(mapX0) && !ImGui::GetIO().WantCaptureMouse;

            for (int i = 1; i < static_cast<int>(d.rooms.size()); ++i) {
                const auto& room = d.rooms[static_cast<size_t>(i)];
                const auto c = screenSdl(room.pos);
                const float s = cell * 0.38f;
                SDL_Rect rect{static_cast<int>(c.x - s), static_cast<int>(c.y - s), static_cast<int>(s * 2),
                              static_cast<int>(s * 2)};
                const bool locked =
                    room.event == dungeon::EventKind::Door && !session->hasKeyFor(room.id);
                const auto col = colorFor(room, session->player(), locked);
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 255);
                SDL_RenderFillRect(renderer, &rect);
                SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
                SDL_RenderDrawRect(renderer, &rect);

                if (clickMap && mx >= rect.x && mx < rect.x + rect.w && my >= rect.y && my < rect.y + rect.h) {
                    applyMove(room.id);
                }
            }
        }

        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
