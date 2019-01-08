#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

using namespace std;

// tiny helper
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

enum class Token { Illegal, Eof, Whitespace, Plus, Minus, Mul, Div, NumberLiteral, RndOpen, RndClose };

ostream& operator<<(ostream& os, Token t) {
	assert(t >= Token::Illegal && t <= Token::RndClose);
	static const std::string map[] = {
		"<<Illegal>>", "<<EOF>>", "<<Whitespace>>", "+", "-", "*", "/", "<<NUMBER>>", "(", ")"
	};
	return os << map[static_cast<size_t>(t)];
}

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
// {{{ AST
struct NumberLiteral;
struct AddExpr;
struct SubExpr;
struct MulExpr;
struct DivExpr;

using Expr = variant<NumberLiteral, AddExpr, SubExpr, MulExpr, DivExpr>;

struct NumberLiteral { int literal; };
struct AddExpr { unique_ptr<Expr> lhs; unique_ptr<Expr> rhs; };
struct SubExpr { unique_ptr<Expr> lhs; unique_ptr<Expr> rhs; };
struct MulExpr { unique_ptr<Expr> lhs; unique_ptr<Expr> rhs; };
struct DivExpr { unique_ptr<Expr> lhs; unique_ptr<Expr> rhs; };
// }}}
class ExprParser { // {{{
  private:
	Scanner scanner_;

	Token currentToken() const noexcept { return scanner_.currentToken(); }
	void nextToken() { scanner_.tokenize(); }

  public:
	ExprParser(string&& input) : scanner_{move(input)} {
	}

	Expr parse() { return expr(); }

  private:
	Expr expr() { return addExpr(); }

	Expr addExpr() {
		auto lhs = mulExpr();
		for (;;) {
			switch (currentToken()) {
				case Token::Plus:
					nextToken();
					lhs = AddExpr{make_unique<Expr>(move(lhs)), make_unique<Expr>(mulExpr())};
					break;
				case Token::Minus:
					nextToken();
					lhs = SubExpr{make_unique<Expr>(move(lhs)), make_unique<Expr>(mulExpr())};
					break;
				default:
					return lhs;
			}
		}
	}

	Expr mulExpr() {
		auto lhs = primaryExpr();
		for (;;) {
			switch (currentToken()) {
				case Token::Mul:
					nextToken();
					lhs = MulExpr{make_unique<Expr>(move(lhs)), make_unique<Expr>(primaryExpr())};
					break;
				case Token::Div:
					nextToken();
					lhs = DivExpr{make_unique<Expr>(move(lhs)), make_unique<Expr>(primaryExpr())};
					break;
				default:
					return lhs;
			}
		}
	}

	Expr primaryExpr() {
		switch (currentToken()) {
			case Token::NumberLiteral: {
				int number = stoi(scanner_.literal());
				nextToken();
				return NumberLiteral{number};
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
int calculate(const Expr& e) { // {{{
	return visit(overloaded{
		[&](const NumberLiteral& e) { return e.literal; },
		[&](const AddExpr& e) { return calculate(*e.lhs) + calculate(*e.rhs); },
		[&](const SubExpr& e) { return calculate(*e.lhs) + calculate(*e.rhs); },
		[&](const MulExpr& e) { return calculate(*e.lhs) + calculate(*e.rhs); },
		[&](const DivExpr& e) { return calculate(*e.lhs) + calculate(*e.rhs); },
	}, e);
} // }}}
struct AstPrinter { // {{{
	ostream& os;

	void print(const Expr& e, const string& label, int d = 0) {
		prefix(d); os << label << ":\n";
		visit(overloaded{
			[&](NumberLiteral const& e) {
				prefix(d + 1); os << "NumberLiteral: " << e.literal << '\n';
			},
			[&](AddExpr const& e) {
				prefix(d + 1); os << "AddExpr:\n";
				print(*e.lhs, "lhs", d + 2);
				print(*e.rhs, "rhs", d + 2);
			},
			[&](SubExpr const& e) {
				prefix(d + 1); os << "SubExpr:\n";
				print(*e.lhs, "lhs", d + 2);
				print(*e.rhs, "rhs", d + 2);
			},
			[&](MulExpr const& e) {
				prefix(d + 1); os << "MulExpr:\n";
				print(*e.lhs, "lhs", d + 2);
				print(*e.rhs, "rhs", d + 2);
			},
			[&](DivExpr const& e) {
				prefix(d + 1); os << "DivExpr:\n";
				print(*e.lhs, "lhs", d + 2);
				print(*e.rhs, "rhs", d + 2);
			},
		}, e);
	};

	void prefix(int d) { if (d) os << setw(d * 2) << setfill(' ') << " "; }
}; // }}}

int main(int argc, const char* argv[])
{
	const string input {argc > 1 ? argv[1] : "2 + 3 * 4"};

	Expr expr = ExprParser{string{input}}.parse();

	cout << "Result: " << calculate(expr) << '\n';

	AstPrinter{cout}.print(expr, "expr");

	return EXIT_SUCCESS;
}

