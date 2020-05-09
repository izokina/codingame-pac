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
	const char* msg;

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
		if (msg != nullptr)
			s << ' ' << msg;
	}

	void walk(const Point& p, const char* msg = nullptr) {
		type = MoveType::WALK;
		pos = p;
		this->msg = msg;
	}

	void turn(GuyType t, const char* msg = nullptr) {
		type = MoveType::TURN;
		guyType = t;
		this->msg = msg;
	}

	void speed(const char* msg = nullptr) {
		type = MoveType::SPEED;
		this->msg = msg;
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
	};

	struct SimGuy {
		Point pos;
		std::vector<Point> path, done;
		Board<Point> prev;
		bool stop;
	};

	Board<SimCell> sb;
	std::array<SimGuy, 5> sg;
	std::vector<Point> q;

	void prepare(const Board<Cell>& board, const std::vector<Guy>& guys) {
		for (auto& g : guys) {
			auto& g2 = sg[g.id];
			q.push_back(g.pos);
			sb[g.pos].visited = true;
			for (size_t i = 0; i < q.size(); i++) {
				for (auto& d : Point::DIRS) {
					auto p = (q[i] + d).norm();
					if (!sb[p].visited && board[p].type != CellType::WALL) {
						q.push_back(p);
						sb[p].visited = true;
						g2.prev[p] = q[i];
					}
				}
			}
			for (auto p : q)
				sb[p].visited = false;
			q.clear();
		}
	}

	int run(const Board<Cell>& board, const std::vector<Guy>& guys, const std::array<Move, 5>& moves, int steps) {
		for (auto& g : guys) {
			auto& m = moves[g.id];
			if (m.type != MoveType::WALK)
				continue;
			auto& g2 = sg[g.id];
			for (Point p = m.pos; p != g.pos; p = g2.prev[p])
				g2.path.push_back(p);
		}
		for (auto& g : guys) {
			auto& g2 = sg[g.id];
			g2.done.push_back(g2.pos = g.pos);
			sb[g.pos].visited = true;
		}
		int res = 0;
		for (int i = 0; i < steps; i++) {
			for (int j = 0; j < 2; j++) {
				for (auto& g : guys) {
					auto& g2 = sg[g.id];
					auto& ps = sg[g.id].path;
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
						if (sb[sg[g.id].path.back()].guys > 1) {
							g2.stop = true;
							sb[g2.pos].guys++;
							go = true;
						}
					}
				}
				for (auto& g : guys) {
					auto& g2 = sg[g.id];
					sb[g2.pos].guys = 0;
					if (g2.path.size())
						sb[g2.path.back()].guys = 0;
					if (!g2.stop) {
						g2.done.push_back(g2.pos = g2.path.back());
						g2.path.pop_back();
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
		for (auto& g : guys) {
			auto& g2 = sg[g.id];
			for (auto p : g2.done) {
				sb[p].visited = false;
			}
			g2.path.clear();
			g2.done.clear();
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
	std::vector<Point> poops1, poops2;
	std::vector<int> perm1, perm2;
	std::vector<int> perm1u, perm2u;

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
				if (c.type == CellType::BIG_POOP) {
					c.type = CellType::FLOOR;
					c.seen = step;
				}
			}
		}

		for (auto& p : poops) {
			auto& c = cells[p.pos];
			c.type = (p.size == 10 ? CellType::BIG_POOP : CellType::POOP);
			if (c.seen != step)
				fprintf(stderr, "Ooops! See poop on %dx%x\n", p.pos.x, p.pos.y);
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
				move.walk(b.pos, "GET YOU");
				return;
			}
		}
		for (auto& b : bitches) {
			if (g.pos.dst(b.pos) == 1 && g.cooldown == 0) {
				move.turn(b.type.up(), "COME ON!");
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
			move.speed("WHOOP!");
			return;
		}
	}

	void walkTogether() {
		poops1.clear();
		poops2.clear();
		perm1.clear();
		perm2.clear();
		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				Point p { x, y };
				auto t = cells[p].type;
				if (t == CellType::BIG_POOP)
					poops1.push_back(p);
				if (t == CellType::POOP)
					poops2.push_back(p);
			}
		}
		for (int i = 0; i < (int) poops1.size(); i++)
			perm1.push_back(i);
		for (int i = 0; i < (int) poops2.size(); i++)
			perm2.push_back(i);
		auto fill = [&](auto& m) {
			size_t i = 0;
			while (true) {
				if (i == walkers.size() || perm2.size() == 0)
					break;
				Point p;
				if (perm1.size()) {
					int idx = RND() % perm1.size();
					int idxx = perm1[idx];
					p = poops1[idxx];
					perm1u.push_back(idxx);
					perm1[idx] = perm1.back();
					perm1.pop_back();
				} else {
					int idx = RND() % perm2.size();
					int idxx = perm2[idx];
					p = poops2[idxx];
					perm2u.push_back(idxx);
					perm2[idx] = perm2.back();
					perm2.pop_back();
				}
				m[walkers[i].id].walk(p, "SMART");
				i++;
			}
			for (auto j : perm1u)
				perm1.push_back(j);
			for (auto j : perm2u)
				perm2.push_back(j);
			perm1u.clear();
			perm2u.clear();
		};
		sim.prepare(cells, guys);
		int best = 0;
		std::array<Move, 5> moves1;
		Cycler cycler { 45'000'000, timer };
		int lim;
		timer.spam("Start sim");
		while ((lim = cycler.getCycles())) {
			// timer.spam("Cycle...");
			for (int i = 0; i < lim; i++) {
				std::shuffle(walkers.begin(), walkers.end(), RND);
				fill(moves1);
				int cur = sim.run(cells, guys, moves1, 5);
				// std::cerr << "+: " << cur << std::endl;
				if (cur > best) {
					fprintf(stderr, "New best: %d -> %d\n", best, cur);
					best = cur;
					for (auto& g : walkers) {
						moves[g.id] = moves1[g.id];
					}
				}
			}
		}
		// timer.spam("Cycle...");
		std::cerr << "Total sim: " << cycler.total() << std::endl;
	}

	void simplePlayWalk(const Guy& g, Move& move) {
		for (auto& p : poops) {
			if (p.size == 10) {
				move.walk(p.pos, "IDIOT 1");
				p = poops.back();
				poops.pop_back();
				return;
			}
		}
		for (auto& p : poops) {
			move.walk(p.pos, "IDIOT 2");
			p = poops.back();
			poops.pop_back();
			return;
		}
		if (g.pos.dst(dst[g.id]) <= 2) {
			dst[g.id] = (dst[g.id] + Point { 3, 11 }).norm();
		}
		move.walk(dst[g.id], "IDIOT 3");
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

        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;

		game.update();

		std::cout << game.simplePlay() << std::endl;
		game.clear();
    }
}
