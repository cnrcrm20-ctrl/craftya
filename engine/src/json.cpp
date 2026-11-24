// Craftya Engine — JSON ayrıştırıcı uygulaması
#include "craftya/json.hpp"
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <cmath>

namespace craftya {

const Json Json::nullJson_{};

class JsonParser {
public:
    JsonParser(const std::string& t, std::string* err) : t_(t), err_(err) {}

    Json parse() {
        skipWs();
        Json v = parseValue();
        skipWs();
        return v;
    }

    bool ok() const { return ok_; }

private:
    const std::string& t_;
    size_t i_ = 0;
    std::string* err_;
    bool ok_ = true;

    void fail(const char* msg) {
        if (!ok_ && err_) return;
        ok_ = false;
        if (err_) {
            std::ostringstream os;
            os << msg << " (konum " << i_ << ")";
            *err_ = os.str();
        }
    }
    char peek() const { return i_ < t_.size() ? t_[i_] : '\0'; }
    char get() { return i_ < t_.size() ? t_[i_++] : '\0'; }
    void skipWs() {
        while (i_ < t_.size()) {
            char c = t_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i_;
            else break;
        }
    }
    bool match(const char* lit) {
        size_t n = std::char_traits<char>::length(lit);
        if (t_.compare(i_, n, lit) == 0) { i_ += n; return true; }
        return false;
    }

    Json parseValue() {
        skipWs();
        Json v;
        char c = peek();
        if (c == '{') { parseObject(v); return v; }
        if (c == '[') { parseArray(v); return v; }
        if (c == '"') { v.type_ = Json::Type::String; v.s_ = parseString(); return v; }
        if (c == 't' && match("true")) { v.type_ = Json::Type::Bool; v.b_ = true; return v; }
        if (c == 'f' && match("false")) { v.type_ = Json::Type::Bool; v.b_ = false; return v; }
        if (c == 'n' && match("null")) { v.type_ = Json::Type::Null; return v; }
        if (c == '-' || (c >= '0' && c <= '9')) { v.type_ = Json::Type::Number; v.n_ = parseNumber(); return v; }
        fail("beklenmeyen karakter");
        return v;
    }

    void parseObject(Json& out) {
        out.type_ = Json::Type::Object;
        get(); // {
        skipWs();
        if (peek() == '}') { get(); return; }
        while (ok_) {
            skipWs();
            if (peek() != '"') { fail("anahtar dize olmalı"); return; }
            std::string key = parseString();
            skipWs();
            if (get() != ':') { fail("':' bekleniyordu"); return; }
            Json val = parseValue();
            out.obj_[key] = std::move(val);
            skipWs();
            char c = get();
            if (c == ',') continue;
            if (c == '}') return;
            fail("',' veya '}' bekleniyordu");
            return;
        }
    }

    void parseArray(Json& out) {
        out.type_ = Json::Type::Array;
        get(); // [
        skipWs();
        if (peek() == ']') { get(); return; }
        while (ok_) {
            out.arr_.push_back(parseValue());
            skipWs();
            char c = get();
            if (c == ',') continue;
            if (c == ']') return;
            fail("',' veya ']' bekleniyordu");
            return;
        }
    }

    std::string parseString() {
        std::string s;
        get(); // "
        while (i_ < t_.size()) {
            char c = get();
            if (c == '"') return s;
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        // \uXXXX → UTF-8 (BMP)
                        unsigned cp = 0;
                        for (int k = 0; k < 4 && i_ < t_.size(); ++k) {
                            char h = get();
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        }
                        if (cp < 0x80) s += static_cast<char>(cp);
                        else if (cp < 0x800) {
                            s += static_cast<char>(0xC0 | (cp >> 6));
                            s += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            s += static_cast<char>(0xE0 | (cp >> 12));
                            s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            s += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: s += e; break;
                }
            } else {
                s += c;
            }
        }
        fail("dize kapanmadı");
        return s;
    }

    double parseNumber() {
        size_t start = i_;
        if (peek() == '-') ++i_;
        while (i_ < t_.size() && std::isdigit(static_cast<unsigned char>(t_[i_]))) ++i_;
        if (peek() == '.') { ++i_; while (i_ < t_.size() && std::isdigit((unsigned char)t_[i_])) ++i_; }
        if (peek() == 'e' || peek() == 'E') {
            ++i_;
            if (peek() == '+' || peek() == '-') ++i_;
            while (i_ < t_.size() && std::isdigit((unsigned char)t_[i_])) ++i_;
        }
        return std::strtod(t_.substr(start, i_ - start).c_str(), nullptr);
    }
};

Json Json::parse(const std::string& text, std::string* error) {
    JsonParser p(text, error);
    Json v = p.parse();
    if (!p.ok() && error && error->empty()) *error = "ayrıştırma hatası";
    return v;
}

Json Json::parseFile(const std::string& path, std::string* error) {
    std::ifstream f(path);
    if (!f) {
        if (error) *error = "dosya açılamadı: " + path;
        return Json{};
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parse(ss.str(), error);
}

const Json& Json::operator[](const std::string& key) const {
    if (type_ != Type::Object) return nullJson_;
    auto it = obj_.find(key);
    return it == obj_.end() ? nullJson_ : it->second;
}

bool Json::has(const std::string& key) const {
    return type_ == Type::Object && obj_.find(key) != obj_.end();
}

std::vector<std::string> Json::keys() const {
    std::vector<std::string> out;
    out.reserve(obj_.size());
    for (auto& kv : obj_) out.push_back(kv.first);
    return out;
}

const Json& Json::at(size_t i) const {
    if (type_ != Type::Array || i >= arr_.size()) return nullJson_;
    return arr_[i];
}

static void escapeInto(std::string& out, const std::string& s) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    out += '"';
}

std::string Json::dump(int indent) const {
    std::string out;
    std::string pad(static_cast<size_t>(indent) * 2, ' ');
    std::string pad2(static_cast<size_t>(indent + 1) * 2, ' ');
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Number: {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%g", n_);
            out += buf;
            break;
        }
        case Type::String: escapeInto(out, s_); break;
        case Type::Array: {
            if (arr_.empty()) { out += "[]"; break; }
            out += "[\n";
            for (size_t i = 0; i < arr_.size(); ++i) {
                out += pad2 + arr_[i].dump(indent + 1);
                if (i + 1 < arr_.size()) out += ',';
                out += '\n';
            }
            out += pad + "]";
            break;
        }
        case Type::Object: {
            if (obj_.empty()) { out += "{}"; break; }
            out += "{\n";
            size_t n = 0;
            for (auto& kv : obj_) {
                out += pad2;
                escapeInto(out, kv.first);
                out += ": ";
                out += kv.second.dump(indent + 1);
                if (++n < obj_.size()) out += ',';
                out += '\n';
            }
            out += pad + "}";
            break;
        }
    }
    return out;
}

std::vector<BlockDefJson> parseBlockDefinitions(const std::string& jsonText, std::string* error) {
    std::vector<BlockDefJson> out;
    std::string err;
    Json root = Json::parse(jsonText, &err);
    if (!err.empty()) {
        if (error) *error = err;
        return out;
    }
    // hem dizi hem {"blocks": [...]} biçimini kabul et
    const Json* list = &root;
    if (root.isObject() && root.has("blocks")) list = &root["blocks"];
    Json emptyArr;
    if (!list->isArray()) {
        if (error) *error = "blocks dizisi bekleniyordu";
        return out;
    }
    for (size_t i = 0; i < list->size(); ++i) {
        const Json& b = list->at(i);
        BlockDefJson d;
        d.id = b["id"].asInt(static_cast<int>(i));
        d.key = b["key"].asString();
        if (d.key.empty()) d.key = b["name"].asString();
        d.displayName = b.has("display_name") ? b["display_name"].asString() : b["name"].asString();
        if (b.has("tex")) {
            const Json& tex = b["tex"];
            d.texTop = tex["top"].asInt(0);
            d.texSide = tex["side"].asInt(0);
            d.texBottom = tex.has("bottom") ? tex["bottom"].asInt(d.texSide) : d.texSide;
        } else {
            d.texTop = b["tex_top"].asInt(0);
            d.texSide = b["tex_side"].asInt(0);
            d.texBottom = b.has("tex_bottom") ? b["tex_bottom"].asInt(d.texSide) : d.texSide;
        }
        d.hardness = b["hardness"].asFloat(1.f);
        d.tool = b["tool"].asString();
        d.light = b["light"].asInt(0);
        d.opaque = b.has("opaque") ? b["opaque"].asBool(true) : true;
        d.solid = b.has("solid") ? b["solid"].asBool(true) : true;
        if (d.id >= 0 && d.id < 65536) out.push_back(d);
    }
    return out;
}

} // namespace craftya
