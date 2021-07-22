// DO NOT USE THIS TOOL ON CODE THAT YOU HAVE NOT WRITTEN,
// IT HAS NOT BEEN *THOROUGHLY* SECURITY-VETTED THEREFORE
// IT SHOULD BE ASSUMED THAT THIS IS NOT SECURITY-SAFE TO
// USE WITH UNTRUSTED CODE.

#ifdef VERSION
// This way developers are able
// to provide version increments
// without having to modify src
// code.
#define g_version VERSION
#else
#define g_version "0.0.0"
#endif

#include <regex>
#include <iostream>
#include <string>
#include <vector>
#include <map>
// Standalone copy of POSIX's dirent from Github ( https://raw.githubusercontent.com/tronkko/dirent/master/include/dirent.h )
// This is used for enumerating directories' children as the only ways to do that (afaik)
// are FindFirstFile/FindNextFile (Windows' API), std::filesystem::directory_iterator/
// std::filesystem::directory_entry (C++17 or something), and dirent (opendir, closedir, readdir,
// etc) and the only one that I could port to C++98 was dirent as its implementations
// are open-source.
#include "dirent.h"

struct ENUMSTRUCT {
	std::string name; // e.g 'enum alpha {' becomes 'alpha'
	//        object_name,{old_value, new_value}
	std::map< std::string, std::pair< int, int > > values;
	void obfuscate( );
	const std::string recreate( );
};


struct MODIFIEDFILE {
	std::string path; // The path of the current file.
	std::vector<ENUMSTRUCT> enums; // Each enum that was obfuscated.
};

struct ACCEPTUM {
	time_t start; // Obfuscation started (since epoch)
	time_t finish; // Obfuscation concluded (since epoch)
	std::vector<MODIFIEDFILE> files; // Each file that was obfuscated
	const bool write( FILE* filehandle );
};

const std::string ENUMSTRUCT::recreate( ) {
	std::string strRecreatedValues( "\n" );
	for ( std::pair<std::string, std::pair<int, int>> iValue : this->values ) {
		strRecreatedValues += "\t" + iValue.first + " = " + std::to_string( iValue.second.second ) + ",\n";
	}
	// Remove the last comma. (Most compilers would be fine even if we left it in.)
	strRecreatedValues.erase( strRecreatedValues.size( ) - 2, 1 );

	// I used a sprintf instead of the below
	// 'sticking strings together' technique
	// but I found it hard to calculate exactly
	// how many bytes would be needed for the
	// sprintf buffer so instead reverted to
	// this simpler method.
	return std::string("enum ") + this->name +
		" {"  + strRecreatedValues + "};";
}

void ENUMSTRUCT::obfuscate( ) {
	// Generate a new random value for each enum object.
	// O(n)
	for ( std::pair<const std::string, std::pair<int, int>>& iMapVal : this->values ) {
		const int nNewVal = rand( ) % INT_MAX;
		iMapVal.second.second = iMapVal.second.first > 0 ? nNewVal : (nNewVal * -1) ;
	}
	// Now, we need to make sure that any enum values with
	// identical values get the same new 'random' values.
	// Time: O(n²).

	// TODO: I think iMapValA only needs to cover half of
	// the list while iMapValB covers the other half, give
	// it a try later.
	for ( std::pair<const std::string, std::pair<int, int>>& iMapValA : this->values ) {
		const int nOldValueA = iMapValA.second.first;
		for ( std::pair<const std::string, std::pair<int, int>>& iMapValB : this->values ) {
			const int nOldValueB = iMapValB.second.first;
			if ( nOldValueA == nOldValueB ) {
				// These need to have the same 'after'
				// value(s).
				iMapValB.second.second = iMapValA.second.second; // Take iMapValA's 'after' value.
			}
		}
	}
}

const bool ACCEPTUM::write( FILE* filehandle ) {
	// Instead of wasting ram and serializing
	// the whole structure before writing anything
	// to disk, why not serialize smaller chunks
	// (MODIFIEDFILEs) and write them to disk?
	// This way, it would take one extremely large
	// file to cause resource exhaustion whereas
	// it would otherwise have taken lots of
	// relatively large files to cause the same impact.
	if ( filehandle == nullptr ) {
		return false;
	}

	// Timestamps & metadata (version info, compile time, os, etc).
	fprintf( filehandle, "===============ACCEPTUM===============\n" );
	fprintf( filehandle, "Version: %s\n", g_version );
	fprintf( filehandle, "Compiled: %s\n", __DATE__ );
	fprintf( filehandle, "Processing (start): %s\n", ctime( &this->start ) );
	fprintf( filehandle, "Processing  (end) : %s\n", ctime( &this->finish ) );

	for ( MODIFIEDFILE& iObfuscatedFile : this->files ) {
		fprintf( filehandle, "\n\t| %s |\n", iObfuscatedFile.path.c_str( ) );
		for ( ENUMSTRUCT& iEnumObj : iObfuscatedFile.enums ) {
			fprintf( filehandle, "\t\tenum('%s'): \n", iEnumObj.name.c_str( ) );
			unsigned int i = 0;
			for ( std::pair<const std::string, std::pair<int, int>>& iEnumObjVal : iEnumObj.values  ) {
				fprintf( filehandle, "\t\t\tenum('%s')->items[%d]->name: \"%s\"\n", iEnumObj.name.c_str( ),
					i, iEnumObjVal.first.c_str( ) );
				fprintf( filehandle, "\t\t\tenum('%s')->items[%d]->old_value: %d\n", iEnumObj.name.c_str( ),
					i, iEnumObjVal.second.first );
				fprintf( filehandle, "\t\t\tenum('%s')->items[%d]->new_value: %d\n\n", iEnumObj.name.c_str( ),
					i, iEnumObjVal.second.second );
				i++;
			}
			fprintf( filehandle, "\n" );
		}
	}

	// Serialized files & enum objects.

	return true;
}

enum USEMODES {
	// This is the default usage of
	// the project, it iterates over
	// the files under a given path
	// and obfuscates enums.
	// Argument: None.
	MAIN = 0,
	// This is used to restore the
	// '.bkup' files after MAIN has
	// finished executing and the
	// relevant source code has
	// been compiled.
	// Argument: -r / --restore
	RESTORE
};

const std::string replaceall( std::string str, const std::string& replace, const std::string& replacewith ) {
	size_t szIndex = 0x0;
	while ( ( szIndex = str.find( replace ) ) != std::string::npos ) {
		str.replace( szIndex, replace.size( ), replacewith );
	}
	return str;
}

// I believe that C++20 has this built into the std::string member. (std::string::ends_with)
#define STRING_ENDS_WITH(a, b) ( strlen(a) < strlen(b) ) ? false : ( strcmp( a + ( strlen( a ) - strlen( b ) ), b ) == 0 )

const bool backupfile( const char* path, std::string& newpath ) {
	newpath = std::string( path ) + ".bkup";

	const int nRenameSuccess = rename( path, newpath.c_str( ) );
	if ( nRenameSuccess != 0 ) {
		return false;
	}
	
	return true;
}

const bool obfuscatefile( const char* path, MODIFIEDFILE* trackingout ) {
	// trackingout could be a reference but it wouldn't be (easily)
	// possible for developers/users to provide it with a nullptr
	// and disregard it.

	std::string strBackedUpPath;
	if ( !backupfile( path, strBackedUpPath ) ) {
		return false;
	}

	FILE* fHandle = fopen( strBackedUpPath.c_str( ), "r" );
	if ( !fHandle ) {
		printf( "Unable to open handle to backed up file: '%s'.\n", strBackedUpPath.c_str( ) );
		return false;
	}

	FILE* ofHandle = fopen( path, "w" );

	if ( trackingout != nullptr ) {
		trackingout->path = std::string( path );
	}

	// Regex for the whole ENUM:
	// enum[\s\t]{1,}[a-zA-Z0-9]{1,}[\s\t\n]{0,}\{[\s\t\n]{0,}[a-zA-Z0-9,\s\t=+\-]{0,}\}[;]?
	// enum [NAME] { a = 0, b = 1 }
	// Regex for the first line of an enum:
	// enum[\s\t]{1,}[a-zA-Z0-9]{1,}
	// enum [NAME]

	const std::regex rgFirstLine( R"(enum[\s\t]{1,}[a-zA-Z0-9]{1,})" );
	const std::regex rgWholeEnum( R"(enum[\s\t]{1,}[a-zA-Z0-9]{1,}[\s\t\n]{0,}\{[\s\t\n]{0,}[a-zA-Z0-9,\s\t=+\-]{0,}\}[;]?)" );

	while ( !feof( fHandle ) ) {
		// Read a line into the buffer.
		std::string sCurrentLine;
		if ( ftell( fHandle ) == std::string::npos ) {
			printf( "[Error] IO error.\n" );
		}
		for ( char c = fgetc( fHandle ); (int)c != -1 && c != '\n'; c = fgetc( fHandle ) ) {
			sCurrentLine += c;
		}
		std::smatch smMatch;
		if ( std::regex_search( sCurrentLine, smMatch, rgFirstLine ) ) {
			// This is the first line of an enum.
			// We need to convert the whole multi-line enum into sCurrentLine.
			for ( char c = fgetc(fHandle); (int)c != -1 && c != '}'; c = fgetc( fHandle ) ) {
				// This loop essentially reads the enum into sCurrentLine while ignoring newline
				// characters.
				if ( c != '\n' ) {
					sCurrentLine += c;
				}
			}
			sCurrentLine += "};";
		}
		if ( std::regex_search( sCurrentLine, smMatch, rgWholeEnum ) ) {
			// sCurrentLine holds the entire enum.
			ENUMSTRUCT esThisEnum;
			// sModificationBuf should be something like this:
			// "enum name { one = 1, two = 2, three = 3, four = 4, five = 5 };"
			std::string sModificationBuf = sCurrentLine.substr( smMatch.position( ), sCurrentLine.size( ) - smMatch.position( ) );
			// Calculating the szMaxEnumLength does not account for a space between the
			// name and the opening curly bracket, thus assuming that there will be one
			// so that; even if there isn't, there is more than enough memory allocated.
			const size_t szMaxEnumNameLength = sModificationBuf.find_first_of( '{' ) -
				sModificationBuf.find_first_of( ' ' ) + 1;
			char* strEnumName = (char*)calloc( sModificationBuf.size( ), sizeof( char ) );
			if ( strEnumName == nullptr ) {
				// We've probably run out of memory, malloc( ) returned nullptr/failed.
				printf( "[Error] Enum too large to allocate memory for.\n" );
				// TODO: Provide users with an option to disregard this error and
				// allocate a fixed-size buffer for all enum names if they are confident
				// that it won't fail/drain resources.
				fclose( ofHandle );
				fclose( fHandle );
				free( strEnumName );
				return false;
			}
			if ( !sscanf( sModificationBuf.c_str( ), "enum %s{", strEnumName ) ) {
				printf( "[Error] Invalid enum.\n" );
				fclose( ofHandle );
				fclose( fHandle );
				free( strEnumName );
				return false;
			}
			esThisEnum.name = std::string( strEnumName );
			free( strEnumName );
			// Replace all commas within curly brackets with newlines:
			// enum alpha { val1 = 0
			//  val2 = 1
			//  val3 = 2 };
			sModificationBuf = replaceall( sCurrentLine, ",", "\n" );

			// Strip all code before/after curly brackets, leaving us with the enum:
			//  val1 = 0 [ 1 ]
			//  val2 = 1 [ 2 ]
			//  val3 = 2 [ 3 ]
			size_t szFirstCurly = 0x0;
			if ( ( szFirstCurly = sModificationBuf.find( "{" ) ) == std::string::npos ) {
				printf( "[Error] Invalid enum.\n" );
				fclose( ofHandle );
				fclose( fHandle );
				return false;
			}
			size_t szLastCurly = 0x0;
			if ( ( szLastCurly = sModificationBuf.find( "}" ) ) == std::string::npos ) {
				printf( "[Error] Invalid enum.\n" );
				fclose( ofHandle );
				fclose( fHandle );
				return false;
			}
			sModificationBuf = sModificationBuf.substr( szFirstCurly + 1, szLastCurly - szFirstCurly - 1 );

			// Iterate over all enum values
			char* iterator = strtok( (char*)sModificationBuf.c_str( ), "\n" );
			int liLastEnumVal = 0;
			while ( iterator != nullptr ) {
				std::string iEnumValue = std::string( iterator );
				// Iterator holds the value of an enum segment, e.g:
				// alpha = 0
				// Remove all non-printable characters:
				for ( unsigned short iCharIndex = 0; iCharIndex < iEnumValue.size( ); iCharIndex++ ) {
					const char iChar = iEnumValue[ iCharIndex ];
					if ( !isprint( iChar ) || iChar == ' ' || iChar == '\t' || iChar == '\n' ) {
						iEnumValue.erase( iCharIndex, 1 );
					}
				}

				// Get value & name of enum value:
				int liEnumVal;
				std::string strEnumValName;
				// alpha=0
				size_t szEqualSymbolIndex = 0x0;
				if ( (szEqualSymbolIndex = iEnumValue.find( "=" ) ) == std::string::npos ) {
					// This enum's value is the last enum's value incremented by one
					// (no explicitly declared value)
					liEnumVal = liLastEnumVal + 1;
				}
				else {
					liEnumVal = std::stoi( iEnumValue.substr( szEqualSymbolIndex + 1, iEnumValue.size( ) - 1 - szEqualSymbolIndex ) );
				}
				strEnumValName = iEnumValue.substr( 0, szEqualSymbolIndex );
				liLastEnumVal = liEnumVal;

				esThisEnum.values.insert( 
					std::pair<std::string, std::pair<int, int>>( strEnumValName,
						std::pair<int, int>(liEnumVal, -1)
						) );

				iterator = strtok( NULL, "\n" );
			}
			esThisEnum.obfuscate( );
			sCurrentLine = esThisEnum.recreate( );
			if ( trackingout != nullptr ) {
				trackingout->enums.push_back( esThisEnum );
			}
		}

		fprintf( ofHandle, "%s\n", sCurrentLine.c_str( ) );

		continue;
		quitloop:
		break;
	}
	fclose( ofHandle );
	fclose( fHandle );

	return true;
}

const bool obfuscatefiles( const std::vector<std::string>& paths, ACCEPTUM* trackingout ) {
	if ( trackingout != nullptr ) {
		time(&trackingout->start);
	}
	for ( const std::string& strIterativePath : paths ) {
		MODIFIEDFILE mfFileStatus;
		if ( !obfuscatefile( strIterativePath.c_str( ), &mfFileStatus ) ) {
			return false;
		}
		if ( trackingout != nullptr ) {
			trackingout->files.push_back( mfFileStatus );
		}
	}
	if ( trackingout != nullptr ) {
		time(&trackingout->finish);
	}
	return true;
}

const bool collectfiles( const char* path, std::vector<std::string>& collectedpaths,
	const std::vector<const char*>& extensions ) {

	struct stat stFileInfo;
	if ( stat( path, &stFileInfo ) == -1 ) {
		return false; // Uh oh!
	}

	if ( S_ISREG( stFileInfo.st_mode ) ) {
		for ( const char* strIterativeExtension : extensions ) {
			if ( STRING_ENDS_WITH( path, strIterativeExtension ) ) {
				// The path provided to us will be freed after closing
				// the directory so we create a copy before it is freed
				// and then free the copy when we are finished with it.
				char* strPathBuffer = (char*)calloc( strlen( path ) + 1, sizeof( char ) );
				memcpy( strPathBuffer, path, strlen( path ) );
				collectedpaths.push_back( strPathBuffer );
				break;
			}
		}
	}
	else if ( S_ISDIR( stFileInfo.st_mode ) ) {
		// This is a recursive function that calls itself when it encounters a
		// directory until we hit the bottom of a given 'directory-tree'.
		DIR* lpDirectoryStream = opendir( path );
		if ( lpDirectoryStream == nullptr ) {
			return false;
		}

		struct dirent* lpDirectoryEntry = readdir( lpDirectoryStream );
		while ( lpDirectoryEntry != nullptr ) {
			// dirent is a POSIX-native concept so it provides
			// entries like '.', we don't want these.
			if ( strcmp( lpDirectoryEntry->d_name, "." ) == 0 ||
				 strcmp( lpDirectoryEntry->d_name, ".." ) == 0 ) {
				// Skip this iteration.
				lpDirectoryEntry = readdir( lpDirectoryStream );
				continue;
			}

			if ( !collectfiles( std::string( path + std::string( "/" ) + lpDirectoryEntry->d_name ).c_str( ),
				collectedpaths, extensions ) ) {
				
				closedir( lpDirectoryStream );
				return false;
			}
			lpDirectoryEntry = readdir( lpDirectoryStream );
		}
		closedir( lpDirectoryStream );
	}
	else {
		return true; // Probably something weird like a symlink.
	}

	return true;
}

const bool restorefiles( const std::vector<std::string>& paths ) {
	for ( const std::string& strIterativePath : paths ) {
		if ( !STRING_ENDS_WITH( strIterativePath.c_str( ), ".bkup" ) ) {
			// This code should literally never be hit but
			// sanity checks are never wasted!
			return false;
		}

		const std::string strOriginalPath = strIterativePath.substr( 0,
			strIterativePath.size( ) - 5/* strlen( ".bkup" ) */ );

		// This may fail or succeed
		// but we shouldn't care
		// either way as it guarantees
		// that there is no file
		// at location 'x'.
		unlink( strOriginalPath.c_str( ) );

		if ( rename( strIterativePath.c_str( ), strOriginalPath.c_str( ) ) != 0 ) {
			return false;
		}
	}


	return true;
}

int main( unsigned int argc, char* argv[] ) {
	std::string strTargetPath( "" );
	USEMODES UMode = USEMODES::MAIN;

	// Parse arguments:

	for ( unsigned int nArgumentIndex = 0; nArgumentIndex < argc; nArgumentIndex++ ) {
		if ( strcmp( argv[ nArgumentIndex ], "-f" ) == 0 ||
			 strcmp( argv[ nArgumentIndex ], "--file" ) == 0 ) {
			if ( nArgumentIndex == argc - 1 ) {
				printf( "[Error] Expected an additional parameter to be passed with '%s'.\n", argv[ nArgumentIndex ] );
				return -1;
			}
			strTargetPath = std::string( argv[ nArgumentIndex + 1 ] );
			continue;
		}
		if ( strcmp( argv[ nArgumentIndex ], "-r" ) == 0 || 
				  strcmp( argv[ nArgumentIndex ], "--restore" ) == 0 ) {
			
			UMode = USEMODES::RESTORE;
			continue;
		}
	}

	std::vector<std::string> vecApplicableFiles;

	// Execute primary functionality:
	switch ( UMode ) {
		case USEMODES::MAIN: {
			srand( time( NULL ) ); // TODO: Change seed source.
			if ( !collectfiles( strTargetPath.c_str( ),
				  vecApplicableFiles, { ".cpp", ".hpp", ".h", ".c", ".cs" } ) ) {
				
				printf( "[Error] Unable to gather the files that should be acted upon.\n" );
				return -1;
			}
			ACCEPTUM acConfirmation;
			if ( !obfuscatefiles( vecApplicableFiles, &acConfirmation ) ) {
				printf( "[Error] Unable to obfuscate all files.\n" );
				return -1;
			}
			// Write the confirmation to a new file.
			// (If it already exists, I'm OK with overwriting
			// it but maybe TODO: give users an option to check
			// if one already exists.)
			const char* strAcceptumPath = "Acceptum.acc";
			FILE* fConfirmationFile = fopen( strAcceptumPath, "w" );
			if ( fConfirmationFile == nullptr ) {
				printf( "[Error] Unable to open acceptum file for writing ('%s').\n", strAcceptumPath );
				return -1;
			}
			if ( !acConfirmation.write( fConfirmationFile ) ) {
				fclose( fConfirmationFile );
				printf( "[Error] Unable to write acceptum to '%s'.\n", strAcceptumPath );
				return -1;
			}
			break;
		}

		case USEMODES::RESTORE: {
			if ( !collectfiles( strTargetPath.c_str( ),
				  vecApplicableFiles, { ".bkup" } ) ) {
				printf( "[Error] Unable to gather the files that should be acted upon.\n" );
				return -1;
			}
			if ( !restorefiles( vecApplicableFiles ) ) {
				printf( "[Error] Unable to restore all files.\n" );
				return -1;
			}
			break;
		}
		default: {
			printf( "[Error] Invalid use-mode.\n" );
			return -1;
			break;
		}
	}

	return 1;
}