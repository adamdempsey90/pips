if exists("b:current_syntax")
  finish
endif

syntax case match

syntax match pipsComment /#.*$/
syntax region pipsString start=/"/ end=/"/
syntax match pipsNumber /\v(\<\d+(\.\d*)?|\.\d+)([eEdD][+-]?\d+)?/

syntax keyword pipsConditional if else
syntax keyword pipsRepeat for while
syntax keyword pipsStatement return
syntax keyword pipsKeyword var fn class new this super
syntax keyword pipsOperatorWord and or not
syntax keyword pipsBoolean true false
syntax keyword pipsConstant nil pi
syntax keyword pipsBuiltin print setattr getattr hasattr str
syntax keyword pipsBuiltin exp sin cos tan abs log log10 sign sqrt acos asin atan ceil floor env atan2 min max
syntax keyword pipsInspector __list__ __globals__ __locals__ __stack__ __funcs__

syntax match pipsFunction /\<fn\>\s\+\zs[A-Za-z_][A-Za-z0-9_]*/
syntax match pipsClass /\<class\>\s\+\zs[A-Za-z_][A-Za-z0-9_]*/
syntax match pipsType /\<new\>\s\+\zs[A-Za-z_][A-Za-z0-9_]*/
syntax match pipsMember /\.\zs[A-Za-z_][A-Za-z0-9_]*/
syntax match pipsOperator /<<=\|>>=\|++\|--\|+=\|-=\|\*=\|\/=\|%=\||=\|&=\|\*\*\|\/\/\|==\|!=\|>=\|<=\|<<\|>>/
syntax match pipsOperator /[?::~^|&+*%=!<>\/-]/
syntax match pipsDelimiter /[(){}\[\],.;]/

highlight default link pipsComment Comment
highlight default link pipsString String
highlight default link pipsNumber Number
highlight default link pipsConditional Conditional
highlight default link pipsRepeat Repeat
highlight default link pipsStatement Statement
highlight default link pipsKeyword Keyword
highlight default link pipsOperatorWord Operator
highlight default link pipsBoolean Boolean
highlight default link pipsConstant Constant
highlight default link pipsBuiltin Function
highlight default link pipsInspector Macro
highlight default link pipsFunction Function
highlight default link pipsClass Type
highlight default link pipsType Type
highlight default link pipsMember Identifier
highlight default link pipsOperator Operator
highlight default link pipsDelimiter Delimiter

let b:current_syntax = "pips"