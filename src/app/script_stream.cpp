#include "script_stream.h"

const std::set<char> singlePunctuations = {
		'{', '}', '[', ']', '<', '>', '(', ')'
};

const std::set<std::string> multiPunctuations = {
		"+=", "-=", "*=", "/=", "&=", "|=", "++", "--",
		"&&", "||", "<=", ">=", "==", "!="
};

mt::ScriptStream::ScriptStream(const std::string &value)
{
	load(value);
}

mt::ScriptStream::ScriptStream(const Ref<IO::MTFile> &file)
{
	load(file);
}

void mt::ScriptStream::load(const Ref<IO::MTFile> &file)
{
	load(file->string());
}

void mt::ScriptStream::load(const std::string &value)
{
	fullScript = value;
	offset = 0;
	lines = 0;
	currentToken = {};
}

bool mt::ScriptStream::valid() const
{
	return !fullScript.empty();
}

std::string mt::ScriptStream::token(bool allowLineBreaks)
{
	parseNext(allowLineBreaks);
	return currentToken;
}

mt::ScriptStream &mt::ScriptStream::parse(bool allowLineBreaks)
{
	parseNext(allowLineBreaks);
	return *this;
}

bool mt::ScriptStream::isBlockStart(bool parse)
{
	if (parse)
	{
		parseNext(true);
	}

	return currentToken == "{";
}

bool mt::ScriptStream::isBlockEnd(bool parse)
{
	if (parse)
	{
		parseNext(true);
	}

	return currentToken == "}";
}

void mt::ScriptStream::skipLine()
{
	for (; offset < fullScript.length(); offset++)
	{
		if (fullScript[offset] == '\n')
		{
			while (fullScript.length() > offset && fullScript[offset] == '\n')
			{
				offset++;
				lines++;
			}
			break;
		}
	}
}

void mt::ScriptStream::skipSection()
{
	unsigned int depth = 0;
	while (current() != '\0')
	{
		if (current() == '{')
		{
			depth++;
		}
		else if (current() == '}')
		{
			depth--;
			if (depth <= 0)
			{
				offset++;
				return;
			}
		}

		offset++;
	}
}

bool mt::ScriptStream::matches(const std::string &match) const
{
	return !currentToken.empty() && currentToken == match;
}

std::string mt::ScriptStream::last()
{
	return currentToken;
}

std::string mt::ScriptStream::previous()
{
	return previousToken;
}

std::string mt::ScriptStream::peekNext()
{
	// Stash our current values
	auto tmpOffset = offset;
	auto tmpCurrent = currentToken;
	auto tmpPrevious = previousToken;

	// parse the next token
	parseNext(true);
	auto next = currentToken;

	// return our old values
	offset = tmpOffset;
	currentToken = tmpCurrent;
	previousToken = tmpPrevious;

	// our shiny peeked value is returned
	return next;
}

void mt::ScriptStream::parse1DMatrix(int columns, float *output)
{
	if (token() != "(")
	{
		throw std::invalid_argument("Expected ( but found: " + last());
	}

	for (int i = 0; i < columns; i++)
	{
		output[i] = parseFloat(false);
	}

	if (token() != ")")
	{
		throw std::invalid_argument("Expected ) but found: " + last());
	}
}

float mt::ScriptStream::parseFloat(bool allowLineBreaks)
{
	return std::stof(token(allowLineBreaks));
}

int mt::ScriptStream::parseInt(bool allowLineBreaks)
{
	return std::stoi(token(allowLineBreaks));
}

bool mt::ScriptStream::parseBool(bool allowLineBreaks)
{
	std::string tmp = token(allowLineBreaks);
	for (auto &s : tmp)
	{
		s = static_cast<char>(tolower(s));
	}
	return tmp == "true";
}

bool mt::ScriptStream::hasNext()
{
	return !peekNext().empty();
}

bool mt::ScriptStream::operator==(const std::string &other) const
{
	return currentToken == other;
}

mt::ScriptStream::operator float()
{
	return std::stof(currentToken);
}

mt::ScriptStream::operator int()
{
	return std::stoi(currentToken);
}

mt::ScriptStream::operator bool()
{
	std::string tmp = currentToken;
	for (auto &s : tmp)
	{
		s = static_cast<char>(tolower(s));
	}
	return tmp == "true";
}

char mt::ScriptStream::current()
{
	return fullScript.length() <= (offset) ? '\0' : fullScript[offset];
}

char mt::ScriptStream::next()
{
	return fullScript.length() <= (offset + 1) ? '\0' : fullScript[offset + 1];
}

void mt::ScriptStream::parseNext(bool linebreaks)
{
	previousToken = currentToken;
	currentToken = {};

	// skip whitespace at the beginning
	if (skipWhitespace() && !linebreaks)
	{
		return;
	}

	// skip whitespace and comments
	while (current() != '\0')
	{
		// Commented line
		if (current() == '/' && next() == '/')
		{
			if (!currentToken.empty())
			{
				return;
			}

			skipLine();

			// If we did not allow line breaks then set the current token to be empty but still move the offset forward
			if (!linebreaks)
			{
				return;
			}

			skipWhitespace();

			continue;
		}
			// Block comment
		else if (current() == '/' && next() == '*')
		{
			if (!currentToken.empty())
			{
				return;
			}

			// If we do not allow line breaks and the block comment expands multiple lines
			// then we need to return an empty token and exit after moving the offset
			if (skipBlockComment() && !linebreaks)
			{
				return;
			}

			continue;
		}

		if (current() == '\n')
		{
			if (!currentToken.empty())
			{
				return;
			}

			skipLine();

			if (!linebreaks)
			{
				return;
			}
		}

		// Quoted string
		if (current() == '\"')
		{
			offset++;
			while (current() != '\0')
			{
				// allow quoted strings to have escaped " characters
				if (current() == '\\' && next() == '\"')
				{
					currentToken.push_back('\"');
				}
				else if (current() == '\"')
				{
					offset++;
					return;
				}
				else if (current() == '\n')
				{
					lines++;
					offset++;
					return;
				}
				else
				{
					currentToken.push_back(current());
				}
				offset++;
			}

			return;
		}

		if (isspace(current()))
		{
			if (currentToken.empty())
			{
				offset++;
				continue;
			}

			return;
		}

		if (multiPunctuations.find({current(), next()}) != multiPunctuations.end())
		{
			if (currentToken.empty())
			{
				currentToken = {current(), next()};
				offset += 2;
			}

			return;
		}

		if (singlePunctuations.find(current()) != singlePunctuations.end())
		{
			if (currentToken.empty())
			{
				currentToken = {current()};
				offset++;
			}

			return;
		}

		// Just push the char into the buffer
		currentToken.push_back(current());
		offset++;
	}
}

bool mt::ScriptStream::skipBlockComment()
{
	// If we do not allow line breaks and the block comment expands multiple lines
	// then we need to return an empty token and exit after moving the offset
	bool hadLineBreaks = false;

	while (current() != '\0')
	{
		offset++;

		if (current() == '\n')
		{
			lines++;

			hadLineBreaks = true;
		}

		if (current() == '*' && next() == '/')
		{
			offset += 2;
			break;
		}
	}

	return hadLineBreaks;
}

bool mt::ScriptStream::skipWhitespace()
{
	bool hadLineBreaks = false;

	while (current() != '\0')
	{
		if (current() == '\n')
		{
			lines++;
			hadLineBreaks = true;
		}

		if (!isspace(current()))
		{
			break;
		}
		offset++;
	}

	return hadLineBreaks;
}
