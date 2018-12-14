#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

using namespace std;

enum class Token { Illegal, Eof, Whitespace, Plus, Minus, Mul, Div, NumberLiteral, RndOpen, RndClose };

ostream& operator<<(ostream& os, Token t) {
	assert(t >= Token::Illegal && t <= Token::RndClose);
	static const std::string map[] = {
		"<<Illegal>>", "<<EOF>>", "<<Whitespace>>", "+", "-", "*", "/", "<<NUMBER>>", "(", ")"
	};
	return os << map[static_cast<size_t>(t)];
}

struct Expr { // {{{ AST
	virtual ~Expr() = default;
};

struct NumberLiteral : public Expr {
	int literal;

	explicit NumberLiteral(int n) : literal{n} {}
};

struct BinaryExpr : public Expr {
	unique_ptr<Expr> lhs;
	unique_ptr<Expr> rhs;

	BinaryExpr(unique_ptr<Expr>&& a, unique_ptr<Expr>&& b) : lhs{move(a)}, rhs{move(b)} {}
};

struct PlusExpr : public BinaryExpr {
	PlusExpr(unique_ptr<Expr>&& a, unique_ptr<Expr>&& b) : BinaryExpr{move(a), move(b)} {}
};

struct MinusExpr : public BinaryExpr {
	MinusExpr(unique_ptr<Expr>&& a, unique_ptr<Expr>&& b) : BinaryExpr{move(a), move(b)} {}
};

struct MulExpr : public BinaryExpr {
	MulExpr(unique_ptr<Expr>&& a, unique_ptr<Expr>&& b) : BinaryExpr{move(a), move(b)} {}
};
struct DivExpr : public BinaryExpr {
	DivExpr(unique_ptr<Expr>&& a, unique_ptr<Expr>&& b) : BinaryExpr{move(a), move(b)} {}
};
// }}}
class Scanner { // {{{
  private:
	const string input_;
	size_t offset_;
	string literal_;
	Token currentToken_;

	bool eof() const noexcept { return offset_ >= input_.size(); }
	void nextChar() { ++offset_; }
	int currentChar() const { return eof() ? -1 : input_[offset_]; }

  public:
	explicit Scanner(string&& input) : input_{move(input)}, offset_{0}, literal_{} {
		tokenize();
	}

	Token currentToken() const noexcept { return currentToken_; }
	const std::string& literal() const noexcept { return literal_; }

	Token tokenizeOnce() {
		literal_ = "";

		if (eof())
			return Token::Eof;

		switch (currentChar()) {
			case ' ': nextChar(); return Token::Whitespace;
			case '+': nextChar(); return Token::Plus;
			case '-': nextChar(); return Token::Minus;
			case '*': nextChar(); return Token::Mul;
			case '/': nextChar(); return Token::Div;
			case '(': nextChar(); return Token::RndOpen;
			case ')': nextChar(); return Token::RndClose;
			default:
				if (isdigit(currentChar())) {
					while (!eof() && isdigit(currentChar())) {
						literal_ += static_cast<char>(currentChar());
						nextChar();
					}
					return Token::NumberLiteral;
				} else {
					nextChar();
					return Token::Illegal;
				}
		}
	}

	Token tokenize() {
		Token t = tokenizeOnce();
		while (t == Token::Whitespace)
			t = tokenizeOnce();
		return currentToken_ = t;
	}
}; // }}}
class ExprParser { // {{{
  private:
	Scanner scanner_;

	Token currentToken() const noexcept { return scanner_.currentToken(); }
	void nextToken() { scanner_.tokenize(); }

  public:
	ExprParser(string&& input) : scanner_{move(input)} {
	}

	unique_ptr<Expr> parse() { return expr(); }

  private:
	unique_ptr<Expr> expr() { return addExpr(); }

	unique_ptr<Expr> addExpr() {
		auto lhs = mulExpr();
		for (;;) {
			switch (currentToken()) {
				case Token::Plus:
					nextToken();
					lhs = make_unique<PlusExpr>(move(lhs), mulExpr());
					break;
				case Token::Minus:
					nextToken();
					lhs = make_unique<MinusExpr>(move(lhs), mulExpr());
					break;
				default:
					return lhs;
			}
		}
	}

	unique_ptr<Expr> mulExpr() {
		auto lhs = primaryExpr();
		for (;;) {
			switch (currentToken()) {
				case Token::Mul:
					nextToken();
					lhs = make_unique<MulExpr>(move(lhs), primaryExpr());
					break;
				case Token::Div:
					nextToken();
					lhs = make_unique<DivExpr>(move(lhs), primaryExpr());
					break;
				default:
					return lhs;
			}
		}
	}

	unique_ptr<Expr> primaryExpr() {
		switch (currentToken()) {
			case Token::NumberLiteral: {
				int number = stoi(scanner_.literal());
				nextToken();
				return make_unique<NumberLiteral>(number);
			}
			case Token::RndOpen: {
				nextToken();
				auto subExpr = expr();
				consumeToken(Token::RndClose);
				return subExpr;
			}
			default:
				cerr << "Primary expression expected.\n";
				exit(EXIT_FAILURE);
		}
	}

	void consumeToken(Token t) {
		if (currentToken() != t) {
			cerr << "Unexpected token" << currentToken() << ". Expected token " << t << " instead.\n";
			exit(EXIT_FAILURE);
		}
	}
}; // }}}
struct Calculator { // {{{
	int evaluate(const Expr& e) {
		if (auto x = dynamic_cast<const PlusExpr*>(&e)) return operator()(*x);
		if (auto x = dynamic_cast<const MinusExpr*>(&e)) return operator()(*x);
		if (auto x = dynamic_cast<const MulExpr*>(&e)) return operator()(*x);
		if (auto x = dynamic_cast<const DivExpr*>(&e)) return operator()(*x);
		if (auto x = dynamic_cast<const NumberLiteral*>(&e)) return operator()(*x);
		return -1; // TODO: we should assert(false) instead
	};
	int operator()(const PlusExpr& e) { return evaluate(*e.lhs) + evaluate(*e.rhs); }
	int operator()(const MinusExpr& e) { return evaluate(*e.lhs) - evaluate(*e.rhs); }
	int operator()(const MulExpr& e) { return evaluate(*e.lhs) * evaluate(*e.rhs); }
	int operator()(const DivExpr& e) { return evaluate(*e.lhs) / evaluate(*e.rhs); }
	int operator()(const NumberLiteral& e) { return e.literal; }
}; // }}}
struct AstPrinter { // {{{
	ostream& os;

	void print(const Expr& e, const string& label, int d = 0) {
		prefix(d); os << label << ":\n";
		if (auto x = dynamic_cast<const PlusExpr*>(&e)) return print(*x, d + 1);
		if (auto x = dynamic_cast<const MinusExpr*>(&e)) return print(*x, d + 1);
		if (auto x = dynamic_cast<const MulExpr*>(&e)) return print(*x, d + 1);
		if (auto x = dynamic_cast<const DivExpr*>(&e)) return print(*x, d + 1);
		if (auto x = dynamic_cast<const NumberLiteral*>(&e)) return print(*x, d + 1);
	};
	void print(const NumberLiteral& e, int d) {
		prefix(d); os << "NumberLiteral: " << e.literal << '\n';
	}

	void print(const PlusExpr& e, int d) {
		prefix(d); os << "PlusExpr:\n";
		print(*e.lhs, "lhs", d + 1);
		print(*e.rhs, "rhs", d + 1);
	}

	void print(const MinusExpr& e, int d) {
		prefix(d); os << "MinusExpr:\n";
		print(*e.lhs, "lhs", d + 1);
		print(*e.rhs, "rhs", d + 1);
	}

	void print(const MulExpr& e, int d) {
		prefix(d); os << "MulExpr:\n";
		print(*e.lhs, "lhs", d + 1);
		print(*e.rhs, "rhs", d + 1);
	}

	void print(const DivExpr& e, int d) {
		prefix(d); os << "DivExpr:\n";
		print(*e.lhs, "lhs", d + 1);
		print(*e.rhs, "rhs", d + 1);
	}

	void prefix(int d) { if (d) os << setw(d * 2) << setfill(' ') << " "; }
}; // }}}

int main(int argc, const char* argv[])
{
	const string input {argc > 1 ? argv[1] : "2 + 3 * 4"};

	unique_ptr<Expr> expr = ExprParser{string{input}}.parse();

	cout << "Result: " << Calculator{}.evaluate(*expr) << '\n';

	AstPrinter{cout}.print(*expr, "expr");

	return EXIT_SUCCESS;
}

