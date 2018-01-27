/*-------------------------------------------------------------------------
File    : $Archive: Tokenizer.cpp $
Author  : $Author: FKling $
Version : $Revision: 1 $
Orginal : 2009-10-17, 15:50
Descr   : Simple and extensible line tokenzier, pre-processes the data 
	  and stores tokens in a list.

Modified: $Date: $ by $Author: FKling $
---------------------------------------------------------------------------
TODO: [ -:Not done, +:In progress, !:Completed]
<pre>
 - Block comments
 - Extend to handle multi line
 - Line comments
 - Multi character operators
</pre>


\History
- 14.03.14, FKling, published on github
- 25.10.09, FKling, Implementation

---------------------------------------------------------------------------*/

#include <stdlib.h>
#include <vector>
#include <string>
#include <string.h>

#include "tokenizer.h"

using namespace gnilk;

Tokenizer::Tokenizer(const char *sInput, const char *sOperators)
{
	operators = strdup(sOperators);
	pOrgInput = strdup(sInput);
	parsepoint = pOrgInput;
	iTokenIndex = 0;
	Prepare();
}

Tokenizer::Tokenizer(const char *sInput)
{
	operators = strdup(" ");	// default operator is space only
	pOrgInput = strdup(sInput);
	parsepoint = pOrgInput;
	iTokenIndex = 0;
	Prepare();
}

Tokenizer::~Tokenizer()
{
	free((void *)operators);
	free((void *)pOrgInput);
	tokens.clear();
}

bool Tokenizer::IsOperator(char input)
{
	if (strchr(operators,input)) return true;
//	if (Tokenizer::Case((const char*)input, operators) != -1) return true;
	return false;
}

char *Tokenizer::GetNextToken(char *dst, int nMax, char **input)
{
	if (**input == '\0') return NULL;
	while((isspace(**input)) && (**input != '\0')) (*input)++;
	if (**input == '\0') return NULL;	// only trailing space

	int i=0;
	if (IsOperator(**input))
	{
		dst[i++] = **input;
		(*input)++;
	} else
	{
		while(!isspace(**input) && !IsOperator(**input) && (**input!='\0'))
		{
			dst[i++] = **input;
			(*input)++;
		}
	}
	dst[i]='\0';
	return dst;
}

void Tokenizer::Prepare()
{
	char tmp[256];
	while(GetNextToken(tmp,256,&parsepoint))
	{
		tokens.push_back(std::string(tmp));
	}
}

bool Tokenizer::HasMore()
{
	return (iTokenIndex < tokens.size());

}

const char *Tokenizer::Next()
{
	if (iTokenIndex > tokens.size()) return NULL;
	return tokens[iTokenIndex++].c_str();
}

const char *Tokenizer::Previous()
{
	if (iTokenIndex > 0) return tokens[--iTokenIndex].c_str();
	return NULL;
}

const char *Tokenizer::Peek()
{
	if (iTokenIndex < tokens.size()) return tokens[iTokenIndex].c_str();
	return NULL;
}

int Tokenizer::Case(const char *sValue, const char *sInput)
{
	Tokenizer tokens(sInput);// = new Tokenizer(sInput);

	int idx = 0;
	while (tokens.HasMore())
	{
		if (!strcmp(sValue,tokens.Next())) return idx;
		idx++;
	}
	return -1;
}
