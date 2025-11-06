#include <clang-c/Index.h>
#include <print>
#include <string>
#include <vector>

struct MemberInfo
{
    std::string name;
    bool useToString = false;

    MemberInfo(const std::string& name, bool useToString = false)
        :name(name), useToString(useToString)
    {}
};

// Convert CXString to std::string safely
std::string toString(CXString str) {
    std::string result = clang_getCString(str);
    clang_disposeString(str);
    return result;
}

// Main visitor
CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    if(!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl) {
        std::string structName = toString(clang_getCursorSpelling(cursor));
        if (structName.empty())
            return CXChildVisit_Continue;

        // Skip forward declarations (incomplete types)
        CXType structType = clang_getCursorType(cursor);
        if (clang_isInvalid(clang_getCursorKind(cursor)) || clang_Type_getSizeOf(structType) < 0)
            return CXChildVisit_Continue;

        // Count fields
        std::vector<MemberInfo> memberInfos;

        clang_visitChildren(
            cursor,
            [](CXCursor c, CXCursor, CXClientData data) {
                auto *mInfos = static_cast<std::vector<MemberInfo> *>(data);
                CXCursorKind kind = clang_getCursorKind(c);
                CXString spelling = clang_getCursorSpelling(c);
                switch (kind)
                {
                case CXCursor_FieldDecl:
                {
                    auto nameOfField = std::string(clang_getCString(spelling));
                    if(!nameOfField.starts_with('_'))
                        mInfos->emplace_back(nameOfField);
                }
                break;
                case CXCursor_CXXMethod:
                {
                    auto nameOfFunction = std::string(clang_getCString(spelling));
                    if(nameOfFunction != "toString")
                        break;
                    
                    int argCount = clang_Cursor_getNumArguments(c);
                    if(argCount != 1)
                        break;
                    
                    CXCursor arg = clang_Cursor_getArgument(c, 0);
                    CXString argName = clang_getCursorSpelling(arg);
                    
                    auto itArgNameMember = std::find_if(mInfos->begin(), mInfos->end(), [&](const MemberInfo& m){ return m.name == std::string(clang_getCString(argName)); });
                    clang_disposeString(argName);
                    if(itArgNameMember == mInfos->end())
                        break;
                    itArgNameMember->useToString = true;
                }
                break;
                }
                clang_disposeString(spelling);

                return CXChildVisit_Continue;
            },
            &memberInfos
        );


        std::println("Struct: {}", structName);

        const std::string FORMAT_STR = "{}";
        const std::string COMMA = ",";
        const std::string OPEN_CURLY = "{";
        const std::string CLOSE_CURLY = "}";
    
        std::println(R"(template <>)");
        std::println(R"(struct fmt::formatter<{}> {})", structName, OPEN_CURLY);
        std::println(R"(constexpr auto parse(fmt::format_parse_context& ctx) {}return ctx.begin();{})", OPEN_CURLY, CLOSE_CURLY);
        std::println(R"(auto format(const {}& s, fmt::format_context& ctx) const {})", structName, OPEN_CURLY);
        std::println(R"(return fmt::format_to(ctx.out())");

        std::println(R"("{} ")", OPEN_CURLY);
        for(const auto& m: memberInfos)
        {
            std::println(R"("{} = {}{} ")", m.name, FORMAT_STR, &m != &memberInfos.back() ? COMMA : "");
        }
        std::println(R"("{}",)", CLOSE_CURLY);

        for(const auto& m: memberInfos)
        {
            std::println(R"(s.{}{})", m.name, &m != &memberInfos.back() ? COMMA : "");
        }
        std::println(");");
        std::println("{}", CLOSE_CURLY);
        std::println("{};", CLOSE_CURLY);
        std::println("//-----------------------------------");
    }

    return CXChildVisit_Recurse;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::println("Usage: {} <source.cpp>", argv[0]);
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
        std::println("Failed to parse translation unit: {}", filename);
        clang_disposeIndex(index);
        return 1;
    }

    CXCursor rootCursor = clang_getTranslationUnitCursor(tu);
    clang_visitChildren(rootCursor, visitor, nullptr);

    clang_disposeTranslationUnit(tu);
    clang_disposeIndex(index);
    return 0;
}
