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

	Point rot90() const {
		return { -y, x };
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
	uint8_t lvl;

	static GuyType get(const std::string& s) {
		static const auto TM = genTypesMap();
		return { TM.find(s)->second };
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
	static constexpr uint8_t UP[3] { 1, 2, 0 };
	static constexpr uint8_t DOWN[3] { 2, 0, 1 };

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
	union {
		Point pos;
		GuyType guyType;
	} data;

	Move() { }

	void dumpSimple(std::stringstream& s, int id) const {
		if (type == MoveType::NONE)
			return;
		if (s.tellp())
			s << '|';
		switch(type) {
			case MoveType::WALK:
				s << "MOVE " << id << ' ' << data.pos.x << ' ' << data.pos.y;
				break;
			case MoveType::TURN:
				s << "SWITCH " << id << ' ' << data.guyType.str();
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
		data.pos = p;
	}

	void turn(GuyType t) {
		type = MoveType::TURN;
		data.guyType = t;
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

struct MoveFull : public Move {
	const char* msg;

	MoveFull() = default;

	MoveFull(const Move& move, const char* msg = nullptr) : Move(move), msg(msg) { }

	void dump(std::stringstream& s, int id) const {
		if (type == MoveType::NONE)
			return;
		Move::dumpSimple(s, id);
		if (msg != nullptr)
			s << ' ' << msg;
	}

	void walk(const Point& p, const char* msg) {
		Move::walk(p);
		this->msg = msg;
	}

	void turn(GuyType t, const char* msg) {
		Move::turn(t);
		this->msg = msg;
	}

	void speed(const char* msg) {
		Move::speed();
		this->msg = msg;
	}
};

struct Sim {
	struct SimCell {
		int guys = 0;
		bool visited = false;
	};

	struct SimGuy {
		Point pos;
		Point target;
		std::vector<Point> done;
		int idx;
		bool stop;
	};

	struct GuyIn {
		Guy guy;
		std::vector<Move> moves;
	};

	Board<SimCell> sb;
	Board<Board<Point>> dir;
	std::array<SimGuy, 5> sg;
	std::vector<Point> q;

	void init(const Board<Cell>& board) {
		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				Point pos { x, y };
				auto& dirr = dir[pos] = { pos };
				if (board[pos].type == CellType::WALL)
					continue;
				q.push_back(pos);
				sb[pos].visited = true;
				dirr[pos] = pos;
				for (size_t i = 0; i < q.size(); i++) {
					auto qq = q[i];
					for (auto& d : Point::DIRS) {
						auto p = (qq + d).norm();
						if (!sb[p].visited && board[p].type != CellType::WALL) {
							q.push_back(p);
							sb[p].visited = true;
							if (qq == pos)
								dirr[p] = p;
							else
								dirr[p] = dirr[qq];
						}
					}
				}
				for (auto p : q)
					sb[p].visited = false;
				q.clear();
			}
		}
	}

	double run(const Board<Cell>& board, const std::vector<GuyIn>& guys, int steps) {
		for (auto& g : guys) {
			auto& g2 = sg[g.guy.id];
			sb[g.guy.pos].visited = true;
			g2.done.push_back(g2.pos = g.guy.pos);
			g2.idx = 0;
		}
		double res = 0;
		double k = 1.0;
		for (int i = 0; i < steps; i++) {
			for (int j = 0; j < 2; j++) {
				for (auto& g : guys) {
					auto& g2 = sg[g.guy.id];
					if (j == 0) {
						while (g2.idx < (int) g.moves.size()) {
							auto& m = g.moves[g2.idx];
							if (m.type != MoveType::WALK || m.data.pos != g2.pos)
								break;
							g2.idx++;
						}
					}
					if (g2.idx < (int) g.moves.size()) {
						auto& m = g.moves[g2.idx];
						if (m.type == MoveType::WALK && (j == 0 || g.guy.speed > i)) {
							g2.stop = false;
							sb[g2.target = dir[g2.pos][m.data.pos]].guys++;
							continue;
						}
					}
					g2.stop = true;
					sb[g2.target = g2.pos].guys++;
				}
				bool go = true;
				while (go) {
					go = false;
					for (auto& g : guys) {
						auto& g2 = sg[g.guy.id];
						if (!g2.stop && sb[g2.target].guys > 1) {
							g2.stop = true;
							sb[g2.pos].guys++;
							go = true;
						}
					}
				}
				for (auto& g : guys) {
					auto& g2 = sg[g.guy.id];
					sb[g2.pos].guys = 0;
					sb[g2.target].guys = 0;
					if (!g2.stop) {
						g2.done.push_back(g2.pos = g2.target);
						bool& v = sb[g2.pos].visited;
						if (!v) {
							auto t = board[g2.pos].type;
							if (t == CellType::POOP)
								res += k;
							if (t == CellType::BIG_POOP)
								res += 10 * k;
						}
						v = true;
					}
				}
			}
			k *= 0.99;
		}
		for (auto& g : guys) {
			auto& g2 = sg[g.guy.id];
			for (auto p : g2.done) {
				sb[p].visited = false;
			}
			g2.done.clear();
		}
		return res;
	}
};

struct Game {
	struct Gins {
		std::vector<Sim::GuyIn> guys;
		std::vector<Point> poops1, poops2;
	};

	// const
	int guyCnt;

	// global
	Board<Cell> cells;
	Sim sim;
	std::vector<Guy> guys, bitches;
	std::vector<Poop> poops;
	std::array<MoveFull, 5> moves;

	// temp
	int step = 0;
	Timer timer;
	std::vector<int> walkers, walkers2;
	Gins gins, gins2;

	// trash
	std::vector<Point> dst;

	void clear() {
		step++;
		guys.clear();
		bitches.clear();
		poops.clear();
	}

	void init() {
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
	}

	void update() {
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
		for (size_t i = 0; i < guys.size(); i++) {
			auto& g = guys[i];
			auto& m = moves[g.id];
			simplePlayAttack(g, m);
			if (m.empty())
				walkers.push_back((int) i);
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

	void simplePlayAttack(const Guy& g, MoveFull& move) {
		/*
		for (auto& b : bitches) {
			if (g.pos.dst(b.pos) <= 2 && g.type.down() == b.type) {
				move.walk(b.pos, "CHASE");
				return;
			}
		}
		*/
		for (auto& b : bitches) {
			if (g.type.up() == b.type && g.pos.dst(b.pos) == 1) {
				if (g.cooldown == 0) {
					move.turn(b.type.up(), "TRAP");
					return;
				} else {
					Point d = g.pos - b.pos;
					Point dr = d.rot90();
					move.walk((g.pos + d + dr).norm(), "EVADE");
					return;
				}
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

	void fillGins(Gins& g, const std::vector<int>& w) {
		for (int i : w) {
			for (auto& m : g.guys[i].moves) {
				if (m.type == MoveType::WALK) {
					(cells[m.data.pos].type == CellType::POOP ? g.poops2 : g.poops1).push_back(m.data.pos);
				}
			}
			g.guys[i].moves.clear();
		}
		for (int i : w) {
			if (g.poops2.size() == 0)
				break;
			Point p { 0, 0 };
			if (g.poops1.size()) {
				int idx = RND() % g.poops1.size();
				p = g.poops1[idx];
				g.poops1[idx] = g.poops1.back();
				g.poops1.pop_back();
			} else {
				int idx = RND() % g.poops2.size();
				p = g.poops2[idx];
				g.poops2[idx] = g.poops2.back();
				g.poops2.pop_back();
			}
			Move m;
			m.walk(p);
			g.guys[i].moves.push_back(m);
		}
	}

	void walkTogether() {
		gins.poops1.clear();
		gins.poops2.clear();
		for (int y = 0; y < Point::SIZE.y; y++) {
			for (int x = 0; x < Point::SIZE.x; x++) {
				Point p { x, y };
				auto t = cells[p].type;
				if (t == CellType::BIG_POOP)
					gins.poops1.push_back(p);
				if (t == CellType::POOP)
					gins.poops2.push_back(p);
			}
		}
		gins.guys.clear();
		for (auto& g : guys)
			gins.guys.push_back({ g, { moves[g.id] } });
		std::shuffle(walkers.begin(), walkers.end(), RND);
		fillGins(gins, walkers);
		double best = 0;
		Cycler cycler { 45'000'000, timer };
		int lim;
		timer.spam("Start sim");
		while ((lim = cycler.getCycles())) {
			// timer.spam("Cycle...");
			for (int i = 0; i < lim; i++) {
				std::shuffle(walkers.begin(), walkers.end(), RND);
				walkers2.clear();
				size_t lim = std::min<size_t>(walkers.size(), 2);
				for (size_t i = 0; i < lim; i++)
					walkers2.push_back(walkers[i]);
				gins2 = gins;
				fillGins(gins2, walkers2);
				double cur = sim.run(cells, gins2.guys, 5);
				// std::cerr << "+: " << cur << std::endl;
				if (cur > best) {
					for (int j : walkers) {
						auto& g = gins2.guys[j];
						auto& m = g.moves;
						if (m.size())
							moves[g.guy.id] = { m[0], "SMART" };
					}
				}
				if (cur > best) {
					fprintf(stderr, "New best: %lf -> %lf\n", best, cur);
					best = cur;
					gins = gins2;
				}
			}
		}
		// timer.spam("Cycle...");
		std::cerr << "Total sim: " << cycler.total() << std::endl;
	}

	void simplePlayWalk(const Guy& g, MoveFull& move) {
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
	game.sim.init(game.cells);

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
			(mine ? game.guys : game.bitches)
				.push_back({ pacId, { x, y }, speedTurnsLeft, abilityCooldown, GuyType::get(typeId) });
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

		game.init();
		game.timer = { };
		game.update();

		std::cout << game.simplePlay() << std::endl;
		game.clear();
    }
}
