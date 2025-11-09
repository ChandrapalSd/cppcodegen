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

std::string getQualifiedName(CXCursor cursor) {
    std::string name = toString(clang_getCursorSpelling(cursor));
    CXCursor parent = clang_getCursorSemanticParent(cursor);

    while (!clang_Cursor_isNull(parent) &&
           clang_getCursorKind(parent) != CXCursor_TranslationUnit) {

        std::string parentName = toString(clang_getCursorSpelling(parent));
        if (!parentName.empty())
            name = parentName + "::" + name;

        parent = clang_getCursorSemanticParent(parent);
    }

    return name;
}


// Main visitor
CXChildVisitResult visitor(CXCursor cursor, CXCursor parent, CXClientData) {
    CXCursorKind kind = clang_getCursorKind(cursor);
    if(!clang_Location_isFromMainFile(clang_getCursorLocation(cursor)))
        return CXChildVisit_Continue;

    
    const std::string FORMAT_STR = "{}";
    const std::string COMMA = ",";
    const std::string DOUBLE_QUOTE = "\"";
    const std::string OPEN_CURLY = "{";
    const std::string CLOSE_CURLY = "}";
    const std::string INDENT = "    ";

    if (kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl) {
        std::string structName = getQualifiedName(cursor);//toString(clang_getCursorSpelling(cursor));
        if (structName.empty())
            return CXChildVisit_Continue;

        // Skip forward declarations (incomplete types)
        CXType structType = clang_getCursorType(cursor);
        if (clang_isInvalid(clang_getCursorKind(cursor)) || clang_Type_getSizeOf(structType) < 0)
            return CXChildVisit_Continue;

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

        std::println(R"(template <>)");
        std::println(R"(struct fmt::formatter<{}> {})", structName, OPEN_CURLY);
        std::println(R"({}constexpr auto parse(fmt::format_parse_context& ctx) {}return ctx.begin();{})", INDENT, OPEN_CURLY, CLOSE_CURLY);
        std::println(R"({}auto format(const {}& s, fmt::format_context& ctx) const {})", INDENT, structName, OPEN_CURLY);
        std::println(R"({}return fmt::format_to(ctx.out(),)", INDENT+INDENT);

        std::println(R"({}"{} ")", INDENT+INDENT+INDENT, OPEN_CURLY);
        for(const auto& m: memberInfos)
        {
            std::println(R"({}"{} = {}{} ")", INDENT+INDENT+INDENT, m.name, FORMAT_STR, &m != &memberInfos.back() ? COMMA : "");
        }
        std::println(R"({}"{}",)", INDENT+INDENT+INDENT, CLOSE_CURLY);

        for(const auto& m: memberInfos)
        {
            std::println(R"({}s.{}{})", INDENT+INDENT+INDENT, m.useToString ? std::format("toString(&{}::{})", structName, m.name) : m.name , &m != &memberInfos.back() ? COMMA : "");
        }
        std::println("{});", INDENT+INDENT);
        std::println("{}{}", INDENT, CLOSE_CURLY);
        std::println("{};\n", CLOSE_CURLY);
    }
    else if(kind == CXCursor_EnumDecl)
    {
        // CXString spelling = clang_getCursorSpelling(cursor);
        // auto enumName = std::string(clang_getCString(spelling));
        auto enumName = getQualifiedName(cursor);
        
        std::vector<std::string> enumConstantInfos;

        std::println(R"(template <>)");
        std::println(R"(struct fmt::formatter<{}> : fmt::formatter<std::string_view> {})", enumName, OPEN_CURLY);
        std::println(R"({}auto format({} e, fmt::format_context& ctx) const {})", INDENT, enumName, OPEN_CURLY);
        std::println(R"({}std::string result;)", INDENT+INDENT);
        std::println(R"({}switch (e) {})", INDENT+INDENT, OPEN_CURLY);

        clang_visitChildren(
            cursor,
            [](CXCursor c, CXCursor, CXClientData data) {
                auto enumConstantInfos = static_cast<std::vector<std::string> *>(data);
                auto *mInfos = static_cast<std::vector<MemberInfo> *>(data);
                CXCursorKind kind = clang_getCursorKind(c);
                CXString cxSpellingConstant = clang_getCursorSpelling(c);
                const auto enumConstantName = clang_getCString(cxSpellingConstant);
                enumConstantInfos->push_back(enumConstantName);

                clang_disposeString(cxSpellingConstant);
                return CXChildVisit_Continue;
            },
            &enumConstantInfos
        );

        for(const auto& enumConstantName : enumConstantInfos)
        {
            std::println(R"({}case {}::{} : result = "{}" ; break;)", INDENT+INDENT+INDENT, enumName, enumConstantName, enumConstantName);
        }
        std::println(R"({}default: result = "Unknown {}"; break;)", INDENT+INDENT+INDENT, enumName);
        std::println(R"({}{})", INDENT+INDENT, CLOSE_CURLY);
        std::println(R"({}return fmt::formatter<std::string_view>::format()", INDENT+INDENT);
        std::println(R"({}fmt::format("{} ({}){}, result, static_cast<std::underlying_type_t<{}>>(e)),)", INDENT+INDENT, FORMAT_STR, FORMAT_STR, DOUBLE_QUOTE, enumName);
        std::println(R"({}ctx)", INDENT+INDENT);
        std::println(R"({});)", INDENT+INDENT);
        std::println(R"({}{})", INDENT, CLOSE_CURLY);
        std::println(R"({};)", CLOSE_CURLY);
        std::print("\n\n");

        // clang_disposeString(spelling);
    }
    return CXChildVisit_Recurse;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::println("Usage: {} <source.cpp>", argv[0]);
        return 1;
    }

    std::println("// [auto generated]");

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
