// Craftya Engine — JSON ayrıştırıcı (sunucudan gelen blok/mod/harita tanımları)
// Sıfır bağımlılık, tahsis dostu, hata konumlu.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace craftya {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    static Json parse(const std::string& text, std::string* error = nullptr);
    static Json parseFile(const std::string& path, std::string* error = nullptr);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }

    bool asBool(bool def = false) const { return type_ == Type::Bool ? b_ : def; }
    double asNumber(double def = 0.0) const { return type_ == Type::Number ? n_ : def; }
    int asInt(int def = 0) const { return type_ == Type::Number ? static_cast<int>(n_) : def; }
    float asFloat(float def = 0.f) const { return type_ == Type::Number ? static_cast<float>(n_) : def; }
    const std::string& asString() const { return s_; }

    // nesne erişimi
    const Json& operator[](const std::string& key) const;
    bool has(const std::string& key) const;
    std::vector<std::string> keys() const;

    // dizi erişimi
    size_t size() const { return arr_.size(); }
    const Json& at(size_t i) const;

    std::string dump(int indent = 0) const;

private:
    Type type_ = Type::Null;
    bool b_ = false;
    double n_ = 0.0;
    std::string s_;
    std::vector<Json> arr_;
    std::unordered_map<std::string, Json> obj_;
    static const Json nullJson_;

    friend class JsonParser;
};

// --- blok tanımını sunucu JSON'undan okuma ---
struct BlockDefJson {
    int id = 0;
    std::string key;
    std::string displayName;
    int texTop = 0, texSide = 0, texBottom = 0;
    float hardness = 1.f;
    std::string tool;
    int light = 0;
    bool opaque = true;
    bool solid = true;
};

std::vector<BlockDefJson> parseBlockDefinitions(const std::string& jsonText, std::string* error = nullptr);

} // namespace craftya
