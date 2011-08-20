# .zshrc

# User specific aliases and functions
alias grep='grep --color=always'
alias ll='ls -l --color'

export LESSCHARSET=utf-8

#
# thx. http://ho-ki-boshi.blogspot.com/2007/12/zsh.html
#
local LEFTC=$'%{\e[1;32m%}' #ターミナル.app 用
local RIGHTC=$'%{\e[1;34m%}' #ターミナル.app 用
#local LEFTC=$'%{\e[38;5;30m%}'
#local RIGHTC=$'%{\e[38;5;88m%}'
local DEFAULTC=$'%{\e[m%}'
PROMPT=$LEFTC"%U%%%u "$DEFAULTC
#PROMPT=$LEFTC"%U$USER%%%u "$DEFAULTC
#export RPROMPT=$RIGHTC"[%~]"$DEFAULTC
#PROMPT2="%_%% "
#SPROMPT="%r is correct? [n,y,a,e]: "

# プロンプトのカラー表示を有効
autoload -U colors
colors

# デフォルトの補完機能を有効
autoload -U compinit
compinit
