//
//  main.c
//  EmojicodeCompiler
//
//  Created by Theo Weidmann on 27.02.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "CodeGenerator.hpp"
#include "Writer.hpp"
#include "Class.hpp"
#include "EmojicodeCompiler.hpp"
#include "CompilerErrorException.hpp"
#include "PackageReporter.hpp"
#include "ValueType.hpp"
#include "Function.hpp"
#include "../utf8.h"

std::string EmojicodeString::utf8() const {
    // Size needed for UTF8 representation
    size_t ds = u8_codingsize(c_str(), size());
    char utf8str[ds + 1];
    size_t written = u8_toutf8(utf8str, ds, c_str(), size());
    utf8str[written] = 0;
    return std::string(utf8str);
}

static bool outputJSON = false;
static bool printedErrorOrWarning = false;
static bool hasError = false;
std::string packageDirectory = defaultPackagesDirectory;

void printJSONStringToFile(const char *string, FILE *f) {
    char c;
    fputc('"', f);
    while ((c = *(string++))) {
        switch (c) {
            case '\\':
                fprintf(f, "\\\\");
                break;
            case '"':
                fprintf(f, "\\\"");
                break;
            case '/':
                fprintf(f, "\\/");
                break;
            case '\b':
                fprintf(f, "\\b");
                break;
            case '\f':
                fprintf(f, "\\f");
                break;
            case '\n':
                fprintf(f, "\\n");
                break;
            case '\r':
                fprintf(f, "\\r");
                break;
            case '\t':
                fprintf(f, "\\t");
                break;
            default:
                fputc(c, f);
        }
    }
    fputc('"', f);
}

//MARK: Warnings

void printError(const CompilerErrorException &ce) {
    hasError = true;
    if (outputJSON) {
        fprintf(stderr, "%s{\"type\": \"error\", \"line\": %zu, \"character\": %zu, \"file\":",
                printedErrorOrWarning ? ",": "", ce.position().line, ce.position().character);
        printJSONStringToFile(ce.position().file.c_str(), stderr);
        fprintf(stderr, ", \"message\":");
        printJSONStringToFile(ce.error(), stderr);
        fprintf(stderr, "}\n");
    }
    else {
        fprintf(stderr, "🚨 line %zu column %zu %s: %s\n", ce.position().line, ce.position().character,
                ce.position().file.c_str(), ce.error());
    }
    printedErrorOrWarning = true;
}

void compilerWarning(SourcePosition p, const char *err, ...) {
    va_list list;
    va_start(list, err);
    
    char error[450];
    vsprintf(error, err, list);
    
    if (outputJSON) {
        fprintf(stderr, "%s{\"type\": \"warning\", \"line\": %zu, \"character\": %zu, \"file\":",
                printedErrorOrWarning ? ",": "", p.line, p.character);
        printJSONStringToFile(p.file.c_str(), stderr);
        fprintf(stderr, ", \"message\":");
        printJSONStringToFile(error, stderr);
        fprintf(stderr, "}\n");
    }
    else {
        fprintf(stderr, "⚠️ line %zu col %zu %s: %s\n", p.line, p.character, p.file.c_str(), error);
    }
    printedErrorOrWarning = true;
    
    va_end(list);
}

Class* getStandardClass(EmojicodeString name, Package *_, SourcePosition errorPosition) {
    Type type = Type::nothingness();
    _->fetchRawType(name, globalNamespace, false, errorPosition, &type);
    if (type.type() != TypeContent::Class) {
        throw CompilerErrorException(errorPosition, "s package class %s is missing.", name.utf8().c_str());
        
    }
    return type.eclass();
}

Protocol* getStandardProtocol(EmojicodeString name, Package *_, SourcePosition errorPosition) {
    Type type = Type::nothingness();
    _->fetchRawType(name, globalNamespace, false, errorPosition, &type);
    if (type.type() != TypeContent::Protocol) {
        throw CompilerErrorException(errorPosition, "s package protocol %s is missing.", name.utf8().c_str());
    }
    return type.protocol();
}

ValueType* getStandardValueType(EmojicodeString name, Package *_, SourcePosition errorPosition) {
    Type type = Type::nothingness();
    _->fetchRawType(name, globalNamespace, false, errorPosition, &type);
    if (type.type() != TypeContent::ValueType) {
        throw CompilerErrorException(errorPosition, "s package value type %s is missing.", name.utf8().c_str());
    }
    return type.valueType();
}

void loadStandard(Package *_, SourcePosition errorPosition) {
    auto package = _->loadPackage("s", globalNamespace, errorPosition);
    
    VT_DOUBLE = getStandardValueType(EmojicodeString(E_ROCKET), _, errorPosition);
    VT_BOOLEAN = getStandardValueType(EmojicodeString(E_OK_HAND_SIGN), _, errorPosition);
    VT_SYMBOL = getStandardValueType(EmojicodeString(E_INPUT_SYMBOL_FOR_SYMBOLS), _, errorPosition);
    VT_INTEGER = getStandardValueType(EmojicodeString(E_STEAM_LOCOMOTIVE), _, errorPosition);
    
    CL_STRING = getStandardClass(EmojicodeString(0x1F521), _, errorPosition);
    CL_LIST = getStandardClass(EmojicodeString(0x1F368), _, errorPosition);
    CL_ERROR = getStandardClass(EmojicodeString(0x1F6A8), _, errorPosition);
    CL_DATA = getStandardClass(EmojicodeString(0x1F4C7), _, errorPosition);
    CL_DICTIONARY = getStandardClass(EmojicodeString(0x1F36F), _, errorPosition);
    
    PR_ENUMERATOR = getStandardProtocol(EmojicodeString(0x1F361), _, errorPosition);
    PR_ENUMERATEABLE = getStandardProtocol(EmojicodeString(E_CLOCKWISE_RIGHTWARDS_AND_LEFTWARDS_OPEN_CIRCLE_ARROWS_WITH_CIRCLED_ONE_OVERLAY), _, errorPosition);
    
    package->setRequiresBinary(false);
}

int main(int argc, char * argv[]) {
    try {
        const char *packageToReport = nullptr;
        char *outPath = nullptr;
        
        const char *ppath;
        if ((ppath = getenv("EMOJICODE_PACKAGES_PATH"))) {
            packageDirectory = ppath;
        }
        
        signed char ch;
        while ((ch = getopt(argc, argv, "vrjR:o:")) != -1) {
            switch (ch) {
                case 'v':
                    puts("Emojicode 0.3. Built with 💚 by Theo Weidmann.");
                    return 0;
                case 'R':
                    packageToReport = optarg;
                    break;
                case 'r':
                    packageToReport = "_";
                    break;
                case 'o':
                    outPath = optarg;
                    break;
                case 'j':
                    outputJSON = true;
                    break;
                default:
                    break;
            }
        }
        argc -= optind;
        argv += optind;
        
        if (outputJSON) {
            fprintf(stderr, "[");
        }
        
        if (argc == 0) {
            compilerWarning(SourcePosition(0, 0, ""), "No input files provided.");
            return 1;
        }
        
        if (outPath == nullptr) {
            outPath = strdup(argv[0]);
            outPath[strlen(outPath) - 1] = 'b';
        }
        
        auto errorPosition = SourcePosition(0, 0, argv[0]);
        
        Package pkg = Package("_", errorPosition);
        pkg.setPackageVersion(PackageVersion(1, 0));
        
        FILE *out = fopen(outPath, "wb");
    
        try {
            loadStandard(&pkg, errorPosition);
            
            pkg.parse(argv[0]);
            
            if (!out || ferror(out)) {
                throw CompilerErrorException(errorPosition, "Couldn't write output file.");
                return 1;
            }
            
            if (!Function::foundStart) {
                throw CompilerErrorException(errorPosition, "No 🏁 block was found.");
            }
            
            Writer writer = Writer(out);
            generateCode(writer);
        }
        catch (CompilerErrorException &ce) {
            printError(ce);
        }
        
        fclose(out);
        
        if (packageToReport) {
            if (auto package = Package::findPackage(packageToReport)) {
                reportPackage(package);
            }
            else {
                compilerWarning(errorPosition, "Report for package %s failed as it was not loaded.", packageToReport);
            }
        }
        
        if (outputJSON) {
            fprintf(stderr, "]");
        }
        
        if (hasError) {
            unlink(outPath);
            return 1;
        }
    }
    catch (std::exception &ex) {
        printf("💣 The compiler crashed due to an internal problem: %s\nPlease report this message and the code that you were trying to compile as an issue on GitHub.", ex.what());
        return 1;
    }
    catch (...) {
        printf("💣 The compiler crashed due to an unidentifiable internal problem.\nPlease report this message and the code that you were trying to compile as an issue on GitHub.");
        return 1;
    }
    
    return 0;
}
