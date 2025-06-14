#ifndef IROPERAND_H
#define IROPERAND_H

#include <string>


namespace ir {

enum class Type {
    Int,
    Float,
    IntLiteral,
    FloatLiteral,
    IntPtr,
    FloatPtr,
    null
};

std::string toString(Type t);

struct Operand {
    std::string name;
    Type type;
    Operand(std::string = "null", Type = Type::null);
};

// allow using Operand as key in map
inline bool operator<(const Operand& a, const Operand& b) {
    if (a.name != b.name) return a.name < b.name;
    return static_cast<int>(a.type) < static_cast<int>(b.type);
}

}

#endif
