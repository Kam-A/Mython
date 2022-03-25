#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <unordered_set>

#include <iostream>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}


const Token& Lexer::CurrentToken() const {
    return current_token_;
}

optional<Token> Lexer::CheckIndentDedent() {
    if (indent_number_ == 0) {
        return {};
    } else if (indent_number_ < 0) {
        ++indent_number_;
        return token_type::Dedent();
    } else {
        --indent_number_;
        return token_type::Indent();
    }
}

bool Lexer::UpdateCurrentTokenByIndent() {
    auto token = CheckIndentDedent();
    if (token) {
        current_token_ = token.value();
        return true;
    } else {
        return false;
    }
}

token_type::String MakeStringToken(string_view& cursor) {
//    char delim = cursor[0];
//    cursor.remove_prefix(1);
//    string value;
//    for (const char c : string(cursor)) {
//        if (c == '\\') {
//
//        }
//        cout << c << endl;
//    }
//    for (int i = 0; i < cursor.size();++i) {
//        if (cursor[i] == delim && cursor[i - 1] != '\\') {
//            value = string(cursor.substr(0, i));
//            cursor.remove_prefix(i + 1);
//            break;
//        }
//    }
//
////    while(true) {
////        end_string_pos = cursor.find(delim);
////        if (cursor[end_string_pos - 1] != mirror) {
////            value += string(cursor.substr(0, end_string_pos));
////            break;
////        }
////        value += string(cursor.substr(0, end_string_pos));
////        cursor.remove_prefix(end_string_pos);
////    }
//    cout << value << endl;
//    return {value};
    char delim = cursor[0];
    cursor.remove_prefix(1);
    std::string s;
    int64_t index = 0;
    while (true) {
        const char ch = cursor[index];
        if (ch == delim) {
            ++index;
            break;
        } else if (ch == '\\') {
            ++index;
            const char escaped_char = cursor[index];
            switch (escaped_char) {
                case 'n':
                    s.push_back('\n');
                    break;
                case 't':
                    s.push_back('\t');
                    break;
                case 'r':
                    s.push_back('\r');
                    break;
                case '"':
                    s.push_back('"');
                    break;
                case '\\':
                    s.push_back('\\');
                    break;
                case '\'':
                    s.push_back('\'');
                    break;
                default:
                    break;
            }
        } else {
            s.push_back(ch);
        }
        ++index;
    }
    cursor.remove_prefix(index);
    return {std::move(s)};
}

token_type::Number MakeNumberToken(string_view& cursor) {
    string value;
    int index = 0;
    while(isdigit(cursor[index])) {
        value += cursor[index];
        ++index;
    }
    cursor.remove_prefix(index);
    return {stoi(value)};
}

Token DetectTokenAndCreate(string word) {
    static unordered_map<string, Token> reserved_string = {{"class"s, token_type::Class()}, {"return"s, token_type::Return()},
                                                            {"if"s, token_type::If()}, {"else"s, token_type::Else()},
                                                            {"def"s, token_type::Def()}, {"print"s, token_type::Print()},
                                                            {"or"s, token_type::Or()}, {"None"s, token_type::None()},
                                                            {"False"s, token_type::False()}, {"True"s, token_type::True()},
                                                            {"and"s, token_type::And()}, {"not"s, token_type::Not()},
                                                            {"or"s, token_type::Or()}};
    if (reserved_string.count(word) != 0){
        return reserved_string[word];
    } else {
        return token_type::Id{string(word)};
    }
}

Token MakeCharIdReservedToken(string_view& cursor) {
    static unordered_set<char> reserved_char = { '.', ',', '(' , '+', ')', '-', ':', ' ', '/', '*', '>', '<', '=', '!', '#'};
    static unordered_map<string, Token> compare_string = {{"=="s, token_type::Eq()}, {">="s, token_type::GreaterOrEq()},
                                                        {"<="s, token_type::LessOrEq()}, {"!="s, token_type::NotEq()}};
    string word;
    for(size_t i = 0; i < cursor.size(); ++i) {
        if (reserved_char.count(cursor[i])) {
            if (word.size() == 0) {
                if (i + 1 < cursor.size() && compare_string.count(string(cursor.substr(i, i + 2)))) {
                    Token token = compare_string[string(cursor.substr(i, i + 2))];
                    cursor.remove_prefix(i + 2);
                    return token;
                } else {
                    token_type::Char token{cursor[i]};
                    cursor.remove_prefix(i + 1);
                    return token;
                }
            } else {
                cursor.remove_prefix(i);
                return DetectTokenAndCreate(word);
            }
        }
        word += cursor[i];
    }
    cursor.remove_prefix(cursor.size());
    return DetectTokenAndCreate(word);
    
}

Token Lexer::GetTokenFromBuffer() {
    int64_t symb_pos = cursor_.find_first_not_of(' ');
    cursor_.remove_prefix(symb_pos);
    if (cursor_[0] == '\'' || cursor_[0] == '"') {
        return MakeStringToken(cursor_);
    } else if (isdigit(cursor_[0])) {
        return MakeNumberToken(cursor_);
    } else if (cursor_[0] == '#') {
        current_line_.clear();
        cursor_ = current_line_;
        return token_type::Newline();
    } else {
        return MakeCharIdReservedToken(cursor_);
    }
}

optional<string> Lexer::GetNewLine() {
    string input_string;
    while(getline(input_, input_string)) {
        if (input_string[0] != '#' &&  input_string.size() != 0) {
            break;
        }
    }
    if (input_string.size() == 0) {
        return {};
    } else if (input_string[0] == '#') {
        return {};
    } else {
        return input_string;
    }
}

Token Lexer::FinishedStreamProcess() {
    if (indent_ != 0) {
        --indent_;
        return token_type::Dedent();
    } else {
        return token_type::Eof();
    }
}

string Lexer::ProcessStringAndIndent(string& line) {
    int64_t start_symbol_pos = line.find_first_not_of(" "s);
    // check indent
    indent_number_ += (start_symbol_pos / 2 - indent_);
    indent_ = start_symbol_pos / 2;
    return line.substr(start_symbol_pos);
}

Lexer::Lexer(std::istream& input) : input_(input), indent_(0), indent_number_(0), is_stream_finished(false) {
    NextToken();
}

Token Lexer::NextToken() {
    if (is_stream_finished) {
        current_token_ = FinishedStreamProcess();
    }
    if (UpdateCurrentTokenByIndent()) {
        return current_token_;
    }
    if (cursor_.empty()) {
        if (current_line_.size() == 0) {
            auto line = GetNewLine();
            if (line) {
                current_line_ = ProcessStringAndIndent(line.value());
                cursor_ = current_line_;
                if (!UpdateCurrentTokenByIndent()) {
                    current_token_ =  GetTokenFromBuffer();
                }
            } else {
                is_stream_finished = true;
                current_token_ = FinishedStreamProcess();
            }
        } else {
            current_line_.clear();
            current_token_ = token_type::Newline();
        }
    } else {
        current_token_ = GetTokenFromBuffer();
    }
    return current_token_;
}

}  // namespace parse

