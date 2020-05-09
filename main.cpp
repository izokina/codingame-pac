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
#include <random>
#include <chrono>

/**
 * Grab the pellets as fast as you can!
 **/

std::mt19937 RND(324);

using Clock = std::chrono::high_resolution_clock;
using TimeNano = std::chrono::nanoseconds;
using TimeMilli = std::chrono::milliseconds;

class Timer {
public:
	TimeNano get() const {
		Clock::time_point finish = Clock::now();
		return std::chrono::duration_cast<TimeNano>(finish - start);
	}

	TimeMilli getMilli() const {
		return std::chrono::duration_cast<TimeMilli>(get());
	}

	double getMilliDouble() const {
		return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(get()).count();
	}

	void spam(const char* msg) {
		fprintf(stderr, "Time %s: %10.6lf ms\n", msg, getMilliDouble());
	}

private:
	Clock::time_point start = Clock::now();
};

class Alarm {
public:
	Alarm(TimeNano::rep ttl, Timer& timer) : ttl(ttl), timer(timer) { }

	bool ended() const {
		return timer.get().count() > ttl;
	}

private:
	const TimeNano::rep ttl;
	Timer& timer;
};

class Cycler {
public:
	static const int PHASES = 10;
	Cycler(TimeNano::rep ttl, Timer& timer) : ttl(ttl), timer(timer) { }

	int getCycles() {
		int c = calc();
		cycles += c;
		return c;
	}

	int total() const {
		return cycles;
	}

private:
	int calc() {
		if (cycles == 0)
			return 1;
		if (timer.get().count() >= ttl)
			return 0;
		auto phases = (timer.get().count() * PHASES) / ttl;
		if (phases == 0)
			return cycles;
		auto res = cycles / phases;
		if (phases + 2 >= PHASES)
			res /= 2;
		if (phases + 1 >= PHASES)
			res /= 2;
		return std::max<int>(1, res);
	}

	int cycles = 0;
	const TimeNano::rep ttl;
	Timer& timer;
};

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
	{ 0, -1 },
	{ 0, 1 },
	{ -1, 0 },
	{ 1, 0 }
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
	int speed;
	int cooldown;
	GuyType type;
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

	auto& operator[](const Point& p) {
		return board[p.y][p.x];
	}

	auto& operator[](int y) {
		return board[y];
	}

	auto& operator[](const Point& p) const {
		return board[p.y][p.x];
	}

	auto& operator[](int y) const {
		return board[y];
	}
};

enum class MoveType {
	NONE, WALK, TURN, SPEED
};

struct Move {
	MoveType type = MoveType::NONE;
	Point pos;
	GuyType guyType;

	Move() { }

	void dump(std::stringstream& s, int id) const {
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

	bool empty() {
		return type == MoveType::NONE;
	}
};

struct Sim {
	struct SimCell {
		int guys = 0;
		bool visited = false;
		Point prev;
	};

	struct SimGuy {
		Point pos;
		std::vector<Point> paths;
		bool stop;
	};

	Board<SimCell> sb;
	std::array<SimGuy, 5> sg;
	std::vector<Point> q;

	int run(const Board<Cell>& board, const std::vector<Guy>& guys, const std::array<Move, 5>& moves, int steps) {
		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				sb[{ x, y }] = { };
			}
		}
		for (auto& g : guys) {
			sg[g.id].paths.clear();
			auto& m = moves[g.id];
			if (m.type != MoveType::WALK)
				continue;
			q.push_back(g.pos);
			sb[g.pos].visited = true;
			for (size_t i = 0; i < q.size(); i++) {
				for (auto& d : Point::DIRS) {
					auto p = (q[i] + d).norm();
					if (!sb[p].visited && board[p].type != CellType::WALL) {
						q.push_back(p);
						auto& sc = sb[p];
						sc.visited = true;
						sc.prev = q[i];
					}
				}
			}
			if (sb[m.pos].visited) {
				for (Point p = m.pos; p != g.pos; p = sb[p].prev)
					sg[g.id].paths.push_back(p);
			}
			for (auto p : q)
				sb[p] = { };
			q.clear();
		}
		for (auto& g : guys) {
			sg[g.id].pos = g.pos;
			sb[g.pos].visited = true;
		}
		int res = 0;
		for (int i = 0; i < steps; i++) {
			for (int j = 0; j < 2; j++) {
				for (auto& g : guys) {
					auto& g2 = sg[g.id];
					auto& ps = sg[g.id].paths;
					bool stop = (ps.size() == 0 || (j == 1 && g.speed <= i));
					g2.stop = stop;
					if (stop)
						sb[g2.pos].guys++;
					else
						sb[ps.back()].guys++;
				}
				bool go = true;
				while (go) {
					go = false;
					for (auto& g : guys) {
						auto& g2 = sg[g.id];
						if (g2.stop)
							continue;
						if (sb[sg[g.id].paths.back()].guys > 1) {
							g2.stop = true;
							sb[g2.pos].guys++;
							go = true;
						}
					}
				}
				for (auto& g : guys) {
					auto& g2 = sg[g.id];
					sb[g2.pos].guys = 0;
					if (g2.paths.size())
						sb[g2.paths.back()].guys = 0;
					if (!g2.stop) {
						g2.pos = g2.paths.back();
						g2.paths.pop_back();
						bool& v = sb[g2.pos].visited;
						if (!v) {
							auto t = board[g2.pos].type;
							if (t == CellType::POOP)
								res += 1;
							if (t == CellType::BIG_POOP)
								res += 10;
						}
						v = true;
					}
				}
			}
		}
		return res;
	}
};

struct Game {
	// const
	int guyCnt;

	// global
	Board<Cell> cells;
	std::vector<Guy> guys, bitches;
	std::vector<Poop> poops;
	std::array<Move, 5> moves;

	// temp
	int step = 0;
	Sim sim;
	std::vector<Guy> walkers;
	Timer timer;

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

		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				auto& c = cells[{ x, y }];
				if (c.type == CellType::BIG_POOP)
					c.type = CellType::FLOOR;
			}
		}

		for (auto& p : poops) {
			cells[p.pos].type = (p.size == 10 ? CellType::BIG_POOP : CellType::POOP);
			cells[p.pos].seen = step;
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

		walkers.clear();
		for (auto& g : guys) {
			auto& m = moves[g.id];
			simplePlayAttack(g, m);
			if (m.empty())
				walkers.push_back(g);
		}
		walkTogether();
		for (auto& g : guys) {
			auto& m = moves[g.id];
			if (m.empty())
				simplePlayWalk(g, m);
		}

		std::stringstream ans;
		for (int i = 0; i < 5; i++) {
			moves[i].dump(ans, i);
		}
		return ans.str();
	}

	void simplePlayAttack(const Guy& g, Move& move) {
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
	}

	void walkTogether() {
		int best = 0;
		std::array<Move, 5> moves1;
		std::vector<Poop> allPoops;
		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				Point p { x, y };
				auto t = cells[p].type;
				if (t == CellType::POOP)
					allPoops.push_back({ p, 1 });
				if (t == CellType::BIG_POOP)
					allPoops.push_back({ p, 10 });
			}
		}
		auto fill = [&](auto& m) {
			for (size_t i = 0; i < std::min(walkers.size(), allPoops.size()); i++) {
				m[walkers[i].id].walk(allPoops[i].pos);
			}
		};
		Cycler cycler { 45'000'000, timer };
		int lim;
		while ((lim = cycler.getCycles())) {
			// timer.spam("Cycle...");
			for (int i = 0; i < lim; i++) {
				std::shuffle(walkers.begin(), walkers.end(), RND);
				std::shuffle(allPoops.begin(), allPoops.end(), RND);
				std::stable_sort(allPoops.begin(), allPoops.end(), [](auto& a, auto& b) {
					return a.size > b.size;
				});
				fill(moves1);
				int cur = sim.run(cells, guys, moves1, 5);
				// std::cerr << "+: " << cur << std::endl;
				if (cur > best) {
					best = cur;
					fill(moves);
				}
			}
		}
		// timer.spam("Cycle...");
		std::cerr << "Total sim: " << cycler.total() << std::endl;
	}

	void simplePlayWalk(const Guy& g, Move& move) {
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
			(mine ? game.guys : game.bitches).push_back({ pacId, { x, y }, speedTurnsLeft, abilityCooldown, typeId });
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

		game.timer = { };
		game.timer.spam("Input done");

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;

		game.update();

		std::cout << game.simplePlay() << std::endl;
		game.clear();
    }
}
