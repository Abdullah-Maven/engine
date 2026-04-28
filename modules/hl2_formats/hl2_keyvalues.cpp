/**************************************************************************/
/*  hl2_keyvalues.cpp                                                     */
/**************************************************************************/

#include "hl2_keyvalues.h"

#include "core/io/file_access.h"
#include "core/string/print_string.h"

void HL2KeyValues::Tokenizer::_advance_char() {
	if (_is_eof()) {
		return;
	}

	if (source[cursor] == '\n') {
		line++;
		column = 1;
	} else {
		column++;
	}
	cursor++;
}

char32_t HL2KeyValues::Tokenizer::_peek_char() const {
	if (_is_eof()) {
		return 0;
	}
	return source[cursor];
}

bool HL2KeyValues::Tokenizer::_is_eof() const {
	return cursor >= source.length();
}

void HL2KeyValues::Tokenizer::_skip_whitespace_and_comments() {
	while (!_is_eof()) {
		char32_t c = _peek_char();
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			_advance_char();
			continue;
		}

		if (c == '/' && cursor + 1 < source.length()) {
			char32_t c2 = source[cursor + 1];
			if (c2 == '/') {
				while (!_is_eof() && _peek_char() != '\n') {
					_advance_char();
				}
				continue;
			}
			if (c2 == '*') {
				_advance_char();
				_advance_char();
				while (!_is_eof()) {
					if (_peek_char() == '*' && cursor + 1 < source.length() && source[cursor + 1] == '/') {
						_advance_char();
						_advance_char();
						break;
					}
					_advance_char();
				}
				continue;
			}
		}

		break;
	}
}

HL2KeyValues::Token HL2KeyValues::Tokenizer::_read_quoted_token() {
	Token token;
	token.type = Token::TYPE_STRING;
	token.line = line;
	token.column = column;
	_advance_char();

	StringBuilder sb;
	while (!_is_eof()) {
		char32_t c = _peek_char();
		if (c == '"') {
			_advance_char();
			token.text = sb.as_string();
			return token;
		}

		if (c == '\\') {
			_advance_char();
			if (_is_eof()) {
				set_error("Unterminated escape sequence in quoted token.", line, column);
				return Token();
			}
			char32_t escaped = _peek_char();
			switch (escaped) {
				case 'n': sb += '\n'; break;
				case 'r': sb += '\r'; break;
				case 't': sb += '\t'; break;
				case '"': sb += '"'; break;
				case '\\': sb += '\\'; break;
				default: sb += escaped; break;
			}
			_advance_char();
			continue;
		}

		sb += c;
		_advance_char();
	}

	set_error("Unterminated quoted token.", token.line, token.column);
	return Token();
}

HL2KeyValues::Token HL2KeyValues::Tokenizer::_read_unquoted_token() {
	Token token;
	token.type = Token::TYPE_STRING;
	token.line = line;
	token.column = column;
	StringBuilder sb;

	while (!_is_eof()) {
		char32_t c = _peek_char();
		if (c == '{' || c == '}' || c == '"' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			break;
		}
		if (c == '/' && cursor + 1 < source.length() && (source[cursor + 1] == '/' || source[cursor + 1] == '*')) {
			break;
		}
		sb += c;
		_advance_char();
	}

	token.text = sb.as_string();
	return token;
}

HL2KeyValues::Token HL2KeyValues::Tokenizer::_next_token_internal() {
	_skip_whitespace_and_comments();

	if (_is_eof()) {
		Token token;
		token.type = Token::TYPE_END;
		token.line = line;
		token.column = column;
		return token;
	}

	char32_t c = _peek_char();
	if (c == '{') {
		Token token;
		token.type = Token::TYPE_LBRACE;
		token.line = line;
		token.column = column;
		_advance_char();
		return token;
	}
	if (c == '}') {
		Token token;
		token.type = Token::TYPE_RBRACE;
		token.line = line;
		token.column = column;
		_advance_char();
		return token;
	}
	if (c == '"') {
		return _read_quoted_token();
	}

	return _read_unquoted_token();
}

HL2KeyValues::Tokenizer::Tokenizer(const String &p_source) :
		source(p_source) {}

HL2KeyValues::Token HL2KeyValues::Tokenizer::next() {
	if (has_lookahead) {
		has_lookahead = false;
		return lookahead;
	}
	return _next_token_internal();
}

HL2KeyValues::Token HL2KeyValues::Tokenizer::peek() {
	if (!has_lookahead) {
		lookahead = _next_token_internal();
		has_lookahead = true;
	}
	return lookahead;
}

void HL2KeyValues::Tokenizer::set_error(const String &p_error_message, int p_line, int p_column) {
	if (error_message.is_empty()) {
		error_message = p_error_message;
		error_line = p_line;
		error_column = p_column;
	}
}

bool HL2KeyValues::Tokenizer::has_error() const {
	return !error_message.is_empty();
}

String HL2KeyValues::Tokenizer::get_error_message() const {
	return error_message;
}

int HL2KeyValues::Tokenizer::get_error_line() const {
	return error_line;
}

int HL2KeyValues::Tokenizer::get_error_column() const {
	return error_column;
}

void HL2KeyValues::_insert_entry(Dictionary &p_dict, const String &p_key, const Variant &p_value) {
	if (!p_dict.has(p_key)) {
		p_dict[p_key] = p_value;
		return;
	}

	Variant existing = p_dict[p_key];
	if (existing.get_type() == Variant::ARRAY) {
		Array arr = existing;
		arr.push_back(p_value);
		p_dict[p_key] = arr;
		return;
	}

	Array arr;
	arr.push_back(existing);
	arr.push_back(p_value);
	p_dict[p_key] = arr;
}

bool HL2KeyValues::_parse_block(Tokenizer &p_tokenizer, Dictionary &r_dict, bool p_expect_closing_brace) {
	while (true) {
		Token key_token = p_tokenizer.next();
		if (p_tokenizer.has_error()) {
			return false;
		}

		if (key_token.type == Token::TYPE_END) {
			if (p_expect_closing_brace) {
				p_tokenizer.set_error("Unexpected end of input while parsing block.", key_token.line, key_token.column);
				return false;
			}
			return true;
		}

		if (key_token.type == Token::TYPE_RBRACE) {
			if (!p_expect_closing_brace) {
				p_tokenizer.set_error("Unexpected closing brace in root scope.", key_token.line, key_token.column);
				return false;
			}
			return true;
		}

		if (key_token.type != Token::TYPE_STRING) {
			p_tokenizer.set_error("Expected key token.", key_token.line, key_token.column);
			return false;
		}

		Token value_token = p_tokenizer.peek();
		if (p_tokenizer.has_error()) {
			return false;
		}

		if (value_token.type == Token::TYPE_LBRACE) {
			p_tokenizer.next();
			Dictionary child;
			if (!_parse_block(p_tokenizer, child, true)) {
				return false;
			}
			_insert_entry(r_dict, key_token.text, child);
			continue;
		}

		if (value_token.type == Token::TYPE_STRING) {
			p_tokenizer.next();
			_insert_entry(r_dict, key_token.text, value_token.text);
			continue;
		}

		p_tokenizer.set_error("Expected value string or opening brace after key.", value_token.line, value_token.column);
		return false;
	}
}

void HL2KeyValues::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_text", "text"), &HL2KeyValues::parse_text);
	ClassDB::bind_method(D_METHOD("parse_file", "path"), &HL2KeyValues::parse_file);
	ClassDB::bind_method(D_METHOD("get_last_error_message"), &HL2KeyValues::get_last_error_message);
	ClassDB::bind_method(D_METHOD("get_last_error_line"), &HL2KeyValues::get_last_error_line);
	ClassDB::bind_method(D_METHOD("get_last_error_column"), &HL2KeyValues::get_last_error_column);
}

Dictionary HL2KeyValues::parse_text(const String &p_text) {
	_last_error_message = String();
	_last_error_line = 0;
	_last_error_column = 0;

	Tokenizer tokenizer(p_text);
	Dictionary root;
	if (!_parse_block(tokenizer, root, false)) {
		_last_error_message = tokenizer.get_error_message();
		_last_error_line = tokenizer.get_error_line();
		_last_error_column = tokenizer.get_error_column();
		return Dictionary();
	}

	Token trailing = tokenizer.next();
	if (!tokenizer.has_error() && trailing.type != Token::TYPE_END) {
		_last_error_message = "Unexpected trailing token after root block.";
		_last_error_line = trailing.line;
		_last_error_column = trailing.column;
		return Dictionary();
	}

	if (tokenizer.has_error()) {
		_last_error_message = tokenizer.get_error_message();
		_last_error_line = tokenizer.get_error_line();
		_last_error_column = tokenizer.get_error_column();
		return Dictionary();
	}

	return root;
}

Dictionary HL2KeyValues::parse_file(const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		_last_error_message = vformat("Failed to open '%s'.", p_path);
		_last_error_line = 0;
		_last_error_column = 0;
		return Dictionary();
	}

	String text = file->get_as_utf8_string();
	return parse_text(text);
}

String HL2KeyValues::get_last_error_message() const {
	return _last_error_message;
}

int HL2KeyValues::get_last_error_line() const {
	return _last_error_line;
}

int HL2KeyValues::get_last_error_column() const {
	return _last_error_column;
}
