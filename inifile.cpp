#include <string>
#include <map>

#include "logger.h"
#include "tokenizer.h"
#include "inifile.h"

using namespace gnilk::inifile;

class IniFileParser {
public:
	static void FromBuffer(SectionContainer *container, std::string data);
};

// Note: this is just a hack!
void IniFileParser::FromBuffer(SectionContainer *container, std::string data) {
	//SectionContainer *container = new SectionContainer();
	const char *pdata = data.c_str();
	char *pc = (char *)&pdata[0];
	int tc = 0;
	char token[256];	// MAX TOKEN
	std::string sectionName;
	std::string valueName;
	std::string value;
	int state = 0;

	typedef enum {
		kParseState_ExpectSectionStart,
		kParseState_SectionName,
		kParseState_ExpectSectionOrNameStart,
		kParseState_ValueName,
		kParseState_Value,
	} kParseState;

	state = kParseState_ExpectSectionStart;

	while(*pc != '\0') {
		switch(state) {
			case kParseState_ExpectSectionStart :	// expect section start
				if (*pc == '[') {
					state = kParseState_SectionName;
					tc = 0;
				}
				break;
			case kParseState_SectionName :	// parse section name
				if (*pc == ']') {
					state = kParseState_ExpectSectionOrNameStart;
					token[tc] = '\0';
					sectionName = std::string(token);
					tc = 0;
				} else {
					token[tc++] = *pc;
				}
				break;
			case kParseState_ExpectSectionOrNameStart :  // drop white space after section name
				if (*pc == '[') {
					state = kParseState_SectionName;
					tc = 0;
				} else if (!isspace(*pc)) {
					state = kParseState_ValueName;
					token[tc++] = *pc;
				}
				break;
			case kParseState_ValueName : // parse value name
				if (*pc == '=') {
					state = kParseState_Value;
					token[tc] = '\0';
					valueName = std::string(token);
					tc = 0;
				} 	else {
					token[tc++] = *pc;					
				}
				break;
			case kParseState_Value : // parse value
				if (*pc == '\n') {
					state = kParseState_ExpectSectionOrNameStart;
					token[tc] = '\0';
					value = std::string(token);
					tc = 0;

					container->SetValue(sectionName, valueName, value);
					// Logger::GetLogger("IniFileParser")->Debug("SetValue(%s,%s,%s)",
					// 	sectionName.c_str(),
					// 	valueName.c_str(),
					// 	value.c_str());

				} else {
					token[tc++] = *pc;
				}
				break;
 		}
		pc++;
	}
	if (state == kParseState_Value) {
		container->SetValue(sectionName, valueName, value);
		// Logger::GetLogger("IniFileParser")->Debug("SetValue(%s,%s,%s)",
		// 	sectionName.c_str(),
		// 	valueName.c_str(),
		// 	value.c_str());		
	}

}

// --> section container
SectionContainer::SectionContainer() {

}
SectionContainer::~SectionContainer() {

}

void SectionContainer::FromBuffer(std::string buffer) {
	return IniFileParser::FromBuffer(this, buffer);
}
void SectionContainer::SetValue(std::string section, std::string name, std::string value) {
	auto it = sections.find(section);
	if (it == sections.end()) {
		Section *s = new Section(section);
		s->SetValue(name, value);
		sections.insert(std::make_pair(section, s));
	} else {
		it->second->SetValue(name, value);
	}
}
bool SectionContainer::HasValue(std::string section, std::string name) {
	auto it = sections.find(section);
	if (it == sections.end()) {
		return false;
	}
	return it->second->HasValue(name);
}

std::string SectionContainer::GetValue(std::string section, std::string name, std::string defaultValue) {
	auto it = sections.find(section);
	if (it == sections.end()) {
		return defaultValue;
	}
	return it->second->GetValue(name, defaultValue);

}

bool SectionContainer::DeleteValue(std::string section, std::string name) {
	Section *s = GetSection(section);
	if (s == NULL) return false;
	return s->DeleteValue(name);
}

bool SectionContainer::DeleteSection(std::string section) {
	auto it = sections.find(section);
	if (it == sections.end()) {
		return false;
	}
	Section *s = it->second;
	sections.erase(it);
	delete s;
	return true;
}

Section *SectionContainer::GetSection(std::string section) {
	auto it = sections.find(section);
	if (it == sections.end()) {
		return NULL;
	}
	return it->second;
}

void SectionContainer::Dump(std::string &out) {
	for(auto it=sections.begin(); it != sections.end(); it++) {
		it->second->Dump(out);
	}
}


// --> section
Section::Section(std::string name) {
	this->name = name;
}

Section::~Section() {

}

void Section::SetValue(std::string name, std::string value) {
	auto it = values.find(name);
	if (it == values.end()) {
		values.insert(std::make_pair(name, value));
	} else {
		// replace
		it->second = value;		
	}
}

bool Section::HasValue(std::string name) {
	auto it = values.find(name);
	if (it == values.end()) {
		return false;
	}
	return true;
}

std::string Section::GetValue(std::string name, std::string defaultValue) {
	auto it = values.find(name);
	if (it == values.end()) {
		return defaultValue;
	}
	return it->second;
}
bool Section::DeleteValue(std::string name) {
	auto it = values.find(name);
	if (it == values.end()) return false;
	values.erase(it);
	return true;
}

std::string Section::Name() {
	return name;
}

#define SEC_BEGIN (std::string("["))
#define SEC_END (std::string("]"))
#define VAL_SEP (std::string("="))

void Section::Dump(std::string &out) {
	out.append(SEC_BEGIN + name + SEC_END + std::string("\n"));
	for(auto it = values.begin(); it != values.end(); it++) {
		out.append(it->first + VAL_SEP + it->second);
	}
}
