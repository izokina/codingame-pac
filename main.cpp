#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

/**
 * Grab the pellets as fast as you can!
 **/

struct Point {
	static Point SIZE;

	int x = 0, y = 0;

	Point operator +(const Point& p) const {
		return { (x + p.x) % SIZE.x, (y + p.y) % SIZE.y };
	}

	Point operator -(const Point& p) const {
		return { (x - p.x + SIZE.x) % SIZE.x, (y - p.y + SIZE.x) % SIZE.y };
	}

	int dst(const Point& p) const {
		return abs(x - p.x) + abs(y - p.y);
	}
};

struct GuyType {
	GuyType(const std::string& s) {
		static const auto TM = genTypesMap();
		lvl = TM.find(s)->second;
	}

	bool operator <(const GuyType& t) const {
		return UP[lvl] == t.lvl;
	}

	bool operator ==(const GuyType& t) const {
		return lvl == t.lvl;
	}

	GuyType up() const {
		return { UP[lvl] };
	}

	GuyType down() const {
		return { DOWN[lvl] };
	}

	const std::string& str() const {
		return TYPES[lvl];
	}

private:
	GuyType(uint8_t l) : lvl { l } { }
	uint8_t lvl;
	uint8_t UP[3] { 1, 2, 0 };
	uint8_t DOWN[3] { 2, 0, 1 };

	static std::unordered_map<std::string, uint8_t> genTypesMap() {
		std::unordered_map<std::string, uint8_t> res;
		for (size_t i = 0; i < std::size(TYPES); i++)
			res[TYPES[i]] = i;
		return res;
	}

	static std::string TYPES[3];
};

std::string GuyType::TYPES[3] {
	"PAPER",
	"SCISSORS",
	"ROCK"
};

struct Guy {
	int id;
	Point pos;
	GuyType type;
	int speed;
	int cooldown;
};

struct Poop {
	Point pos;
	int size;
};

void move(std::stringstream& s, int id, const Point& p) {
	s << "MOVE " << id << " " << p.x << " " << p.y;
}

void turn(std::stringstream& s, int id, GuyType t) {
	s << "SWITCH " << id << " " << t.str();
}

int main()
{
    int width; // size of the grid
    int height; // top left corner is (x=0, y=0)
	std::cin >> width >> height; std::cin.ignore();
    for (int i = 0; i < height; i++) {
		std::string row;
		std::getline(std::cin, row); // one line of the grid: space " " is floor, pound "#" is wall
    }

    // game loop
    while (1) {
        int myScore;
        int opponentScore;
		std::cin >> myScore >> opponentScore; std::cin.ignore();
        int visiblePacCount; // all your pacs and enemy pacs in sight
		std::cin >> visiblePacCount; std::cin.ignore();
		std::vector<Guy> guys, bitches;
        for (int i = 0; i < visiblePacCount; i++) {
            int pacId; // pac number (unique within a team)
            bool mine; // true if this pac is yours
            int x; // position in the grid
            int y; // position in the grid
			std::string typeId; // unused in wood leagues
            int speedTurnsLeft; // unused in wood leagues
            int abilityCooldown; // unused in wood leagues
			std::cin >> pacId >> mine >> x >> y >> typeId >> speedTurnsLeft >> abilityCooldown; std::cin.ignore();
			(mine ? guys : bitches).push_back({ pacId, { x, y }, typeId, speedTurnsLeft, abilityCooldown });
        }
        int visiblePelletCount; // all pellets in sight
		std::cin >> visiblePelletCount; std::cin.ignore();
		std::vector<Poop> poops;
        for (int i = 0; i < visiblePelletCount; i++) {
            int x;
            int y;
            int value; // amount of points this pellet is worth
			std::cin >> x >> y >> value; std::cin.ignore();
			poops.push_back({ { x, y }, value });
        }

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;

		std::stringstream ans;
		bool first = true;
		for (auto& g : guys) {
			if (!first) {
				ans << "|";
			}
			first = false;
			auto strategy = [&]() {
				for (auto& b : bitches) {
					if (g.pos.dst(b.pos) <= 2 && g.type.down() == b.type) {
						move(ans, g.id, b.pos);
						return;
					}
					if (g.pos.dst(b.pos) == 1 && g.cooldown == 0) {
						turn(ans, g.id, b.type.up());
						return;
					}
				}
				for (auto& p : poops) {
					if (p.size == 10) {
						move(ans, g.id, p.pos);
						p = poops.back();
						poops.pop_back();
						return;
					}
				}
				for (auto& p : poops) {
					move(ans, g.id, p.pos);
					p = poops.back();
					poops.pop_back();
					return;
				}
				move(ans, g.id, { 15, 10 });
			};
			strategy();
		}
		std::cout << ans.str() << std::endl;
    }
}
