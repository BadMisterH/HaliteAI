#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include <random>
#include <ctime>
#include <unordered_set>
#include <algorithm>
#include <cmath>

using namespace std;
using namespace hlt;

struct ShipHaliteComparator {
    bool operator()(const shared_ptr<Ship>& a, const shared_ptr<Ship>& b) {
        return a->halite > b->halite;
    }
};

class ReturnScheduler {
private:
    unordered_set<int> returning_ships;
    shared_ptr<Ship> current_returning_ship;

public:
    ReturnScheduler() : current_returning_ship(nullptr) {}

    bool should_return(shared_ptr<Ship> ship, int turn_number, const vector<shared_ptr<Ship>>& sorted_ships) {
        // Si déjà en train de retourner
        if (returning_ships.find(ship->id) != returning_ships.end()) {
            return true;
        }

        // Fin de partie
        if (turn_number >= constants::MAX_TURNS - 25) {
            returning_ships.insert(ship->id);
            return true;
        }

        // Bateau plein
        if (ship->halite >= constants::MAX_HALITE * 0.9) {
            returning_ships.insert(ship->id);
            return true;
        }

        // Si c'est le bateau avec le plus d'halite et qu'aucun autre ne retourne
        if (!current_returning_ship &&
            ship == sorted_ships[0] &&
            ship->halite >= constants::MAX_HALITE * 0.5) {
            current_returning_ship = ship;
            returning_ships.insert(ship->id);
            return true;
        }

        return false;
    }

    void update_status(shared_ptr<Ship> ship, const Position& base_pos) {
        if (ship->position == base_pos) {
            returning_ships.erase(ship->id);
            if (current_returning_ship && current_returning_ship->id == ship->id) {
                current_returning_ship = nullptr;
            }
        }
    }

    bool is_returning(int ship_id) const {
        return returning_ships.find(ship_id) != returning_ships.end();
    }
};

int main(int argc, char* argv[]) {
    unsigned int rng_seed;
    if (argc > 1) {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    }
    else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;
    game.ready("MyBot");

    ReturnScheduler return_scheduler;

    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;
        unordered_set<Position> targeted_positions;

        vector<shared_ptr<Ship>> sorted_ships;
        for (const auto& ship_iterator : me->ships) {
            sorted_ships.push_back(ship_iterator.second);
        }
        sort(sorted_ships.begin(), sorted_ships.end(), ShipHaliteComparator());

        for (const auto& ship : sorted_ships) {
            return_scheduler.update_status(ship, me->shipyard->position);

            if (return_scheduler.should_return(ship, game.turn_number, sorted_ships)) {
                Direction move_to_base = game_map->naive_navigate(ship, me->shipyard->position);
                Position target_position = ship->position.directional_offset(move_to_base);

                if (targeted_positions.count(target_position) == 0) {
                    command_queue.push_back(ship->move(move_to_base));
                    targeted_positions.insert(target_position);
                }
                else {
                    command_queue.push_back(ship->stay_still());
                }
                continue;
            }

            if (game_map->at(ship)->halite >= constants::MAX_HALITE / 10) {
                command_queue.push_back(ship->stay_still());
                continue;
            }

            Position best_position = ship->position;
            int max_halite = game_map->at(ship)->halite;

            for (const Direction& direction : ALL_CARDINALS) {
                Position pos = ship->position.directional_offset(direction);
                if (!game_map->at(pos)->is_occupied()) {
                    int halite = game_map->at(pos)->halite;
                    if (halite > max_halite) {
                        max_halite = halite;
                        best_position = pos;
                    }
                }
            }

            if (best_position != ship->position) {
                Direction move = game_map->naive_navigate(ship, best_position);
                Position target_position = ship->position.directional_offset(move);

                if (targeted_positions.count(target_position) == 0) {
                    command_queue.push_back(ship->move(move));
                    targeted_positions.insert(target_position);
                }
                else {
                    command_queue.push_back(ship->stay_still());
                }
            }
            else {
                command_queue.push_back(ship->stay_still());
            }
        }

        if (game.turn_number <= 200 &&
            me->halite >= constants::SHIP_COST &&
            !game_map->at(me->shipyard->position)->is_occupied()) {
            command_queue.push_back(me->shipyard->spawn());
        }

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}
