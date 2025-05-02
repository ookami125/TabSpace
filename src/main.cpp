#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

enum TokenType {
	TAB,
	SPACE,
	NEWLINE,
	UNKNOWN,
};

enum TokenTag {
	NONE,
	WARNING,
	ERROR,
};

struct Token {
	TokenType type;
	size_t line;
	size_t column;
	size_t pos;
	TokenTag tag;
	std::string text;
};

TokenType getTokenType(char c) {
	switch(c) {
		case '\t': return TAB;
		case ' ': return SPACE;
		case '\r': return NEWLINE;
		case '\n': return NEWLINE;
		default: return UNKNOWN;
	}
}

std::vector<Token> tokenize(std::string text)
{
	std::vector<Token> tokens = {};
	size_t line = 1;
	size_t column = 1;
	size_t pos = 0;

	Token last = {
		.type = getTokenType(text[pos]),
		.line = line,
		.column = column,
		.pos = pos,
		.text = "",
	};

	while(pos < text.size()) {
		char c = text[pos];
		TokenType type = getTokenType(c);
		switch(type) {
			case TAB:
			case SPACE: {
				tokens.push_back(last);
				last = Token{
					.type = type,
					.line = line,
					.column = column,
					.pos = pos,
					.text = "",
				};
				last.text += c;
			} break;
			case NEWLINE:
			case UNKNOWN: {
				if(type != last.type) {
					tokens.push_back(last);
					last = Token{
						.type = type,
						.line = line,
						.column = column,
						.pos = pos,
						.text = "",
					};
				}
				last.text += c;
			};
		}
		column += 1;
		if(type == NEWLINE) {
			line += 1;
			column = 1;
		}
		pos += 1;
	}
	tokens.push_back(last);

	return tokens;
}

struct TabSpaceCount {
	size_t tabs;
	size_t spaces;

	std::vector<Token> tokens;

	bool isEmpty() {
		return tabs == 0 && spaces == 0 && ((tokens.size() == 1 && tokens[0].type == NEWLINE) || tokens.size() == 1);
	}
};

#define BULLET "\xE2\x80\xA2"

void printErrorStart(std::vector<TabSpaceCount> counts) {
	Token firstToken;
	for(auto count : counts) {
		if(count.tokens.size() > 0) {
			firstToken = count.tokens[0];
			break;
		}
	}
	printf("Line: %d\n", firstToken.line);
	for(auto& count : counts) {
		bool ErrorTokens = true;
		for(auto& token : count.tokens) {
			if(ErrorTokens && token.type == TAB) {
				printf("\e[31;1m--->\e[0m");
			}
			else if(ErrorTokens && token.type == SPACE) {
				printf("\e[31;1m" BULLET "\e[0m");
			}
			else {
				ErrorTokens = false;
				printf("%s", token.text.c_str());
			}
		}
	}
	printf("\n");
}

void printWarnErr(std::vector<TabSpaceCount> counts) {
	Token firstToken;
	for(auto count : counts) {
		if(count.tokens.size() > 0) {
			firstToken = count.tokens[0];
			break;
		}
	}
	printf("Line: %d\n", firstToken.line);
	for(auto count : counts) {
		TokenTag lastTag = NONE;
		for(auto token : count.tokens) {
			if(lastTag != token.tag) {
				lastTag = token.tag;
				switch(lastTag) {
				case NONE:
					printf("\e[0m");
					break;
				case WARNING:
					printf("\e[33;1m");
					break;
				case ERROR:
					printf("\e[31;1m");
					break;
				}
			}
			if(token.type == TAB) {
				printf("--->");
			}
			else if(token.type == SPACE) {
				printf(BULLET);
			} else printf("%s", token.text.c_str());
		}
		printf("\e[0m");
	}
	printf("\n");
}

bool checkTrailingWhitespace(TabSpaceCount& count) {
	bool ret = false;
	for(int i=((int)count.tokens.size())-1; i >= 0; --i) {
		if(count.tokens[i].type == SPACE || count.tokens[i].type == TAB) {
			count.tokens[i].tag = ERROR;
			ret = true;
		}
		else if (count.tokens[i].type == NEWLINE) {}
		else {
			break;
		}
	}
	return ret;
}

void tagLeadingWhitespace(TabSpaceCount& count, TokenTag tag) {
	for(auto& token : count.tokens) {
		if(token.type == UNKNOWN) break;
		token.tag = tag;
	}
}

int main(int argc, char** argv)
{
	int count = 0;

	for (int i = 1; i < argc; ++i) {
		printf("---------- %s ----------\n", argv[i]);
		std::ifstream t(argv[i]);
		std::stringstream buffer;
		buffer << t.rdbuf();
		std::string str = buffer.str();

		auto tokens = tokenize(str);

		std::vector<TabSpaceCount> counts = {};
		size_t tabCount = 0;
		bool recording = true;
		TabSpaceCount lastCount = {};
		for(Token token : tokens) {
			switch(token.type) {
				case TAB:
					if(recording) lastCount.tabs += 1;
					lastCount.tokens.push_back(token);
					break;
				case SPACE:
					if(recording) lastCount.spaces += 1;
					lastCount.tokens.push_back(token);
					break;
				case UNKNOWN:
					recording = false;
					lastCount.tokens.push_back(token);
					break;
				case NEWLINE: {
					lastCount.tokens.push_back(token);
					counts.push_back(lastCount);
					lastCount = {};
					recording = true;
				} break;
			}
		}
		counts.push_back(lastCount);

		for(size_t i=0; i<counts.size(); ++i) {
			TabSpaceCount& count = counts[i];
			TabSpaceCount& lastIndentCount = (i>0) ? counts[i-1] : counts[0];

			bool hasIssue = false;
			bool needsContext = false;

			//Tabs and spaces changed
			if(lastIndentCount.tabs != count.tabs && count.spaces != 0) {
				tagLeadingWhitespace(count, ERROR);
				tagLeadingWhitespace(lastIndentCount, ERROR);
				hasIssue = true;
				needsContext = true;
			}
			if(lastIndentCount.tabs != count.tabs && lastIndentCount.spaces != 0) {
				tagLeadingWhitespace(count, ERROR);
				tagLeadingWhitespace(lastIndentCount, ERROR);
				hasIssue = true;
				needsContext = true;
			}
			if(abs(lastIndentCount.tabs - count.tabs) > 1 &&
			   abs(counts[i-2].tabs - count.tabs) != 1 &&
			   count.tabs != 0 && lastIndentCount.tabs != 0)
			{
				tagLeadingWhitespace(count, ERROR);
				tagLeadingWhitespace(lastIndentCount, ERROR);
				hasIssue = true;
				needsContext = true;
			}
			if(checkTrailingWhitespace(count)) {
				hasIssue = true;
			}
			if(count.tabs == 0 && count.spaces > 0) {
				tagLeadingWhitespace(count, WARNING);
				hasIssue = true;
			}

			if(hasIssue) {
				if(needsContext) {
					printWarnErr({lastIndentCount, count});
				}
				else
				{
					printWarnErr({count});
				}
			}
		}
	}

	return count;
}
