#ifndef TOKEN_FACTORY_H
#define TOKEN_FACTORY_H
#include <string>
#include "./location.h"
#include "./token.h"
#include "./token_utils.h"

namespace frontend {

class TokenFactory {
 public:
  // 获取全局单例实例
  static auto GetInstance() -> TokenFactory & {
    static TokenFactory instance;
    return instance;
  }

  // 禁止拷贝和赋值
  TokenFactory(const TokenFactory &) = delete;
  auto operator=(const TokenFactory &) -> TokenFactory & = delete;

  // Factory method to create Token instances
  auto CreateToken(TokenType type, std::string &lexeme, SourceRange &source_range) -> Token {
    return {type, lexeme, source_range};
  }

 private:
  // 私有构造函数，防止外部实例化
  TokenFactory() = default;
};

};  // namespace frontend

#endif