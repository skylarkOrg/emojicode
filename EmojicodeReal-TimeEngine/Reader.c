    //
//  Reader.c
//  Emojicode
//
//  Created by Theo Weidmann on 01.03.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include "Emojicode.h"
#include <string.h>
#include <dlfcn.h>

#ifdef DEBUG
#define DEBUG_LOG(format, ...) printf(format "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

uint16_t readUInt16(FILE *in){
    return ((uint16_t)fgetc(in)) | (fgetc(in) << 8);
}

EmojicodeChar readEmojicodeChar(FILE *in){
    return ((EmojicodeChar)fgetc(in)) | (fgetc(in) << 8) | ((EmojicodeChar)fgetc(in) << 16) | ((EmojicodeChar)fgetc(in) << 24);
}

EmojicodeCoin readCoin(FILE *in){
    return ((EmojicodeCoin)fgetc(in)) | (fgetc(in) << 8) | ((EmojicodeCoin)fgetc(in) << 16) | ((EmojicodeCoin)fgetc(in) << 24);
}

PackageLoadingState packageLoad(const char *name, uint16_t major, uint16_t minor,
                                FunctionFunctionPointer **linkingTable, mpfc *mpfc, SizeForClassFunction *sfch){
    char *path;
    asprintf(&path, "%s/%s-v%d/%s.%s", packageDirectory, name, major, name, "so");
    
    void* package = dlopen(path, RTLD_LAZY);
    
    free(path);
    
    if(!package)
        return PACKAGE_LOADING_FAILED;

    *linkingTable = dlsym(package, "linkingTable");
    *mpfc = dlsym(package, "markerPointerForClass");
    *sfch = dlsym(package, "sizeForClass");
    
    PackageVersion(*pvf)() = dlsym(package, "getVersion");
    PackageVersion pv = pvf();
    
    if(!*linkingTable)
        return PACKAGE_LOADING_FAILED;
    
    if(pv.major != major)
        return PACKAGE_INAPPROPRIATE_MAJOR;

    if(pv.minor < minor)
        return PACKAGE_INAPPROPRIATE_MINOR;
    
    return PACKAGE_LOADED;
}

char* packageError(){
    return dlerror();
}

uint32_t readBlock(EmojicodeCoin **destination, int *variableCount, FILE *in){
    *variableCount = fgetc(in);
    uint32_t coinCount = readEmojicodeChar(in);

    *destination = malloc(sizeof(EmojicodeCoin) * coinCount);
    for (uint32_t i = 0; i < coinCount; i++) {
        (*destination)[i] = readCoin(in);
    }
    
    DEBUG_LOG("Read block with %d coins and %d local variable(s)", coinCount, *variableCount);
    
    return coinCount;
}

void readFunction(Function **table, FILE *in, FunctionFunctionPointer *linkingTable){
    uint16_t vti = readUInt16(in);
    
    Function *method = malloc(sizeof(Function));
    method->argumentCount = fgetc(in);
    
    DEBUG_LOG("Reading function with vti %d and takes %d argument(s)", vti, method->argumentCount);

    uint16_t native = readUInt16(in);
    if (native) {
        DEBUG_LOG("Function is native");
        method->native = true;
        method->handler = linkingTable[native];
    }
    else {
        method->native = false;
        method->tokenCount = readBlock(&method->tokenStream, &method->variableCount, in);
    }
    table[vti] = method;
}

void readProtocolAgreement(Function **vmt, Function ***pmt, uint_fast16_t offset, FILE *in){
    uint_fast16_t index = readUInt16(in) - offset;
    uint_fast16_t count = readUInt16(in);
    pmt[index] = malloc(sizeof(Function *) * count);
    DEBUG_LOG("Reading protocol %d agreement: %d method(s)", index + offset, count);
    for (uint_fast16_t i = 0; i < count; i++) {
        const uint_fast16_t vti = readUInt16(in);
        DEBUG_LOG("Protocol agreement method VTI %d", vti);
        pmt[index][i] = vmt[vti];
    }
}

void readPackage(FILE *in){
    static uint16_t classNextIndex = 0;
    
    FunctionFunctionPointer *linkingTable;
    mpfc mpfc;
    SizeForClassFunction sfch;
    
    uint_fast8_t packageNameLength = fgetc(in);
    if (!packageNameLength) {
        DEBUG_LOG("Package does not have native binary");
        linkingTable = sLinkingTable;
        mpfc = markerPointerForClass;
        sfch = sizeForClass;
    }
    else {
        DEBUG_LOG("Package has native binary");
        char *name = malloc(sizeof(char) * packageNameLength);
        fread(name, sizeof(char), packageNameLength, in);
        
        uint16_t major = readUInt16(in);
        uint16_t minor = readUInt16(in);
        
        DEBUG_LOG("Package is named %s and has version %d.%d.x", name, major, minor);
        
        PackageLoadingState s = packageLoad(name, major, minor, &linkingTable, &mpfc, &sfch);
        
        if (s == PACKAGE_INAPPROPRIATE_MAJOR) {
            error("Installed version of package \"%s\" is incompatible with required version %d.%d. (How did you made Emojicode load this version of the package?!)", name, major, minor);
        }
        else if (s == PACKAGE_INAPPROPRIATE_MINOR) {
            error("Installed version of package \"%s\" is incompatible with required version %d.%d. Please update to the latest minor version.", name, major, minor);
        }
        else if (s == PACKAGE_LOADING_FAILED) {
            error("Could not load package \"%s\" %s.", name, packageError());
        }
        
        free(name);
    }
    
    for (int classCount = readUInt16(in); classCount; classCount--) {
        DEBUG_LOG("➡️ Still %d class(es) to load", classCount);
        EmojicodeChar name = readEmojicodeChar(in);
        
        DEBUG_LOG("Loading class %X", name);
        
        Class *class = malloc(sizeof(Class));
        classTable[classNextIndex++] = class;
        
        class->superclass = classTable[readUInt16(in)];
        class->instanceVariableCount = readUInt16(in);
        
        class->methodCount = readUInt16(in);
        class->methodsVtable = malloc(sizeof(Function*) * class->methodCount);
        
        bool inheritsInitializers = fgetc(in);
        class->initializerCount = readUInt16(in);
        class->initializersVtable = malloc(sizeof(Function*) * class->initializerCount);
        
        DEBUG_LOG("Inherting intializers: %s", inheritsInitializers ? "true" : "false");
        
        DEBUG_LOG("%d instance variable(s); %d methods; %d initializer(s)",
                  class->instanceVariableCount, class->methodCount, class->initializerCount);
        
        uint_fast16_t localMethodCount = readUInt16(in);
        uint_fast16_t localInitializerCount = readUInt16(in);
        
        DEBUG_LOG("Reading %d method(s) and %d initializer(s) that are not inherited or overriden",
                  localMethodCount, localInitializerCount);
        
        if (class != class->superclass) {
            memcpy(class->methodsVtable, class->superclass->methodsVtable,
                   class->superclass->methodCount * sizeof(Function*));
            if (inheritsInitializers) {
                memcpy(class->initializersVtable, class->superclass->initializersVtable,
                       class->superclass->initializerCount * sizeof(Function*));
            }
        }
        else {
            class->superclass = NULL;
        }
        
        for (uint_fast16_t i = 0; i < localMethodCount; i++) {
            DEBUG_LOG("Reading method %d", i);
            readFunction(class->methodsVtable, in, linkingTable);
        }
        
        for (uint_fast16_t i = 0; i < localInitializerCount; i++) {
            DEBUG_LOG("Reading initializer %d", i);
            readFunction(class->initializersVtable, in, linkingTable);
        }
        
        uint_fast16_t protocolCount = readUInt16(in);
        DEBUG_LOG("Reading %d protocol(s)", protocolCount);
        if(protocolCount > 0){
            class->protocolsMaxIndex = readUInt16(in);
            class->protocolsOffset = readUInt16(in);
            class->protocolsTable = calloc((class->protocolsMaxIndex - class->protocolsOffset + 1), sizeof(Function **));
            
            DEBUG_LOG("Protocol max index is %d, offset is %d", class->protocolsMaxIndex, class->protocolsOffset);
            
            for (uint_fast16_t i = 0; i < protocolCount; i++) {
                readProtocolAgreement(class->methodsVtable, class->protocolsTable, class->protocolsOffset, in);
            }
        }
        else {
            class->protocolsTable = NULL;
        }
        
        class->mark = mpfc(name);
        size_t size = sfch(class, name);
        class->valueSize = class->superclass && class->superclass->valueSize ? class->superclass->valueSize : size;
        class->size = class->valueSize + class->instanceVariableCount * sizeof(Something);
    }

    for (int functionCount = readUInt16(in); functionCount; functionCount--) {
        DEBUG_LOG("➡️ Still %d functions to come", functionCount);
        readFunction(functionTable, in, linkingTable);
    }
}

Function* readBytecode(FILE *in) {
    uint8_t version = fgetc(in);
    if (version != ByteCodeSpecificationVersion) {
        error("The bytecode file (bcsv %d) is not compatible with this interpreter (bcsv %d).\n", version, ByteCodeSpecificationVersion);
    }
    
    DEBUG_LOG("Bytecode version %d", version);
    
    const int classCount = readUInt16(in);
    classTable = malloc(sizeof(Class*) * classCount);

    DEBUG_LOG("%d class(es) on the whole", classCount);

    const int functionCount = readUInt16(in);
    functionTable = malloc(sizeof(Function*) * functionCount);

    DEBUG_LOG("%d function(s) on the whole", functionCount);
    
    for (int i = 0, l = fgetc(in); i < l; i++) {
        DEBUG_LOG("Reading package %d of %d", i + 1, l);
        readPackage(in);
    }
    
    CL_STRING = classTable[0];
    CL_LIST = classTable[1];
    CL_ERROR = classTable[2];
    CL_DATA = classTable[3];
    CL_DICTIONARY = classTable[4];
    CL_CAPTURED_FUNCTION_CALL = classTable[5];
    CL_CLOSURE = classTable[6];
    
    DEBUG_LOG("✅ Read all packages");
    
    stringPoolCount = readUInt16(in);
    DEBUG_LOG("Reading string pool with %d strings", stringPoolCount);
    stringPool = malloc(sizeof(Object*) * stringPoolCount);
    for (int i = 0; i < stringPoolCount; i++) {
        Object *o = newObject(CL_STRING);
        String *string = o->value;

        string->length = readUInt16(in);
        string->characters = newArray(string->length * sizeof(EmojicodeChar));
        
        for (int j = 0; j < string->length; j++) {
            ((EmojicodeChar*)string->characters->value)[j] = readEmojicodeChar(in);
        }

        stringPool[i] = o;
    }
    
    DEBUG_LOG("✅ Program ready for execution");
    return functionTable[0];
}
