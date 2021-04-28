#pragma once

#include "mt.h"
#include "filesystem.h"

namespace mt {

	class ScriptStream {
	public:
		ScriptStream();

		explicit ScriptStream(const std::string &);

		explicit ScriptStream(const Ref<IO::MTFile> &);

		~ScriptStream();

		void load(const std::string &);

		[[nodiscard]] bool valid() const;

		std::string token(bool allowLineBreaks = true);

		bool isBlockStart(bool parse = false);

		bool isBlockEnd(bool parse = false);

		void skipLine();

		void skipSection();

		[[nodiscard]] bool matches(const std::string &match) const;

		std::string last();

		std::string previous();

		std::string peekNext();

		bool operator == (const std::string &other) const;

		explicit operator float();

		explicit operator int();

		explicit operator bool();

	private:
		char current();

		char next();

		void parseNext(bool linebreaks);

		bool skipBlockComment();

		bool skipWhitespace();

		std::string fullScript{};
		unsigned int offset = 0;
		unsigned int lines = 0;
		std::string currentToken{};
		std::string previousToken{};
	};
}
