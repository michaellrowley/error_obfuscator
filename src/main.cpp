#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <regex>
#include <map>

// RTError -> Runtime-error
struct RTError {
	const std::string description;
	// TODO: Add error codes.
	RTError(const std::string& _description) : description(description) { };
};

class ModifiedEnum {
	const std::string originalEnum;
	std::string newEnum;
private:
	const int64_t randomNumber(const int64_t& _min, const int64_t& _max) {
		return (rand() % _max) + _min; // TODO: Migrate to a CSPRNG
	}
public:
	ModifiedEnum(const std::string& _originalEnum) : originalEnum(_originalEnum), newEnum(_originalEnum) {}
	const bool modify(std::ofstream* _obfuscateAccStream, std::ifstream* _restoreAccStream) {
		if (this->newEnum.length() < 2) {
			return false;
		}
		// TODO: Move this inside the main 'parsing loop'.
		size_t commentStart = 0;
		for (size_t i = 1; i < this->newEnum.length(); i++) {
			if (this->newEnum[i] == '*' && this->newEnum[i - 1] == '/') {
				commentStart = i;
			}
			else if (this->newEnum[i] == '/' && this->newEnum[i - 1] == '*') {
				this->newEnum.erase(commentStart - 1, (i - commentStart) + 2);
			}
		}
		const size_t start = newEnum.find('{');
		if (start == std::string::npos) {
			return false;
		}
		std::string curItem("");
		int64_t lastItemValue(-1);
		// The bounds for this loop should skip the first curly bracket
		for (size_t i = start + 1; i < newEnum.length(); i++) {
			if ((newEnum[i] == ',' || i == newEnum.length() - 1)) {
				bool isLastItem = newEnum[i] == '}';
				bool isCurItemEmpty = true;
				for (const char& c : curItem) {
					if (!std::isspace(c)) {
						isCurItemEmpty = false;
						break;
					}
				}
				if (isCurItemEmpty) {
					break;
				}
				// 'curItem' is the enum item.
				const size_t assignmentOffset = curItem.find('=');
				std::string newValueStr("");
				if (assignmentOffset == std::string::npos) {
					// The item hasn't been provided a value (explicitly, at least)
					// so we add to the value of the list item (default -> zero)
					if (_obfuscateAccStream != nullptr) {
						newValueStr = std::to_string(this->randomNumber(lastItemValue <= 0 ? 0 : 1, INT32_MAX) *
							(lastItemValue <= 0 ? -1 : 1));
						*_obfuscateAccStream << lastItemValue << std::endl;
					}
					// There's no way that this path will be hit during the restoration
					// process as the obfuscation-writer makes sure that all of the new
					// enum values have hardcoded values ('a = 10') so no _reloadAccStream
					// evaluation is needed here.
					if (isLastItem) {
						this->newEnum.insert(i - (isLastItem ? 1 : 2), "=" + newValueStr);
						break;
					}
					this->newEnum.replace(i - curItem.length(), curItem.length(), curItem + "=" + newValueStr);
					// this->newEnum.insert(i - (isLastItem ? 1 : 0), "=" + newValueStr);
					i += newValueStr.length() + 1;
				}
				else {
					const std::string oldValueStr = curItem.substr(assignmentOffset + 1,
							curItem.length() - assignmentOffset - 1);
					if (_obfuscateAccStream != nullptr) {
						try {
							lastItemValue = std::stoull(oldValueStr);
						} catch (std::exception) {
							// TODO: Notify user of the error.
						}
						*_obfuscateAccStream << oldValueStr << std::endl;
						newValueStr = std::to_string(this->randomNumber(1, INT64_MAX) *
						(lastItemValue <= 0 ? -1 : 1));
					}
					else if (_restoreAccStream != nullptr) {
						if (!*_restoreAccStream) {
							// TODO: Notify user of the error.
						}
						*_restoreAccStream >> newValueStr;
					}
					// Overwrite the old value with our new one.
					const size_t oldItemLength = curItem.length();
					curItem.replace(assignmentOffset + 1, curItem.length() - assignmentOffset - 1, newValueStr);
					this->newEnum.replace(i - oldItemLength, oldItemLength, curItem);
					i += curItem.length() - oldItemLength;
				}
				lastItemValue++;
				curItem = "";
				continue;
			}
			curItem += newEnum[i];
		}
		return true;
	}
	void reset() {
		this->newEnum = this->originalEnum;
	}
	operator std::string() const {
		return this->newEnum;
	}
};

const RTError* ModifyAllEnums(std::ifstream* _sourceStream,
					   std::ofstream* _outputStream,
					   const void* _acceptumStream,
					   const bool obfuscate) {
	if (_sourceStream == nullptr ||
		_outputStream == nullptr) {
		return new RTError("One or more of the provided arguments was invalid.");
	}
	std::string startBuf("");
	while (*_sourceStream) {
		startBuf += _sourceStream->get();
		std::smatch startMatch;
		const std::regex startRegex(R"(enum[\s]{0,}[\w]+[\s]{0,}\{$)");
		if (!std::regex_search(startBuf, startMatch, startRegex)) {
			if ((startBuf[0] & 0xFF) != 0xFF /* EOF */ &&
				startBuf.length() > 127) {
				*_outputStream << startBuf[0];
				startBuf.erase(0, 1);
			}
			continue;
		}
		for (size_t i = 0; i < startMatch.position(0); i++) {
			*_outputStream << startBuf[i];
		}
		startBuf.erase(0, startMatch.position(0));
		// Due to the logic of this function, the enum's start-chunk
		// will already have been written - only the body and closing bracket
		// needs writing, now.
		// Read until the closing-bracket
		while (*_sourceStream && startBuf[startBuf.length() - 1] != '}') {
			startBuf += _sourceStream->get();
		}
		ModifiedEnum newEnum(startBuf);
		startBuf.clear();
		newEnum.modify(obfuscate ? (std::ofstream*)_acceptumStream : nullptr,
			!obfuscate ? (std::ifstream*)_acceptumStream : nullptr);
		const std::string newEnumStr = newEnum;
		_outputStream->write(newEnumStr.c_str(), newEnumStr.length()); // Write new enum
	}
	_outputStream->write(startBuf.c_str(), startBuf.length() - 1 /* EOF */);
	_outputStream->flush();
	return nullptr;
}

const RTError* ObfuscatePath(const std::filesystem::path& _sourcePath,
					   const std::filesystem::path* _accPathPtr = nullptr) {
	const std::filesystem::path newSourcePath = (_sourcePath.string() + ".bkup");
	if (!std::filesystem::exists(_sourcePath) ||
		std::filesystem::exists(newSourcePath) ||
		(_accPathPtr != nullptr && (std::filesystem::exists(*_accPathPtr) &&
		!std::filesystem::is_empty(*_accPathPtr)))) {
		return new RTError("The file-creation state(s) of one or more"
						   "of the provided arguments was invalid.");
	}
	if (std::filesystem::is_directory(_sourcePath)) {
		const RTError* lastError = nullptr;
		for (auto& iterativePath : std::filesystem::directory_iterator(_sourcePath)) {
			if ((lastError = ObfuscatePath(iterativePath, _accPathPtr)) != nullptr) {
				return lastError;
			}
		}
		return nullptr;
	}
	else if (!std::filesystem::is_regular_file(_sourcePath)) {
		return new RTError("One or more of the created files was"
						   "of an invalid type.");
	}
	std::ofstream acceptumStream(_accPathPtr == nullptr ? std::filesystem::path("") : *_accPathPtr);
	std::filesystem::rename(_sourcePath, newSourcePath);
	std::ifstream sourceStream(newSourcePath);
	std::ofstream outputStream(_sourcePath.string());
	const RTError* lastError = nullptr;
	if ((lastError = ModifyAllEnums(&sourceStream, &outputStream,
		(void*)&acceptumStream, true)) != nullptr) {
		return lastError;
	}
	std::filesystem::remove(newSourcePath);
	return nullptr;
}

const RTError* RestorePath(const std::filesystem::path& _sourcePath,
						   const std::filesystem::path& _accPath) {
	std::filesystem::path backupPath = std::filesystem::path(_sourcePath.string() + ".bkup");
	if (!std::filesystem::exists(_accPath) ||
		std::filesystem::is_empty(_accPath) ||
		!std::filesystem::exists(_sourcePath)) {
		return new RTError("One or more of the provided parameters was invalid.");
	}
	if (std::filesystem::is_directory(_sourcePath)) {
		const RTError* lastError = nullptr;
		for (auto& iterativePath : std::filesystem::directory_iterator(_sourcePath)) {
			if ((lastError = RestorePath(iterativePath, _accPath)) != nullptr) {
				return lastError;
			}
		}
		return nullptr;
	}
	else if (!std::filesystem::is_regular_file(_sourcePath)) {
		return new RTError("One or more of the created files was"
						   "of an invalid type or did not exist.");
	}
	std::filesystem::rename(_sourcePath, backupPath);
	std::ifstream acceptumStream(_accPath);
	std::ifstream sourceStream(backupPath);
	std::ofstream outputStream(_sourcePath);
	const RTError* lastError = nullptr;
	if ((lastError = ModifyAllEnums(&sourceStream, &outputStream,
		(void*)&acceptumStream, false)) != nullptr) {
		return lastError;
	}
	// The modification call must have succeeded, now
	// we can overwrite the source file with its original.
	std::filesystem::remove(backupPath);
	return nullptr;
}

int main(const int _argc, const char* const * _argv) {
	srand(time(NULL)); // TODO: Migrate to a CSPRNG
	std::map<std::string, std::string> argMap;
	for (int i = 1; i < _argc; i++) { 
		std::pair<std::string, std::string> iterativePair;
		iterativePair.first = _argv[i];
		static const std::vector<std::string> flagArgs = {"-r", "--restore"};
		bool isFlag = false;
		for (size_t i = 0; i < flagArgs.size(); i++) {
			if (flagArgs[i] == iterativePair.first) {
				isFlag = true;
				break;
			}
		}
		if (!isFlag) {
			if (i == _argc - 1) {
				// TODO: ERROR LOG.
				return -1;
			}
			iterativePair.second = _argv[i + 1];
			i++;
		}
		argMap.insert(iterativePair);
	}
	std::map<std::string, std::string>::iterator restoreArgument = argMap.find("-r");
	if (restoreArgument == argMap.end()) {
		restoreArgument = argMap.find("--restore");
	}
	std::map<std::string, std::string>::iterator pathArgument = argMap.find("-p");
	if (pathArgument == argMap.end()) {
		pathArgument = argMap.find("--path");
	}
	std::map<std::string, std::string>::iterator acceptumArgument = argMap.find("-a");
	if (acceptumArgument == argMap.end()) {
		acceptumArgument = argMap.find("--acceptum");
	}
	if (acceptumArgument != argMap.end() && pathArgument != argMap.end()) {
		std::filesystem::path acceptumPath = std::filesystem::path(acceptumArgument->second);
		if (restoreArgument != argMap.end()) {
			RestorePath(pathArgument->second, acceptumPath);
			return 1;
		}
		ObfuscatePath(pathArgument->second, &acceptumPath);
	}
	return 1;
}
