// See tokenzier.cpp for more details
// 14.03.14, FKling, published to github
#pragma once

#include <vector>
#include <string>

namespace gnilk
{

	class Tokenizer
	{
	protected:
		std::vector<std::string> tokens;
		size_t iTokenIndex;

	protected:
		const char *operators;
		char *pOrgInput;
		char *parsepoint;

		bool IsOperator(char input);
		char *GetNextToken(char *dst, int nMax, char **input);
		void Prepare();

	public:
		Tokenizer(const char *sInput);
		Tokenizer(const char *sInput, const char *sOperators);
		virtual ~Tokenizer();

		bool HasMore();
		const char *Previous();
		const char *Next();
		const char *Peek();

		static int Case(const char *sValue, const char *sInput);
	};
}