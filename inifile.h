#pragma once
#include <map>
#define DEFAULT_PROP_SEPARATORS ((char*)("= [ ]"))


namespace gnilk {
	namespace inifile {

		class Section;

		class SectionContainer {
		public:
			SectionContainer();
			virtual ~SectionContainer();

			void FromBuffer(std::string buffer);
			void SetValue(std::string section, std::string name, std::string value);
			bool HasValue(std::string section, std::string name);
			std::string GetValue(std::string section, std::string name, std::string defaultValue);
			bool DeleteValue(std::string section, std::string name);
			bool DeleteSection(std::string section);
			void Dump(std::string &out);
		private:
			Section *GetSection(std::string section);
			std::map<std::string, Section *> sections;
		};

		class Section {
		public:
			Section(std::string name);
			virtual ~Section();

			void SetValue(std::string name, std::string value);
			bool HasValue(std::string name);
			std::string GetValue(std::string name, std::string defaultValue);
			bool DeleteValue(std::string name);
			std::string Name();
			void Dump(std::string &out);
		private:
			std::string name;
			std::map<std::string, std::string> values;
		};
	};
};