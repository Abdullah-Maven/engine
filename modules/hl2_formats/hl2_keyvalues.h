/**************************************************************************/
/*  hl2_keyvalues.h                                                       */
/**************************************************************************/

#pragma once

#include "core/object/object.h"

class HL2KeyValues : public Object {
	GDCLASS(HL2KeyValues, Object)

	String _last_error_message;
	int _last_error_line = 0;
	int _last_error_column = 0;

	struct Token {
		enum Type {
			TYPE_END,
			TYPE_STRING,
			TYPE_LBRACE,
			TYPE_RBRACE,
		} type = TYPE_END;
		String text;
		int line = 1;
		int column = 1;
	};

	class Tokenizer {
		const String &source;
		int cursor = 0;
		int line = 1;
		int column = 1;
		Token lookahead;
		bool has_lookahead = false;
		String error_message;
		int error_line = 0;
		int error_column = 0;

		void _advance_char();
		char32_t _peek_char() const;
		bool _is_eof() const;
		void _skip_whitespace_and_comments();
		Token _read_quoted_token();
		Token _read_unquoted_token();
		Token _next_token_internal();

	public:
		Tokenizer(const String &p_source);
		Token next();
		Token peek();
		void set_error(const String &p_error_message, int p_line, int p_column);
		bool has_error() const;
		String get_error_message() const;
		int get_error_line() const;
		int get_error_column() const;
	};

	static void _insert_entry(Dictionary &p_dict, const String &p_key, const Variant &p_value);
	bool _parse_block(Tokenizer &p_tokenizer, Dictionary &r_dict, bool p_expect_closing_brace);

protected:
	static void _bind_methods();

public:
	Dictionary parse_text(const String &p_text);
	Dictionary parse_file(const String &p_path);

	String get_last_error_message() const;
	int get_last_error_line() const;
	int get_last_error_column() const;
};
