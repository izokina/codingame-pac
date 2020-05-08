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
	static const Point DIRS[4];

	int x, y;

	Point() : Point { 0, 0 } { }

	Point(int x, int y) : x { x }, y { y } { }

	Point(const Point&) = default;

	Point operator +(const Point& p) const {
		return { x + p.x, y + p.y };
	}

	Point operator -(const Point& p) const {
		return { x - p.x, y - p.y };
	}

	Point operator -() const {
		return { -x, -y };
	}

	Point& operator +=(const Point& p) {
		x += p.x;
		y += p.y;
		return *this;
	}

	Point& operator -=(const Point& p) {
		x -= p.x;
		y -= p.y;
		return *this;
	}

	bool operator ==(const Point& p) const {
		auto g = to(p);
		return g.x == 0 && g.y == 0;
	}

	bool operator !=(const Point& p) const {
		return !(*this == p);
	}

	Point mirror() const {
		return { SIZE.x - 1 - x, y };
	}

	Point norm() const {
		return { (x % SIZE.x + SIZE.x) % SIZE.x, (y % SIZE.y + SIZE.y) % SIZE.y };
	}

	Point to(const Point& p) const {
		return (p - *this).norm();
	}

	int dst(const Point& p) const {
		Point d = to(p);
		return std::min(d.x, SIZE.x - d.x) + std::min(d.y, SIZE.y - d.y);
	}
};

Point Point::SIZE;
const Point Point::DIRS[4] {
	{ 1, 0 },
	{ 0, 1 },
	{ -1, 0 },
	{ 0, -1 }
};

struct GuyType {
	GuyType() : GuyType(0) { }

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

enum class CellType {
	WALL, FLOOR, POOP, BIG_POOP
};

struct Cell {
	CellType type = CellType::WALL;
	int seen = 0;
	int pid = -1;
};

template <typename T>
struct Board {
	std::array<std::array<T, 35>, 17> board;

	T& operator[](const Point& p) {
		return board[p.y][p.x];
	}

	auto& operator[](int y) {
		return board[y];
	}
};

enum class MoveType {
	NONE, WALK, TURN, SPEED
};

struct Move {
	MoveType type = MoveType::NONE;
	int id;
	Point pos;
	GuyType guyType;

	Move(int id) : id { id } { }

	void dump(std::stringstream& s) const {
		if (type == MoveType::NONE)
			return;
		if (s.tellp())
			s << '|';
		switch(type) {
			case MoveType::WALK:
				s << "MOVE " << id << ' ' << pos.x << ' ' << pos.y;
				break;
			case MoveType::TURN:
				s << "SWITCH " << id << ' ' << guyType.str();
				break;
			case MoveType::SPEED:
				s << "SPEED " << id;
				break;
			case MoveType::NONE:
				std::cerr << "NONE IN SWITCH!!!" << std::endl;
				exit(5);
		}
	}

	void walk(const Point& p) {
		type = MoveType::WALK;
		pos = p;
	}

	void turn(GuyType t) {
		type = MoveType::TURN;
		guyType = t;
	}

	void speed() {
		type = MoveType::SPEED;
	}

	void none() {
		type = MoveType::NONE;
	}
};

struct Game {
	// const
	int guyCnt;

	// global
	Board<Cell> cells;
	std::vector<Guy> guys, bitches;
	std::vector<Poop> poops;
	std::array<Move, 5> moves { 0, 1, 2, 3, 4 };

	// temp
	int step = 0;

	// trash
	std::vector<Point> dst;

	void clear() {
		step++;
		guys.clear();
		bitches.clear();
		poops.clear();
	}

	void update() {
		if (step == 0) {
			guyCnt = (int) guys.size();
			for (auto& g : guys) {
				auto& c = cells[g.pos.mirror()];
				c.type = CellType::FLOOR;
				c.pid = g.id + guyCnt;
			}

			dst.resize(guyCnt, { 15, 10 });
			for (size_t i = 1; i < dst.size(); i++)
				dst[i] = (dst[i] + dst[i - 1]).norm();
		}

		for (auto& g : guys) {
			for (auto& d : Point::DIRS) {
				Point cur = g.pos;
				for (int i = 0; i < 20; i++) {
					Cell& c = cells[cur];
					if (c.type == CellType::WALL)
						break;
					c.type = CellType::FLOOR;
					c.seen = step;
					c.pid = -1;
					cur = (cur + d).norm();
				}
			}
		}

		for (auto& p : poops) {
			cells[p.pos].type = (p.size == 10 ? CellType::BIG_POOP : CellType::POOP);
		}

		for (auto& g : guys) {
			cells[g.pos].pid = g.id;
		}

		for (auto& b : bitches) {
			cells[b.pos].pid = b.id + guyCnt;
		}
	}

	std::string simplePlay() {
		for (int i = 0; i < guyCnt; i++)
			moves[i].none();

		auto cmp = [](auto& a, auto& b) {
			if (a.pos.y == b.pos.y)
				return a.pos.x < b.pos.x;
			return a.pos.y < b.pos.y;
		};
		std::sort(guys.begin(), guys.end(), cmp);
		for (auto& g : guys) {
			std::sort(poops.begin(), poops.end(), cmp);
			simplePlay(g, moves[g.id]);
		}

		std::stringstream ans;
		for (auto& move : moves) {
			move.dump(ans);
		}
		return ans.str();
	}

	void simplePlay(const Guy& g, Move& move) {
		for (auto& b : bitches) {
			if (g.pos.dst(b.pos) <= 2 && g.type.down() == b.type) {
				move.walk(b.pos);
				return;
			}
		}
		for (auto& b : bitches) {
			if (g.pos.dst(b.pos) == 1 && g.cooldown == 0) {
				move.turn(b.type.up());
				return;
			}
		}
		bool far = true;
		for (auto& b : bitches) {
			if (g.pos.dst(b.pos) <= 10) {
				far = false;
			}
		}
		if (far && g.cooldown == 0) {
			move.speed();
			return;
		}
		for (auto& p : poops) {
			if (p.size == 10) {
				move.walk(p.pos);
				p = poops.back();
				poops.pop_back();
				return;
			}
		}
		for (auto& p : poops) {
			move.walk(p.pos);
			p = poops.back();
			poops.pop_back();
			return;
		}
		if (g.pos.dst(dst[g.id]) <= 2) {
			dst[g.id] = (dst[g.id] + Point { 3, 11 }).norm();
		}
		move.walk(dst[g.id]);
	}
} game;

int main()
{
    int width; // size of the grid
    int height; // top left corner is (x=0, y=0)
	std::cin >> width >> height; std::cin.ignore();
	Point::SIZE = { width, height };
    for (int y = 0; y < height; y++) {
		std::string row;
		std::getline(std::cin, row); // one line of the grid: space " " is floor, pound "#" is wall
		for (int x = 0; x < width; x++)
			game.cells[y][x].type = (row[x] == '#' ? CellType::WALL : CellType::POOP);
    }

    // game loop
    while (1) {
        int myScore;
        int opponentScore;
		std::cin >> myScore >> opponentScore; std::cin.ignore();
        int visiblePacCount; // all your pacs and enemy pacs in sight
		std::cin >> visiblePacCount; std::cin.ignore();
        for (int i = 0; i < visiblePacCount; i++) {
            int pacId; // pac number (unique within a team)
            bool mine; // true if this pac is yours
            int x; // position in the grid
            int y; // position in the grid
			std::string typeId; // unused in wood leagues
            int speedTurnsLeft; // unused in wood leagues
            int abilityCooldown; // unused in wood leagues
			std::cin >> pacId >> mine >> x >> y >> typeId >> speedTurnsLeft >> abilityCooldown; std::cin.ignore();
			(mine ? game.guys : game.bitches).push_back({ pacId, { x, y }, typeId, speedTurnsLeft, abilityCooldown });
        }
        int visiblePelletCount; // all pellets in sight
		std::cin >> visiblePelletCount; std::cin.ignore();
        for (int i = 0; i < visiblePelletCount; i++) {
            int x;
            int y;
            int value; // amount of points this pellet is worth
			std::cin >> x >> y >> value; std::cin.ignore();
			game.poops.push_back({ { x, y }, value });
        }

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;

		game.update();

		std::cout << game.simplePlay() << std::endl;
		game.clear();
    }
}
