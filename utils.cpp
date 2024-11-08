ImVec2 operator- (const ImVec2& a, const ImVec2 & b) {
    return {a.x - b.x, a.y - b.y};
}

ImVec2 operator+ (const ImVec2& a, const ImVec2 & b) {
    return {a.x + b.x, a.y + b.y};
}

ImVec2 & operator+= (ImVec2& a, const ImVec2 & b) {
    a = a + b;
    return a;
}

ImVec2 operator/ (const ImVec2& a, const float & b) {
    return {a.x / b, a.y / b};
}

ImVec2 operator* (const ImVec2& a, const float & b) {
    return {a.x * b, a.y * b};
}

bool operator> (const ImVec2& a, const ImVec2 & b) {
    return a.x > b.x and a.y > b.y;
}

auto min(auto a, auto b) {
  return a < b ? a : b;
}

auto max(auto a, auto b) {
  return a > b ? a : b;
}

auto clip(auto a, auto min, auto max) {
  return ::min(::max(a, min), max);
}
