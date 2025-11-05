#include <clang-c/Index.h>
#include <iostream>
#include <string>

// Convert CXString to std::string safely
std::string toString(CXString str) {
    std::string result = clang_getCString(str);
    clang_disposeString(str);
    return result;
}

// Helper: check if a struct has a member function named "toString"
// bool hasToStringMethod(CXCursor structCursor) {
//     bool found = false;

//     clang_visitChildren(
//         structCursor,
//         [](CXCursor c, CXCursor, CXClientData data) {
//             CXCursorKind kind = clang_getCursorKind(c);
//             if (kind == CXCursor_CXXMethod) {
//                 std::string name = toString(clang_getCursorSpelling(c));
//                 if (name == "toString") {
//                     bool *flag = static_cast<bool *>(data);
//                     *flag = true;
//                     return CXChildVisit_Break;
//                 }
//             }
//             return CXChildVisit_Continue;
//         },
//         &found);

//     return found;
// }

// Main visitor
CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData) {
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl) {
        std::string structName = toString(clang_getCursorSpelling(cursor));
        if (structName.empty())
            structName = "<anonymous>";

        // Skip forward declarations (incomplete types)
        CXType structType = clang_getCursorType(cursor);
        if (clang_isInvalid(clang_getCursorKind(cursor)) || clang_Type_getSizeOf(structType) < 0)
            return CXChildVisit_Continue;

        // Count fields
        int memberCount = 0;
        clang_visitChildren(
            cursor,
            [](CXCursor c, CXCursor, CXClientData data) {
                if (clang_getCursorKind(c) == CXCursor_FieldDecl) {
                    int *count = static_cast<int *>(data);
                    (*count)++;
                }
                return CXChildVisit_Continue;
            },
            &memberCount);

        // Get size
        long long size = clang_Type_getSizeOf(structType);
        if (size < 0) size = 0;

        std::cout << "Struct: " << structName << "\n";
        std::cout << "  Size: " << size << " bytes\n";
        std::cout << "  Members: " << memberCount << "\n";
        std::cout << "----------------------\n";
    }

    return CXChildVisit_Recurse;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source.cpp>\n";
        return 1;
    }

    const char *filename = argv[1];
    CXIndex index = clang_createIndex(0, 0);

    CXTranslationUnit tu = clang_parseTranslationUnit(
        index, filename,
        nullptr, 0,
        nullptr, 0,
        CXTranslationUnit_None);

    if (!tu) {
        std::cerr << "Failed to parse translation unit: " << filename << "\n";
        clang_disposeIndex(index);
        return 1;
    }

    CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(rootCursor, visitor, nullptr);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return 0;
}
